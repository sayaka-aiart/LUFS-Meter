# Releases

The Windows ZIP is published as a GitHub Release asset named
LUFS-Meter-Windows-x64.zip. The stable README download URL is:

https://github.com/sayaka-aiart/LUFS-Meter/releases/latest/download/LUFS-Meter-Windows-x64.zip

1. Update the version in CMakeLists.txt and the changelog.
2. Build and test from a clean checkout using docs/BUILDING.md.
3. Commit the version and push a matching vX.Y.Z tag.
4. The CI workflow builds/tests the sources and publishes the ZIP and SHA-256
   file after both CI jobs pass. A failed job prevents automatic publication.
5. Download the public asset and check it against SHA256SUMS.txt.

The workflow has read-only permissions for tests. Only the separate release
job has contents: write, and it runs for version tags. No personal token is
stored in this repository.

Local python scripts/package.py produces the same asset layout without GitHub.
The ZIP contains the DLL, usage docs, licenses and corresponding source.
OBS/Qt runtimes, local profiles, SDKs, build caches and raw QA logs are excluded.

The first preview is labeled as such in the release notes. A normal release
flag is used so GitHub's /releases/latest/download/ URL resolves.
