#!/usr/bin/env python3
"""External, local-only configured-gh fixture; never supplies product consent.

prepare creates a disposable canonical gh executable and synthetic responses.
launch runs the actual tests-disabled app for external accessibility automation.
The structural call report is deliberately not a rendered-public-path verdict.
"""

import argparse
import base64
import copy
import json
import os
from pathlib import Path
import re
import random
import subprocess
import sys
from urllib.parse import parse_qs, urlsplit

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "tests/fixtures/github/pull-requests/cli-read-v1"
# The shipped macOS locator (native_gh_executable in github_application.cpp)
# falls back to these directories when PATH holds no gh ([[BPR-0909]]).
PRODUCT_GH_FALLBACK_DIRECTORIES = ("/opt/homebrew/bin", "/usr/local/bin", "/opt/local/bin")


def missing_process_refusal(course, directories=PRODUCT_GH_FALLBACK_DIRECTORIES):
    """Why a missing-process launch would reach a real gh, or None.

    Removing the fixture's gh from PATH no longer leaves the app without one
    where an installed gh sits in a fallback directory: the course would run
    the machine's own gh and account instead of failing closed.
    """
    if course != "missing-process":
        return None
    installed = [str(Path(directory) / "gh") for directory in directories
                 if os.access(Path(directory) / "gh", os.X_OK)]
    if not installed:
        return None
    return ("missing-process would reach the installed gh the app falls back to: "
            + ", ".join(installed))


def is_large(course):
    return course in ("large-positive", "modified-missing-left")


def add_large_repository_history(directory, repository, base, head):
    """Two changed files still inherit an unrelated over-limit history blob."""
    environment = {"PATH": "/usr/bin:/bin", "HOME": str(directory / "home"),
                   "GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": "/dev/null",
                   "GIT_AUTHOR_NAME": "Fixture", "GIT_AUTHOR_EMAIL": "fixture@example.invalid",
                   "GIT_COMMITTER_NAME": "Fixture", "GIT_COMMITTER_EMAIL": "fixture@example.invalid",
                   "GIT_AUTHOR_DATE": "2000-01-01T00:00:00+0000",
                   "GIT_COMMITTER_DATE": "2000-01-01T00:00:00+0000"}

    def git(*arguments, data=None):
        return subprocess.check_output(["/usr/bin/git", "--git-dir", str(repository),
                                        *arguments], input=data, env=environment).strip()

    blob = git("hash-object", "-w", "--stdin", data=random.Random(1199).randbytes(34 * 1024 * 1024))
    tree = git("mktree", data=b"100644 blob " + blob + b"\tunrelated.bin\n")
    ancestor = git("commit-tree", tree.decode(), data=b"unrelated repository history\n")
    new_base = git("commit-tree", git("rev-parse", base + "^{tree}").decode(),
                   "-p", ancestor.decode(), data=b"base without unrelated file\n")
    new_head = git("commit-tree", git("rev-parse", head + "^{tree}").decode(),
                   "-p", new_base.decode(), data=b"small pull request\n")
    return new_base.decode(), new_head.decode()


def prepare(directory, course):
    directory.mkdir(mode=0o700, parents=True, exist_ok=False)
    (directory / "home").mkdir()
    (directory / "bin").mkdir()
    large = is_large(course)
    object_fixture = (ROOT / "tests/fixtures/github/pull-requests/large-review-v1"
                      if large else FIXTURE)
    objects = json.loads((object_fixture / ("manifest.json" if large else "objects.json")).read_text())
    base, head = ((objects["base"], objects["heads"][-1]) if large else
                  (objects["commits"]["base"], objects["commits"]["head1"]))
    repository = directory / "repository.git"
    subprocess.run(["/usr/bin/git", "init", "--bare", str(repository)],
                   check=True, capture_output=True)
    with (object_fixture / "objects.pack").open("rb") as pack:
        subprocess.run(["/usr/bin/git", "--git-dir", str(repository),
                        "index-pack", "--stdin"], stdin=pack, check=True, capture_output=True)
    if large:
        # Real blobless clones omit these too. Without them the previous
        # production course missed Git's special missing-blob fsck failures.
        environment = {"PATH": "/usr/bin:/bin", "HOME": str(directory / "home"),
                       "GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": "/dev/null",
                       "GIT_AUTHOR_NAME": "Fixture", "GIT_AUTHOR_EMAIL": "fixture@example.invalid",
                       "GIT_COMMITTER_NAME": "Fixture", "GIT_COMMITTER_EMAIL": "fixture@example.invalid",
                       "GIT_AUTHOR_DATE": "2000-01-01T00:00:00+0000",
                       "GIT_COMMITTER_DATE": "2000-01-01T00:00:00+0000"}

        def git(*arguments, data=None):
            return subprocess.check_output(["/usr/bin/git", "--git-dir", str(repository),
                                            *arguments], input=data, env=environment)

        attributes = git("hash-object", "-w", "--stdin", data=b"*.cpp text\n").strip()
        modules = git("hash-object", "-w", "--stdin", data=b"").strip()
        replacements = []
        for oid in [base, *objects["heads"]]:
            entries = git("ls-tree", oid)
            if course == "modified-missing-left" and oid == base:
                # Provider M uses another comparison basis; the selected exact
                # base tip really lacks this path, not an empty source blob.
                original_src = git("rev-parse", oid + ":src").strip()
                src_entries = b"".join(entry + b"\n" for entry in git("ls-tree", original_src.decode()).splitlines()
                                        if not entry.endswith(b"\tfile_0000.cpp"))
                replacement_src = git("mktree", data=src_entries).strip()
                entries = entries.replace(original_src + b"\tsrc\n", replacement_src + b"\tsrc\n")
            entries += b"100644 blob " + attributes + b"\t.gitattributes\n"
            entries += b"100644 blob " + modules + b"\t.gitmodules\n"
            tree = git("mktree", data=entries).decode().strip()
            parent = ["-p", replacements[-1]] if replacements else []
            replacements.append(git("commit-tree", tree, *parent, data=b"sparse special blobs\n").decode().strip())
        base, head = replacements[0], replacements[-1]
        objects["heads"] = replacements[1:]
    if course == "small-large-repository":
        base, head = add_large_repository_history(directory, repository, base, head)
    source = ROOT / "tests/fixtures/github/contracts/v1/github-com"
    manifest = json.loads((source / "manifest.json").read_text())
    routes = {}
    for entry in manifest["interactions"]:
        body = (source / entry["response_file"]).read_text()
        body = body.replace("1" * 40, base).replace("2" * 40, head)
        routes[entry["path"].replace("2" * 40, head)] = json.loads(body)
    # REST, GraphQL and Git describe the same stored synthetic tree.
    paths = ([item["path"] for item in objects["files"]] if large else
             ["review.cpp", "secondary.cpp"])
    routes["/repos/octo-org/review-repository/pulls/17"]["changed_files"] = len(paths)
    files = [{"filename": name, "status": "modified", "additions": 1,
              "deletions": 1} for name in paths]
    routes["/repos/octo-org/review-repository/pulls/17/files"] = files
    if large:
        routes["/repos/octo-org/review-repository/pulls/17"]["commits"] = len(objects["heads"])
        prototype = routes["/repos/octo-org/review-repository/pulls/17/commits"][0]
        commits = []
        for index, oid in enumerate(objects["heads"]):
            commit = copy.deepcopy(prototype)
            commit.update(sha=oid, node_id=f"C_large_{index}")
            commits.append(commit)
        routes["/repos/octo-org/review-repository/pulls/17/commits"] = commits
    routes["/repos/octo-org/review-repository/pulls/17/reviews"] = []
    graph = routes["/graphql"]["data"]["repository"]["pullRequest"]
    graph["files"] = {"totalCount": len(paths), "nodes": [
        {"path": item["filename"], "viewerViewedState": "UNVIEWED"}
        for item in files], "pageInfo": {"hasNextPage": False, "endCursor": "fixture-files-end"}}
    graph["reviewThreads"] = {"totalCount": 0, "nodes": [],
                              "pageInfo": {"hasNextPage": False, "endCursor": None}}
    parts = [json.loads((ROOT / "tests/fixtures/github/capabilities/v2-parts/github-com"
                         / f"supported-{part}.json").read_text()) for part in range(9)]
    (directory / "fixture.json").write_text(json.dumps({
        "course": course, "routes": routes, "parts": parts,
        "base": base, "head": head}))
    executable = directory / "bin/gh"
    executable.write_text(f"#!{sys.executable}\n" + Path(__file__).read_text().split("\n", 1)[1])
    executable.chmod(0o700)
    if course == "missing-process":
        executable.rename(directory / "bin/gh.absent")
    for ref, oid in (("refs/heads/main", base), ("refs/pull/17/head", head)):
        subprocess.run(["/usr/bin/git", "--git-dir", str(directory / "repository.git"),
                        "update-ref", ref, oid], check=True, capture_output=True)
    subprocess.run(["/usr/bin/git", "--git-dir", str(repository),
                    "config", "uploadpack.allowFilter", "true"], check=True, capture_output=True)
    return {"directory": str(directory), "gh": str(executable),
            "pull_request": "https://github.com/octo-org/review-repository/pull/17"}


def record_operation(directory, operation):
    record = json.dumps({"operation": operation}) + "\n"
    try:
        with (directory / "calls.jsonl").open("a") as output:
            output.write(record)
    except PermissionError:
        # Successful version/API requests require empty stderr. A denied
        # optional trace must not change the production response contract.
        pass


def clone_fixture(directory, fixture, args):
    """Materialize only the closed production clone command, using local Git."""
    large = is_large(fixture["course"])
    sparse = len(args) == 20
    if len(args) not in (18, 20) or (large and not sparse):
        return 64
    destination = Path(args[3])
    root = destination.parent
    template = root / "empty"
    oid = args[8].removeprefix("--revision=")
    expected = ["repo", "clone", "https://github.com/octo-org/review-repository.git",
                str(destination), "--no-upstream", "--", "--bare", "--quiet",
                "--revision=" + oid, "--template=" + str(template),
                "--config", "transfer.unpackLimit=0", "--config", "fetch.unpackLimit=0",
                "--config", "core.hooksPath=/dev/null", "--config", "core.fsmonitor=false"]
    if sparse:
        expected.extend(["--depth=1", "--filter=blob:none"])
    if args != expected or oid not in (fixture["base"], fixture["head"]):
        return 64
    if (not destination.is_absolute() or root.resolve() != root
            or root.parent != (Path("/private/tmp") if sys.platform == "darwin" else Path("/tmp"))
            or not re.fullmatch(r"bpr-cli-git-[A-Za-z0-9-]+", root.name)
            or destination.name not in ("clone-0.git", "clone-1.git")
            or destination.exists() or destination.is_symlink()
            or not root.is_dir() or root.stat().st_uid != os.getuid()
            or root.stat().st_mode & 0o077
            or template.is_symlink() or not template.is_dir()
            or any(template.iterdir())):
        return 64
    record_operation(directory, "clone-base" if oid == fixture["base"] else "clone-head")
    if fixture["course"] not in ("positive", "small-large-repository") and not large:
        return 1
    # An explicitly local fixture capability: no ambient credential/config
    # environment reaches Git, and only the file protocol is enabled.
    environment = {"PATH": "/usr/bin:/bin", "HOME": str(directory / "home"),
                   "GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": "/dev/null",
                   "GIT_ATTR_NOSYSTEM": "1", "GIT_TERMINAL_PROMPT": "0",
                   "GIT_ALLOW_PROTOCOL": "file"}
    fetch_source = ((directory / "repository.git").as_uri() if sparse else
                    str(directory / "repository.git"))
    fetch_options = ["--depth=1", "--filter=blob:none"] if sparse else []
    commands = [
        ["init", "--bare", "--quiet", "--template=" + str(template), str(destination)],
        ["--git-dir", str(destination), "-c", "fetch.unpackLimit=0",
         "-c", "core.hooksPath=/dev/null", "fetch", "--quiet", "--no-tags",
         "--no-write-fetch-head", *fetch_options, fetch_source, oid],
        ["--git-dir", str(destination), "update-ref", "--no-deref", "HEAD", oid],
    ]
    try:
        for command in commands:
            subprocess.run(["/usr/bin/git", *command], env=environment,
                           stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, timeout=10, check=True)
    except (subprocess.SubprocessError, OSError):
        return 1
    return 0


def fake_gh():
    directory = Path(__file__).resolve().parents[1]
    fixture = json.loads((directory / "fixture.json").read_text())
    args = sys.argv[1:]
    if args[:2] == ["repo", "clone"]:
        return clone_fixture(directory, fixture, args)
    if args in (["api", "--help"], ["repo", "clone", "--help"], ["auth", "git-credential", "--help"]):
        # The local capability probes the Git-object path runs before cloning.
        print({
            "api": "FLAGS\n  -H, --header key:value\n      --hostname string\n"
                   "  -i, --include\n      --input file\n  -X, --method string",
            "repo": "USAGE\n  gh repo clone <repository> [<directory>] [-- <gitflags>...]\n\n"
                    "FLAGS\n      --no-upstream   Do not add an upstream remote",
            "auth": "USAGE\n  gh auth git-credential [flags]",
        }[args[0]])
        return 0
    if args == ["--version"]:
        record_operation(directory, "version")
        print("gh version 2.100.0 (synthetic local fixture)")
        return 0
    if args == ["auth", "status", "--active", "--json", "hosts"]:
        record_operation(directory, "active-account-discovery")
        print(json.dumps({"hosts": {"github.com": [{
            "active": True, "host": "github.com", "login": "octo-reviewer",
            "state": "success"}]}}))
        return 0
    blob_match = re.fullmatch(r"repos/octo-org/review-repository/git/blobs/([0-9a-f]{40})", args[1]) if len(args) > 1 else None
    if blob_match:
        expected = ["api", args[1], "--hostname", "github.com", "--method", "GET",
                    "--header", "Accept: application/vnd.github+json",
                    "--header", "X-GitHub-Api-Version: 2022-11-28"]
        if args != expected or not (is_large(fixture["course"])
                                    or fixture["course"] in ("positive", "small-large-repository")):
            return 64
        record_operation(directory, "exact-blob")
        blob = subprocess.run(["/usr/bin/git", "--git-dir", str(directory / "repository.git"),
                               "cat-file", "blob", blob_match[1]], capture_output=True,
                              timeout=5, check=True).stdout
        print(json.dumps({"sha": blob_match[1], "size": len(blob), "encoding": "base64",
                          "content": base64.b64encode(blob).decode()}))
        return 0
    if len(args) < 7 or args[0] != "api" or args[2:7] not in (
            ["--hostname", "github.com", "--include", "--method", "GET"],
            ["--hostname", "github.com", "--include", "--method", "POST"]):
        return 64
    request_url = urlsplit("/" + args[1].lstrip("/"))
    endpoint = request_url.path
    document = ""
    variables = {}
    if args[6] == "POST":
        if endpoint != "/graphql" or args[-2:] != ["--input", "-"]:
            return 64
        raw = sys.stdin.buffer.read(262145)
        if len(raw) > 262144:
            return 65
        request = json.loads(raw)
        document = request["query"]
        variables = request.get("variables", {})
        if not document.lstrip().startswith("query "):
            return 64
    part = re.search(r"BetterPullReviewCapabilityProbeV1Part([0-8])", document)
    operation = "capability-" + part[1] if part else endpoint
    # Only sanitized structural route names are retained, never stdin/headers.
    denied = endpoint == "/repos/octo-org/inaccessible-repository"
    if endpoint not in fixture["routes"] and not denied:
        return 64
    record_operation(directory, operation)
    if fixture["course"] == "process-failure":
        return 1
    if fixture["course"] == "malformed-response":
        print("invalid HTTP framing")
        return 0
    body = ({"message": "Resource not accessible"} if denied else
            fixture["parts"][int(part[1])] if part else fixture["routes"][endpoint])
    continuation = ""
    if is_large(fixture["course"]) and endpoint.endswith("/files"):
        page = int(parse_qs(request_url.query).get("page", ["1"])[0])
        start = (page - 1) * 100
        if start + 100 < len(body):
            continuation = (f"Link: <https://api.github.com{endpoint}?page={page + 1}&per_page=100>; rel=\"next\"\r\n")
        body = body[start:start + 100]
    if is_large(fixture["course"]) and endpoint == "/graphql" and not part:
        body = copy.deepcopy(body)
        connection = body["data"]["repository"]["pullRequest"]["files"]
        cursor = re.fullmatch(r"fixture-files-([0-9]+)", variables.get("after", ""))
        start = int(cursor[1]) if cursor else 0
        nodes = connection["nodes"][start:start + 100]
        connection.update(nodes=nodes, pageInfo={
            "hasNextPage": start + 100 < connection["totalCount"],
            "endCursor": f"fixture-files-{start + len(nodes)}"})
        if "reviewThreads(" not in document:
            body["data"]["repository"]["pullRequest"].pop("reviewThreads", None)
    resource = ("graphql" if endpoint == "/graphql" else
                "search" if endpoint == "/search/issues" else "core")
    status = "403 Forbidden" if denied else "200 OK"
    wire = (f"HTTP/2.0 {status}\r\nContent-Type: application/json\r\n"
            "X-RateLimit-Limit: 5000\r\nX-RateLimit-Remaining: 4999\r\n"
            "X-RateLimit-Reset: 2000000000\r\n"
            f"X-RateLimit-Resource: {resource}\r\n{continuation}\r\n" + json.dumps(body))
    sys.stdout.buffer.write(wire.encode())
    return 0


def launch(args):
    course = json.loads((args.directory / "fixture.json").read_text())["course"]
    if refusal := missing_process_refusal(course):
        raise ValueError(refusal)
    app = args.app.resolve(strict=True)
    cache = args.cmake_cache.resolve(strict=True).read_text()
    if "BUILD_TESTING:BOOL=OFF" not in cache:
        raise ValueError("tests-disabled CMake cache required")
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(("GH_", "GITHUB_"))}
    env.update(HOME=str(args.directory / "home"),
               CFFIXED_USER_HOME=str(args.directory / "home"),
               PATH=str(args.directory / "bin") + ":/usr/bin:/bin:/usr/sbin:/sbin")
    # No --self-test or in-product fixture/auto-consent argument is passed.
    with (args.directory / "application.log").open("wb") as log:
        process = subprocess.Popen([str(app), "--automated-run-budget-ms",
                                    str(args.seconds * 1000)], env=env,
                                   cwd=args.directory, stdout=log, stderr=log)
        print(json.dumps({"pid": process.pid, "external_ui_required": True}), flush=True)
        try:
            code = process.wait(timeout=args.seconds + 10)
        except subprocess.TimeoutExpired:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            raise RuntimeError("app exceeded shutdown bound")
    print(json.dumps({"exit_code": code, "render_verdict": "not inferred from exit"}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prepare", "launch"))
    parser.add_argument("--directory", type=lambda value: Path(value).resolve(), required=True)
    parser.add_argument("--course", choices=("positive", "small-large-repository", "large-positive", "modified-missing-left", "missing-process", "process-failure", "malformed-response"),
                        default="positive")
    parser.add_argument("--app", type=Path)
    parser.add_argument("--cmake-cache", type=Path)
    parser.add_argument("--seconds", type=int, default=90)
    args = parser.parse_args()
    if args.action == "prepare":
        print(json.dumps(prepare(args.directory, args.course)))
    elif args.app and args.cmake_cache and 1 <= args.seconds <= 300:
        launch(args)
    else:
        parser.error("launch requires --app, --cmake-cache and 1..300 seconds")


if __name__ == "__main__":
    if Path(__file__).name == "gh":
        sys.exit(fake_gh())
    main()
