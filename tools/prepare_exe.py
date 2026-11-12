"""Prepare Juiced 1.00's EXE (the build without copy protection) for the lifter: put back the code its
protection had moved out of the game's functions.

SecuROM moved instructions out of game functions into stubs placed where
relocated functions used to be, and left `nop* ; jmp stub` at the site. A stub
runs the moved instructions, may compute a constant from two image dwords
behind pushfd/popfd, and returns to site+5 with `jmp` or a computed `ret`.
This writes the instructions back in place, with the constant folded.

usage: prepare_exe.py <in.exe> <instructions.txt[.gz]> <out.exe> <report.tsv>
instructions: the input's instruction addresses (hex, first column; prep/instructions.txt.gz).
"""
import bisect
import gzip
import hashlib
import re
import struct
import sys

import capstone
from capstone import x86 as cx
from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn import x86_const as ux

BASE = 0x400000
GAME_TEXT = (0x401000, 0x6C9000)
SECUROM = (0x104C000, 0x1073000)
STALE_MAX = 0x400
VM_ENTRY = 0x7AF270
INPUT_SHA256 = '6e9b5d5ed5e3bd2319da5ab7d73dd396c78fc95cacbd40101427dc6f03ca9d1b'

REGS = ['eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi']
UREG = [ux.UC_X86_REG_EAX, ux.UC_X86_REG_ECX, ux.UC_X86_REG_EDX, ux.UC_X86_REG_EBX,
        ux.UC_X86_REG_ESP, ux.UC_X86_REG_EBP, ux.UC_X86_REG_ESI, ux.UC_X86_REG_EDI]
STACK_TOP = 0x200000


class Image:
    def __init__(self, path):
        self.raw = bytearray(open(path, 'rb').read())
        b = self.raw
        pe = struct.unpack_from('<I', b, 0x3C)[0]
        n = struct.unpack_from('<H', b, pe + 6)[0]
        sec = pe + 24 + struct.unpack_from('<H', b, pe + 20)[0]
        self.secs = [struct.unpack_from('<IIII', b, sec + 40 * i + 8) for i in range(n)]

    def off(self, va):
        rva = va - BASE
        for vs, v, rs, ro in self.secs:
            if v <= rva < v + min(vs, rs):
                return rva - v + ro
        raise KeyError(hex(va))

    def read(self, va, n):
        o = self.off(va)
        return bytes(self.raw[o:o + n])

    def write(self, va, data):
        o = self.off(va)
        self.raw[o:o + len(data)] = data

    def demote_securom_sections(self):
        """SecuROM's .data (a copy of its code plus junk) and .init (its loader) become data:
        the lifter scans every '.data' for code pointers and every executable section for code."""
        b = self.raw
        pe = struct.unpack_from('<I', b, 0x3C)[0]
        sec = pe + 24 + struct.unpack_from('<H', b, pe + 20)[0]
        seen_data = False
        for i in range(len(self.secs)):
            h = sec + 40 * i
            name = bytes(b[h:h + 8]).rstrip(b'\0')
            if name == b'.data' and not seen_data:
                seen_data = True
                continue
            if name in (b'.data', b'.init'):
                if name == b'.data':
                    b[h:h + 8] = b'.srdata\0'
                ch = struct.unpack_from('<I', b, h + 36)[0]
                struct.pack_into('<I', b, h + 36, ch & ~0x20000000)


def in_range(a, r):
    return r[0] <= a < r[1]


def make_emu(img):
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    for vs, v, rs, ro in img.secs:
        start = (BASE + v) & ~0xFFF
        size = ((BASE + v + max(vs, rs) + 0xFFF) & ~0xFFF) - start
        uc.mem_map(start, size)
        uc.mem_write(BASE + v, bytes(img.raw[ro:ro + min(vs, rs)]))
    uc.mem_map(STACK_TOP - 0x10000, 0x10000)
    return uc


def emulate(uc, start, end):
    """Runs [start, end) with sentinel registers; returns (regs, stack, writes, reads)."""
    for i, r in enumerate(UREG):
        uc.reg_write(r, 0x11110000 + i * 0x1000 if i != 4 else STACK_TOP - 0x100)
    uc.reg_write(ux.UC_X86_REG_EFLAGS, 0x202)
    uc.mem_write(STACK_TOP - 0x200, b'\0' * 0x200)
    writes, reads = [], []
    h1 = uc.hook_add(UC_HOOK_MEM_WRITE, lambda u, a, addr, size, val, d: writes.append((addr, size, val)))
    h2 = uc.hook_add(UC_HOOK_MEM_READ, lambda u, a, addr, size, val, d: reads.append((addr, size)))
    try:
        uc.emu_start(start, end)
    finally:
        uc.hook_del(h1)
        uc.hook_del(h2)
    regs = [uc.reg_read(r) for r in UREG]
    return regs, writes, reads


def enc_mov_load(reg, addr):
    return b'\xA1' + struct.pack('<I', addr) if reg == 0 else bytes([0x8B, 0x05 | reg << 3]) + struct.pack('<I', addr)


def enc_mov_store(reg, addr):
    return b'\xA3' + struct.pack('<I', addr) if reg == 0 else bytes([0x89, 0x05 | reg << 3]) + struct.pack('<I', addr)


ALU_EXT = {'add': 0, 'or': 1, 'adc': 2, 'sbb': 3, 'and': 4, 'sub': 5, 'xor': 6, 'cmp': 7}


def fold_integrity_reads(img, base, listing, md):
    """SecuROM mixes bytes of game code into constants (`mov r,[A]; sub r,[B]`). Reads of [B]
    that see bytes this tool rewrote get the input image's value as an immediate, flags intact."""
    rows = [int(l.split('\t', 1)[0], 16) for l in read_lines(listing)]
    rows = [a for a in rows if in_range(a, SECUROM) or in_range(a, GAME_TEXT)]
    folded, left = [], {}
    while True:
        n_before = len(folded)
        for a in rows:
            if img.read(a, 6) != base.read(a, 6):
                continue
            i = next(md.disasm(base.read(a, 16), a))
            if len(i.operands) != 2:
                continue
            o0, o1 = i.operands
            if not (o1.type == cx.X86_OP_MEM and o1.mem.base == 0 and o1.mem.index == 0 and o1.mem.segment == 0
                    and in_range(o1.mem.disp, GAME_TEXT) and base.read(o1.mem.disp, 4) != img.read(o1.mem.disp, 4)):
                continue
            if o0.type != cx.X86_OP_REG or o0.size != 4 or o1.size != 4:
                left[a] = 'form ' + i.mnemonic
                continue
            reg = REGS.index(i.reg_name(o0.reg))
            imm = base.read(o1.mem.disp, 4)
            if i.mnemonic == 'mov':
                new = bytes([0xB8 + reg]) + imm
            elif i.mnemonic in ALU_EXT:
                new = bytes([0x81, 0xC0 | ALU_EXT[i.mnemonic] << 3 | reg]) + imm
            else:
                left[a] = 'form ' + i.mnemonic
                continue
            if len(new) > i.size:
                left[a] = 'too long'
                continue
            img.write(a, new + b'\x90' * (i.size - len(new)))
            left.pop(a, None)
            folded.append((a, i.mnemonic, int.from_bytes(imm, 'little')))
        if len(folded) == n_before:
            break
    return folded, sorted(left.items())


API_TRAMPOLINE = 0xC3C790
API_KEY_XOR = 0x214D4C48


def fold_api_trampolines(img, base):
    """Calls through 0xC3C790 pass a key K (a code address) as first stack argument; the trampoline
    reads IAT slot [[K]^API_KEY_XOR] and `ret`s into the API, which the lifter takes as a return.
    The site becomes `call [slot]`; K is read from the input image."""
    folded = []
    lo, hi = SECUROM
    data = img.read(lo, hi - lo)
    pats = [  # push K; mov eax,X; pushfd; xor eax,Y; popfd; call eax
        (re.compile(rb'\x68(....)\xB8(....)\x9C\x35(....)\x9D\xEB?\xFF\xD0', re.S), lambda m: (m.group(1), m.group(2), m.group(3))),
        # push V; push K; pushfd; xor [esp+8],Y; popfd; pop eax; xchg [esp],eax; call eax
        (re.compile(rb'\x68(....)\x68(....)\x9C\x81\x74\x24\x08(....)\x9D\x58\x87\x04\x24\xEB?\xFF\xD0', re.S), lambda m: (m.group(2), m.group(1), m.group(3))),
    ]
    for rx, parts in pats:
        for m in rx.finditer(data):
            k, x, y = (struct.unpack('<I', b)[0] for b in parts(m))
            if x ^ y != API_TRAMPOLINE or not in_range(k, GAME_TEXT):
                continue
            slot = struct.unpack('<I', base.read(k, 4))[0] ^ API_KEY_XOR
            site = lo + m.start()
            img.write(site, b'\xFF\x15' + struct.pack('<I', slot) + b'\x90' * (m.end() - m.start() - 6))
            folded.append((site, slot))
    return folded


def fold_vm_tailjumps(img, md):
    """`call $+5; add [esp],K; jmp VM` (padding nops/self-moves between) with the jmp ending at
    site+5+K is a call to the VM that returns to the code after the jmp; the lifter makes it a tail call."""
    def junk(i):
        return i.mnemonic == 'nop' or (i.mnemonic in ('lea', 'mov', 'xchg') and len(i.operands) == 2
                                       and i.operands[0].type == cx.X86_OP_REG and i.operands[1].type == cx.X86_OP_REG
                                       and i.operands[0].reg == i.operands[1].reg) or (
            i.mnemonic == 'lea' and i.op_str.split(',')[0] == i.op_str.split('[')[1].rstrip(']'))

    folded = []
    for lo, hi in (GAME_TEXT, SECUROM):
        data = img.read(lo, hi - lo)
        i = data.find(b'\xE8\0\0\0\0')
        while i >= 0:
            site, i = lo + i, data.find(b'\xE8\0\0\0\0', i + 1)
            a, k, ok = site + 5, None, False
            for _ in range(8):
                ins = next(md.disasm(img.read(a, 16), a))
                if k is None and ins.mnemonic == 'add' and ins.op_str.startswith('dword ptr [esp], '):
                    k = int(ins.op_str.split(', ')[1], 16)
                elif ins.mnemonic == 'jmp' and k is not None:
                    ok = ins.operands[0].type == cx.X86_OP_IMM and ins.operands[0].imm == VM_ENTRY and ins.address + ins.size == site + 5 + k
                    break
                elif not junk(ins):
                    break
                a += ins.size
            if ok:
                img.write(site, b'\xE8' + struct.pack('<i', VM_ENTRY - site - 5) + b'\x90' * k)
                folded.append((site, VM_ENTRY))
    return folded


def fold_pushret_calls(img, uc, md):
    """`call $+5; push r; clc; mov r,[A]; sbb r,[B]; xchg [esp],r; add [esp+4],K; ret` is a call
    to A-B returning past the ret; the lifter takes the ret for the function's return."""
    folded = []
    esp0 = STACK_TOP - 0x100
    for lo, hi in (GAME_TEXT, SECUROM):
        data = img.read(lo, hi - lo)
        i = data.find(b'\xE8\0\0\0\0')
        while i >= 0:
            site, i = lo + i, data.find(b'\xE8\0\0\0\0', i + 1)
            for k, r in enumerate(UREG):
                uc.reg_write(r, 0x11110000 + k * 0x1000 if k != 4 else esp0)
            ret = [None]

            def hook(u, a, size, _):
                if u.mem_read(a, 1) == b'\xC3':
                    ret[0] = a
                    u.emu_stop()
            h = uc.hook_add(UC_HOOK_CODE, hook)
            try:
                uc.emu_start(site, 0xFFFFFFFF, count=24)
            except UcError:
                continue
            finally:
                uc.hook_del(h)
            if ret[0] is None:
                continue
            regs = [uc.reg_read(r) for r in UREG]
            target, back = struct.unpack('<II', uc.mem_read(regs[4], 8))
            scratch = (0,) if target == VM_ENTRY else ()
            if (regs[4] != esp0 - 8 or back != ret[0] + 1
                    or any(regs[k] != 0x11110000 + k * 0x1000 for k in range(8) if k != 4 and k not in scratch)
                    or not (in_range(target, GAME_TEXT) or in_range(target, SECUROM) or target == VM_ENTRY)):
                continue
            img.write(site, b'\xE8' + struct.pack('<i', target - site - 5) + b'\x90' * (back - site - 5))
            folded.append((site, target))
    return folded


def read_lines(path):
    return gzip.open(path, 'rt') if path.endswith('.gz') else open(path)


def main():
    src, listing, dst, report = sys.argv[1:5]
    if hashlib.sha256(open(src, 'rb').read()).hexdigest() != INPUT_SHA256:
        sys.exit('%s is not the expected Juiced 1.00 EXE (sha256 mismatch)' % src)
    img = Image(src)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    uc = make_emu(img)

    addrs = [int(line.split('\t', 1)[0], 16) for line in read_lines(listing)]

    class Undecodable(Exception):
        pass

    def insn(a):
        for i in md.disasm(img.read(a, 16), a):
            return i
        raise Undecodable()

    def jmp_target(i):
        if i.id == cx.X86_INS_JMP and i.operands and i.operands[0].type == cx.X86_OP_IMM:
            return i.operands[0].imm
        return None

    game_insns = [a for a in addrs if in_range(a, GAME_TEXT) or in_range(a, SECUROM)]
    relocs = []
    for a in game_insns:
        if in_range(a, GAME_TEXT) and img.read(a, 1) == b'\xE9':
            t = jmp_target(insn(a))
            if t is not None and in_range(t, SECUROM):
                relocs.append(a)
    relocs.sort()

    def stale(t):
        i = bisect.bisect_right(relocs, t) - 1
        return i >= 0 and 0 < t - relocs[i] <= STALE_MAX

    out = []
    listed = set(game_insns)
    text = img.read(GAME_TEXT[0], GAME_TEXT[1] - GAME_TEXT[0])
    unlisted = [GAME_TEXT[0] + m.start() for m in re.finditer(b'\xE9', text) if GAME_TEXT[0] + m.start() not in listed]
    for s in sorted(listed | set(unlisted)):
        if img.read(s, 1) != b'\xE9':
            continue
        t = jmp_target(insn(s))
        if t is None or not in_range(t, GAME_TEXT) or (s in listed and not stale(t)):
            continue
        body, a, kind = [], t, None
        for _ in range(24):
            try:
                i = insn(a)
            except Undecodable:
                break
            jt = jmp_target(i)
            if jt is not None:
                kind = 'jmpback' if jt == s + 5 else None
                break
            if i.id == cx.X86_INS_RET:
                kind = 'retjump'
                break
            body.append(i)
            a += i.size
        if kind is None:
            continue
        end = a
        mn = [i.mnemonic for i in body]
        k = mn.index('pushfd') if 'pushfd' in mn else len(body)
        if k and mn[k - 1] == 'push' and body[k - 1].operands[0].type == cx.X86_OP_REG and k < len(body):
            k -= 1
        prefix, block = body[:k], body[k:]
        if s not in listed and not block:
            continue
        for i in prefix:
            if i.group(capstone.CS_GRP_JUMP) or i.group(capstone.CS_GRP_CALL) or cx.X86_OP_IMM in [
                    o.type for o in i.operands if i.group(capstone.CS_GRP_BRANCH_RELATIVE)]:
                out.append((s, t, 'reject-branch-in-prefix', '', ''))
                break
        else:
            new = b''.join(bytes(i.bytes) for i in prefix)
            what = 'plain'
            if block:
                bstart = block[0].address
                try:
                    regs, writes, reads = emulate(uc, bstart, end)
                except UcError:
                    if s in listed:
                        raise
                    continue
                esp0 = STACK_TOP - 0x100
                changed = [r for r in range(8) if r != 4 and regs[r] != 0x11110000 + r * 0x1000]
                if kind == 'retjump':
                    target = struct.unpack('<I', uc.mem_read(regs[4], 4))[0]
                    what = 'retjump->%08x' % target
                    if target != s + 5 or regs[4] != esp0 - 4 or changed:
                        what = 'reject-' + what
                elif regs[4] == esp0 - 4 and not changed:
                    v = struct.unpack('<I', uc.mem_read(regs[4], 4))[0]
                    new += b'\x68' + struct.pack('<I', v)
                    what = 'push %08x' % v
                elif regs[4] == esp0 and len(changed) == 1:
                    r = changed[0]
                    addr = [x for x in reads if not (STACK_TOP - 0x10000 <= x[0] < STACK_TOP)][-1][0]
                    if struct.unpack('<I', uc.mem_read(addr, 4))[0] != regs[r]:
                        what = 'reject-load'
                    else:
                        new += enc_mov_load(r, addr)
                        what = 'mov %s,[%08x]' % (REGS[r], addr)
                elif regs[4] == esp0 and not changed:
                    gw = [w for w in writes if not (STACK_TOP - 0x10000 <= w[0] < STACK_TOP)]
                    src_reg = [r for r in range(8) if gw and r != 4 and gw[-1][2] == 0x11110000 + r * 0x1000]
                    if len(gw) == 1 and gw[0][1] == 4 and src_reg:
                        new += enc_mov_store(src_reg[0], gw[0][0])
                        what = 'mov [%08x],%s' % (gw[0][0], REGS[src_reg[0]])
                    else:
                        what = 'reject-store'
                else:
                    what = 'reject-unknown'
            if what.startswith('reject'):
                out.append((s, t, what, '', ''))
                continue
            nops = 0
            while img.read(s - nops - 1, 1) == b'\x90':
                nops += 1
            if len(new) > nops + 5:
                out.append((s, t, 'reject-too-long', what, '%d>%d' % (len(new), nops + 5)))
                continue
            # The moved instruction ended at site+5; a shorter one was padding before a function.
            if len(new) >= 5:
                img.write(s + 5 - len(new), new)
            else:
                img.write(s, new + b'\x90' * (5 - len(new)))
                what += ' (len %d)' % len(new)
            out.append((s, t, 'ok', what, new.hex()))

    folded = fold_pushret_calls(img, uc, md)
    vm_tail = fold_vm_tailjumps(img, md)
    base = Image(src)
    api_calls = fold_api_trampolines(img, base)
    integ, integ_open = fold_integrity_reads(img, base, listing, md)
    with open(report, 'w') as f:
        f.write('site\tstub\tstatus\twhat\tbytes\n')
        for row in out:
            f.write('%08x\t%08x\t%s\t%s\t%s\n' % row)
        for site, target in folded + vm_tail:
            f.write('%08x\t%08x\tok\tpushret-call\t\n' % (site, target))
    with open(report, 'a') as f:
        for a, mn, v in integ:
            f.write('%08x\t\tok\tintegrity-%s\t%08x\n' % (a, mn, v))
        for a, why in integ_open:
            f.write('%08x\t\topen\t%s\t\n' % (a, why))
    with open(report, 'a') as f:
        for site, slot in api_calls:
            f.write('%08x\t\tok\tapi-trampoline->[%08x]\t\n' % (site, slot))
    img.demote_securom_sections()
    open(dst, 'wb').write(img.raw)
    ok = [r for r in out if r[2] == 'ok']
    print('relocated functions: %d' % len(relocs))
    print('splices: %d ok, %d rejected' % (len(ok), len(out) - len(ok)))
    kinds = {}
    for r in ok:
        key = r[3].split(' ')[0].split('->')[0]
        kinds[key] = kinds.get(key, 0) + 1
    print('kinds:', kinds)
    print('push/ret calls folded: %d, VM tail jumps folded: %d' % (len(folded), len(vm_tail)))
    print('API trampoline calls folded: %d' % len(api_calls))
    print('integrity reads folded: %d, open: %d' % (len(integ), len(integ_open)))
    print('short (<5 bytes):', sum(1 for r in ok if '(len' in r[3]))
    for r in out:
        if r[2] != 'ok':
            print('  %08x -> %08x %s %s %s' % r)


if __name__ == '__main__':
    main()
