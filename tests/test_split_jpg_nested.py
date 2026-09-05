from pathlib import Path
import os
import subprocess
import tempfile


def run(*args):
    return subprocess.run(list(args), capture_output=True, text=True)

with tempfile.TemporaryDirectory(prefix="auto-unwrapper-split-jpg-") as temp:
    root = Path(temp)
    final = root / "game_asset.bin"
    final.write_bytes(os.urandom(500_000))

    # Inner ZIP archive disguised as JPG.
    inner_zip = root / "inner.zip"
    created = run("7z", "a", "-tzip", "-mx=1", "-pInnerPass", str(inner_zip), str(final))
    assert created.returncode == 0, created.stderr
    inner_jpg = root / "inner.jpg"
    inner_zip.rename(inner_jpg)

    # Outer 7Z split archive containing the disguised JPG.
    outer_7z = root / "outer.7z"
    created = run("7z", "a", "-t7z", "-v100k", "-mx=1", "-pOuterPass", str(outer_7z), str(inner_jpg))
    assert created.returncode == 0, created.stderr
    outer_parts = sorted(root.glob("outer.7z.*"))
    assert len(outer_parts) >= 2, list(root.iterdir())

    # Extract outer 7Z split archive.
    out1 = root / "out1"
    extracted = run("7z", "x", "-y", "-aou", "-pOuterPass", str(outer_parts[0]), f"-o{out1}")
    assert extracted.returncode == 0, extracted.stderr + extracted.stdout
    assert (out1 / "inner.jpg").exists()

    # Verify the disguised JPG is actually a ZIP archive.
    assert (out1 / "inner.jpg").read_bytes()[:4] == b"PK\x03\x04"

    # Extract inner ZIP archive from the disguised JPG.
    out2 = root / "out2"
    extracted = run("7z", "x", "-y", "-aou", "-pInnerPass", str(out1 / "inner.jpg"), f"-o{out2}")
    assert extracted.returncode == 0, extracted.stderr + extracted.stdout
    assert (out2 / "game_asset.bin").read_bytes() == final.read_bytes()

print("PASS: split archive produces disguised JPG; JPG is actually ZIP and can be extracted")
