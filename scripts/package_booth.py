"""Create a source-free BOOTH kit from the verified published DLL and selected runtime files."""
from pathlib import Path
import argparse
import hashlib
import io
import json
import re
import subprocess
import zipfile


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release-zip', type=Path, default=root / 'qa/public-release-0.4.0/LUFS-Meter-Windows-x64.zip')
    parser.add_argument('--checksums', type=Path, default=root / 'qa/public-release-0.4.0/SHA256SUMS.txt')
    args = parser.parse_args()
    version = re.search(r'project\(obs_lufs_meter VERSION ([0-9.]+)', (root / 'CMakeLists.txt').read_text()).group(1)
    expected = dict((name, digest) for digest, name in (line.split() for line in args.checksums.read_text().splitlines()))
    payload = args.release_zip.read_bytes()
    assert hashlib.sha256(payload).hexdigest() == expected['LUFS-Meter-Windows-x64.zip'], 'Release ZIP checksum mismatch'
    with zipfile.ZipFile(io.BytesIO(payload)) as published:
        assert set(published.namelist()) == {'obs-lufs-meter.dll', '導入方法.txt'}
        assert 'v' + version in published.read('導入方法.txt').decode('utf-8-sig')
        dll = published.read('obs-lufs-meter.dll')
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
    manual = (root / 'docs/BOOTH_MANUAL_JA.txt').read_text(encoding='utf-8').replace('@VERSION@', version).replace('@SOURCE_COMMIT@', revision)
    members = {'obs-lufs-meter.dll': dll, '説明書.txt': manual.replace('\n', '\r\n').encode('utf-8-sig')}
    mapping = {'API/lufs_meter_mcp.py': 'scripts/lufs_meter_mcp.py', 'API/read_meter.py': 'scripts/read_meter.py',
               'API/obs_control.ps1': 'scripts/obs_control.ps1', 'API/API.md': 'docs/BOOTH_API_JA.md',
               'MCP設定/codex-config.toml': 'mcp/codex-config.toml', 'MCP設定/mcp-config.json': 'mcp/mcp-config.json'}
    for target, source in mapping.items():
        members[target] = (root / source).read_bytes()
    dist = root / 'dist/BOOTH'
    dist.mkdir(parents=True, exist_ok=True)
    output = dist / f'LUFS-Meter-{version}-BOOTH-Windows-x64.zip'
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, contents in members.items():
            archive.writestr(name, contents)
    with zipfile.ZipFile(output) as archive:
        assert archive.testzip() is None
        assert set(archive.namelist()) == set(members)
        assert archive.read('obs-lufs-meter.dll') == dll
        assert b'@SOURCE_COMMIT@' not in archive.read('説明書.txt')
    description = ('LUFS Meter — OBS用LUFS / True Peakメーター\n\n'
                   'マイク・BGM・ゲーム音と配信全体を個別に計測。Windows x64 / OBS Studio 32系対応。\n'
                   'DLL、導入方法とAIへの依頼例をまとめた説明書、AI連携API、MCP設定ひな形を同梱。\n'
                   'メーター単独でも利用でき、AI連携にはPython 3.10以降・PowerShell 7以降と対応AIアプリが必要です。\n\n'
                   'ソースコード（GPL-2.0-or-later）：https://github.com/sayaka-aiart/LUFS-Meter\n'
                   f'同梱APIの対応ソース：https://github.com/sayaka-aiart/LUFS-Meter/tree/{revision}\n'
                   f'DLLの対応ソース：https://github.com/sayaka-aiart/LUFS-Meter/releases/tag/v{version}\n')
    (dist / 'BOOTH掲載文.txt').write_text(description, encoding='utf-8-sig')
    result = {'zip': str(output), 'bytes': output.stat().st_size,
              'sha256': hashlib.sha256(output.read_bytes()).hexdigest(), 'sourceRevision': revision,
              'dllSha256': hashlib.sha256(dll).hexdigest(), 'files': list(members)}
    (dist / 'verification.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
