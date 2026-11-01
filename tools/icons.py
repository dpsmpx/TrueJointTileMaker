#!/usr/bin/env python3
"""Значки инструментов 2.0 (план «Интерфейс», C7, UX-31, ДС-4).

    tools/icons.py [ПАПКА]     по умолчанию Resources/Icons/modern
    tools/icons.py --preview   ещё и лист всех значков ×8 в build/icons.png

У 1.6 значки нарисованы кто во что: у части синие уголки, пипетка похожа на
карандаш, четыре значка карты — одинаковые сетки. Этот набор — одного
стиля: сетка 16 точек, показ ровно ×2 (линия выходит в 2 точки экрана),
контур в одну точку, один акцент, у каждого инструмента свой силуэт.

Цвета — две маски: чёрное — форма, синее — акцент. Редактор окрашивает
их ролями темы (текст и акцент), так что набор годится для любой темы и
любого акцента. Значки карты оставляют левый нижний угол свободным: там
панель показывает образец местности или вида объекта.

Значки рисуются кодом, а не руками в редакторе, чтобы набор можно было
пересобрать целиком после правки стиля. Править отдельный значок можно и
в самом редакторе — вкладка «Значки» на экране наборов.
"""
import math
import pathlib
import struct
import sys
import zlib

N = 16
INK = (0x1C, 0x20, 0x26, 255)     # форма: окрашивается цветом текста темы
ACC = (0x24, 0x68, 0xD8, 255)     # акцент: окрашивается акцентом темы
CLEAR = (0, 0, 0, 0)


class Icon:
    def __init__(self):
        self.px = [[CLEAR] * N for _ in range(N)]

    def put(self, x, y, c=INK):
        if 0 <= x < N and 0 <= y < N:
            self.px[y][x] = c

    def get(self, x, y):
        return self.px[y][x] if 0 <= x < N and 0 <= y < N else CLEAR

    def line(self, x0, y0, x1, y1, c=INK):
        """Брезенхем: ровная лесенка без углов «Г»."""
        dx, dy = abs(x1 - x0), -abs(y1 - y0)
        sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
        err = dx + dy
        while True:
            self.put(x0, y0, c)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    def rect(self, x0, y0, x1, y1, c=INK):
        for x in range(x0, x1 + 1):
            self.put(x, y0, c)
            self.put(x, y1, c)
        for y in range(y0, y1 + 1):
            self.put(x0, y, c)
            self.put(x1, y, c)

    def fill(self, x0, y0, x1, y1, c=INK):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.put(x, y, c)

    def shape(self, inside, outline=INK, interior=None):
        """Фигура по проверке «точка внутри»: контур — точки фигуры с
        соседом снаружи (по четырём сторонам), остальное — interior."""
        mask = [[inside(x + 0.5, y + 0.5) for x in range(N)] for y in range(N)]
        for y in range(N):
            for x in range(N):
                if not mask[y][x]:
                    continue
                edge = any(not (0 <= x + dx < N and 0 <= y + dy < N and mask[y + dy][x + dx])
                           for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
                if edge:
                    self.put(x, y, outline)
                elif interior is not None:
                    self.put(x, y, interior)
        return mask


def poly(points):
    """Проверка «точка внутри многоугольника» — чётность пересечений."""
    def inside(x, y):
        hit = False
        n = len(points)
        for i in range(n):
            x0, y0 = points[i]
            x1, y1 = points[(i + 1) % n]
            if (y0 > y) != (y1 > y) and x < (x1 - x0) * (y - y0) / (y1 - y0) + x0:
                hit = not hit
        return hit
    return inside


def disk(cx, cy, r):
    return lambda x, y: (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def along(x0, y0, x1, y1, half, t0=0.0, t1=1.0):
    """Полоса шириной 2·half вдоль отрезка — тело карандаша, ручки."""
    dx, dy = x1 - x0, y1 - y0
    ln = math.hypot(dx, dy)
    ux, uy = dx / ln, dy / ln
    nx, ny = -uy, ux
    a = (x0 + ux * ln * t0, y0 + uy * ln * t0)
    b = (x0 + ux * ln * t1, y0 + uy * ln * t1)
    return [(a[0] + nx * half, a[1] + ny * half), (b[0] + nx * half, b[1] + ny * half),
            (b[0] - nx * half, b[1] - ny * half), (a[0] - nx * half, a[1] - ny * half)]


# --- значки -------------------------------------------------------------------

def pen():
    # Карандаш наискосок: грифель внизу слева, тело — акцентом, торец.
    i = Icon()
    tip = (2.0, 14.0)
    end = (13.5, 2.5)
    body = along(*tip, *end, 2.2, 0.28, 0.92)
    i.shape(poly(body), INK, ACC)
    a = along(*tip, *end, 2.2, 0.28, 0.28)
    i.shape(poly([tip, a[0], a[3]]), INK, INK)
    cap = along(*tip, *end, 2.2, 0.92, 1.0)
    i.shape(poly(cap), INK, INK)
    return i


def eraser():
    # Ластик: брусок наклонён, рабочая треть — акцентом; след внизу.
    i = Icon()
    blk = along(3.0, 11.0, 12.5, 1.5, 3.0)
    i.shape(poly(blk), INK)
    work = along(3.0, 11.0, 12.5, 1.5, 3.0, 0.0, 0.38)
    i.shape(poly(work), INK, ACC)
    i.line(2, 14, 13, 14)
    return i


def bucket(i, ox=0, oy=0):
    # Ведро наклонено, краска в нём и течёт с края — акцентом; дужка
    # ручки — дугой над ведром.
    body = [(1.5 + ox, 8.0 + oy), (7.0 + ox, 2.5 + oy), (12.5 + ox, 8.0 + oy),
            (7.0 + ox, 13.5 + oy)]
    i.shape(poly(body), INK)
    i.shape(poly([(3.0 + ox, 8.0 + oy), (7.0 + ox, 4.0 + oy), (11.0 + ox, 8.0 + oy)]), ACC, ACC)
    for x, y in ((1, 6), (1, 5), (2, 4), (2, 3), (3, 2), (4, 1), (5, 1), (6, 0)):
        i.put(x + ox, y + oy)
    i.line(13 + ox, 9 + oy, 13 + ox, 10 + oy, ACC)
    i.fill(12 + ox, 11 + oy, 14 + ox, 13 + oy, ACC)
    i.put(12 + ox, 13 + oy, CLEAR)
    i.put(14 + ox, 13 + oy, CLEAR)
    return i


def fill():
    return bucket(Icon())


def pick():
    # Пипетка: тонкая трубка с жидкостью и груша; кончик — одна точка.
    i = Icon()
    i.shape(disk(12.0, 4.0, 3.0), INK, INK)
    tube = along(2.0, 14.0, 10.5, 5.5, 1.6, 0.14, 1.0)
    i.shape(poly(tube), INK)
    liquid = along(2.0, 14.0, 10.5, 5.5, 1.6, 0.14, 0.55)
    i.shape(poly(liquid), INK, ACC)
    i.put(1, 14)
    i.put(2, 13)
    return i


def line_icon():
    i = Icon()
    i.line(3, 12, 12, 3)
    i.fill(1, 12, 3, 14, ACC)
    i.fill(12, 1, 14, 3, ACC)
    return i


def rect_icon():
    i = Icon()
    i.rect(1, 3, 14, 12)
    i.fill(11, 9, 14, 12, ACC)
    i.rect(11, 9, 14, 12)
    return i


def ellipse_icon():
    i = Icon()
    i.shape(disk(8.0, 8.0, 6.6), INK)
    return i


def dither_icon():
    i = Icon()
    i.rect(1, 1, 14, 14)
    for y in range(3, 13):
        for x in range(3, 13):
            if (x + y) % 2 == 0:
                i.put(x, y, ACC)
    return i


def dashed(i, x0, y0, x1, y1, c=INK):
    """Рамка выделения: штрих 2 точки через 2, углы целые."""
    for x in range(x0, x1 + 1):
        if (x - x0) % 4 < 2 or x == x1:
            i.put(x, y0, c)
            i.put(x, y1, c)
    for y in range(y0, y1 + 1):
        if (y - y0) % 4 < 2 or y == y1:
            i.put(x0, y, c)
            i.put(x1, y, c)


def select_icon():
    i = Icon()
    dashed(i, 1, 2, 14, 13)
    i.fill(12, 11, 14, 13, ACC)
    return i


# Карта: левый нижний угол (5×6 точек) свободен под образец местности.

def map_paint():
    # Кисть: ручка наискосок, обойма, ворс — акцентом.
    i = Icon()
    i.shape(poly(along(14.5, 1.5, 7.5, 8.5, 1.5)), INK, INK)
    i.shape(poly(along(7.5, 8.5, 5.0, 11.0, 2.2)), INK)
    i.shape(poly([(5.5, 9.5), (8.6, 12.6), (9.5, 15.5), (6.0, 15.5)]), INK, ACC)
    return i


def map_fill():
    # То же ведро, сдвинутое вправо и вверх: угол слева внизу — образцу.
    return bucket(Icon(), 1, -1)


def map_rect():
    # Прямоугольник клеток: сетка 3×3 внутри рамки.
    i = Icon()
    i.rect(5, 1, 14, 10)
    for k in (8, 11):
        i.line(k, 1, k, 10)
        i.line(5, k - 3, 14, k - 3)
    i.fill(6, 2, 7, 3, ACC)
    i.fill(9, 5, 10, 6, ACC)
    i.fill(12, 8, 13, 9, ACC)
    return i


def map_object():
    # Метка на карте: булавка с акцентной головкой.
    i = Icon()
    head = disk(10.0, 5.0, 4.4)
    i.shape(lambda x, y: head(x, y) or poly([(6.6, 7.5), (13.4, 7.5), (10.0, 14.5)])(x, y),
            INK)
    i.shape(disk(10.0, 5.0, 2.0), ACC, ACC)
    return i


def map_select():
    i = Icon()
    dashed(i, 5, 1, 14, 10)
    i.fill(12, 8, 14, 10, ACC)
    return i


ICONS = {
    "tool_pen": pen, "tool_eraser": eraser, "tool_fill": fill, "tool_pick": pick,
    "tool_line": line_icon, "tool_rect": rect_icon, "tool_ellipse": ellipse_icon,
    "tool_dither": dither_icon, "tool_select": select_icon,
    "tool_map_paint": map_paint, "tool_map_fill": map_fill, "tool_map_rect": map_rect,
    "tool_map_object": map_object, "tool_map_select": map_select,
}


def write_png(path, rows):
    w, h = len(rows[0]), len(rows)
    raw = b"".join(b"\x00" + bytes(v for px in row for v in px) for row in rows)

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    data = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
    pathlib.Path(path).write_bytes(data)


def main():
    root = pathlib.Path(__file__).resolve().parent.parent
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    out = pathlib.Path(args[0]) if args else root / "Resources" / "Icons" / "modern"
    out.mkdir(parents=True, exist_ok=True)
    made = {name: fn() for name, fn in ICONS.items()}
    for name, icon in made.items():
        write_png(out / f"{name}.png", icon.px)
    if "--preview" in sys.argv:
        # Лист ×8 на светлом и тёмном фоне: видно и форму, и пиксели.
        s, gap = 8, 4
        names = list(made)
        rows = []
        for bg in ((247, 249, 254, 255), (18, 20, 22, 255)):
            for y in range(N * s + gap):
                row = []
                for name in names:
                    px = made[name].px
                    for x in range(N * s + gap):
                        c = bg
                        if y < N * s and x < N * s:
                            p = px[y // s][x // s]
                            if p[3]:
                                c = p if p != INK or bg[0] > 128 else (224, 227, 230, 255)
                        row.append(c)
                rows.append(row)
        (root / "build").mkdir(exist_ok=True)
        write_png(root / "build" / "icons.png", rows)
    print(f"{len(made)} значков → {out}")


if __name__ == "__main__":
    main()
