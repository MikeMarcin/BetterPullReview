#!/usr/bin/env python3
"""RFC-BPR-0083 candidate L's macOS evidence course ([[BPR-1458]]).

Builds each corpus repository with the installed Git, asks Git (an isolated
oracle) and libgit2 (`libgit2_probe`) the same C1-C6 questions, and prints
every divergence. Containment cases add canaries outside the root; with
`--shim` every path the probe touches and every process it starts is logged
through an interposer and checked against the root and its Git directories.
After every case the checkout must be byte-identical and free of lock files.

The probe runs with HOME and XDG_CONFIG_HOME pointing at a canary home whose
global config, excludes and attributes would change the answers, so an
agreement also shows that L reads no global or system file.

Run by hand; not a ctest row:
  python3 libgit2_course.py --probe build/dev/tests/evaluation/libgit2/libgit2_probe \
      --work /absolute/scratch/dir [--git /usr/bin/git] [--shim interposer.dylib]
"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

GIT = "/usr/bin/git"
IDENTITY = ["-c", "user.name=Course", "-c", "user.email=course@example.invalid",
            "-c", "init.defaultBranch=main", "-c", "commit.gpgSign=false"]


class Course:
    def __init__(self, arguments: argparse.Namespace) -> None:
        self.probe = Path(arguments.probe).resolve()
        self.work = Path(arguments.work).resolve()
        self.shim = Path(arguments.shim).resolve() if arguments.shim else None
        self.failures: list[str] = []
        self.divergences: list[str] = []
        self.containment: list[str] = []
        self.benign: dict[str, int] = {}
        if self.work.exists():
            shutil.rmtree(self.work)
        self.work.mkdir(parents=True)
        self.oracle_home = self.work / "oracle-home"
        self.oracle_home.mkdir()
        (self.oracle_home / "empty-config").write_bytes(b"")
        self.canary_home = self.work / "canary-home"
        self.outside = self.work / "outside"
        self.outside.mkdir()
        self.make_canary_home()
        self.git_env = {
            "PATH": "/usr/bin:/bin", "GIT_CONFIG_NOSYSTEM": "1",
            "GIT_CONFIG_GLOBAL": str(self.oracle_home / "empty-config"),
            "HOME": str(self.oracle_home), "XDG_CONFIG_HOME": str(self.oracle_home),
            "GIT_OPTIONAL_LOCKS": "0", "LC_ALL": "C",
        }
        version = self.git(self.work, "--version").decode().strip()
        print(f"oracle: {version}; probe: {self.probe.name}")

    def make_canary_home(self) -> None:
        home = self.canary_home
        (home / "git").mkdir(parents=True)
        (home / ".gitconfig").write_text(
            "[core]\n\tignorecase = true\n\texcludesfile = " + str(home / "global-exclude") +
            "\n\tattributesfile = " + str(home / "global-attributes") + "\n")
        (home / "global-exclude").write_text("*.canary-global\n")
        (home / "global-attributes").write_text("*.canary-global filter=global\n")
        (home / "git" / "ignore").write_text("*.canary-xdg\n")
        (home / "git" / "attributes").write_text("*.canary-xdg filter=xdg\n")
        (home / "git" / "config").write_text("[core]\n\tignorecase = true\n")

    # -- running ----------------------------------------------------------
    def git(self, cwd: Path, *arguments: str, stdin: bytes | None = None, check: bool = True) -> bytes:
        result = subprocess.run([GIT, *IDENTITY, *arguments], cwd=cwd, env=self.git_env, input=stdin,
                                capture_output=True, timeout=60)
        if check and result.returncode != 0:
            raise RuntimeError(f"git {' '.join(arguments)} in {cwd}: {result.stderr.decode(errors='replace')}")
        return result.stdout

    def run_probe(self, case: str, root: Path, question: str, *rest: str, stdin: bytes = b"") -> list[bytes] | None:
        env = {"PATH": "/usr/bin:/bin", "HOME": str(self.canary_home), "XDG_CONFIG_HOME": str(self.canary_home),
               "LC_ALL": "C"}
        log = self.work / f"open-{case}.log"
        if self.shim:
            env["DYLD_INSERT_LIBRARIES"] = str(self.shim)
            env["GMR_OPEN_LOG"] = str(log)
        started = time.monotonic()
        try:
            result = subprocess.run([str(self.probe), question, str(root), *rest], input=stdin, env=env,
                                    capture_output=True, timeout=10)
        except subprocess.TimeoutExpired:
            self.containment.append(f"{case}: probe {question} did not finish within 10 s (blocked)")
            return None
        elapsed = time.monotonic() - started
        if elapsed > 1.0:
            self.divergences.append(f"{case}: H6 probe {question} took {elapsed:.2f} s")
        if result.returncode != 0:
            raise RuntimeError(f"probe {question} {root}: {result.stderr.decode(errors='replace')}")
        return result.stdout.split(b"\0")[:-1]

    # -- oracle answers ---------------------------------------------------
    def oracle_index(self, root: Path) -> tuple[str, object]:
        result = subprocess.run([GIT, "ls-files", "--stage", "--sparse", "--debug", "-z"], cwd=root,
                                env=self.git_env, capture_output=True)
        if result.returncode != 0:
            return "refused", result.stderr.decode(errors="replace").strip()
        entries = []
        # With -z and --debug each record is "mode oid stage\tpath\0" then
        # indented debug lines ending in "\tflags: <hex>\n".
        data = result.stdout
        position = 0
        while position < len(data):
            end = data.index(b"\0", position)
            head, path = data[position:end].split(b"\t", 1)
            mode, oid, stage = head.split(b" ")
            flags_at = data.index(b"flags: ", end)
            flags_end = data.index(b"\n", flags_at)
            flags = int(data[flags_at + 7:flags_end], 16)
            position = flags_end + 1
            entries.append((path, int(mode, 8), oid.decode(), int(stage),
                            bool(flags & (1 << 30)), bool(flags & (1 << 29))))
        return "ok", entries

    def oracle_format(self, root: Path) -> tuple[str, object]:
        result = subprocess.run([GIT, "rev-parse", "--show-object-format", "--show-ref-format"], cwd=root,
                                env=self.git_env, capture_output=True)
        if result.returncode != 0:
            return "refused", result.stderr.decode(errors="replace").strip()
        object_format, ref_format = result.stdout.decode().split()
        case = subprocess.run([GIT, "config", "--type=bool", "core.ignorecase"], cwd=root, env=self.git_env,
                              capture_output=True)
        return "ok", (object_format, ref_format, case.stdout.strip() == b"true")

    def oracle_head(self, root: Path) -> tuple[str, object]:
        symbolic = subprocess.run([GIT, "symbolic-ref", "--no-recurse", "-q", "HEAD"], cwd=root,
                                  env=self.git_env, capture_output=True)
        commit = subprocess.run([GIT, "rev-parse", "--verify", "-q", "HEAD"], cwd=root, env=self.git_env,
                                capture_output=True)
        if symbolic.returncode not in (0, 1) or commit.returncode not in (0, 1):
            return "refused", (symbolic.stderr + commit.stderr).decode(errors="replace").strip()
        return "ok", (symbolic.stdout.strip().decode() if symbolic.returncode == 0 else None,
                      commit.stdout.strip().decode() if commit.returncode == 0 else None)

    def oracle_reference(self, root: Path, name: str) -> tuple[str, object]:
        result = subprocess.run([GIT, "rev-parse", "--verify", "-q", name], cwd=root, env=self.git_env,
                                capture_output=True)
        if result.returncode not in (0, 1):
            return "refused", result.stderr.decode(errors="replace").strip()
        return "ok", result.stdout.strip().decode() if result.returncode == 0 else None

    def oracle_discover(self, root: Path) -> tuple[str, object]:
        result = subprocess.run([GIT, "rev-parse", "--path-format=absolute", "--git-dir", "--git-common-dir"],
                                cwd=root, env=self.git_env, capture_output=True)
        if result.returncode != 0:
            return "refused", result.stderr.decode(errors="replace").strip()
        git_dir, common = result.stdout.decode().splitlines()
        return "ok", (os.path.realpath(git_dir), os.path.realpath(common), os.path.realpath(git_dir) != os.path.realpath(common))

    def oracle_ignored(self, root: Path, paths: list[tuple[bytes, bool]]) -> tuple[str, object]:
        questions = b"".join((path + (b"/" if directory else b"")) + b"\0" for path, directory in paths)
        result = subprocess.run([GIT, "check-ignore", "--no-index", "--stdin", "-z"], cwd=root, env=self.git_env,
                                input=questions, capture_output=True)
        if result.returncode not in (0, 1):
            return "refused", result.stderr.decode(errors="replace").strip()
        ignored = set(result.stdout.split(b"\0"))
        return "ok", [(path + (b"/" if directory else b"")) in ignored for path, directory in paths]

    def oracle_filter(self, root: Path, paths: list[tuple[bytes, bool]]) -> tuple[str, object]:
        questions = b"".join(path + b"\0" for path, _ in paths)
        result = subprocess.run([GIT, "check-attr", "filter", "--stdin", "-z"], cwd=root, env=self.git_env,
                                input=questions, capture_output=True)
        if result.returncode != 0:
            return "refused", result.stderr.decode(errors="replace").strip()
        fields = result.stdout.split(b"\0")[:-1]
        return "ok", [fields[index + 2].decode(errors="replace") for index in range(0, len(fields), 3)]

    # -- candidate answers ------------------------------------------------
    @staticmethod
    def parsed(fields: list[bytes] | None, shape) -> tuple[str, object]:
        if fields is None:
            return "blocked", None
        if fields[0] == b"refused":
            return "refused", fields[1].decode(errors="replace")
        return "ok", shape(fields[1:])

    @staticmethod
    def index_shape(fields: list[bytes]) -> object:
        count = int(fields[1])
        entries = []
        for index in range(count):
            path, mode, oid, stage, skip, ita = fields[2 + index * 6: 8 + index * 6]
            entries.append((path, int(mode), oid.decode(), int(stage), skip == b"1", ita == b"1"))
        return entries

    @staticmethod
    def optional(present: bytes, value: bytes) -> str | None:
        return value.decode() if present == b"1" else None

    def compare(self, case: str, component: str, git: tuple[str, object], libgit2: tuple[str, object]) -> None:
        if git == libgit2:
            print(f"  {component}: agree ({git[0]})")
            return
        detail = f"git={self.describe(git)} libgit2={self.describe(libgit2)}"
        print(f"  {component}: DIVERGE {detail}")
        self.divergences.append(f"{case} {component}: {detail}")

    @staticmethod
    def describe(answer: tuple[str, object]) -> str:
        text = repr(answer)
        return text if len(text) < 600 else text[:600] + "..."

    # -- one case ---------------------------------------------------------
    def snapshot(self, root: Path) -> dict[str, tuple]:
        state = {}
        for directory, directories, files in os.walk(root, followlinks=False):
            for name in directories + files:
                path = Path(directory) / name
                info = path.lstat()
                digest = ""
                if path.is_file() and not path.is_symlink():
                    digest = hashlib.sha256(path.read_bytes()).hexdigest()
                state[str(path)] = (info.st_mode, info.st_size, info.st_mtime_ns, digest)
        return state

    def ask(self, case: str, root: Path, paths: list[tuple[bytes, bool]], references: list[str],
            allowed: list[Path], watched: list[Path] | None = None) -> None:
        print(f"case {case}")
        roots = [root, *allowed]
        log = self.work / f"open-{case}.log"
        log.unlink(missing_ok=True)
        stdin_paths = b"".join((b"d" if directory else b"f") + path + b"\0" for path, directory in paths)

        # Git answers first: the oracle itself may write (it freshens a split
        # index's shared file) or run core.fsmonitor, so only the probe's runs
        # sit between the two snapshots.
        oracle = [("C1 index", self.oracle_index(root)), ("C2 format", self.oracle_format(root)),
                  ("C3 head", self.oracle_head(root))]
        oracle += [(f"C3 {name}", self.oracle_reference(root, name)) for name in references]
        oracle.append(("C4 discover", self.oracle_discover(root)))
        if paths:
            oracle += [("C5 ignored", self.oracle_ignored(root, paths)), ("C6 filter", self.oracle_filter(root, paths))]

        before = {str(path): self.snapshot(path) for path in roots + (watched or [])}
        candidate = [("C1 index", self.parsed(self.run_probe(case, root, "index"), self.index_shape)),
                     ("C2 format", self.parsed(self.run_probe(case, root, "format"),
                                               lambda f: (f[0].decode(), f[1].decode(), f[2] == b"1"))),
                     ("C3 head", self.parsed(self.run_probe(case, root, "head"),
                                             lambda f: (self.optional(f[0], f[1]), self.optional(f[2], f[3]))))]
        candidate += [(f"C3 {name}", self.parsed(self.run_probe(case, root, "reference", name),
                                                 lambda f: self.optional(f[0], f[1]))) for name in references]
        candidate.append(("C4 discover", self.parsed(self.run_probe(case, root, "discover"),
                                                     lambda f: (os.path.realpath(f[0].decode()),
                                                                os.path.realpath(f[1].decode()), f[2] == b"1"))))
        if paths:
            candidate += [("C5 ignored", self.parsed(self.run_probe(case, root, "ignored", stdin=stdin_paths),
                                                     lambda f: [value == b"1" for value in f])),
                          ("C6 filter", self.parsed(self.run_probe(case, root, "filter", stdin=stdin_paths),
                                                    lambda f: [value.decode(errors="replace") for value in f]))]
        for path in roots + (watched or []):
            after = self.snapshot(path)
            if after != before[str(path)]:
                changed = sorted(set(after.items()) ^ set(before[str(path)].items()))
                self.containment.append(f"{case}: files changed under {path} while L ran: {changed[:4]}")
        locks = [p for p in roots for p in p.rglob("*.lock")]
        if locks:
            self.containment.append(f"{case}: lock files left: {locks}")

        for (component, git), (_, libgit2) in zip(oracle, candidate):
            self.compare(case, component, git, libgit2)
        if self.shim and log.exists():
            self.check_log(case, log, roots)

    def check_log(self, case: str, log: Path, roots: list[Path]) -> None:
        allowed = [os.path.realpath(path) for path in roots] + [str(path) for path in roots]
        for line in log.read_text(errors="replace").splitlines():
            call, flags, path = line.split("\t", 2)
            if call in ("execve", "posix_spawn", "posix_spawnp", "fork", "popen", "system"):
                self.containment.append(f"{case}: process created: {line}")
                continue
            if call in ("open", "openat") and int(flags, 16) & (os.O_WRONLY | os.O_RDWR | os.O_CREAT):
                self.containment.append(f"{case}: opened for writing: {line}")
            if call in ("mkdir", "rename", "rename-to", "unlink"):
                self.containment.append(f"{case}: wrote: {line}")
            absolute = path if path.startswith("/") else None
            if absolute is None:
                continue
            normal = os.path.normpath(absolute)
            # Process start-up (dyld and CoreFoundation reading the probe's
            # own directory), libgit2's random seed (src/util/rand.c) and its
            # stat of "/" while walking up for ownership are counted, not
            # listed: none of them is repository content.
            if normal.startswith(str(self.probe.parent)) or normal in ("/dev/urandom", "/"):
                self.benign[f"{call} {normal}"] = self.benign.get(f"{call} {normal}", 0) + 1
                continue
            if not any(normal == root or normal.startswith(root.rstrip("/") + "/") for root in allowed):
                if call in ("open", "openat", "fopen", "opendir"):
                    self.containment.append(f"{case}: OPENED OUTSIDE: {line}")
                else:
                    self.containment.append(f"{case}: touched outside ({call}): {path}")

    # -- corpus -----------------------------------------------------------
    def repository(self, name: str, *init: str) -> Path:
        root = self.work / name
        root.mkdir()
        self.git(root, "init", "-q", *init)
        return root

    def commit_files(self, root: Path, files: dict[str, str], message: str = "c") -> None:
        for name, text in files.items():
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
        self.git(root, "add", "-A")
        self.git(root, "commit", "-q", "-m", message)

    def worktree_paths(self, root: Path, extra: list[tuple[bytes, bool]] = ()) -> list[tuple[bytes, bool]]:
        paths = []
        for directory, directories, files in os.walk(root):
            directories[:] = [d for d in directories if d != ".git"]
            relative = os.path.relpath(directory, root)
            for name in directories:
                paths.append((os.fsencode(os.path.normpath(os.path.join(relative, name))), True))
            for name in files:
                paths.append((os.fsencode(os.path.normpath(os.path.join(relative, name))), False))
        canaries = [(b"x.canary-global", False), (b"x.canary-xdg", False), (b"SECRET.ENV", False)]
        return sorted(set(paths + list(extra) + canaries))

    def git_dirs(self, root: Path) -> list[Path]:
        output = self.git(root, "rev-parse", "--path-format=absolute", "--git-dir", "--git-common-dir", check=False).decode()
        return sorted({Path(os.path.realpath(line)) for line in output.splitlines()} or {root / ".git"})

    def standard(self, name: str, root: Path, references: list[str] | None = None,
                 extra: list[tuple[bytes, bool]] = (), watched: list[Path] | None = None) -> None:
        # Git refuses some corpus repositories outright; ask L anyway.
        names = self.git(root, "for-each-ref", "--format=%(refname)", check=False).decode().split()
        self.ask(name, root, self.worktree_paths(root, list(extra)), (references or []) + names + ["refs/heads/absent"],
                 self.git_dirs(root), watched)

    def run(self) -> int:
        base = {"a.txt": "a\n", "dir/b.txt": "b\n", "dir/sub/c.log": "c\n", "build/out.o": "o\n",
                ".gitignore": "*.log\n/build/\n", ".gitattributes": "*.txt filter=lfs\n"}

        for version in ("2", "3", "4"):
            root = self.repository(f"sha1-index-v{version}")
            self.git(root, "config", "index.version", version)
            self.commit_files(root, base)
            if version == "2":
                blob = self.git(root, "hash-object", "-w", "--stdin", stdin=b"x").decode().strip()
                self.git(root, "update-index", "--add", "--cacheinfo", os.fsdecode(f"100644,{blob},".encode() + b"caf\xe9.txt"))
            if version == "3":
                (root / "new.txt").write_text("n\n")
                self.git(root, "add", "-N", "new.txt")
                self.git(root, "update-index", "--skip-worktree", "a.txt")
            self.git(root, "update-index", f"--index-version={version}")
            self.standard(f"sha1-index-v{version}", root)

        root = self.repository("many-files")
        self.git(root, "config", "feature.manyFiles", "true")
        self.commit_files(root, base)
        self.standard("many-files", root)

        root = self.repository("skip-hash")
        self.git(root, "config", "index.skipHash", "true")
        self.commit_files(root, base)
        self.standard("skip-hash", root)

        root = self.repository("split-index")
        self.commit_files(root, base)
        self.git(root, "update-index", "--split-index")
        (root / "later.txt").write_text("l\n")
        self.git(root, "add", "later.txt")
        self.standard("split-index", root)

        root = self.repository("sparse-index")
        self.commit_files(root, {**base, "keep/k.txt": "k\n", "drop/d.txt": "d\n", "drop/e/f.txt": "f\n"})
        self.git(root, "sparse-checkout", "set", "--cone", "--sparse-index", "keep")
        self.standard("sparse-index", root)

        root = self.repository("sparse-full-index")
        self.commit_files(root, {**base, "keep/k.txt": "k\n", "drop/d.txt": "d\n"})
        self.git(root, "sparse-checkout", "set", "--cone", "--no-sparse-index", "keep")
        self.standard("sparse-full-index", root)

        root = self.repository("unmerged")
        self.commit_files(root, {"m.txt": "base\n"})
        self.git(root, "checkout", "-q", "-b", "other")
        self.commit_files(root, {"m.txt": "other\n"})
        self.git(root, "checkout", "-q", "main")
        self.commit_files(root, {"m.txt": "main\n"})
        self.git(root, "merge", "-q", "other", check=False)
        self.standard("unmerged", root)

        root = self.repository("sha256", "--object-format=sha256")
        self.commit_files(root, base)
        self.standard("sha256", root)

        root = self.repository("objectformat-comment")
        self.commit_files(root, base)
        config = root / ".git" / "config"
        config.write_text(config.read_text().replace("repositoryformatversion = 0", "repositoryformatversion = 1") +
                          "[extensions]\n\tobjectformat = sha1 ; inline note\n")
        self.standard("objectformat-comment", root)

        main = self.repository("worktree-main")
        self.commit_files(main, base)
        linked = self.work / "worktree-linked"
        self.git(main, "worktree", "add", "-q", "-b", "side", str(linked))
        self.standard("worktree-linked", linked)
        relative = self.work / "worktree-relative"
        relative_result = subprocess.run([GIT, *IDENTITY, "worktree", "add", "-q", "--relative-paths", "-b", "rel",
                                          str(relative)], cwd=main, env=self.git_env, capture_output=True)
        if relative_result.returncode == 0:
            self.standard("worktree-relative", relative)
        else:
            print("case worktree-relative: skipped,", relative_result.stderr.decode().strip())

        root = self.repository("reftable", "--ref-format=reftable")
        self.commit_files(root, base)
        self.standard("reftable", root)

        root = self.repository("refstorage-v0")
        self.commit_files(root, base)
        self.git(root, "config", "extensions.refStorage", "reftable")
        self.standard("refstorage-v0", root)

        root = self.repository("head-packed")
        self.commit_files(root, base)
        self.git(root, "tag", "-a", "-m", "t", "v1")
        self.git(root, "pack-refs", "--all")
        self.standard("head-packed", root)

        root = self.repository("head-symbolic")
        self.commit_files(root, base)
        self.git(root, "symbolic-ref", "refs/heads/alias", "refs/heads/main")
        self.git(root, "symbolic-ref", "HEAD", "refs/heads/alias")
        self.standard("head-symbolic", root, ["refs/heads/alias"])

        root = self.repository("head-detached")
        self.commit_files(root, base)
        self.git(root, "checkout", "-q", "--detach")
        self.standard("head-detached", root)

        root = self.repository("head-unborn")
        (root / "a.txt").write_text("a\n")
        self.git(root, "add", "a.txt")
        self.standard("head-unborn", root)

        root = self.repository("ignore-case-true")
        self.git(root, "config", "core.ignoreCase", "true")
        self.commit_files(root, {**base, ".gitignore": "Secret.env\n[A-C]*.bak\nDIR/\n",
                                 ".gitattributes": "*.TXT filter=upper\n"})
        self.standard("ignore-case-true", root, extra=[(b"secret.env", False), (b"b1.bak", False),
                                                        (b"dir/x", False), (b"a.txt", False)])

        root = self.repository("ignore-case-false")
        self.git(root, "config", "core.ignoreCase", "false")
        self.commit_files(root, {**base, ".gitignore": "Secret.env\n"})
        self.standard("ignore-case-false", root, extra=[(b"secret.env", False)])

        root = self.repository("config-include")
        self.commit_files(root, base)
        (root / ".git" / "extra.cfg").write_text("[core]\n\tignorecase = true\n")
        with (root / ".git" / "config").open("a") as stream:
            stream.write("[include]\n\tpath = extra.cfg\n")
        self.standard("config-include", root, extra=[(b"secret.env", False)])

        root = self.repository("config-includeif")
        self.commit_files(root, base)
        (root / ".git" / "cond.cfg").write_text("[core]\n\tignorecase = true\n")
        with (root / ".git" / "config").open("a") as stream:
            stream.write(f'[includeIf "gitdir:{root}/"]\n\tpath = cond.cfg\n')
        self.standard("config-includeif", root)

        root = self.repository("config-quoting")
        self.commit_files(root, base)
        with (root / ".git" / "config").open("a") as stream:
            stream.write('[core]\n\tignoreCase = "tr\\\nue" # comment\n[branch "a\\"b"]\n\tremote = x\n')
        self.standard("config-quoting", root)

        root = self.repository("ignore-rules")
        rules = ("a[[:space:]]b\nx[![:space:]]y\n[[:digit:]]*.num\n[[:bogus:]]z\n\\#hash\n\\!bang\n"
                 "trail   \ntrailesc\\ \n*.tmp\n!keep.tmp\n**/deep/**\nonlydir/\n/anchored\nmid/**/end\n"
                 "crlf.txt\r\n[\xe9]x\n")
        self.commit_files(root, {"a.txt": "a\n", ".gitignore": rules})
        extra = [(name.encode() if isinstance(name, str) else name, directory) for name, directory in [
            ("a b", False), ("x y", False), ("xzy", False), ("1.num", False), ("zz", False), ("#hash", False),
            ("!bang", False), ("trail", False), ("trailesc ", False), ("trailesc", False), ("a.tmp", False),
            ("keep.tmp", False), ("q/deep/r", False), ("deep/s", False), ("onlydir", True), ("onlydir", False),
            ("onlydir/f", False), ("anchored", False), ("sub/anchored", False), ("mid/end", False),
            ("mid/a/b/end", False), ("crlf.txt", False), (b"\xe9x", False), (b"\xc9x", False)]]
        self.standard("ignore-rules", root, extra=extra)

        root = self.repository("attributes")
        self.commit_files(root, {"a.txt": "a\n", "b.bin": "b\n", "c.dat": "c\n", "d.m": "d\n", "sub/e.txt": "e\n",
                                 ".gitattributes": "[attr]mymacro filter=macro -diff\n*.txt filter=lfs\n*.bin -filter\n"
                                                   "*.dat filter\n*.m mymacro\nsub/*.txt !filter\n",
                                 "sub/.gitattributes": "e.txt filter=inner\n"})
        (root / ".git" / "info").mkdir(exist_ok=True)
        (root / ".git" / "info" / "attributes").write_text("c.dat filter=info\n")
        self.standard("attributes", root, extra=[(b"z.unknown", False)])

        root = self.repository("adversarial")
        line = "*a" * 3000 + "\n"
        self.commit_files(root, {"a.txt": "a\n", ".gitignore": line})
        self.standard("adversarial", root, extra=[(b"a" * 6000 + b"b", False)])

        self.containment_cases()

        print("\nDIVERGENCES")
        for line in self.divergences:
            print(" ", line)
        print("\nCONTAINMENT, EXECUTION AND WRITES")
        for line in self.containment:
            print(" ", line)
        print("\nnone" if not self.containment else "")
        print("BENIGN PROCESS-LEVEL CALLS (all cases)")
        for name, count in sorted(self.benign.items()):
            print(f"  {count:5d} {name}")
        return 0

    def containment_cases(self) -> None:
        canary_log = self.outside / "executed.log"
        script = self.outside / "canary.sh"
        # The canary records which program ran it and never waits on input:
        # the Git oracle itself runs core.fsmonitor, and that must neither hang
        # the course nor be mistaken for L running it.
        script.write_text(f"#!/bin/sh\necho \"$(ps -o comm= -p $PPID) ran $0 $*\" >> {canary_log}\nexit 0\n")
        script.chmod(0o755)
        secret = self.outside / "outside-ignore"
        secret.write_text("*.secret\n")
        attributes = self.outside / "outside-attributes"
        attributes.write_text("*.secret filter=leaked\n")
        include = self.outside / "outside-include.cfg"
        include.write_text("[core]\n\tignorecase = true\n")
        excludes = self.outside / "outside-excludes"
        excludes.write_text("*.excluded-outside\n")
        attributes_file = self.outside / "outside-attributes-file"
        attributes_file.write_text("*.excluded-outside filter=outside\n")

        root = self.repository("canary-symlinks")
        self.commit_files(root, {"a.txt": "a\n", "dir/b.txt": "b\n"})
        (root / ".gitignore").symlink_to(secret)
        (root / ".gitattributes").symlink_to(attributes)
        (root / "dir" / ".gitignore").symlink_to(secret)
        (root / ".git" / "info").mkdir(exist_ok=True)
        (root / ".git" / "info" / "exclude").unlink(missing_ok=True)
        (root / ".git" / "info" / "exclude").symlink_to(excludes)
        self.standard("canary-symlinks", root, extra=[(b"a.secret", False), (b"dir/c.secret", False),
                                                      (b"z.excluded-outside", False)], watched=[self.outside])

        root = self.repository("canary-include-file")
        self.commit_files(root, {"a.txt": "a\n"})
        with (root / ".git" / "config").open("a") as stream:
            stream.write(f"[include]\n\tpath = {include}\n[core]\n\texcludesFile = {excludes}\n"
                         f"\tattributesFile = {attributes_file}\n")
        self.standard("canary-include-file", root, extra=[(b"SECRET.ENV", False), (b"z.excluded-outside", False)],
                      watched=[self.outside])

        root = self.repository("canary-include-relative")
        self.commit_files(root, {"a.txt": "a\n"})
        with (root / ".git" / "config").open("a") as stream:
            stream.write(f"[include]\n\tpath = {os.path.relpath(include, root / '.git')}\n")
        self.standard("canary-include-relative", root, watched=[self.outside])

        root = self.repository("canary-include-fifo")
        self.commit_files(root, {"a.txt": "a\n"})
        fifo = self.outside / "include.fifo"
        os.mkfifo(fifo)
        with (root / ".git" / "config").open("a") as stream:
            stream.write(f"[include]\n\tpath = {fifo}\n")
        # Git blocks on the FIFO too, so the oracle is not asked here.
        print("case canary-include-fifo")
        answer = self.run_probe("canary-include-fifo", root, "format")
        print("  C2 format:", "blocked" if answer is None else answer)

        root = self.repository("canary-execution")
        self.commit_files(root, {"a.txt": "a\n", ".gitattributes": "*.txt filter=canary\n"})
        hooks = root / ".git" / "hooks"
        hooks.mkdir(exist_ok=True)
        for hook in ("post-index-change", "pre-commit", "post-checkout", "reference-transaction"):
            shutil.copy(script, hooks / hook)
        with (root / ".git" / "config").open("a") as stream:
            stream.write(f"[core]\n\tfsmonitor = {script}\n\thooksPath = {hooks}\n"
                         f"[filter \"canary\"]\n\tclean = {script} clean\n\tsmudge = {script} smudge\n"
                         f"\tprocess = {script} process\n\trequired = true\n"
                         f"[diff \"canary\"]\n\ttextconv = {script} textconv\n")
        (root / "a.txt").write_text("changed so a refresh would have work\n")
        self.standard("canary-execution", root, watched=[self.outside])
        if canary_log.exists():
            for line in canary_log.read_text().splitlines():
                if "libgit2_probe" in line:
                    self.containment.append(f"canary-execution: L ran the canary: {line}")
                else:
                    print(f"  oracle (not L) ran the canary: {line}")


def main() -> int:
    global GIT
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--probe", required=True)
    parser.add_argument("--work", required=True)
    parser.add_argument("--git", default=GIT)
    parser.add_argument("--shim")
    arguments = parser.parse_args()
    GIT = arguments.git
    sys.stdout.reconfigure(line_buffering=True)
    return Course(arguments).run()


if __name__ == "__main__":
    sys.exit(main())
