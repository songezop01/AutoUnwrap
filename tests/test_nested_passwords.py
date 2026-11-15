#!/usr/bin/env python3
"""Reproduce nested archive passwords and the candidate-order fix.

Archives are generated in a temporary directory. Passwords used here are
synthetic and are not taken from user diagnostics.
"""

import os
import shutil
import subprocess
import tempfile
from pathlib import Path


OUTER = "outer-pass"
WRONG = "wrong-pass"
# Space, hash, and ampersand. This Linux 7-Zip build rejects non-ASCII
# passwords (E_INVALIDARG), so Unicode is covered by the C++ candidate test.
INNER = "in ner#1&x"


def build_candidates(one_time, saved):
    candidates = []

    def add(value):
        if value not in candidates:
            candidates.append(value)

    if one_time:
        add(one_time)
    for value in saved:
        add(value)
    add("")
    return candidates


def promote(passwords, succeeded):
    if passwords and passwords[0] == succeeded:
        return passwords
    if succeeded in passwords:
        passwords = [value for value in passwords if value != succeeded]
    return [succeeded] + passwords


def find_7z():
    for name in ("7z", "7zz", "7za"):
        path = shutil.which(name)
        if path:
            return path
    return None


def run_7z(seven, archive, password, destination):
    destination.mkdir(parents=True, exist_ok=True)
    command = [seven, "x", "-y", "-bd", f"-p{password}", f"-o{destination}", str(archive)]
    completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = completed.stdout.decode("utf-8", errors="replace")
    wrong = "wrong password" in output.lower() or "incorrect password" in output.lower()
    return completed.returncode, wrong, output


def extract_with_order(seven, archive, passwords, destination):
    attempts = []
    for index, password in enumerate(passwords, start=1):
        attempt_dir = destination / f"attempt-{index}"
        if attempt_dir.exists():
            shutil.rmtree(attempt_dir)
        code, wrong, _output = run_7z(seven, archive, password, attempt_dir)
        produced = any(attempt_dir.rglob("*"))
        attempts.append({"index": index, "password": password, "code": code, "wrong": wrong, "produced": produced})
        if code == 0 and produced:
            return attempts
    return attempts


def create_nested_archives(seven, root: Path):
    payload = root / "payload"
    payload.mkdir()
    (payload / "說明.txt").write_text("layer-3-ok\n", encoding="utf-8")
    long_dir = payload / ("nested-" + ("p" * 80))
    long_dir.mkdir()
    (long_dir / "note.txt").write_text("long-path-ok\n", encoding="utf-8")

    layer3 = root / "layer3.zip"
    layer2 = root / "layer2.zip"
    layer1 = root / "layer1.zip"
    subprocess.check_call([seven, "a", "-y", "-bd", f"-p{INNER}", str(layer3), str(payload)], stdout=subprocess.DEVNULL)
    subprocess.check_call([seven, "a", "-y", "-bd", f"-p{INNER}", str(layer2), str(layer3)], stdout=subprocess.DEVNULL)
    subprocess.check_call([seven, "a", "-y", "-bd", f"-p{OUTER}", str(layer1), str(layer2)], stdout=subprocess.DEVNULL)
    return layer1, layer2, layer3


def main():
    seven = find_7z()
    if not seven:
        raise SystemExit("7-Zip CLI (7z/7zz/7za) is required to generate nested archives")

    with tempfile.TemporaryDirectory(prefix="autounwrap-nested-") as temp:
        root = Path(temp)
        layer1, _layer2, _layer3 = create_nested_archives(seven, root)
        saved = [WRONG, INNER, "other-pass"]
        initial = build_candidates(OUTER, saved)

        # Old behavior: every layer restarts at the original candidate list.
        old_passwords = list(initial)
        work = root / "old"
        first = extract_with_order(seven, layer1, old_passwords, work / "layer1")
        assert first[-1]["code"] == 0 and first[-1]["password"] == OUTER, first
        layer2_file = next((work / "layer1").rglob("layer2.zip"))
        second = extract_with_order(seven, layer2_file, old_passwords, work / "layer2")
        assert second[0]["password"] == OUTER and second[0]["code"] != 0, second
        assert second[-1]["password"] == INNER and second[-1]["code"] == 0, second
        layer3_file = next((work / "layer2").rglob("layer3.zip"))
        third_old = extract_with_order(seven, layer3_file, old_passwords, work / "layer3-old")
        assert third_old[0]["password"] == OUTER and third_old[0]["wrong"], third_old
        assert any(item["password"] == INNER and item["code"] == 0 for item in third_old), third_old

        # Fixed behavior: the password that opened the previous layer is tried first.
        fixed = list(initial)
        work = root / "fixed"
        first = extract_with_order(seven, layer1, fixed, work / "layer1")
        fixed = promote(fixed, first[-1]["password"])
        layer2_file = next((work / "layer1").rglob("layer2.zip"))
        second = extract_with_order(seven, layer2_file, fixed, work / "layer2")
        assert second[-1]["code"] == 0 and second[-1]["password"] == INNER, second
        fixed = promote(fixed, second[-1]["password"])
        assert fixed[0] == INNER, fixed
        layer3_file = next((work / "layer2").rglob("layer3.zip"))
        third = extract_with_order(seven, layer3_file, fixed, work / "layer3")
        assert len(third) == 1 and third[0]["password"] == INNER and third[0]["code"] == 0, third
        text = (work / "layer3").joinpath("attempt-1", "payload", "說明.txt").read_text(encoding="utf-8")
        assert "layer-3-ok" in text
        note = next((work / "layer3").rglob("note.txt")).read_text(encoding="utf-8")
        assert "long-path-ok" in note

    print("PASS: nested archives accept the promoted inner password on the next layer")


if __name__ == "__main__":
    main()
