#!/usr/bin/env python3
"""Install the Dainvo app tiles into the shipped icon themes.

Replaces the per-app images in icon-themes/<theme>/res (Start Center buttons,
New menus, document-type images) with the tiles in dainvo/icons/source, and the
LibreOffice app logo (mainapp_*) with the Dainvo mark. PNG themes get resized
PNGs; SVG themes get an SVG wrapping a 2x PNG so it stays sharp on Retina.

Run from the repository root on macOS (uses sips):  python3 dainvo/icons/install.py
"""
import base64
import os
import re
import subprocess
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'dainvo', 'icons', 'source')
THEMES = ['colibre', 'colibre_dark', 'colibre_svg', 'colibre_dark_svg']

# file-name prefix -> source tile
TILES = {
    'odt': 'writer', 'writer': 'writer',
    'ods': 'calc', 'calc': 'calc',
    'odp': 'impress', 'impress': 'impress',
    'odg': 'draw', 'draw': 'draw',
    'odf': 'math', 'math': 'math',
    'mainapp': 'dainvo',
}
NAME = re.compile(r'^(odt|ods|odp|odg|odf)_(16|32|48|96)_8$|^(writer|calc|impress|draw|math)(128)$|^(mainapp)_(16|32|48)(_8)?$')


def resized_png(tile, size, tmp):
    out = os.path.join(tmp, f'{tile}_{size}.png')
    if not os.path.exists(out):
        subprocess.run(['sips', '-z', str(size), str(size), os.path.join(SRC, tile + '.png'), '--out', out],
                       check=True, capture_output=True)
    return out


def main():
    count = 0
    with tempfile.TemporaryDirectory() as tmp:
        for theme in THEMES:
            res = os.path.join(ROOT, 'icon-themes', theme, 'res')
            for fname in sorted(os.listdir(res)):
                stem, ext = os.path.splitext(fname)
                m = NAME.match(stem)
                if not m or ext not in ('.png', '.svg'):
                    continue
                prefix = next(g for g in (m.group(1), m.group(3), m.group(5)) if g)
                size = int(next(g for g in (m.group(2), m.group(4), m.group(6)) if g))
                tile = TILES[prefix]
                path = os.path.join(res, fname)
                if ext == '.png':
                    with open(resized_png(tile, size, tmp), 'rb') as f, open(path, 'wb') as o:
                        o.write(f.read())
                else:
                    data = base64.b64encode(open(resized_png(tile, size * 2, tmp), 'rb').read()).decode()
                    with open(path, 'w', encoding='utf-8') as o:
                        o.write(f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
                                f'width="{size}" height="{size}" viewBox="0 0 {size} {size}">'
                                f'<image width="{size}" height="{size}" xlink:href="data:image/png;base64,{data}"/></svg>\n')
                count += 1
    print(f'replaced {count} icons in {len(THEMES)} themes')


if __name__ == '__main__':
    main()
