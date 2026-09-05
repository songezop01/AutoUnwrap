from pathlib import Path
import os
import subprocess
import tempfile


def run(*args):
    return subprocess.run(list(args), capture_output=True, text=True)

with tempfile.TemporaryDirectory(prefix="auto-unwrapper-split-nested-") as temp:
    root = Path(temp)
    final = root / "game_asset.bin"
    final.write_bytes(os.urandom(500_000))

    # Inner ZIP split archive containing the final file.
    inner_zip = root / "inner.zip"
    created = run("7z", "a", "-tzip", "-v50k", "-mx=1", "-pInnerPass", str(inner_zip), str(final))
    assert created.returncode == 0, created.stderr
    inner_parts = sorted(root.glob("inner.zip.*"))
    assert len(inner_parts) >= 2, list(root.iterdir())

    # Outer 7Z split archive containing the inner ZIP split parts.
    outer_7z = root / "outer.7z"
    created = run("7z", "a", "-t7z", "-v100k", "-mx=1", "-pOuterPass", str(outer_7z), *inner_parts)
    assert created.returncode == 0, created.stderr
    outer_parts = sorted(root.glob("outer.7z.*"))
    assert len(outer_parts) >= 2, list(root.iterdir())

    # Extract outer 7Z split archive.
    out1 = root / "out1"
    extracted = run("7z", "x", "-y", "-aou", "-pOuterPass", str(outer_parts[0]), f"-o{out1}")
    assert extracted.returncode == 0, extracted.stderr + extracted.stdout
    assert (out1 / "inner.zip.001").exists()
    assert (out1 / "inner.zip.002").exists()

    # Extract inner ZIP split archive from the extracted parts.
    out2 = root / "out2"
    extracted = run("7z", "x", "-y", "-aou", "-pInnerPass", str(out1 / "inner.zip.001"), f"-o{out2}")
    assert extracted.returncode == 0, extracted.stderr + extracted.stdout
    assert (out2 / "game_asset.bin").read_bytes() == final.read_bytes()

print("PASS: nested split archives extract correctly; outer 7Z split produces inner ZIP split parts")
