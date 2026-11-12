# Bundled Word-compatible fonts

The Dainvo installer includes Carlito, the original LibreOffice Caladea,
Liberation Sans, Liberation Serif, Liberation Mono and Liberation Sans Narrow.
Build 102 additionally bundles these unmodified upstream fonts:

| Family | Included styles | Missing-font substitution |
| --- | --- | --- |
| Gelasio | Regular, Medium, SemiBold, Bold, with italics | Georgia |
| Intos | Regular, Bold, Italic, Bold Italic | Aptos |
| Intos Display | Regular, Bold, Italic, Bold Italic | Aptos Display |
| Intos Narrow | Regular, Bold, Italic, Bold Italic | Aptos Narrow |
| Intos Serif | Regular, Bold, Italic, Bold Italic | Aptos Serif |

An installed original font remains preferred. The fallback mappings in
`officecfg/registry/data/org/openoffice/VCL.xcu` apply when it is missing.
These substitutions do not rename the fonts or change existing document styles.
There is no Aptos Mono replacement in this bundle.

## Sources and redistribution

`dainvo/fonts/manifest.json` pins every file to an upstream Git revision,
download URL and SHA-256. Intos Git LFS objects were checked against their
upstream object hashes. The actual font files are vendored so an offline build
does not depend on moving branches or a Git LFS installation.

- Gelasio: https://github.com/SorkinType/Gelasio at
  `7ab20e7e5c42791e603b9ee3201a0b49849cfdb2`.
- Intos: https://github.com/muglug/intos at
  `fef9315c14da9e4b23b4c3cac8e718998d4e4736`.

Both are distributed under SIL OFL 1.1. Their original copyright/license files
and manifest ship in `Contents/Resources/fonts/licenses/`; the TTFs ship in
`Contents/Resources/fonts/truetype/`. The fonts retain their own license.

Microsoft Aptos was downloaded solely as a local test reference from
https://www.microsoft.com/en-us/download/details.aspx?id=106087 (version 4.40).
Reference archive SHA-256:
`6528fd120e719a9f985e94214eca6887d1653b88456916a792a630b02e95b025`.
It is not vendored or included in the installer.

## Compatibility results

Tests compare the same documents rendered by Dainvo/LibreOffice on macOS with
real Aptos and with Intos. This is not a claim of testing Microsoft Word's
separate layout engine. Raw results are in `dainvo/tests/fonts/results/`.

| Font pair | Pages per document | Line breaks | Character origins |
| --- | ---: | --- | --- |
| Aptos / Intos | 12 | Identical | Identical |
| Aptos Display / Intos Display | 11 | Identical | Identical |
| Aptos Narrow / Intos Narrow | 11 | Identical | Identical |
| Aptos Serif / Intos Serif | 13 | Identical | Identical |

Each pair exercises all four standard styles, long paragraphs, tables, numbers,
currency, punctuation, ligatures and accented Latin text. Exported PDF font
names confirm that each intended font was actually used. The maximum measured
character-origin difference was 0 points across the 47 pages in each set.
Rendered samples were also visually inspected.

The final build 102 DMG passed the same 47-page comparison when opening the
original Aptos fixtures without Aptos in the app bundle: all four families
automatically used Intos, with identical line breaks and character positions.
The Gelasio smoke document used all four standard Gelasio styles. See
`package-layout-results.json` for the packaged-app results.

FontTools and HarfBuzz checks additionally compare glyph advances, line metrics
and supported multilingual samples:

- Intos, Display and Narrow match all 987 shared Unicode character advances
  and the tested line metrics. Each lacks 99 characters present in Aptos,
  including some symbols, combining marks and historical letters.
- Serif matches the tested Latin documents, but has substantially less coverage:
  782 shared characters in upright styles and 611 in italics. U+0256 differs in
  advance width in all four styles; U+0471 also differs in upright styles.
  Missing characters can invoke other fonts and affect layout.
- The upstream Intos Semibold review styles were evaluated but excluded:
  shaped sample widths differed by up to 0.0733 em despite matching nominal
  character advances. Do not add them without retesting.
- Light, ExtraBold, Black and Mono Aptos variants are not covered by these tests.

## Reproducing the checks

Use an isolated Python environment with `dainvo/tests/fonts/requirements.txt`.
Create a temporary work directory outside the repository containing
`upstream/intos/`, `upstream/gelasio/` (copies of the vendored families),
`reference-only-aptos/` (the official reference download), and `Reference.app`
(a private copy of the built application). Never put reference fonts into the
build's `instdir` or a distributable app.

Run `metrics.py --workdir DIR`, then `documents.py --workdir DIR`. The latter
adds fonts only to that private application and creates eight DOCX fixtures.
Convert `DIR/documents/*.docx` with the private app's `Contents/MacOS/soffice`,
using `-env:UserInstallation=file:///ABSOLUTE/DIR/profile-reference --headless
--convert-to pdf --outdir DIR/pdfs`. Run `compare_pdfs.py --workdir DIR` to
produce the page, line and character-origin comparison. Inspect the JSON
results rather than treating a zero process exit as a compatibility guarantee.

After building the installer, mount it read-only and run:

```sh
python3 dainvo/tools/verify-font-package.py '/Volumes/Dainvo Office/DainvoOffice.app'
```

Also export the original Aptos fixtures with the packaged application and an
isolated user profile. Confirm the resulting PDFs use Intos, preserve the
reference page/line layout, and do not contain an Aptos font. This verifies
automatic missing-font substitution rather than only explicit Intos selection.
The executable check is `check_package_layout.py --app APP --workdir DIR`.
