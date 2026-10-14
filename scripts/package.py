"""Build the public Windows ZIP with corresponding source and license notices."""
from pathlib import Path
import hashlib
import json
import re
import shutil
import tempfile
import zipfile

root = Path(__file__).resolve().parent.parent
version = re.search(r"project\(obs_lufs_meter VERSION ([0-9.]+)", (root / "CMakeLists.txt").read_text()).group(1)
package = root / "dist" / f"obs-lufs-meter-{version}-windows-x64"
dll = package / "obs-plugins/64bit/obs-lufs-meter.dll"
if not dll.exists():
    raise SystemExit("Run scripts/build.ps1 before packaging.")
# Use a fresh staging folder so old verification data is never carried into a release.
stage = Path(tempfile.mkdtemp(prefix=f"release-stage-{version}-", dir=root / "dist"))
binary = stage / "obs-plugins/64bit"
binary.mkdir(parents=True)
shutil.copy2(dll, binary / dll.name)
public_files = ("CMakeLists.txt", "README.md", "README_JA.md", "LICENSE", "THIRD_PARTY_NOTICES.md", "CHANGELOG.md", "CONTRIBUTING.md", ".gitignore", ".gitattributes", ".editorconfig")
public_dirs = ("src", "tests", "sdk", "scripts", "third_party", "cmake", "docs", ".github")
source = stage / "source"
source.mkdir()
for name in public_files:
    shutil.copy2(root / name, source / name)
for name in public_dirs:
    shutil.copytree(root / name, source / name, ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
for name in ("README.md", "README_JA.md", "LICENSE", "THIRD_PARTY_NOTICES.md", "CHANGELOG.md"):
    shutil.copy2(root / name, stage / name)
shutil.copytree(root / "docs", stage / "docs")
shutil.copytree(root / "cmake", stage / "cmake")
licenses = stage / "licenses"
licenses.mkdir()
shutil.copy2(root / "third_party/libebur128/COPYING", licenses / "libebur128-MIT.txt")
queue = (root / "third_party/libebur128/queue/sys/queue.h").read_text()
(licenses / "queue-BSD.txt").write_text(queue[:queue.index("*/") + 2] + "\n", encoding="utf-8")
shutil.copy2(root / "LICENSE", licenses / "GPL-2.0.txt")
manifest = [
    dict(path=p.relative_to(stage).as_posix(), bytes=p.stat().st_size, sha256=hashlib.sha256(p.read_bytes()).hexdigest())
    for p in sorted(stage.rglob("*")) if p.is_file()
]
(stage / "SHA256.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
archive = root / "dist" / f"obs-lufs-meter-{version}-windows-x64.zip"
with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as output:
    for path in sorted(stage.rglob("*")):
        if path.is_file():
            output.write(path, Path(package.name) / path.relative_to(stage))
stable = root / "dist/LUFS-Meter-Windows-x64.zip"
shutil.copy2(archive, stable)
checksum = hashlib.sha256(stable.read_bytes()).hexdigest()
(root / "dist/SHA256SUMS.txt").write_text(f"{checksum}  {stable.name}\n", encoding="ascii")
print(json.dumps(dict(zip=str(stable), bytes=stable.stat().st_size, sha256=checksum, files=len(manifest)), indent=2))
