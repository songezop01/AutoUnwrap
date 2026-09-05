from pathlib import Path
import os
import subprocess
import tempfile


def run(*args):
    return subprocess.run(list(args), capture_output=True, text=True)

with tempfile.TemporaryDirectory(prefix="auto-unwrapper-split-") as temp:
    root = Path(temp)
    source = root / "game_asset.bin"
    source.write_bytes(os.urandom(2_500_000))

    seven_base = root / "PC13338.7z"
    created = run("7z", "a", "-t7z", "-v200k", "-mx=1", "-pVolPass", str(seven_base), str(source))
    assert created.returncode == 0, created.stderr
    seven_parts = sorted(root.glob("PC13338.7z.*"))
    assert len(seven_parts) >= 2, list(root.iterdir())
    first = root / "PC13338.7z.001"
    assert first.exists(), list(root.iterdir())
    assert first.read_bytes()[:6] == bytes.fromhex("377ABCAF271C")

    out = root / "seven_out"
    extracted = run("7z", "x", "-y", "-aou", "-pVolPass", str(first), f"-o{out}")
    assert extracted.returncode == 0, extracted.stderr + extracted.stdout
    assert (out / source.name).read_bytes() == source.read_bytes()

    zip_base = root / "split.zip"
    created_zip = run("7z", "a", "-tzip", "-v200k", "-mx=1", "-pVolPass", str(zip_base), str(source))
    assert created_zip.returncode == 0, created_zip.stderr
    zip_parts = sorted(root.glob("split.zip.*"))
    assert len(zip_parts) >= 2, list(root.iterdir())
    assert (root / "split.zip.001").exists()

print("PASS: real 7Z split volumes use .7z.001 as primary and extract without renaming; ZIP split volumes enumerated")
