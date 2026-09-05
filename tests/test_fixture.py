from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path('/home/ubuntu/auto_unwrap/tests/fixture')
source = ROOT / 'download.jpg'
assert source.read_bytes()[:6] == bytes.fromhex('377ABCAF271C'), 'outer archive is not 7z'

# Verify the expected signatures after each extraction and the intended passwords.
with tempfile.TemporaryDirectory(prefix='auto-unwrapped-test-') as temp:
    work = Path(temp)
    level1 = work / 'level1'
    level1.mkdir()
    bad = subprocess.run(['7z', 'x', '-y', '-pWrong', str(source), f'-o{level1}'], capture_output=True)
    assert bad.returncode != 0, 'wrong password unexpectedly succeeded'
    good = subprocess.run(['7z', 'x', '-y', '-pPass1', str(source), f'-o{level1}'], capture_output=True)
    assert good.returncode == 0, good.stderr.decode(errors='replace')
    middle = level1 / 'middle.png'
    assert middle.read_bytes()[:4] == b'PK\x03\x04', 'middle archive is not ZIP'

    level2 = work / 'level2'
    level2.mkdir()
    good = subprocess.run(['7z', 'x', '-y', '-pPass2', str(middle), f'-o{level2}'], capture_output=True)
    assert good.returncode == 0, good.stderr.decode(errors='replace')
    inner = level2 / 'inner.jpg'
    assert inner.read_bytes()[:6] == bytes.fromhex('377ABCAF271C'), 'inner archive is not 7z'

    level3 = work / 'level3'
    level3.mkdir()
    good = subprocess.run(['7z', 'x', '-y', '-pPass3', str(inner), f'-o{level3}'], capture_output=True)
    assert good.returncode == 0, good.stderr.decode(errors='replace')
    final = level3 / 'game_asset.txt'
    assert final.read_text(encoding='utf-8') == 'nested archive test\n'

print('PASS: magic detection, 3 nested layers, 3 passwords, wrong-password rejection, final content')
