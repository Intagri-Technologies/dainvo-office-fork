#!/usr/bin/env python3
"""Generate the Dainvo Start Center sidebar icons (one consistent stroke set).

Writes res/dainvo_<name>.svg into the SVG themes and res/dainvo_<name>.png
into the PNG themes, dark strokes for the light themes and light strokes for
the dark ones. Run from the repository root on macOS (PNG step uses the built
instdir):  python3 dainvo/icons/sidebar_icons.py
"""
import os
import subprocess
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SOFFICE = os.path.join(ROOT, 'instdir', 'DainvoOffice.app', 'Contents', 'MacOS', 'soffice')

ICONS = {
    'home': 'M3 10.5 12 3l9 7.5V20a1 1 0 0 1-1 1h-5v-6H9v6H4a1 1 0 0 1-1-1z',
    'open': 'M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z',
    'remote': 'M17.5 19a4.5 4.5 0 1 0-1.4-8.8A6 6 0 0 0 4 12a4 4 0 0 0 1 7.9z',
    'templates': 'M4 4h7v7H4zM13 4h7v7h-7zM4 13h7v7H4zM13 13h7v7h-7z',
    'extensions': 'M14 4h6v6h-6zM4 14h6v6H4zM14 14h6v6h-6zM7 4v6M4 7h6',
    'help': 'M12 22a10 10 0 1 0 0-20 10 10 0 0 0 0 20zM9.1 9a3 3 0 0 1 5.8 1c0 2-3 3-3 3M12 17h.01',
    'settings': ('M12 15a3 3 0 1 0 0-6 3 3 0 0 0 0 6zM19.4 15a1.7 1.7 0 0 0 .3 1.8l.1.1a2 2 0 1 1-2.8 '
                 '2.8l-.1-.1a1.7 1.7 0 0 0-1.8-.3 1.7 1.7 0 0 0-1 1.5V21a2 2 0 1 1-4 0v-.1a1.7 1.7 0 '
                 '0 0-1.1-1.5 1.7 1.7 0 0 0-1.8.3l-.1.1a2 2 0 1 1-2.8-2.8l.1-.1a1.7 1.7 0 0 0 .3-1.8 '
                 '1.7 1.7 0 0 0-1.5-1H3a2 2 0 1 1 0-4h.1a1.7 1.7 0 0 0 1.5-1.1 1.7 1.7 0 0 0-.3-1.8l-.1'
                 '-.1a2 2 0 1 1 2.8-2.8l.1.1a1.7 1.7 0 0 0 1.8.3H9a1.7 1.7 0 0 0 1-1.5V3a2 2 0 1 1 4 0v'
                 '.1a1.7 1.7 0 0 0 1 1.5 1.7 1.7 0 0 0 1.8-.3l.1-.1a2 2 0 1 1 2.8 2.8l-.1.1a1.7 1.7 0 0'
                 ' 0-.3 1.8V9a1.7 1.7 0 0 0 1.5 1H21a2 2 0 1 1 0 4h-.1a1.7 1.7 0 0 0-1.5 1z'),
}
# theme -> (stroke colour, svg?)
THEMES = {
    'colibre': ('#3C3C3C', False), 'colibre_svg': ('#3C3C3C', True),
    'colibre_dark': ('#DADADA', False), 'colibre_dark_svg': ('#DADADA', True),
}


def svg(path, color, size=16):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 24 24">'
            f'<path d="{path}" fill="none" stroke="{color}" stroke-width="1.8" '
            f'stroke-linecap="round" stroke-linejoin="round"/></svg>\n')


def main():
    with tempfile.TemporaryDirectory() as tmp:
        for theme, (color, is_svg) in THEMES.items():
            res = os.path.join(ROOT, 'icon-themes', theme, 'res')
            for name, path in ICONS.items():
                if is_svg:
                    open(os.path.join(res, f'dainvo_{name}.svg'), 'w').write(svg(path, color))
                    continue
                src = os.path.join(tmp, f'dainvo_{name}.svg')
                open(src, 'w').write(svg(path, color))
                subprocess.run([SOFFICE, '--headless', f'-env:UserInstallation=file://{tmp}/profile',
                                '--convert-to', 'png', '--outdir', tmp, src], check=True, capture_output=True)
                os.replace(src[:-4] + '.png', os.path.join(res, f'dainvo_{name}.png'))
    print(f'{len(ICONS)} sidebar icons written to {len(THEMES)} themes')


if __name__ == '__main__':
    main()
