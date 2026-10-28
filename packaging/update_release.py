#!/usr/bin/env python3
"""Guarded additions to an existing prerelease, or promotion of its complete set."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

import package_runtime as runtime
import verify_native_release as native
import verify_release_sources as macos

REPOSITORY = "MikeMarcin/BetterPullReview"
TOOL_PATHS = {"tools/publish-release.sh", "packaging/update_release.py", "tests/scripts/native_release_tests.py"}


def command(*args: str, cwd: Path | None = None) -> str:
    return subprocess.check_output(args, cwd=cwd, text=True).strip()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def inventory(rows: list[dict]) -> dict[str, dict]:
    result = {}
    for row in rows:
        name = row["name"]
        require(isinstance(name, str) and name not in {"", ".", ".."} and Path(name).name == name and "/" not in name and "\\" not in name, "unsafe asset name")
        require(name not in result, "duplicate asset name")
        digest = row["sha256"]
        require(re.fullmatch(r"[0-9a-f]{64}", digest) is not None and type(row["bytes"]) is int and row["bytes"] >= 0, "invalid asset identity")
        result[name] = {"sha256": digest, "bytes": row["bytes"]}
    return result


def remote_inventory(release: dict) -> dict[str, dict]:
    rows = []
    for asset in release["assets"]:
        digest = asset.get("digest", "")
        require(isinstance(digest, str) and digest.startswith("sha256:"), "remote asset has no SHA-256 digest")
        rows.append({"name": asset["name"], "sha256": digest.removeprefix("sha256:"), "bytes": asset["size"]})
    return inventory(rows)


def check_assets(observed: dict, baseline: dict, selected: dict, *, complete: bool) -> list[str]:
    require(all(observed.get(name) == row for name, row in baseline.items()), "existing asset disappeared or changed")
    expected = dict(baseline)
    for name, row in selected.items():
        require(name not in expected or expected[name] == row, "selected asset would overwrite existing bytes")
        expected[name] = row
    require(all(name in expected and expected[name] == row for name, row in observed.items()), "unexpected or conflicting remote asset")
    missing = sorted(set(selected) - set(observed))
    require(not complete or (not missing and observed == expected), "complete release asset set is not present")
    return missing


def guard(root: Path, revision: str, version: str, visibility: str, release_id: int | None = None, *, prerelease: bool = True) -> dict:
    urls = command("git", "remote", "get-url", "--push", "--all", "origin", cwd=root).splitlines()
    allowed = {"https://github.com/" + REPOSITORY, "https://github.com/" + REPOSITORY + ".git", "git@github.com:" + REPOSITORY, "git@github.com:" + REPOSITORY + ".git", "ssh://git@github.com/" + REPOSITORY, "ssh://git@github.com/" + REPOSITORY + ".git"}
    require(len(urls) == 1 and urls[0] in allowed, "noncanonical or multiple publication destinations")
    repo = json.loads(command("gh", "repo", "view", REPOSITORY, "--json", "nameWithOwner,visibility"))
    require(repo == {"nameWithOwner": REPOSITORY, "visibility": visibility}, "repository identity or visibility changed")
    immutable = json.loads(command("gh", "api", "-H", "X-GitHub-Api-Version: 2026-03-10", "repos/" + REPOSITORY + "/immutable-releases"))
    require(immutable.get("enabled") is False and immutable.get("enforced_by_owner") is False, "immutable releases prohibit this staged publication")
    tag = "v" + version
    refs = dict((line.split()[1], line.split()[0]) for line in command("git", "ls-remote", "--tags", "origin", "refs/tags/" + tag, "refs/tags/" + tag + "^{}", cwd=root).splitlines())
    require(refs.get("refs/tags/" + tag + "^{}") == revision and refs.get("refs/tags/" + tag) not in {None, revision}, "annotated release tag source changed")
    release = json.loads(command("gh", "api", "repos/" + REPOSITORY + "/releases/tags/" + tag))
    require(release["tag_name"] == tag and release["draft"] is False and release["prerelease"] is prerelease and release.get("immutable") is False, "existing target is not the mutable published prerelease")
    require(type(release["id"]) is int and (release_id is None or release["id"] == release_id), "release identity changed")
    return release


def selected_assets(args: argparse.Namespace) -> tuple[dict[str, Path], set[str]]:
    paths: dict[str, Path] = {}
    platforms = set()
    for manifest in args.manifest:
        record = json.loads(manifest.read_text())
        require(all(runtime.families(record).values()), "staged release requires the full variant")
        require((record["platform"], record["generator"]) in {("Windows", "WIX"), ("Linux", "DEB")}, "unselected native platform")
        platform = record["platform"]
        require(platform not in platforms, "multiple manifests for one platform")
        platforms.add(platform)
        for path in native.verify(manifest.resolve(strict=True), args.version, args.source_revision):
            require(path.name not in paths, "asset collision")
            paths[path.name] = path
    if args.macos_dir:
        directory = args.macos_dir.resolve(strict=True)
        base = "BetterPullReview-" + args.version + "-macos-universal"
        names = [base + suffix for suffix in [".dmg", ".zip", ".spdx.json"]] + ["Source-" + base + "-corresponding-source.zip"]
        for name in names:
            path = directory / name
            sidecar = directory / (name + ".sha256")
            require(sidecar.read_text().strip() == runtime.digest(path) + "  " + name, "macOS checksum mismatch")
            for item in [path, sidecar]:
                require(item.name not in paths, "asset collision")
                paths[item.name] = item
        macos.verify(directory / (base + ".zip"), directory / (base + ".spdx.json"), directory / ("Source-" + base + "-corresponding-source.zip"), args.version, args.source_revision)
        platforms.add("Darwin")
    require(paths, "no verified assets selected")
    return paths, platforms


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--source-revision", required=True)
    parser.add_argument("--visibility", choices=["PRIVATE", "PUBLIC"], required=True)
    parser.add_argument("--notes-file", type=Path, required=True)
    parser.add_argument("--baseline-inventory", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, action="append", default=[])
    parser.add_argument("--macos-dir", type=Path)
    parser.add_argument("--promotion-acceptance", type=Path)
    parser.add_argument("--promote", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    root = args.repo_root.resolve(strict=True)
    require(re.fullmatch(r"[0-9a-f]{40}", args.source_revision) is not None, "explicit full producer revision required")
    require(not command("git", "status", "--porcelain", cwd=root) and command("git", "branch", "--show-current", cwd=root) == "main", "clean main required")
    subprocess.run(["git", "merge-base", "--is-ancestor", args.source_revision, "HEAD"], cwd=root, check=True)
    changed = set(command("git", "diff", "--name-only", args.source_revision, "HEAD", cwd=root).splitlines())
    require(changed <= TOOL_PATHS, "tool revision changes producer inputs")
    require(args.notes_file.is_file(), "notes file missing")
    record = json.loads(args.baseline_inventory.read_text())
    require(record["source"] == args.source_revision and record["version"] == args.version and record["repository"] == REPOSITORY, "baseline identity mismatch")
    baseline = inventory(record["assets"])
    require(baseline, "empty existing-asset baseline")
    paths, platforms = selected_assets(args)
    selected = {name: {"sha256": runtime.digest(path), "bytes": path.stat().st_size} for name, path in paths.items()}
    if args.promote:
        require(platforms == {"Darwin", "Windows", "Linux"} and args.promotion_acceptance is not None, "promotion requires the complete standard platform set and acceptance")
        acceptance = json.loads(args.promotion_acceptance.read_text())
        require(acceptance.get("schema") == "bpr.release-promotion-acceptance.v1" and acceptance.get("accepted") is True and acceptance.get("source") == args.source_revision and acceptance.get("version") == args.version and acceptance.get("remainingBlocking") == [], "stable qualification is not accepted")
        require(acceptance.get("evidence"), "promotion acceptance has no evidence")
        for item in acceptance["evidence"]:
            require(runtime.digest(Path(item["path"])) == item["sha256"], "promotion evidence changed")
    release = guard(root, args.source_revision, args.version, args.visibility)
    release_id = release["id"]
    missing = check_assets(remote_inventory(release), baseline, selected, complete=args.promote)
    print(json.dumps({"result": "EXISTING_RELEASE_PREFLIGHT_OK", "source": args.source_revision, "toolRevision": command("git", "rev-parse", "HEAD", cwd=root), "releaseId": release_id, "missingAssets": missing, "promote": args.promote, "dryRun": args.dry_run}), flush=True)
    if args.dry_run:
        return
    for name in missing:
        current = guard(root, args.source_revision, args.version, args.visibility, release_id)
        remaining = check_assets(remote_inventory(current), baseline, selected, complete=False)
        require(name in remaining, "asset changed concurrently; inspect before retry")
        require(runtime.digest(paths[name]) == selected[name]["sha256"] and paths[name].stat().st_size == selected[name]["bytes"], "selected bytes changed before upload")
        subprocess.run(["gh", "release", "upload", "v" + args.version, str(paths[name]), "--repo", REPOSITORY], check=True)
    current = guard(root, args.source_revision, args.version, args.visibility, release_id)
    check_assets(remote_inventory(current), baseline, selected, complete=True)
    edit = ["gh", "release", "edit", "v" + args.version, "--repo", REPOSITORY, "--notes-file", str(args.notes_file)]
    if args.promote:
        edit += ["--prerelease=false", "--latest=true"]
    subprocess.run(edit, check=True)
    result = guard(root, args.source_revision, args.version, args.visibility, release_id, prerelease=not args.promote)
    require(result["id"] == release_id and result["draft"] is False and result["prerelease"] is (not args.promote), "unexpected final release state")
    check_assets(remote_inventory(result), baseline, selected, complete=True)
    if args.promote:
        require(json.loads(command("gh", "api", "repos/" + REPOSITORY + "/releases/latest"))["id"] == release_id, "promoted release is not Latest")
    print(json.dumps({"result": "RELEASE_PROMOTED" if args.promote else "PRERELEASE_ASSETS_ADDED", "source": args.source_revision, "releaseId": release_id, "url": result["html_url"], "assetCount": len(result["assets"])}))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError, subprocess.CalledProcessError) as error:
        raise SystemExit("existing release update refused; inspect actual remote state before retry: " + str(error)) from error
