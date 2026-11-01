"""A small drawing surface that writes the same picture as SVG and as PNG.

Standard library only: the PNG is rasterized here (zlib + struct) with a
5x7 bitmap font for the labels, so the pictures need no image package.
"""
import struct
import zlib
from html import escape

#  The classic 5x7 font, ' ' to '~': five columns per glyph, bit 0 at the top.
_FONT_HEX = (
    '0000000000' '00005f0000' '0007000700' '147f147f14' '242a7f2a12' '2313086462' '3649562050' '0008070300'
    '001c224100' '0041221c00' '2a1c7f1c2a' '08083e0808' '0080703000' '0808080808' '0000606000' '2010080402'
    '3e5149453e' '00427f4000' '7249494946' '2141494d33' '1814127f10' '2745454539' '3c4a494931' '4121110907'
    '3649494936' '464949291e' '0000140000' '0040340000' '0008142241' '1414141414' '0041221408' '0201590906'
    '3e415d594e' '7c1211127c' '7f49494936' '3e41414122' '7f4141413e' '7f49494941' '7f09090901' '3e41415173'
    '7f0808087f' '00417f4100' '2040413f01' '7f08142241' '7f40404040' '7f021c027f' '7f0408107f' '3e4141413e'
    '7f09090906' '3e4151215e' '7f09192946' '2649494932' '03017f0103' '3f4040403f' '1f2040201f' '3f4038403f'
    '6314081463' '0304780403' '6159494d43' '007f414141' '0204081020' '004141417f' '0402010204' '4040404040'
    '0003070800' '2054547840' '7f28444438' '3844444428' '384444287f' '3854545418' '00087e0902' '18a4a49c78'
    '7f08040478' '00447d4000' '2040403d00' '7f10284400' '00417f4000' '7c0478047c' '7c08040478' '3844444438'
    'fc18242418' '18242418fc' '7c08040408' '4854545424' '04043f4424' '3c4040207c' '1c2040201c' '3c4030403c'
    '4428102844' '4c9090907c' '4464544c44' '0008364100' '0000770000' '0041360800' '0201020402'
)
_FONT = {chr(0x20 + i): bytes.fromhex(_FONT_HEX[i * 10:i * 10 + 10]) for i in range(len(_FONT_HEX) // 10)}


def _rgb(colour):
    colour = colour.lstrip('#')
    if len(colour) == 3:
        colour = ''.join(c * 2 for c in colour)
    return int(colour[0:2], 16), int(colour[2:4], 16), int(colour[4:6], 16)


def text_width(text, scale=1):
    return len(text) * 6 * scale


class Picture:
    def __init__(self, width, height, background='#ffffff'):
        self.width = int(width)
        self.height = int(height)
        self._svg = []
        r, g, b = _rgb(background)
        self._px = bytearray(bytes((r, g, b)) * (self.width * self.height))
        self._svg.append(f'<rect x="0" y="0" width="{self.width}" height="{self.height}" fill="{background}"/>')

    #  Raster helpers.
    def _blend(self, x, y, rgb, alpha):
        if not (0 <= x < self.width and 0 <= y < self.height):
            return
        i = (y * self.width + x) * 3
        if alpha >= 1:
            self._px[i:i + 3] = bytes(rgb)
            return
        for c in range(3):
            self._px[i + c] = int(self._px[i + c] * (1 - alpha) + rgb[c] * alpha + 0.5)

    def _fill(self, x0, y0, x1, y1, rgb, alpha):
        xa, xb = max(0, int(round(x0))), min(self.width, int(round(x1)))
        ya, yb = max(0, int(round(y0))), min(self.height, int(round(y1)))
        if alpha >= 1:
            row = bytes(rgb) * max(0, xb - xa)
            for y in range(ya, yb):
                i = (y * self.width + xa) * 3
                self._px[i:i + len(row)] = row
            return
        for y in range(ya, yb):
            for x in range(xa, xb):
                self._blend(x, y, rgb, alpha)

    #  Drawing.
    def rect(self, x, y, w, h, fill, alpha=1.0, stroke=None):
        attrs = f' fill-opacity="{alpha:.2f}"' if alpha < 1 else ''
        if stroke:
            self._svg.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" fill="none" '
                             f'stroke="{stroke}" stroke-width="1"/>')
            rgb = _rgb(stroke)
            self._fill(x, y, x + w, y + 1, rgb, 1)
            self._fill(x, y + h - 1, x + w, y + h, rgb, 1)
            self._fill(x, y, x + 1, y + h, rgb, 1)
            self._fill(x + w - 1, y, x + w, y + h, rgb, 1)
            return
        self._svg.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" fill="{fill}"{attrs}/>')
        self._fill(x, y, x + w, y + h, _rgb(fill), alpha)

    def line(self, x0, y0, x1, y1, colour, width=1.0, dash=None, alpha=1.0):
        """A line with square ends; `dash` is (on, off) in pixels."""
        extra = f' stroke-dasharray="{dash[0]} {dash[1]}"' if dash else ' stroke-linecap="square"'
        if alpha < 1:
            extra += f' stroke-opacity="{alpha:.2f}"'
        self._svg.append(f'<line x1="{x0:.1f}" y1="{y0:.1f}" x2="{x1:.1f}" y2="{y1:.1f}" stroke="{colour}" '
                         f'stroke-width="{width:.1f}"{extra}/>')
        rgb = _rgb(colour)
        length = ((x1 - x0) ** 2 + (y1 - y0) ** 2) ** 0.5
        h = width / 2
        if length == 0:
            self._fill(x0 - h, y0 - h, x0 + h, y0 + h, rgb, alpha)
            return
        pieces = [(0.0, length)]
        if dash:
            pieces, t = [], 0.0
            while t < length:
                pieces.append((t, min(length, t + dash[0])))
                t += dash[0] + dash[1]
        dx, dy = (x1 - x0) / length, (y1 - y0) / length
        for a, b in pieces:
            ax, ay, bx, by = x0 + dx * a, y0 + dy * a, x0 + dx * b, y0 + dy * b
            if ax == bx or ay == by:
                self._fill(min(ax, bx) - h, min(ay, by) - h, max(ax, bx) + h, max(ay, by) + h, rgb, alpha)
                continue
            #  Diagonal: stamp squares along it (only short connectors are diagonal).
            steps = int((b - a) * 2) + 1
            for s in range(steps + 1):
                px, py = ax + (bx - ax) * s / steps, ay + (by - ay) * s / steps
                self._fill(px - h, py - h, px + h, py + h, rgb, alpha)

    def circle(self, cx, cy, r, fill, stroke=None):
        extra = f' stroke="{stroke}" stroke-width="1.5"' if stroke else ''
        self._svg.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{r:.1f}" fill="{fill}"{extra}/>')
        rgb = _rgb(fill)
        srgb = _rgb(stroke) if stroke else None
        for y in range(int(cy - r - 1), int(cy + r + 2)):
            for x in range(int(cx - r - 1), int(cx + r + 2)):
                d = ((x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2) ** 0.5
                if srgb and r - 1.5 <= d <= r:
                    self._blend(x, y, srgb, 1)
                elif d <= r:
                    self._blend(x, y, rgb, 1)

    def cross(self, cx, cy, r, colour, width=3):
        self.line(cx - r, cy - r, cx + r, cy + r, colour, width)
        self.line(cx - r, cy + r, cx + r, cy - r, colour, width)

    def text(self, x, y, text, colour='#000000', scale=1, halo=None):
        """`text` with its top-left at (x, y); 6 px per character per scale."""
        size = 8 * scale
        style = f' stroke="{halo}" stroke-width="{2 * scale}" paint-order="stroke"' if halo else ''
        self._svg.append(f'<text x="{x:.1f}" y="{y + 7 * scale:.1f}" fill="{colour}" font-size="{size}" '
                         f'font-family="Menlo, monospace"{style}>{escape(text)}</text>')
        rgb = _rgb(colour)
        hrgb = _rgb(halo) if halo else None
        for n, ch in enumerate(text):
            glyph = _FONT.get(ch, _FONT['?'])
            gx = int(x) + n * 6 * scale
            for col, bits in enumerate(glyph):
                for row in range(8):
                    if not bits >> row & 1:
                        continue
                    px, py = gx + col * scale, int(y) + row * scale
                    if hrgb:
                        self._fill(px - 1, py - 1, px + scale + 1, py + scale + 1, hrgb, 0.8)
            for col, bits in enumerate(glyph):
                for row in range(8):
                    if bits >> row & 1:
                        px, py = gx + col * scale, int(y) + row * scale
                        self._fill(px, py, px + scale, py + scale, rgb, 1)

    #  Output.
    def svg(self):
        return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {self.width} {self.height}" '
                f'width="{self.width}" height="{self.height}">\n' + '\n'.join(self._svg) + '\n</svg>\n')

    def png(self):
        stride = self.width * 3
        raw = b''.join(b'\x00' + bytes(self._px[y * stride:(y + 1) * stride]) for y in range(self.height))

        def chunk(kind, data):
            return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
        header = struct.pack('>IIBBBBB', self.width, self.height, 8, 2, 0, 0, 0)
        return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b'')

    def pixel(self, x, y):
        i = (y * self.width + x) * 3
        return tuple(self._px[i:i + 3])
