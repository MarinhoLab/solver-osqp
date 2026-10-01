#!/usr/bin/env python3
"""Pre-publish version check for the PyPI upload job.

PyPI never allows re-publishing a file (name + version + filename).  When
the rolling wheel version computed by ``setuptools-git-versioning``
(``{tag}.{ccount}``) collides with an already-published release -- e.g. after
a force-pushed branch reuses the commit count since the last monthly version
tag -- the upload step fails with a cryptic ``400 File already exists``.

This script checks the distributions in ``dist/`` against PyPI *before* the
upload and turns that situation into an actionable CI result:

* a version not on PyPI, or files of it not on PyPI      -> proceed (exit 0)
* every file already on PyPI with the same blake2_256
  digest (a no-op re-publish)                            -> skip publish
  (exit 0, GitHub output ``all-identical=true``)
* a file with the same name/version on PyPI but different
  content (a different build state)                     -> conflict (exit 1)
  with instructions to cut the next monthly version tag

Usage:
    python tools/pypi_precheck.py [dist_dir]
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from typing import NoReturn

PYPI_JSON_URL = "https://pypi.org/pypi/{name}/{version}/json"
TIMEOUT_S = 30

# PEP 427: {distribution}-{version}(-{build})?-{python}-{abi}-{platform}.whl
# The version is a maximal run of non-``-``/non-``_`` characters: it is a
# single character class, so the match is linear-time (the original nested
# ``(?:\.[^\-_]+)*`` group let the engine re-split dot runs exponentially --
# CodeQL "Inefficient regular expression").
WHEEL_FILENAME_RE = re.compile(
    r"^(?P<n>.+?)-(?P<version>[^\-_]+)(?:-(?P<build>\d+[-\w]*))?"
    r"-(?P<python>\w+)-(?P<abi>\w+)-(?P<platform>\w+)\.whl$"
)


def fail(message: str) -> NoReturn:
    print(f"[pypi-precheck] ERROR: {message}", file=sys.stderr)
    sys.exit(2)


def normalize_name(name: str) -> str:
    """PEP 503 normalization, i.e. PyPI's canonical name form."""
    return re.sub(r"[-_.]+", "-", name).lower()


def read_package_name(root: Path) -> str:
    """Read the distribution name from pyproject.toml (stdlib only)."""
    pyproject = root / "pyproject.toml"
    if not pyproject.is_file():
        fail(f"pyproject.toml not found in {root}")
    text = pyproject.read_text(encoding="utf-8")
    try:
        import tomllib  # Python >= 3.11 (CI uses 3.12)

        name = tomllib.loads(text).get("project", {}).get("name")
    except ModuleNotFoundError:  # older local Pythons: fall back to a regex
        match = re.search(r"(?m)^\s*name\s*=\s*['\"](?P<name>[^'\"]+)['\"]", text)
        name = match.group("name") if match else None
    if not name:
        fail(f"cannot determine package name from {pyproject}")
    return str(name).strip()


def wheel_version(filename: str) -> str | None:
    """Extract the PEP 440 version from a wheel filename.

    ``{distribution}-{version}(-{build})?-{python}-{abi}-{platform}.whl``
    (PEP 427).  The optional build tag is dropped: it only disambiguates the
    file name and never appears in the version reported by the metadata.
    """
    match = WHEEL_FILENAME_RE.match(filename)
    if match is None:
        return None
    return match.group("version")


def sdist_version(filename: str) -> str | None:
    """Extract the PEP 440 version from ``{name}-{version}.tar.gz``."""
    stem = filename[: -len(".tar.gz")]
    _, sep, version = stem.rpartition("-")
    return version if sep else None


def collect_dist_files(dist_dir: Path) -> dict[str, list[Path]]:
    """Group the distributions in ``dist_dir`` by their version."""
    files: dict[str, list[Path]] = {}
    if not dist_dir.is_dir():
        fail(f"dist directory not found: {dist_dir}")
    for path in sorted(dist_dir.iterdir()):
        if path.suffix == ".whl":
            version = wheel_version(path.name)
        elif path.name.endswith(".tar.gz"):
            version = sdist_version(path.name)
        else:
            continue
        if version is None:
            fail(f"cannot parse the version from distribution filename: {path.name}")
        files.setdefault(version, []).append(path)
    if not files:
        fail(f"no .whl or .tar.gz distributions found in {dist_dir}")
    return files


def blake2b_256(path: Path) -> str:
    digest = hashlib.blake2b(digest_size=32)
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def remote_digest(entry: dict) -> str | None:
    """PyPI's JSON API reports the file hash under digests.blake2b_256."""
    digests = entry.get("digests") or {}
    return digests.get("blake2b_256") or digests.get("blake2_256")


def pypi_files(package: str, version: str) -> list[dict] | None:
    """Return the files PyPI serves for (package, version).

    Returns None when the version has not been published yet.  Network
    problems are reported as a warning and treated as "not publishable
    info": the publish step then fails with PyPI's own authoritative error
    instead of this pre-check adding a second failure mode.
    """
    url = PYPI_JSON_URL.format(
        name=normalize_name(package), version=urllib.parse.quote(version, safe="")
    )
    request = urllib.request.Request(
        url, headers={"User-Agent": "solver-osqp-ci/pypi-precheck"}
    )
    try:
        with urllib.request.urlopen(request, timeout=TIMEOUT_S) as response:
            payload = json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as error:
        if error.code == 404:
            return None
        print(
            f"[pypi-precheck] WARNING: unexpected HTTP {error.code} from PyPI "
            f"for {package}=={version}; the publish step will report the real error."
        )
        return []
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError, OSError) as error:
        print(
            f"[pypi-precheck] WARNING: could not reach PyPI ({error!r}); "
            f"proceeding to let the publish step decide."
        )
        return []
    return payload.get("urls", [])


def set_output(key: str, value: str) -> None:
    output = os.environ.get("GITHUB_OUTPUT")
    if output:
        with open(output, "a", encoding="utf-8") as handle:
            handle.write(f"{key}={value}\n")


def main(argv: list[str]) -> int:
    dist_dir = Path(argv[0]) if argv else Path("dist")
    repo_root = Path(__file__).resolve().parent.parent
    package = read_package_name(repo_root)
    by_version = collect_dist_files(dist_dir)

    print(f"[pypi-precheck] package: {package}")
    print(f"[pypi-precheck] dist dir: {dist_dir}")

    all_identical = True
    conflicts: list[tuple[Path, str, str]] = []

    for version, paths in sorted(by_version.items()):
        remote = pypi_files(package, version)
        remote_hashes = {entry["filename"]: remote_digest(entry) for entry in (remote or [])}
        state = "published" if remote is not None else "not published"
        print(f"[pypi-precheck] version {version}: {len(paths)} file(s), {state} on PyPI")
        for path in paths:
            local_hash = blake2b_256(path)
            if path.name not in remote_hashes:
                print(f"[pypi-precheck]   {path.name}: new on PyPI")
                all_identical = False
            elif remote_hashes[path.name] == local_hash:
                print(f"[pypi-precheck]   {path.name}: already on PyPI (identical)")
            else:
                print(
                    f"[pypi-precheck]   {path.name}: CONFLICT - already on PyPI "
                    f"with different content"
                )
                conflicts.append((path, remote_hashes[path.name] or "?", local_hash))
                all_identical = False

    if conflicts:
        for _, remote_hash, local_hash in conflicts:
            print(f"[pypi-precheck]   PyPI  blake2b_256: {remote_hash}")
            print(f"[pypi-precheck]   local blake2b_256: {local_hash}")
        print("[pypi-precheck] Cannot publish: PyPI does not allow re-using an "
              "already-published version with different content.")
        print("[pypi-precheck] The wheel version (setuptools-git-versioning "
              "'{tag}.{ccount}') collided with an older build - usually because "
              "a branch was force-pushed/reset and the commit count since the "
              "last monthly tag was reused.")
        print("[pypi-precheck] Cut the next monthly version tag (e.g. '26.09.0'; "
              "see tools/version.sh), push it, and re-run this workflow.")
        print("[pypi-precheck] See also: https://pypi.org/help/#uploading-an-existing-version")
        set_output("all-identical", "false")
        return 1

    set_output("all-identical", "true" if all_identical else "false")
    if all_identical:
        print("[pypi-precheck] All distribution files are already on PyPI with "
              "identical content; the publish step will be skipped.")
    else:
        print("[pypi-precheck] OK: no version collisions; the publish step may proceed.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
