"""Check the files selected by Git, not ignored local SDK/QA material."""
from pathlib import Path
import json
import re
import subprocess

root = Path(__file__).resolve().parent.parent
raw = subprocess.check_output(["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"], cwd=root)
names = sorted(set(p.decode("utf-8") for p in raw.split(b"\0") if p))
required = {"README.md", "README_JA.md", "LICENSE", "THIRD_PARTY_NOTICES.md", "CMakeLists.txt", ".github/workflows/build.yml", "docs/images/horizontal.png", "docs/images/vertical.png"}
assert required.issubset(names), f"Missing public files: {required - set(names)}"
patterns = [
    re.compile(r"(?:C:[/\\]Users[/\\]|/home/)[A-Za-z0-9_.-]+", re.I),
    re.compile(r"gh[pousr]_[A-Za-z0-9]{20,}"),
    re.compile(r"github_pat_[A-Za-z0-9_]{20,}"),
    re.compile(r"-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----"),
]
for name in names:
    path = root / name
    assert path.is_file(), name
    assert name.split("/")[0] not in {".git", "qa", "deps", "dist"} and not name.startswith("build"), name
    assert path.stat().st_size < 5_000_000, f"Unexpected large public file: {name}"
    if path.suffix.lower() in {".png"}:
        continue
    text = path.read_text(encoding="utf-8")
    for pattern in patterns:
        assert not pattern.search(text), f"Local path or credential pattern in {name}"
    if path.suffix == ".md":
        for match in re.finditer(r"\]\(([^)]+)\)", text):
            target = match.group(1).split("#")[0]
            if target and not re.match(r"[a-z]+://", target):
                assert (path.parent / target).exists(), f"Broken document link: {name}: {target}"
workflow = json.loads((root / ".github/workflows/build.yml").read_text())
assert workflow["permissions"]["contents"] == "read"
assert workflow["jobs"]["release"]["permissions"]["contents"] == "write"
print(f"PASS: {len(names)} public files, size/path/credential scan, document links and workflow permissions")
