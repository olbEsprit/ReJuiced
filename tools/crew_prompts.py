"""Pad pictures for the race HUD's crew orders in the stock ones' form (64x32 A8R8G8B8 .ddt, the stock header): white
with a black edge, two pictures side by side as the stock keys are (ui_crewcontrol_updown "Q W", _leftright "A S",
_buttons "Z X"): the D-pad's down and up, its left and right, the touchpad and create (src/main.cpp's pad_buttons).
Usage: crew_prompts.py <stock ui_crewcontrol_updown_e.ddt> <out dir>"""
import math, os, sys

W, H, SS = 64, 32, 4  # size, supersampling


def dpad(arm):
    """The D-pad in a 32x32 cell, `arm` (up, down, left, right) lit: (x, y) -> (cover, white)."""
    def f(x, y):
        x, y = x - 16, y - 16
        a, b = 4.5, 13.5  # half the arm's width, its reach
        if not ((abs(x) <= a and abs(y) <= b) or (abs(y) <= a and abs(x) <= b)):
            return None
        lit = {'up': y < -a, 'down': y > a, 'left': x < -a, 'right': x > a}[arm]
        return lit
    return f


def touchpad(x, y):
    x, y = x - 16, y - 16
    if abs(x) <= 13 and abs(y) <= 8 and (abs(x) <= 10 or abs(y) <= 5 or math.hypot(abs(x) - 10, abs(y) - 5) <= 3):
        return True
    return None


def create(x, y):
    x, y = x - 16, y - 16
    if abs(x) <= 5 and abs(y) <= 10 and (abs(y) <= 5 or math.hypot(x, abs(y) - 5) <= 5):
        # three lines cut in it
        if abs(x) <= 2.5 and any(abs(y - k) <= 0.9 for k in (-3, 0, 3)):
            return False
        return True
    return None


def render(left, right):
    out = bytearray()
    for py in range(H):
        for px in range(W):
            cover = white = edge = 0.0
            for sy in range(SS):
                for sx in range(SS):
                    x, y = px + (sx + 0.5) / SS, py + (sy + 0.5) / SS
                    f, cx = (left, x) if x < 32 else (right, x - 32)
                    v = f(cx, y)
                    if v is not None:
                        cover += 1
                        white += 1 if v else 0.25
                    else:  # the black edge: within 2 px of the shape
                        if any(f(cx + dx, y + dy) is not None for dx, dy in ((2, 0), (-2, 0), (0, 2), (0, -2), (1.4, 1.4), (-1.4, 1.4), (1.4, -1.4), (-1.4, -1.4))):
                            edge += 1
            n = SS * SS
            a = min(1.0, (cover + edge) / n)
            g = 255 * white / cover if cover else 0
            g = g * cover / (cover + edge) if cover + edge else 0
            out += bytes((round(g), round(g), round(g), round(255 * a)))  # B G R A
    return out


head = bytearray(open(sys.argv[1], 'rb').read()[:44])
for name, l, r in (('oj_crew_updown', dpad('down'), dpad('up')), ('oj_crew_leftright', dpad('left'), dpad('right')),
                   ('oj_crew_buttons', touchpad, create)):
    open(os.path.join(sys.argv[2], name + '.ddt'), 'wb').write(bytes(head) + render(l, r))
