from pathlib import Path
import shutil
import subprocess
import zipfile

ROOT = Path('/home/ubuntu/auto_unwrap/tests/fixture')
if ROOT.exists():
    shutil.rmtree(ROOT)
ROOT.mkdir(parents=True)
final = ROOT / 'final'
final.mkdir()
(final / 'game_asset.txt').write_text('nested archive test\n', encoding='utf-8')

# Inner 7z archive, then disguise it as a JPG.
subprocess.run(['7z', 'a', '-t7z', '-mx=1', '-pPass3', '-mhe=on', str(ROOT / 'inner.7z'), str(final / 'game_asset.txt')], check=True, stdout=subprocess.DEVNULL)
(ROOT / 'inner.7z').rename(ROOT / 'inner.jpg')

# Middle ZIP contains the disguised inner archive. ZIP encryption is not created
# by Python's stdlib, so the test adds a password with 7z after creating it.
subprocess.run(['7z', 'a', '-tzip', '-mx=1', '-pPass2', str(ROOT / 'middle.zip'), str(ROOT / 'inner.jpg')], check=True, stdout=subprocess.DEVNULL)
(ROOT / 'middle.zip').rename(ROOT / 'middle.png')

# Outer 7z contains the disguised middle archive, then is renamed to PNG.
subprocess.run(['7z', 'a', '-t7z', '-mx=1', '-pPass1', '-mhe=on', str(ROOT / 'outer.7z'), str(ROOT / 'middle.png')], check=True, stdout=subprocess.DEVNULL)
(ROOT / 'outer.7z').rename(ROOT / 'download.jpg')

print(ROOT / 'download.jpg')
