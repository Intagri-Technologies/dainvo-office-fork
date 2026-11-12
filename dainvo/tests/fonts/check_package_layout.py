"""Export Aptos DOCX fixtures with the shipped app and verify Intos fallback."""
import argparse
import json
from pathlib import Path
import subprocess
import zipfile

import pymupdf


def extract(path):
    pages, fonts = [], set()
    with pymupdf.open(path) as document:
        for page in document:
            lines = []
            for block in page.get_text("rawdict")["blocks"]:
                for line in block.get("lines", []):
                    chars = []
                    for span in line["spans"]:
                        fonts.add(span["font"])
                        chars.extend((c["c"], c["origin"]) for c in span["chars"])
                    lines.append(chars)
            pages.append(lines)
    return pages, fonts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--workdir", type=Path, required=True)
    args = parser.parse_args()
    root = args.workdir.resolve()
    inputs = sorted((root / "documents").glob("Aptos*.docx"))
    assert len(inputs) == 4, "Generate the four reference fixtures first"
    gelasio = root / "documents/Gelasio.docx"
    with zipfile.ZipFile(root / "documents/Aptos.docx") as source:
        with zipfile.ZipFile(gelasio, "w", zipfile.ZIP_DEFLATED) as output:
            for name in source.namelist():
                data = source.read(name)
                if name.endswith(".xml"):
                    data = data.replace(b'="Aptos"', b'="Gelasio"')
                output.writestr(name, data)
    pdfs = root / "package-pdfs"
    profile = (root / "profile-package").as_uri()
    subprocess.run([
        str(args.app.resolve() / "Contents/MacOS/soffice"),
        "-env:UserInstallation=" + profile, "--headless", "--convert-to", "pdf",
        "--outdir", str(pdfs), *map(str, inputs), str(gelasio),
    ], check=True, timeout=180)
    results = []
    for path in inputs:
        original, _ = extract(root / "pdfs" / (path.stem + ".pdf"))
        actual, fonts = extract(pdfs / (path.stem + ".pdf"))
        text = lambda pages: [["".join(c for c, _ in line) for line in page] for page in pages]
        same = text(original) == text(actual)
        delta = max((abs(x - y) for p, q in zip(original, actual)
                     for a, b in zip(p, q) for (_, ao), (_, bo) in zip(a, b)
                     for x, y in zip(ao, bo)), default=0) if same else None
        expected = path.stem.replace("Aptos", "Intos")
        passed = same and delta < 0.02 and fonts and all(f.startswith(expected) for f in fonts)
        results.append({"document": path.name, "pages": len(actual),
                        "identical_line_breaks": same, "max_character_origin_delta_pt": delta,
                        "fonts": sorted(fonts), "passed": bool(passed)})
    _, fonts = extract(pdfs / "Gelasio.pdf")
    gelasio_ok = len(fonts) == 4 and all(f.startswith("Gelasio") for f in fonts)
    result = {"aptos_substitution": results,
              "gelasio": {"fonts": sorted(fonts), "passed": gelasio_ok}}
    (root / "package-layout-results.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    assert all(r["passed"] for r in results) and gelasio_ok, "Packaged font layout check failed"


if __name__ == "__main__":
    main()
