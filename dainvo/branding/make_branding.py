#!/usr/bin/env python3
"""Generate the Dainvo Office brand images in dainvo/branding.

The wordmark ("Dainvo" Inter SemiBold + "Office" Inter Regular) is converted
to vector outlines with macOS CoreText (tools/outline.swift), so it renders
identically without Inter installed. Outputs:

  logo.svg, logo-sc.svg            dark text, for light backgrounds
  logo_inverted.svg, logo-sc_inverted.svg   light text, for dark backgrounds
  about.svg                        About dialog (neutral grey text)
  intro.png, intro-highres.png     loading screen (dark rounded card)
  progress.conf                    loading-screen progress bar

Run from the repository root on macOS:  python3 dainvo/branding/make_branding.py
(needs swiftc, and a built instdir for the SVG -> PNG step)
"""
import base64
import os
import subprocess
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HERE = os.path.join(ROOT, 'dainvo', 'branding')
# Inter (SIL Open Font License) is not stored here (repo file-size limit); point
# DAINVO_INTER_TTF at Inter.ttf, e.g. the copy in the Dainvo mobile project.
FONT = os.environ.get('DAINVO_INTER_TTF', os.path.expanduser(
    '~/Projects/Dainvo Project/dainvo_mobile/assets/fonts/Inter.ttf'))
MARK = os.path.join(ROOT, 'dainvo', 'icons', 'source', 'dainvo.png')
SOFFICE = os.path.join(ROOT, 'instdir', 'DainvoOffice.app', 'Contents', 'MacOS', 'soffice')

CAP = 72.7    # Inter cap height at font size 100
SPACE = 28.0  # gap between "Dainvo" and "Office" at font size 100

SPLASH_W, SPLASH_H, SPLASH_R = 560, 340, 12
SPLASH_BG, SPLASH_BORDER, SPLASH_TEXT = '#1C1C1C', '#2F2F2F', '#F5F5F5'
BAR_W, BAR_H, BAR_Y = 200, 4, 292


def outline(tool, weight, text):
    out = subprocess.run([tool, FONT, str(weight), '100', text], check=True,
                         capture_output=True, text=True).stdout.split('\n')
    return float(out[0]), out[1]


def mark_png(size, tmp):
    out = os.path.join(tmp, f'mark{size}.png')
    subprocess.run(['sips', '-z', str(size), str(size), MARK, '--out', out], check=True, capture_output=True)
    return base64.b64encode(open(out, 'rb').read()).decode()


def wordmark(words, x, baseline, size, color):
    (adv1, d1), (_, d2) = words
    s = size / 100.0
    return (f'<g fill="{color}">'
            f'<path transform="translate({x:.2f} {baseline:.2f}) scale({s:.4f})" d="{d1}"/>'
            f'<path transform="translate({x + (adv1 + SPACE) * s:.2f} {baseline:.2f}) scale({s:.4f})" d="{d2}"/>'
            f'</g>')


def lockup(words, w, h, mark_b64, mark, size, color, bg=None):
    """Mark on the left, wordmark vertically centred beside it."""
    pad = (h - mark) / 2
    tx = pad + mark + mark * 0.32
    baseline = h / 2 + CAP * size / 100 / 2
    back = f'<rect width="{w}" height="{h}" fill="{bg}"/>' if bg else ''
    return (f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
            f'width="{w}" height="{h}" viewBox="0 0 {w} {h}">{back}'
            f'<image x="{pad:.2f}" y="{pad:.2f}" width="{mark}" height="{mark}" '
            f'xlink:href="data:image/png;base64,{mark_b64}"/>'
            f'{wordmark(words, tx, baseline, size, color)}</svg>\n')


def splash_svg(words, scale, mark_b64):
    w, h, r = SPLASH_W * scale, SPLASH_H * scale, SPLASH_R * scale
    mark = 76 * scale
    size = 30 * scale
    (adv1, _), (adv2, _) = words
    text_w = (adv1 + SPACE + adv2) * size / 100
    mark_y = h * 0.27
    baseline = mark_y + mark + 22 * scale + CAP * size / 100
    return (f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
            f'width="{w}" height="{h}" viewBox="0 0 {w} {h}">'
            f'<rect x="{0.5 * scale}" y="{0.5 * scale}" width="{w - scale}" height="{h - scale}" rx="{r}" '
            f'fill="{SPLASH_BG}" stroke="{SPLASH_BORDER}" stroke-width="{scale}"/>'
            f'<image x="{(w - mark) / 2:.2f}" y="{mark_y:.2f}" width="{mark}" height="{mark}" '
            f'xlink:href="data:image/png;base64,{mark_b64}"/>'
            f'{wordmark(words, (w - text_w) / 2, baseline, size, SPLASH_TEXT)}</svg>\n')


def to_png(svg_text, out_png, tmp):
    src = os.path.join(tmp, os.path.basename(out_png).replace('.png', '.svg'))
    open(src, 'w').write(svg_text)
    subprocess.run([SOFFICE, '--headless', f'-env:UserInstallation=file://{tmp}/profile',
                    '--convert-to', 'png', '--outdir', tmp, src], check=True, capture_output=True)
    os.replace(src[:-4] + '.png', out_png)


def main():
    with tempfile.TemporaryDirectory() as tmp:
        tool = os.path.join(tmp, 'outline')
        subprocess.run(['swiftc', '-O', '-o', tool, os.path.join(HERE, 'tools', 'outline.swift')],
                       check=True, capture_output=True)
        words = [outline(tool, 600, 'Dainvo'), outline(tool, 400, 'Office')]
        m48 = mark_png(96, tmp)
        for name, color in (('logo', '#1C1C1C'), ('logo-sc', '#1C1C1C'),
                            ('logo_inverted', '#F5F5F5'), ('logo-sc_inverted', '#F5F5F5')):
            open(os.path.join(HERE, name + '.svg'), 'w').write(lockup(words, 346, 72, m48, 44, 30, color))
        open(os.path.join(HERE, 'about.svg'), 'w').write(lockup(words, 560, 160, mark_png(192, tmp), 96, 56, '#8E8E93'))
        to_png(splash_svg(words, 1, mark_png(152, tmp)), os.path.join(HERE, 'intro.png'), tmp)
        to_png(splash_svg(words, 2, mark_png(304, tmp)), os.path.join(HERE, 'intro-highres.png'), tmp)
    bar_x = (SPLASH_W - BAR_W) // 2
    with open(os.path.join(HERE, 'progress.conf'), 'w') as f:
        f.write('PROGRESSBARCOLOR="139,147,255"\n'
                f'PROGRESSSIZE="{BAR_W},{BAR_H}"\n'
                f'PROGRESSPOSITION="{bar_x},{BAR_Y}"\n'
                'PROGRESSFRAMECOLOR="51,51,51"\n'
                'PROGRESSTEXTCOLOR="163,163,163"\n'
                'PROGRESSTEXTBASELINE="320"\n')
    print('branding images and progress.conf written')


if __name__ == '__main__':
    main()
