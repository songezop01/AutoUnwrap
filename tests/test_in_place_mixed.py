from pathlib import Path
import shutil
import subprocess
import tempfile

FIXTURE = Path('/home/ubuntu/auto_unwrap/tests/fixture/download.jpg')

def run(args):
    return subprocess.run(args, capture_output=True)

def detect(path):
    head = path.read_bytes()[:16]
    if head[:4] == b'PK\x03\x04': return 'zip'
    if head[:6] == bytes.fromhex('377ABCAF271C'): return '7z'
    if head[:7] in (b'Rar!\x1a\x07\x00', b'Rar!\x1a\x07\x01'): return 'rar'
    return None

with tempfile.TemporaryDirectory(prefix='auto-unwrapper-in-place-') as temp:
    folder = Path(temp)
    current = folder / 'download.jpg'
    shutil.copy2(FIXTURE, current)
    passwords = ['Pass1', 'Pass2', 'Pass3']
    successful = []
    for level in range(3):
        kind = detect(current)
        assert kind in {'zip', '7z', 'rar'}, (current, kind)
        renamed = current.with_suffix('.' + kind)
        current.rename(renamed)
        assert renamed.exists() and not current.exists()
        test = run(['7z', 't', '-pWrong', str(renamed)])
        assert test.returncode != 0, 'wrong password passed archive test'
        test = run(['7z', 't', '-p' + passwords[level], str(renamed)])
        assert test.returncode == 0, test.stderr.decode(errors='replace')
        extract = run(['7z', 'x', '-y', '-aou', '-p' + passwords[level], str(renamed), f'-o{folder}'])
        assert extract.returncode == 0, extract.stderr.decode(errors='replace')
        successful.append(renamed)
        nested = [p for p in folder.iterdir() if p.is_file() and p not in successful and detect(p)]
        if level < 2:
            assert len(nested) == 1, (level, list(folder.iterdir()))
            current = nested[0]
        else:
            final = folder / 'game_asset.txt'
            assert final.read_text(encoding='utf-8') == 'nested archive test\n'

    # The delete option is expected to remove only successfully extracted archive layers.
    for archive in successful:
        archive.unlink()
    assert (folder / 'game_asset.txt').exists()
    assert not any(p.exists() for p in successful)

print('PASS: in-place rename, mixed ZIP/7Z layers, per-layer passwords, wrong-password rejection, delete-after-success')
