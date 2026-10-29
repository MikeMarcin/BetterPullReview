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


def guard(root: Path, revision: str, version: str, visibility: str, release_id: int | None = None, *, prerelease: bool = True, tag: str | None = None) -> dict:
    urls = command("git", "remote", "get-url", "--push", "--all", "origin", cwd=root).splitlines()
    allowed = {"https://github.com/" + REPOSITORY, "https://github.com/" + REPOSITORY + ".git", "git@github.com:" + REPOSITORY, "git@github.com:" + REPOSITORY + ".git", "ssh://git@github.com/" + REPOSITORY, "ssh://git@github.com/" + REPOSITORY + ".git"}
    require(len(urls) == 1 and urls[0] in allowed, "noncanonical or multiple publication destinations")
    repo = json.loads(command("gh", "repo", "view", REPOSITORY, "--json", "nameWithOwner,visibility"))
    require(repo == {"nameWithOwner": REPOSITORY, "visibility": visibility}, "repository identity or visibility changed")
    immutable = json.loads(command("gh", "api", "-H", "X-GitHub-Api-Version: 2026-03-10", "repos/" + REPOSITORY + "/immutable-releases"))
    require(immutable.get("enabled") is False and immutable.get("enforced_by_owner") is False, "immutable releases prohibit this staged publication")
    tag = tag or "v" + version
    refs = dict((line.split()[1], line.split()[0]) for line in command("git", "ls-remote", "--tags", "origin", "refs/tags/" + tag, "refs/tags/" + tag + "^{}", cwd=root).splitlines())
    require(refs.get("refs/tags/" + tag + "^{}") == revision and refs.get("refs/tags/" + tag) not in {None, revision}, "annotated release tag source changed")
    release = json.loads(command("gh", "api", "repos/" + REPOSITORY + "/releases/tags/" + tag))
    require(release["tag_name"] == tag and release["draft"] is False and release["prerelease"] is prerelease and release.get("immutable") is False, "existing target is not the mutable published prerelease")
    require(type(release["id"]) is int and (release_id is None or release["id"] == release_id), "release identity changed")
    return release


def selected_assets(args: argparse.Namespace) -> tuple[dict[str, Path], set[str]]:
    paths: dict[str, Path] = {}
    platforms = set()
    expected = runtime.release_build.read(args.cohort) if args.cohort else None
    if expected:
        runtime.release_build.check(expected, version=args.version)
        require(expected["sourceCommit"] == args.source_revision, "frozen release source mismatch")
    for manifest in args.manifest:
        record = json.loads(manifest.read_text())
        require(record.get("releaseCohort") == expected, "staged asset cohort differs or explicit --cohort is missing")
        require(all(runtime.families(record).values()), "staged release requires the full variant")
        require((record["platform"], record["generator"]) in {("Windows", "WIX"), ("Linux", "DEB")}, "unselected native platform")
        platform = record["platform"]
        require(platform not in platforms, "multiple manifests for one platform")
        platforms.add(platform)
        for path in native.verify(manifest.resolve(strict=True), args.version, args.source_revision, expected):
            require(path.name not in paths, "asset collision")
            paths[path.name] = path
    if args.macos_dir:
        directory = args.macos_dir.resolve(strict=True)
        base = "BetterPullReview-" + (expected["allocation"]["native"]["assetVersion"] if expected else args.version) + "-macos-universal"
        names = [base + suffix for suffix in [".dmg", ".zip", ".spdx.json"]] + ["Source-" + base + "-corresponding-source.zip"]
        for name in names:
            path = directory / name
            sidecar = directory / (name + ".sha256")
            require(sidecar.read_text().strip() == runtime.digest(path) + "  " + name, "macOS checksum mismatch")
            for item in [path, sidecar]:
                require(item.name not in paths, "asset collision")
                paths[item.name] = item
        macos.verify(directory / (base + ".zip"), directory / (base + ".spdx.json"), directory / ("Source-" + base + "-corresponding-source.zip"), args.version, args.source_revision, expected,
                     directory / ("BetterPullReview-" + args.version + "-macos-universal.zip") if expected else None)
        if expected:
            alias = directory / ("BetterPullReview-" + args.version + "-macos-universal.zip")
            for item in [alias, alias.with_name(alias.name + ".sha256")]:
                require(item.name not in paths, "legacy alias collision")
                paths[item.name] = item
        platforms.add("Darwin")
    require(paths, "no verified assets selected")
    return paths, platforms


def release_absent(tag: str) -> None:
    result = subprocess.run(["gh", "api", "repos/" + REPOSITORY + "/releases/tags/" + tag], capture_output=True, text=True)
    require(result.returncode != 0 and json.loads(result.stdout).get("status") == "404", "stable release already exists or absence is unproven; inspect before retry")


def stable_tag_guard(root: Path, tag: str, revision: str, local_tag: str | None = None, *, pushed: bool = False) -> None:
    if local_tag is None:
        result = subprocess.run(["git", "show-ref", "--verify", "--quiet", "refs/tags/" + tag], cwd=root, capture_output=True)
        require(result.returncode == 1, "local stable tag already exists; inspect before retry")
    else:
        require(command("git", "rev-parse", "refs/tags/" + tag, cwd=root) == local_tag
                and command("git", "rev-parse", "refs/tags/" + tag + "^{}", cwd=root) == revision, "local stable tag changed")
    refs = dict((line.split()[1], line.split()[0]) for line in command("git", "ls-remote", "--tags", "origin", "refs/tags/" + tag, "refs/tags/" + tag + "^{}", cwd=root).splitlines())
    expected = {"refs/tags/" + tag: local_tag, "refs/tags/" + tag + "^{}": revision} if pushed else {}
    require(refs == expected, "remote stable tag occupied or changed; inspect before retry")


def stable_permission(path: Path | None, schema: str, args: argparse.Namespace, tag: str, selected: dict, cohort: dict) -> bool:
    if path is None:
        return False
    record = json.loads(path.read_text())
    require(record.get("schema") == schema and record.get("source") == args.source_revision
            and record.get("version") == args.version and record.get("tag") == tag
            and record.get("sourceTag") == cohort["allocation"]["native"]["releaseTag"]
            and record.get("cohortDigest") == cohort["cohortDigest"]
            and inventory(record.get("assets", [])) == selected, "stable permission identity or assets differ")
    require(record.get("evidence"), "stable permission has no bound evidence")
    for item in record["evidence"]:
        require(runtime.digest(Path(item["path"])) == item["sha256"], "stable permission evidence changed")
    return record.get("accepted") is True and record.get("remainingBlocking") == []


def copy_stable(args: argparse.Namespace, root: Path, source_tag: str, baseline: dict, paths: dict[str, Path], platforms: set[str], selected: dict) -> None:
    require(args.cohort is not None and not args.promote, "stable copy requires a numbered cohort and never promotes its RC")
    require(re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", args.version) is not None, "stable copy requires an exact bare marketing version")
    require(platforms == {"Darwin", "Windows", "Linux"}, "stable copy requires the complete standard platform set")
    cohort = runtime.release_build.read(args.cohort)
    runtime.release_build.check(cohort, version=args.version)
    require(cohort["sourceCommit"] == args.source_revision, "stable cohort source differs")
    cohort_digest = runtime.digest(args.cohort)
    tag = args.version
    require(source_tag != tag and baseline and all(selected.get(name) == row for name, row in baseline.items()), "source RC baseline is not preserved in selected assets")
    qualified = stable_permission(args.stable_acceptance, "bpr.release-stable-copy-acceptance.v1", args, tag, selected, cohort)
    authorized = stable_permission(args.publication_authorization, "bpr.release-publication-authorization.v1", args, tag, selected, cohort)
    source = guard(root, args.source_revision, args.version, args.visibility, tag=source_tag)
    source_id = source["id"]
    missing = check_assets(remote_inventory(source), baseline, selected, complete=False)
    stable_tag_guard(root, tag, args.source_revision)
    release_absent(tag)
    notes_digest = runtime.digest(args.notes_file)
    print(json.dumps({"result": "STABLE_COPY_PREFLIGHT_OK", "source": args.source_revision, "sourceTag": source_tag, "stableTag": tag,
                      "assetCount": len(selected), "missingSourceRCAssets": missing, "qualificationAccepted": qualified,
                      "publicationAuthorized": authorized, "readyToPublish": not missing and qualified and authorized,
                      "dryRun": args.dry_run, "externalMutations": 0}), flush=True)
    if args.dry_run:
        return
    require(qualified and authorized, "stable copy requires accepted qualification and specific publication authorization")
    require(not missing, "complete source RC assets are required before stable creation; append and verify missing assets first")

    def source_guard() -> None:
        require(runtime.digest(args.cohort) == cohort_digest, "stable cohort file changed")
        current = guard(root, args.source_revision, args.version, args.visibility, source_id, tag=source_tag)
        check_assets(remote_inventory(current), baseline, selected, complete=True)
        require(stable_permission(args.stable_acceptance, "bpr.release-stable-copy-acceptance.v1", args, tag, selected, cohort)
                and stable_permission(args.publication_authorization, "bpr.release-publication-authorization.v1", args, tag, selected, cohort), "stable permission changed")
        require(runtime.digest(args.notes_file) == notes_digest, "stable notes changed")

    def target(release_id: int | None, *, published: bool = False) -> dict:
        source_guard()
        stable_tag_guard(root, tag, args.source_revision, local_tag, pushed=True)
        current = json.loads(command("gh", "api", "repos/" + REPOSITORY + "/releases/tags/" + tag))
        require(current["tag_name"] == tag and current["draft"] is (not published) and current["prerelease"] is (not published)
                and current.get("immutable") is False and type(current["id"]) is int
                and (release_id is None or current["id"] == release_id), "stable copy release identity or state changed")
        return current

    source_guard()
    stable_tag_guard(root, tag, args.source_revision)
    release_absent(tag)
    subprocess.run(["git", "tag", "-a", tag, args.source_revision, "-m", "BetterPullReview " + tag + " from " + source_tag], cwd=root, check=True)
    local_tag = command("git", "rev-parse", "refs/tags/" + tag, cwd=root)
    source_guard()
    stable_tag_guard(root, tag, args.source_revision, local_tag)
    release_absent(tag)
    subprocess.run(["git", "push", "origin", "refs/tags/" + tag + ":refs/tags/" + tag], cwd=root, check=True)
    source_guard()
    stable_tag_guard(root, tag, args.source_revision, local_tag, pushed=True)
    release_absent(tag)
    subprocess.run(["gh", "release", "create", tag, "--repo", REPOSITORY, "--verify-tag", "--draft", "--prerelease", "--latest=false", "--title", "BetterPullReview " + tag, "--notes-file", str(args.notes_file)], check=True)
    release_id = target(None)["id"]
    for name, path in sorted(paths.items()):
        current = target(release_id)
        remaining = check_assets(remote_inventory(current), {}, selected, complete=False)
        require(name in remaining and runtime.digest(path) == selected[name]["sha256"] and path.stat().st_size == selected[name]["bytes"], "stable asset changed or appeared concurrently; inspect before retry")
        subprocess.run(["gh", "release", "upload", tag, str(path), "--repo", REPOSITORY], check=True)
    check_assets(remote_inventory(target(release_id)), {}, selected, complete=True)
    require(all(runtime.digest(path) == selected[name]["sha256"] and path.stat().st_size == selected[name]["bytes"] for name, path in paths.items()), "stable bytes changed before publication")
    subprocess.run(["gh", "release", "edit", tag, "--repo", REPOSITORY, "--draft=false", "--prerelease=false", "--latest=true"], check=True)
    result = target(release_id, published=True)
    check_assets(remote_inventory(result), {}, selected, complete=True)
    require(json.loads(command("gh", "api", "repos/" + REPOSITORY + "/releases/latest"))["id"] == release_id, "stable copy is not Latest")
    print(json.dumps({"result": "STABLE_RELEASE_COPIED", "source": args.source_revision, "sourceTag": source_tag, "stableTag": tag, "releaseId": release_id, "url": result["html_url"], "assetCount": len(selected)}))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--source-revision", required=True)
    parser.add_argument("--visibility", choices=["PRIVATE", "PUBLIC"], required=True)
    parser.add_argument("--notes-file", type=Path, required=True)
    parser.add_argument("--baseline-inventory", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, action="append", default=[])
    parser.add_argument("--cohort", type=Path)
    parser.add_argument("--macos-dir", type=Path)
    parser.add_argument("--promotion-acceptance", type=Path)
    parser.add_argument("--promote", action="store_true")
    parser.add_argument("--copy-stable", action="store_true")
    parser.add_argument("--stable-acceptance", type=Path)
    parser.add_argument("--publication-authorization", type=Path)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    require(not (args.cohort and args.promote), "numbered RC build tags cannot be promoted to stable; compatible stable policy is a separate decision")
    tag = "v" + args.version
    if args.cohort:
        cohort = runtime.release_build.read(args.cohort)
        tag = runtime.release_build.check(cohort, version=args.version)["native"]["releaseTag"]
    root = args.repo_root.resolve(strict=True)
    require(re.fullmatch(r"[0-9a-f]{40}", args.source_revision) is not None, "explicit full producer revision required")
    require(not command("git", "status", "--porcelain", cwd=root) and command("git", "branch", "--show-current", cwd=root) == "main", "clean main required")
    subprocess.run(["git", "merge-base", "--is-ancestor", args.source_revision, "HEAD"], cwd=root, check=True)
    changed = set(command("git", "diff", "--name-only", args.source_revision, "HEAD", cwd=root).splitlines())
    require(changed <= TOOL_PATHS, "tool revision changes producer inputs")
    require(args.notes_file.is_file(), "notes file missing")
    record = json.loads(args.baseline_inventory.read_text())
    require(record["source"] == args.source_revision and record["version"] == args.version and record["repository"] == REPOSITORY
            and record.get("tag", "v" + args.version) == tag, "baseline identity mismatch")
    baseline = inventory(record["assets"])
    require(baseline, "empty existing-asset baseline")
    cohort_file_digest = runtime.digest(args.cohort) if args.cohort else None
    paths, platforms = selected_assets(args)
    require(args.cohort is None or runtime.digest(args.cohort) == cohort_file_digest, "cohort changed during asset verification")
    selected = {name: {"sha256": runtime.digest(path), "bytes": path.stat().st_size} for name, path in paths.items()}
    if args.copy_stable:
        require(args.promotion_acceptance is None, "stable copy uses its separately bound stable acceptance")
        copy_stable(args, root, tag, baseline, paths, platforms, selected)
        return
    require(args.stable_acceptance is None and args.publication_authorization is None, "stable permission options require --copy-stable")
    if args.promote:
        require(platforms == {"Darwin", "Windows", "Linux"} and args.promotion_acceptance is not None, "promotion requires the complete standard platform set and acceptance")
        acceptance = json.loads(args.promotion_acceptance.read_text())
        require(acceptance.get("schema") == "bpr.release-promotion-acceptance.v1" and acceptance.get("accepted") is True and acceptance.get("source") == args.source_revision and acceptance.get("version") == args.version and acceptance.get("remainingBlocking") == [], "stable qualification is not accepted")
        require(acceptance.get("evidence"), "promotion acceptance has no evidence")
        for item in acceptance["evidence"]:
            require(runtime.digest(Path(item["path"])) == item["sha256"], "promotion evidence changed")
    release = guard(root, args.source_revision, args.version, args.visibility, tag=tag)
    release_id = release["id"]
    missing = check_assets(remote_inventory(release), baseline, selected, complete=args.promote)
    print(json.dumps({"result": "EXISTING_RELEASE_PREFLIGHT_OK", "source": args.source_revision, "toolRevision": command("git", "rev-parse", "HEAD", cwd=root), "releaseId": release_id, "missingAssets": missing, "promote": args.promote, "dryRun": args.dry_run}), flush=True)
    if args.dry_run:
        return
    for name in missing:
        current = guard(root, args.source_revision, args.version, args.visibility, release_id, tag=tag)
        remaining = check_assets(remote_inventory(current), baseline, selected, complete=False)
        require(name in remaining, "asset changed concurrently; inspect before retry")
        require(runtime.digest(paths[name]) == selected[name]["sha256"] and paths[name].stat().st_size == selected[name]["bytes"], "selected bytes changed before upload")
        subprocess.run(["gh", "release", "upload", tag, str(paths[name]), "--repo", REPOSITORY], check=True)
    current = guard(root, args.source_revision, args.version, args.visibility, release_id, tag=tag)
    check_assets(remote_inventory(current), baseline, selected, complete=True)
    edit = ["gh", "release", "edit", tag, "--repo", REPOSITORY, "--notes-file", str(args.notes_file)]
    if args.promote:
        edit += ["--prerelease=false", "--latest=true"]
    subprocess.run(edit, check=True)
    result = guard(root, args.source_revision, args.version, args.visibility, release_id, prerelease=not args.promote, tag=tag)
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
