"""White pad-button hint pictures in the stock hints' form (64x64 A8R8G8B8 .ddt, the stock header's tail):
a white disc with the letter cut out, from OpenJuice's coloured Xbox pictures (Xelu's prompts, CC0).
Usage: pad_prompts.py <stock ui_selecta_e.ddt> <in dir> <out dir>"""
import sys, os

def load(path):
    d = open(path, 'rb').read()
    w, h = int.from_bytes(d[4:8], 'little'), int.from_bytes(d[8:12], 'little')
    return w, h, d[:44], d[44:44 + w * h * 4]

stock_head = load(sys.argv[1])[2]
N = 64
for name in ('oj_pad_a', 'oj_pad_b', 'oj_pad_x', 'oj_pad_y'):
    w, h, _, px = load(os.path.join(sys.argv[2], name + '.ddt'))
    def at(x, y):
        x = min(max(x, 0), w - 1); y = min(max(y, 0), h - 1)
        i = (y * w + x) * 4
        b, g, r, a = px[i:i + 4]
        sat = max(r, g, b) - min(r, g, b)
        letter = min(1.0, max(0.0, (sat - 40) / 80))
        return a / 255 * (1 - letter)
    out = bytearray()
    for y in range(N):
        for x in range(N):
            # box filter over the source pixels this one covers
            x0, x1 = x * w // N, max((x + 1) * w // N, x * w // N + 1)
            y0, y1 = y * h // N, max((y + 1) * h // N, y * h // N + 1)
            s = [at(sx, sy) for sy in range(y0, y1) for sx in range(x0, x1)]
            out += bytes((255, 255, 255, round(255 * sum(s) / len(s))))
    head = bytearray(stock_head)
    head[4:8] = N.to_bytes(4, 'little'); head[8:12] = N.to_bytes(4, 'little')
    open(os.path.join(sys.argv[3], name + '.ddt'), 'wb').write(bytes(head) + bytes(out))
