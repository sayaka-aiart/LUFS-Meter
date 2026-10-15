"""Package a minimal install ZIP and a separate corresponding-source ZIP."""
from pathlib import Path
import ctypes
import hashlib
import json
import os
import re
import shutil
import tempfile
import zipfile

root = Path(__file__).resolve().parent.parent
version = re.search(r"project\(obs_lufs_meter VERSION ([0-9.]+)", (root / "CMakeLists.txt").read_text(encoding="utf-8")).group(1)
dll = root / "dist" / f"obs-lufs-meter-{version}-windows-x64/obs-plugins/64bit/obs-lufs-meter.dll"
if not dll.exists():
    raise SystemExit("Run scripts/build.ps1 before packaging.")

# Inspect resources without executing plugin code or loading OBS/Qt dependencies.
if os.name != "nt":
    raise SystemExit("Package the Windows binary on Windows to verify embedded notices.")
kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.LoadLibraryExW.argtypes = [ctypes.c_wchar_p, ctypes.c_void_p, ctypes.c_uint]
kernel.LoadLibraryExW.restype = ctypes.c_void_p
kernel.FindResourceW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_void_p]
kernel.FindResourceW.restype = ctypes.c_void_p
kernel.SizeofResource.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
kernel.SizeofResource.restype = ctypes.c_uint
kernel.LoadResource.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
kernel.LoadResource.restype = ctypes.c_void_p
kernel.LockResource.argtypes = [ctypes.c_void_p]
kernel.LockResource.restype = ctypes.c_void_p
kernel.FreeLibrary.argtypes = [ctypes.c_void_p]
queue = (root / "third_party/libebur128/queue/sys/queue.h").read_text(encoding="utf-8")
notices = {
    "LUFS_GPL_LICENSE": (root / "LICENSE").read_text(encoding="utf-8"),
    "LUFS_LIBEBUR128_LICENSE": (root / "third_party/libebur128/COPYING").read_text(encoding="utf-8"),
    "LUFS_QUEUE_LICENSE": queue[:queue.index("*/") + 2] + "\n",
}
module = kernel.LoadLibraryExW(str(dll), None, 2)  # LOAD_LIBRARY_AS_DATAFILE
if not module:
    raise ctypes.WinError(ctypes.get_last_error())
try:
    for name, expected in notices.items():
        resource = kernel.FindResourceW(module, name, 10)  # RT_RCDATA
        if not resource:
            raise RuntimeError(f"Missing embedded notice: {name}")
        data = kernel.LockResource(kernel.LoadResource(module, resource))
        actual = ctypes.string_at(data, kernel.SizeofResource(module, resource)).decode("utf-8").replace("\r\n", "\n")
        assert actual == expected, f"Embedded notice differs: {name}"
finally:
    kernel.FreeLibrary(module)

dist = root / "dist"
stable = dist / "LUFS-Meter-Windows-x64.zip"
instructions = (root / "docs/INSTALL_JA.txt").read_text(encoding="utf-8").replace("@VERSION@", version)
with zipfile.ZipFile(stable, "w", zipfile.ZIP_DEFLATED) as output:
    output.write(dll, dll.name)
    output.writestr("導入方法.txt", instructions.encode("utf-8-sig"))

# Keep previous staging folders; include only explicitly selected public files.
stage = Path(tempfile.mkdtemp(prefix=f"source-stage-{version}-", dir=dist))
public_files = ("CMakeLists.txt", "README.md", "README_JA.md", "README_EN.md", "LICENSE", "THIRD_PARTY_NOTICES.md", "CHANGELOG.md", "CONTRIBUTING.md", ".gitignore", ".gitattributes", ".editorconfig")
public_dirs = ("src", "tests", "sdk", "scripts", "third_party", "cmake", "docs", ".github")
for name in public_files:
    shutil.copy2(root / name, stage / name)
for name in public_dirs:
    shutil.copytree(root / name, stage / name, ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
source_zip = dist / "LUFS-Meter-Source-and-Licenses.zip"
with zipfile.ZipFile(source_zip, "w", zipfile.ZIP_DEFLATED) as output:
    for path in sorted(stage.rglob("*")):
        if path.is_file():
            output.write(path, Path(f"LUFS-Meter-{version}-source") / path.relative_to(stage))

with zipfile.ZipFile(stable) as archive:
    assert set(archive.namelist()) == {"obs-lufs-meter.dll", "導入方法.txt"}
    assert archive.read(dll.name) == dll.read_bytes()
    assert archive.testzip() is None
with zipfile.ZipFile(source_zip) as archive:
    assert archive.testzip() is None
    assert archive.read(f"LUFS-Meter-{version}-source/LICENSE") == (root / "LICENSE").read_bytes()

assets = [dict(name=p.name, bytes=p.stat().st_size, sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in (stable, source_zip)]
(dist / "SHA256SUMS.txt").write_text("".join(f"{a['sha256']}  {a['name']}\n" for a in assets), encoding="ascii")
print(json.dumps(dict(version=version, embedded_notices="verified", assets=assets), indent=2))
