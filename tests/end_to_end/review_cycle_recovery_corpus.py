"""[[BPR-1183]]. Review Cycles' hostile-input, data-loss and mixed-cycle recovery corpus.

Every case runs the shipped `bpr cycle` and the reviewer's own desktop
(`review_cycle_two_round_public_path_tests.cpp`) as real processes over one
profile, and is followed by what it must not have changed: the store reads
whole (SQLite's own check, and every revision, round and response names
objects the store holds), the cycle is exactly as before or exactly as after
the interrupted command, a round frozen earlier exports byte for byte as it was,
a second cycle in the same profile is untouched, and no project folder changed.

1. Kill points (RC53-04's store share, RC60-06). Capture, a note, a goal, a
   freeze, a response import and a save to a file are each killed with SIGKILL
   (TerminateProcess on Windows) at their first store write and at a quarter,
   half, three quarters and nineteen twentieths of their measured run. The
   same command repeated with the same operation ID leaves it applied exactly
   once: it completes, or, when the killed run had already written, is refused
   in words that say an earlier run may have completed. A save leaves the
   round's exact bytes or nothing at the chosen name. The store check is shown
   failing on a copy that lost one pinned object.
2. The owner killed. With the desktop owning the profile, a forwarded capture
   loses its owner mid-write. The terminal says it is uncertain or finishes
   elsewhere; repeating it leaves it applied once, and a new desktop shows the
   same cycle.
3. A full disk (POSIX: a file-size limit on the writer). A capture, a frozen
   round, a response and a saved file that need more space than is left are
   refused with nothing recorded and nothing half-written, then succeed once
   when space returns.
4. Damaged and missing retained material (RC54-09's share). A corrupted or
   missing pin reads as unavailable, never as the live file or as "no file";
   a damaged round exports, grants and bundles nothing; recapturing the same
   bytes repairs a pin; the other cycle reads on.
5. A damaged or lost store. A store SQLite cannot read is said to be damaged,
   not another version's, and is left as it is; nothing is recreated over it.
   A lost store is not reconstructed: the profile lists nothing, a new cycle at
   the same folder is new, and a response to the lost round is refused there.
   Restoring the reviewer's own copy brings everything back.
6. Stale drafts, a writer that never stops, replaced and moved roots
   (RC54-03). A goal or freeze against a draft revision another writer moved
   is refused. A folder that keeps changing is retried a bounded number of
   times and refused, nothing recorded. A root replaced at its location, or
   swapped for a link to another folder, is refused, in the terminal and the
   window, with words that say retrying will not help; the folder now there
   opens as a new cycle; a moved root is recognised by identity but not read
   from its new location; moved back, it captures again.
7. Duplicates (RC60-05). The same response twice is one response; another
   response under its identifier, duplicate JSON keys and a repeated
   operation ID are refused with nothing changed.
8. Case collisions. Where the course can make a case-sensitive folder (Linux
   natively, macOS through a disk image), two files differing only in case
   are captured, noted, frozen and exported as two paths, and the bundle holds
   no member a case-insensitive reader would merge.
9. Mixed builds (optional). `--older-cycle-application` names a `bpr` built
   before the store's current schema and `--agent-free-cycle-application` one
   built without agent integrations. The older build writes a profile; the
   current desktop opens it first; each build reads the same round byte for
   byte; the older build refuses the upgraded store and leaves it as it is.

Windows runs every part but the full disk and, on its case-insensitive volumes,
the case collisions, and says which parts it skipped.
"""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path
import secrets
import shutil
import sqlite3
import stat
import subprocess
import sys
import threading
import time
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from bpr_narrow_owner_private import copy_private_root, make_root_private  # noqa: E402
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402
from profile_activation_contract import private_fixture_parent  # noqa: E402
from review_cycle_two_round_contract import Desktop  # noqa: E402

if os.name != "nt":
    import resource
    import signal

POSIX = os.name != "nt"
# The fractions of a command's measured run at which it is also killed.
KILL_FRACTIONS = (0.25, 0.5, 0.75, 0.95)
# What a full disk leaves a writer: more than the profile's own registry needs,
# less than any write below.
FULL_DISK_BYTES = 64 * 1024


def skipped(what, why):
    print(f"RECOVERY_CORPUS_SKIPPED {what}: {why}")


def tree_digest(root):
    """Every name, mode, link and byte under `root`."""
    digest = hashlib.sha256()
    for path in sorted(root.rglob("*")):
        status = path.lstat()
        digest.update(path.relative_to(root).as_posix().encode("utf-8", "surrogateescape") + b"\0" + str(stat.S_IFMT(status.st_mode)).encode() + b"\0")
        if path.is_symlink():
            digest.update(os.readlink(path).encode("utf-8", "surrogateescape"))
        elif path.is_file():
            digest.update(path.read_bytes())
    return digest.hexdigest()


def store_path(data):
    return data / "state" / "v1" / "review-cycles.sqlite3"


def store_files(data):
    """The store and the journal files beside it."""
    folder = store_path(data).parent
    return sorted(path.name for path in folder.iterdir() if path.name.startswith("review-cycles")) if folder.exists() else []


def read_only(path):
    """A read-only connection to a store file, closed when the block ends."""
    return contextlib.closing(sqlite3.connect(path.resolve().as_uri() + "?mode=ro", uri=True))


def user_version(data):
    with read_only(store_path(data)) as database:
        return database.execute("PRAGMA user_version").fetchone()[0]


def check_store(data, where):
    """SQLite's own check, and no record naming an object the store lacks:
    no half-admitted revision, round or response."""
    with read_only(store_path(data)) as database:
        assert database.execute("PRAGMA integrity_check").fetchall() == [("ok",)], f"{where}: the store passes its integrity check"
        dangling = {
            "revision material": "SELECT count(*) FROM revisions r WHERE NOT EXISTS (SELECT 1 FROM objects o WHERE o.digest = r.material)",
            "pinned content": "SELECT count(*) FROM revision_objects p WHERE NOT EXISTS (SELECT 1 FROM objects o WHERE o.digest = p.digest)",
            "frozen rounds": "SELECT count(*) FROM rounds r WHERE NOT EXISTS (SELECT 1 FROM objects o WHERE o.digest = r.digest)",
            "responses": "SELECT count(*) FROM responses r WHERE NOT EXISTS (SELECT 1 FROM objects o WHERE o.digest = r.content)",
            "object chunks": "SELECT count(*) FROM objects o WHERE o.chunks != (SELECT count(*) FROM object_chunks c WHERE c.digest = o.digest)",
        }
        for what, query in dangling.items():
            assert database.execute(query).fetchone() == (0,), f"{where}: no {what} name an object the store lacks"


def kill(process):
    """SIGKILL on POSIX, TerminateProcess on Windows: no handler runs."""
    process.kill()
    process.wait(timeout=60)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True, help="the reviewer's desktop (review_cycle_two_round_public_path)")
    parser.add_argument("--cycle-application", required=True, help="the shipped bpr")
    parser.add_argument("--older-cycle-application", help="a bpr built before the store's current schema")
    parser.add_argument("--agent-free-cycle-application", help="a bpr built without agent integrations (the tour-cli preset)")
    args = parser.parse_args()
    for name in ("application", "cycle_application", "older_cycle_application", "agent_free_cycle_application"):
        if getattr(args, name):
            setattr(args, name, str(Path(getattr(args, name)).resolve()))
    with private_fixture_parent() as parent, contained_temporary_directory(prefix="cycle-recovery-", parent=parent) as temporary:
        workspace = Path(temporary).resolve()
        make_root_private(workspace)
        data = workspace / "data"
        make_root_private(data)
        exports = workspace / "exports"
        exports.mkdir()
        environment = dict(os.environ)
        if environment.get("QT_QPA_PLATFORM") == "offscreen":
            screen = workspace / "offscreen-screen.json"
            screen.write_text(json.dumps({"screens": [{"name": "Course", "x": 0, "y": 0, "width": 1920, "height": 1200, "logicalDpi": 96, "logicalBaseDpi": 96, "dpr": 1}]}), encoding="utf-8")
            # Qt splits the platform argument at every ':', so a Windows drive
            # letter would end the path at "C". The desktop inherits the course's
            # working directory, so a relative path names the same file.
            spec = os.path.relpath(screen) if ":" in str(screen) else str(screen)
            environment["QT_QPA_PLATFORM"] = f"offscreen:configfile={spec}"

        def command(*arguments, operation=None, executable=None, root=None, machine=True):
            return [executable or args.cycle_application, "cycle", *arguments, *(["--json"] if machine else []), "--profile", "1", "--data-root", str(root or data),
                    *(["--operation-id", operation] if operation else [])]

        def bpr(*arguments, stdin=b"", operation=None, executable=None, root=None, machine=True, limit=None, extra=None):
            child = None
            if limit is not None:
                def child():
                    # A write past the limit fails with EFBIG instead of ending the process.
                    signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
                    resource.setrlimit(resource.RLIMIT_FSIZE, (limit, limit))
            return subprocess.run(command(*arguments, operation=operation, executable=executable, root=root, machine=machine), input=stdin, capture_output=True, timeout=300,
                                  cwd=workspace, env=dict(environment, **(extra or {})), preexec_fn=child)

        def envelope(result, arguments):
            assert result.stderr == b"", f"{arguments}: JSON calls keep standard error empty: {result.stderr[-400:]!r}"
            return json.loads(result.stdout)

        def ok(*arguments, **options):
            result = bpr(*arguments, **options)
            value = envelope(result, arguments)
            assert result.returncode == 0 and value["outcome"] == "ok", f"{arguments}: {result.returncode} {value}"
            return value["data"]

        def refused(code, *arguments, **options):
            result = bpr(*arguments, **options)
            value = envelope(result, arguments)
            assert result.returncode != 0 and value["outcome"] == "error" and value["error"]["code"] == code, f"{arguments}: expected {code}, got {result.returncode} {value}"
            return value["error"]

        def exported(cycle, round_name, executable=None, root=None):
            result = bpr("export", cycle, "--round", round_name, "--output", "-", machine=False, executable=executable, root=root)
            assert result.returncode == 0, f"export {cycle} {round_name}: {result.returncode} {result.stderr[-300:]!r}"
            return result.stdout

        def shown(cycle, **options):
            return ok("show", cycle, **options)

        def write_tree(root, files):
            for path, content in files.items():
                (root / path).parent.mkdir(parents=True, exist_ok=True)
                (root / path).write_bytes(content)

        def response_document(cycle, round_name, digest, identifier, item, body="Done.", disposition="addressed"):
            return json.dumps({
                "schema": "bpr.cycle.response.v1", "response_id": identifier, "cycle_id": cycle, "round_id": round_name, "round_payload_sha256": digest,
                "claimed_author": {"display_name": "Recipient", "kind": "human", "provider": None, "model": None, "effort": None},
                "result_source_revision_id": None,
                "items": [{"item_id": item, "item_revision": "1", "disposition": disposition, "body": body, "evidence": []}],
            }).encode()

        def round_digest(cycle, round_name):
            return next(entry["digest"] for entry in shown(cycle)["rounds"] if entry["round"] == round_name)

        # ---------------------------------------------------------------- the profile
        # alpha is attacked; beta, in the same profile, must never change.
        alpha = workspace / "alpha-project"
        files = {f"src/module{index:03}.cpp": secrets.token_hex(1024).encode() + b"\n" for index in range(400)}
        files["src/cache.cpp"] = b"int lookup(int key)\n{\n\treturn table[key];\n}\n"
        files["src/stable.cpp"] = b"int stable;\n"
        write_tree(alpha, files)
        beta = workspace / "beta-witness"
        write_tree(beta, {"notes.txt": b"one\ntwo\nthree\n"})

        assert ok("open", str(alpha))["cycle"] == "RC-1"
        first_item = ok("note", "RC-1", "--file", "src/cache.cpp", "--lines", "3", "--body-file", "-", stdin=b"Bounds-check the key.\n")["item"]
        draft = ok("review", "RC-1")
        assert ok("freeze", "RC-1", "--expected-revision", draft["draft_revision"])["round"] == "F1"
        alpha_f1 = exported("RC-1", "F1")
        alpha_f1_digest = round_digest("RC-1", "F1")

        assert ok("open", str(beta))["cycle"] == "RC-2"
        beta_item = ok("note", "RC-2", "--file", "notes.txt", "--lines", "2", "--body-file", "-", stdin=b"Say two.\n")["item"]
        draft = ok("review", "RC-2")
        assert ok("freeze", "RC-2", "--expected-revision", draft["draft_revision"])["round"] == "F1"
        beta_digest = round_digest("RC-2", "F1")
        (exports / "beta.json").write_bytes(response_document("RC-2", "F1", beta_digest, "RESP-BETA", beta_item))
        assert ok("response", "import", "RC-2", "--round", "F1", "--file", str(exports / "beta.json"))["appended"] is True
        beta_f1 = exported("RC-2", "F1")
        # Reading records a delivery observation; the witness is compared without them.
        beta_state = {key: value for key, value in shown("RC-2").items() if key != "deliveries"}
        beta_tree = tree_digest(beta)

        def witnesses(where, root=None):
            """The other cycle, the earlier round and the store, after every case."""
            check_store(root or data, where)
            assert {key: value for key, value in shown("RC-2", root=root).items() if key != "deliveries"} == beta_state, f"{where}: the other cycle is unchanged"
            assert exported("RC-2", "F1", root=root) == beta_f1, f"{where}: the other cycle's round reads as frozen"
            assert exported("RC-1", "F1", root=root) == alpha_f1, f"{where}: the earlier round reads as frozen"
            assert tree_digest(beta) == beta_tree, f"{where}: the other cycle's folder is unchanged"

        # ---------------------------------------------------------------- 1. kill points
        wal = store_path(data).with_name("review-cycles.sqlite3-wal")

        def wal_size():
            try:
                return wal.stat().st_size
            except FileNotFoundError:
                return 0

        def run_killed(arguments, operation, stdin, trigger):
            """Starts the command and kills it at `trigger`: a delay in seconds, or
            None for the moment the store's journal first grows. Returns whether
            the kill met a running process."""
            baseline = wal_size()
            process = subprocess.Popen(command(*arguments, operation=operation), stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=workspace, env=environment)
            process.stdin.write(stdin)
            process.stdin.close()
            started = time.monotonic()
            while process.poll() is None:
                if (trigger is None and wal_size() > baseline) or (trigger is not None and time.monotonic() - started >= trigger):
                    break
                if time.monotonic() - started > 120:
                    break
                time.sleep(0.0005)
            running = process.poll() is None
            kill(process)
            process.stdout.close()
            process.stderr.close()
            return running

        def repeat(where, arguments, stdin, identity, applied):
            """The same command again under its operation ID. It completes; or,
            when the stopped run had already written, it is refused as a
            conflict that says an earlier run may have completed, or as a
            draft that moved on, which that run moved. It is never applied
            twice. Returns 1 for a refusal."""
            repeated = bpr(*arguments, stdin=stdin, operation=identity)
            value = envelope(repeated, arguments)
            if value["outcome"] == "ok":
                return 0
            error = value["error"]
            said = (error["code"] == "conflict" and "may have completed" in error["message"]) or (error["code"] == "stale_revision" and "Nothing was" in error["message"])
            assert applied and said, f"{where}: a repeat is refused only when the stopped run had already applied it, and says what is true: {value}"
            return 1

        class Operation:
            """One command of the sweep: how to prepare a fresh instance, run it,
            and count what it adds."""

            def __init__(self, name, prepare, count):
                self.name = name
                self.prepare = prepare
                self.count = count

        attempt_index = [0]

        def prepare_capture():
            attempt_index[0] += 1
            for index in range(0, 400, 8):
                (alpha / f"src/module{index:03}.cpp").write_bytes(secrets.token_hex(1024).encode() + b"\n")
            return ("capture", "RC-1"), b"", None

        def prepare_note():
            attempt_index[0] += 1
            return ("note", "RC-1", "--body-file", "-"), (f"Kill point {attempt_index[0]}: " + secrets.token_hex(2048)).encode(), None

        def prepare_goal():
            attempt_index[0] += 1
            revision = ok("review", "RC-1")["draft_revision"]
            return ("review", "RC-1", "--goal-file", "-", "--expected-revision", revision), f"Goal {attempt_index[0]}\n".encode(), None

        def prepare_freeze():
            attempt_index[0] += 1
            revision = ok("review", "RC-1", "--goal-file", "-", "--expected-revision", ok("review", "RC-1")["draft_revision"], stdin=f"Round {attempt_index[0]}\n".encode())["draft_revision"]
            return ("freeze", "RC-1", "--expected-revision", revision), b"", None

        def prepare_response():
            attempt_index[0] += 1
            path = exports / f"response-{attempt_index[0]}.json"
            path.write_bytes(response_document("RC-1", "F1", alpha_f1_digest, f"RESP-KILL-{attempt_index[0]}", first_item, body="Done. " + secrets.token_hex(8192)))
            return ("response", "import", "RC-1", "--round", "F1", "--file", str(path)), b"", None

        def prepare_save():
            attempt_index[0] += 1
            path = exports / f"saved-{attempt_index[0]}.bpr-review.zip"
            return ("export", "RC-1", "--round", "F1", "--format", "bundle", "--output", str(path)), b"", path

        def draft_revision():
            return int(ok("review", "RC-1")["draft_revision"])

        operations = [
            Operation("capture", prepare_capture, lambda: int(shown("RC-1")["captures"])),
            Operation("note", prepare_note, lambda: len(shown("RC-1")["notes"])),
            Operation("goal", prepare_goal, draft_revision),
            Operation("freeze", prepare_freeze, lambda: len(shown("RC-1")["rounds"])),
            Operation("response", prepare_response, lambda: len(shown("RC-1")["responses"])),
            Operation("save", prepare_save, lambda: len(shown("RC-1")["deliveries"])),
        ]
        bundle_bytes = [None]
        summary = []
        for operation in operations:
            # The command's own run, untouched, measures where the fractions fall.
            arguments, stdin, destination = operation.prepare()
            before = operation.count()
            started = time.monotonic()
            result = bpr(*arguments, stdin=stdin)
            measured = time.monotonic() - started
            assert result.returncode == 0, f"{operation.name}: the untouched run succeeds: {result.stdout[-400:]!r}"
            assert operation.count() == before + 1, f"{operation.name}: the untouched run adds one"
            if destination is not None:
                bundle_bytes[0] = bundle_bytes[0] or destination.read_bytes()
                assert destination.read_bytes() == bundle_bytes[0], "a bundle of one round is the same bytes every time"
            landed = {"before": 0, "after": 0, "finished": 0, "refused_on_repeat": 0}
            for trigger in (None, *(measured * fraction for fraction in KILL_FRACTIONS)):
                arguments, stdin, destination = operation.prepare()
                before = operation.count()
                identity = f"bpr1183-{operation.name}-{attempt_index[0]}"
                running = run_killed(arguments, identity, stdin, trigger)
                where = f"{operation.name} killed at {'its first store write' if trigger is None else f'{trigger:.3f} s'}"
                now = operation.count()
                assert now in (before, before + 1), f"{where}: the cycle is as before or as after, never between ({before} -> {now})"
                landed["finished" if not running else ("before" if now == before else "after")] += 1
                if destination is not None:
                    left = destination.read_bytes() if destination.exists() else None
                    assert left in (None, bundle_bytes[0]), f"{where}: the chosen name holds the round's exact bytes or nothing"
                check_store(data, where)
                landed["refused_on_repeat"] += repeat(where, arguments, stdin, identity, applied=now == before + 1)
                assert operation.count() == before + 1, f"{where}: repeated with its operation ID, it is applied exactly once ({before} -> {operation.count()})"
                if destination is not None:
                    assert destination.read_bytes() == bundle_bytes[0], f"{where}: the repeat leaves the exact round"
            leftovers = sorted(path.name for path in exports.iterdir() if path.name.endswith(".bpr-tmp"))
            summary.append(f"{operation.name}:killed_before={landed['before']},after={landed['after']},finished_first={landed['finished']},"
                           f"refused_on_repeat={landed['refused_on_repeat']},measured={measured:.3f}s"
                           + (f",temporary_left={len(leftovers)}" if operation.name == "save" else ""))
            witnesses(f"after the {operation.name} kills")
        # The detector, shown live: a copy whose latest revision lost one pinned object fails the check.
        detector = workspace / "detector"
        copy_private_root(data, detector)
        with contextlib.closing(sqlite3.connect(str(store_path(detector)))) as database:
            database.execute("DELETE FROM objects WHERE digest = (SELECT digest FROM revision_objects ORDER BY revision DESC LIMIT 1)")
            database.commit()
        try:
            check_store(detector, "the detector")
            raise RuntimeError("the store check passed a revision whose pinned object is gone")
        except AssertionError:
            pass
        shutil.rmtree(detector)
        print("RECOVERY_KILL_POINTS_OK " + " ".join(summary))

        # ---------------------------------------------------------------- 2. the owner killed mid-command
        desktop = Desktop(args.application, data, workspace, 1, environment)
        try:
            resumed = desktop.ask("resume", title="alpha-project", notes=1)
            assert resumed["cycle"] == "RC-1", resumed
            for index in range(0, 400, 4):
                (alpha / f"src/module{index:03}.cpp").write_bytes(secrets.token_hex(1024).encode() + b"\n")
            before = int(shown("RC-1")["captures"])
            identity = "bpr1183-owner-killed"
            baseline = wal_size()
            forwarded = subprocess.Popen(command("capture", "RC-1", operation=identity), stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=workspace, env=environment)
            deadline = time.monotonic() + 120
            while forwarded.poll() is None and wal_size() <= baseline and time.monotonic() < deadline:
                time.sleep(0.0005)
            owner_was_writing = desktop.process.poll() is None and forwarded.poll() is None
            kill(desktop.process)
            output, _ = forwarded.communicate(timeout=300)
            said = json.loads(output)
        finally:
            desktop.close()
        assert forwarded.returncode in (0, 6), f"the terminal finishes or says it is uncertain: {forwarded.returncode} {said}"
        if forwarded.returncode == 6:
            assert said["error"]["code"] == "uncertain", said
        now = int(shown("RC-1")["captures"])
        assert now in (before, before + 1), f"the owner's death leaves the cycle as before or as after ({before} -> {now})"
        check_store(data, "the owner killed")
        conflicted = repeat("the owner killed", ("capture", "RC-1"), b"", identity, applied=now == before + 1)
        assert int(shown("RC-1")["captures"]) == before + 1, "repeated with its operation ID, the capture is applied exactly once"
        desktop = Desktop(args.application, data, workspace, 2, environment)
        try:
            resumed = desktop.ask("resume", title="alpha-project", notes=1)
            assert len(resumed["revisions"]) == before + 1, f"a new desktop shows the capture once: {resumed['revisions']}"
            desktop.quit()
        finally:
            desktop.close()
        witnesses("after the owner was killed")
        print(f"RECOVERY_OWNER_KILLED_OK terminal_exit={forwarded.returncode} owner_was_writing={int(owner_was_writing)} refused_on_repeat={conflicted}")

        # ---------------------------------------------------------------- 3. a full disk
        if POSIX:
            def full(where, arguments, stdin=b"", destination=None):
                before = shown("RC-1")
                result = bpr(*arguments, stdin=stdin, limit=FULL_DISK_BYTES)
                value = envelope(result, arguments)
                assert result.returncode != 0 and value["outcome"] == "error", f"{where}: a full disk refuses: {value}"
                assert value["error"]["code"] in ("storage_unavailable", "destination_unavailable"), f"{where}: said as storage, not as something else: {value}"
                after = shown("RC-1")
                assert {key: entry for key, entry in after.items() if key != "deliveries"} == {key: entry for key, entry in before.items() if key != "deliveries"}, f"{where}: nothing was recorded"
                assert len(after["deliveries"]) == len(before["deliveries"]), f"{where}: no delivery is claimed"
                if destination is not None:
                    assert not destination.exists(), f"{where}: nothing is left at the chosen name"
                    assert not any(path.name.endswith(".bpr-tmp") for path in destination.parent.iterdir()), f"{where}: no temporary file is left"
                check_store(data, where)
                result = bpr(*arguments, stdin=stdin)
                value = envelope(result, arguments)
                assert result.returncode == 0 and value["outcome"] == "ok", f"{where}: with space again it succeeds: {value}"
                return value["data"]

            (alpha / "src" / "large.bin").write_bytes(secrets.token_bytes(2 * 1024 * 1024))
            captures = int(shown("RC-1")["captures"])
            full("a capture", ("capture", "RC-1"))
            assert int(shown("RC-1")["captures"]) == captures + 1, "captured once"
            big = exports / "big-response.json"
            big.write_bytes(response_document("RC-1", "F1", alpha_f1_digest, "RESP-FULL", first_item, body="x" * (200 * 1024)))
            responses = len(shown("RC-1")["responses"])
            full("a response", ("response", "import", "RC-1", "--round", "F1", "--file", str(big)))
            assert len(shown("RC-1")["responses"]) == responses + 1, "added once"
            # A round larger than the space left: four long notes and a long goal.
            for index in range(4):
                ok("note", "RC-1", "--body-file", "-", stdin=(f"Long note {index}: " + "y" * (15 * 1024)).encode())
            revision = ok("review", "RC-1")["draft_revision"]
            revision = ok("review", "RC-1", "--goal-file", "-", "--expected-revision", revision, stdin=("z" * (60 * 1024)).encode())["draft_revision"]
            rounds = len(shown("RC-1")["rounds"])
            frozen = full("a freeze", ("freeze", "RC-1", "--expected-revision", revision))
            assert len(shown("RC-1")["rounds"]) == rounds + 1, "frozen once"
            saved = exports / "full" / "round.bpr-review.zip"
            saved.parent.mkdir()
            full("a save", ("export", "RC-1", "--round", frozen["round"], "--format", "bundle", "--output", str(saved)), destination=saved)
            assert zipfile.ZipFile(saved).testzip() is None, "the saved bundle is whole"
            witnesses("after a full disk")
            print("RECOVERY_DISK_FULL_OK capture response freeze save")
        else:
            skipped("full disk", "no per-process file-size limit on Windows")

        # ---------------------------------------------------------------- 4. damaged and missing retained material
        backup = workspace / "backup"
        copy_private_root(data, backup)
        # A file changed after R1, so R1's bytes are retained only.
        (alpha / "src" / "cache.cpp").write_bytes(b"int lookup(int key)\n{\n\treturn key < size ? table[key] : -1;\n}\n")
        latest = ok("capture", "RC-1")["latest"]
        old_cache = hashlib.sha256(files["src/cache.cpp"]).digest()

        def tamper(statement, *parameters):
            database = sqlite3.connect(str(store_path(data)))
            try:
                database.execute(statement, parameters)
                database.commit()
            finally:
                database.close()

        tamper("UPDATE object_chunks SET bytes = ? WHERE digest = ?", files["src/cache.cpp"].replace(b"table", b"TABLE"), old_cache)
        error = refused("source_unavailable", "compare", "RC-1", "--from", "R1", "--to", latest, "--path", "src/cache.cpp")
        assert "nothing was substituted" in error["message"], error
        error = refused("source_unavailable", "compare", "RC-1", "--to", "R1", "--path", "src/cache.cpp")
        assert "TABLE" not in json.dumps(error) and "key < size" not in json.dumps(error), "neither the damaged nor the live bytes are shown"
        assert ok("compare", "RC-1", "--to", "R1", "--path", "src/stable.cpp")["rows"][0]["text"] == "int stable;", "an undamaged pin of the same revision reads"
        tamper("DELETE FROM object_chunks WHERE digest = ?", old_cache)
        tamper("DELETE FROM objects WHERE digest = ?", old_cache)
        error = refused("source_unavailable", "compare", "RC-1", "--to", "R1", "--path", "src/cache.cpp")
        assert "are missing" in error["message"], f"missing bytes are said as missing, not as no file: {error}"
        error = refused("not_found", "compare", "RC-1", "--from", "R1", "--to", latest, "--path", "src/absent.cpp")
        assert error["message"] == f"Neither R1 nor {latest} holds src/absent.cpp.", error
        # A pin whose bytes are still in the folder is repaired by capturing them again.
        stable = hashlib.sha256(files["src/stable.cpp"]).digest()
        tamper("UPDATE object_chunks SET bytes = ? WHERE digest = ?", b"int STABLE;\n", stable)
        refused("source_unavailable", "compare", "RC-1", "--to", "R1", "--path", "src/stable.cpp")
        ok("capture", "RC-1", "--again")
        assert ok("compare", "RC-1", "--to", "R1", "--path", "src/stable.cpp")["rows"][0]["text"] == "int stable;", "recapturing the same bytes repaired the pin"
        # A damaged round: nothing reads it, writes it or hands it on.
        f1_digest = bytes.fromhex(alpha_f1_digest)
        with read_only(store_path(data)) as database:
            chunk = database.execute("SELECT bytes FROM object_chunks WHERE digest = ?", (f1_digest,)).fetchone()[0]
        tamper("UPDATE object_chunks SET bytes = ? WHERE digest = ?", chunk.replace(b"Bounds-check", b"Bounds-CHECK"), f1_digest)
        for form in ("markdown", "bundle"):
            target = exports / f"damaged.{form}"
            error = refused("source_unavailable", "export", "RC-1", "--round", "F1", "--format", form, "--output", str(target))
            assert "damaged" in error["message"] and not target.exists(), f"a damaged round is not written as {form}: {error}"
        printed = bpr("export", "RC-1", "--round", "F1", "--output", "-", machine=False)
        assert printed.returncode == 5 and printed.stdout == b"", "a damaged round prints nothing"
        refused("source_unavailable", "round", "grant", "RC-1", "--round", "F1")
        assert any(entry["round"] == "F1" for entry in shown("RC-1")["rounds"]), "the damaged round is still listed, not forgotten"
        assert exported("RC-2", "F1") == beta_f1, "the other cycle's round reads"
        print("RECOVERY_DAMAGED_MATERIAL_OK")

        # ---------------------------------------------------------------- 5. a damaged or lost store
        shutil.rmtree(data)
        copy_private_root(backup, data)
        witnesses("the reviewer's copy restored")
        sound = store_path(data).read_bytes()
        damaged_header = b"not a database!!" + sound[16:]
        store_path(data).write_bytes(damaged_header)
        for arguments in (("list",), ("show", "RC-1"), ("open", str(alpha))):
            error = refused("storage_unavailable", *arguments)
            assert "damaged" in error["message"] and "different version" not in error["message"], f"{arguments}: a damaged store is said as damaged: {error}"
        assert store_path(data).read_bytes() == damaged_header and store_files(data) == ["review-cycles.sqlite3"], "the damaged store is left as it is and nothing is created beside it"
        with read_only(store_path(backup)) as copy:
            page = copy.execute("PRAGMA page_size").fetchone()[0]
        damaged_pages = bytearray(sound)
        for offset in range(page, len(damaged_pages), page):
            damaged_pages[offset:offset + 64] = b"\xa5" * 64
        store_path(data).write_bytes(bytes(damaged_pages))
        error = refused("storage_unavailable", "list")
        assert "damaged" in error["message"], f"damaged pages are said as damaged: {error}"
        assert store_path(data).read_bytes() == bytes(damaged_pages), "and left as they are"
        # Lost: nothing reconstructs private history.
        for name in store_files(data):
            (store_path(data).parent / name).unlink()
        assert ok("list")["cycles"] == [], "a lost store lists nothing"
        refused("not_found", "show", "RC-1")
        reopened = ok("open", str(alpha))
        assert reopened["created"] is True and reopened["captures"] == "1", f"the folder opens as a new cycle: {reopened}"
        # Its first round has the lost round's name, never its content: a reply to the lost round is refused.
        ok("note", reopened["cycle"], "--file", "src/cache.cpp", "--lines", "3", "--body-file", "-", stdin=b"Bounds-check the key.\n")
        draft = ok("review", reopened["cycle"])
        ok("freeze", reopened["cycle"], "--expected-revision", draft["draft_revision"])
        lost_response = exports / "lost.json"
        lost_response.write_bytes(response_document(reopened["cycle"], "F1", alpha_f1_digest, "RESP-LOST", first_item))
        error = refused("conflict", "response", "import", reopened["cycle"], "--round", "F1", "--file", str(lost_response))
        assert error["detail"]["problem"] == "wrong_digest", f"a reply to the lost round is refused by its content: {error}"
        assert exported(reopened["cycle"], "F1") != alpha_f1, "the new round is not the lost one"
        shutil.rmtree(data)
        copy_private_root(backup, data)
        witnesses("the reviewer's copy restored again")
        print("RECOVERY_DAMAGED_STORE_OK")

        # ---------------------------------------------------------------- 6. stale drafts, replaced and moved roots
        revision = ok("review", "RC-1")["draft_revision"]
        ok("review", "RC-1", "--goal-file", "-", "--expected-revision", revision, stdin=b"Another writer's goal.\n")
        settled = shown("RC-1")
        error = refused("stale_revision", "review", "RC-1", "--goal-file", "-", "--expected-revision", revision, stdin=b"Stale goal.\n")
        assert error["detail"]["draft_revision"] != revision, error
        refused("stale_revision", "freeze", "RC-1", "--expected-revision", revision)
        assert shown("RC-1") == settled, "a stale goal or freeze changes nothing"

        # A writer that never stops: retried a bounded number of times, then refused, nothing recorded.
        captures = shown("RC-1")["captures"]
        writing = threading.Event()
        writing.set()

        def writer():
            turn = 0
            while writing.is_set():
                turn += 1
                for index in range(0, 400, 40):
                    (alpha / f"src/module{index:03}.cpp").write_bytes(f"{turn} {secrets.token_hex(64)}\n".encode())
                time.sleep(0.001)

        thread = threading.Thread(target=writer, daemon=True)
        thread.start()
        try:
            error = refused("capture_unstable", "capture", "RC-1")
        finally:
            writing.clear()
            thread.join(timeout=10)
        assert "kept changing" in error["message"] and error["next"] == "bpr cycle capture RC-1", f"an unstable folder is said as such, with a retry: {error}"
        assert shown("RC-1")["captures"] == captures, "no capture is claimed while the folder kept changing"
        ok("capture", "RC-1")
        assert int(shown("RC-1")["captures"]) == int(captures) + 1, "once quiet, it captures"

        alpha_tree = tree_digest(alpha)
        moved = workspace / "alpha-moved"
        os.rename(alpha, moved)
        impostor = {"src/cache.cpp": b"int impostor;\n"}
        write_tree(alpha, impostor)
        captures = shown("RC-1")["captures"]
        error = refused("source_unavailable", "capture", "RC-1")
        assert "Another folder now answers" in error["message"] and error["next"] is None, f"a replaced root is refused, and retrying is not offered: {error}"
        assert shown("RC-1")["captures"] == captures, "nothing was recorded from the impostor"
        desktop = Desktop(args.application, data, workspace, 3, environment)
        try:
            resumed = desktop.ask("resume", title="alpha-project", notes=1)
            assert resumed["cycle"] == "RC-1", resumed
            said = desktop.ask("capture-refused")
            assert "Another folder now answers" in said["message"], f"the window says the same: {said['message']}"
            desktop.quit()
        finally:
            desktop.close()
        assert shown("RC-1")["captures"] == captures
        opened = ok("open", str(alpha))
        assert opened["cycle"] not in ("RC-1", "RC-2") and opened["created"] is True, f"the folder now there opens as its own cycle: {opened}"
        replaced_cycle = opened["cycle"]
        # A link at the location to another folder is another root too.
        shutil.rmtree(alpha)
        if POSIX:
            os.symlink(beta, alpha)
            refused("source_unavailable", "capture", "RC-1")
            assert tree_digest(beta) == beta_tree, "nothing followed into the folder the link names"
            os.unlink(alpha)
        else:
            skipped("a link at the root's location", "a directory link needs the symlink privilege on Windows")
        # A moved root is the same object, but its old location is what the cycle reads.
        assert ok("open", str(moved))["cycle"] == "RC-1", "the moved folder is recognised as RC-1's root by identity"
        error = refused("source_unavailable", "capture", "RC-1")
        assert "no longer exists" in error["message"] and error["next"] is None, f"nothing is at the old location, and that is what is said: {error}"
        assert shown("RC-1")["captures"] == captures, "a moved root is not captured from its new location silently"
        os.rename(moved, alpha)
        assert tree_digest(alpha) == alpha_tree, "moving changed nothing inside"
        ok("capture", "RC-1", "--again")
        assert int(shown("RC-1")["captures"]) == int(captures) + 1, "moved back, it captures again"
        assert shown(replaced_cycle)["captures"] == "1", "the impostor's cycle keeps only its own capture"
        witnesses("after the roots")
        print("RECOVERY_STALE_AND_ROOTS_OK")

        # ---------------------------------------------------------------- 7. duplicates
        response = exports / "duplicate.json"
        response.write_bytes(response_document("RC-1", "F1", alpha_f1_digest, "RESP-DUPLICATE", first_item))
        responses = len(shown("RC-1")["responses"])
        assert ok("response", "import", "RC-1", "--round", "F1", "--file", str(response))["appended"] is True
        assert ok("response", "import", "RC-1", "--round", "F1", "--file", str(response))["appended"] is False, "the same response twice is one response"
        settled = shown("RC-1")
        assert len(settled["responses"]) == responses + 1
        response.write_bytes(response_document("RC-1", "F1", alpha_f1_digest, "RESP-DUPLICATE", first_item, body="Something else."))
        error = refused("conflict", "response", "import", "RC-1", "--round", "F1", "--file", str(response))
        assert error["detail"]["problem"] == "reused_identifier", error
        response.write_bytes(b'{"schema":"bpr.cycle.response.v1","response_id":"A","response_id":"B"}')
        refused("invalid_input", "response", "import", "RC-1", "--round", "F1", "--file", str(response))
        ok("note", "RC-1", "--body-file", "-", stdin=b"Once.\n", operation="bpr1183-reused")
        settled = shown("RC-1")
        # Run by the terminal itself, the repeat reads the cycle the first run moved: refused, and said so.
        for body in (b"Once.\n", b"Twice?\n"):
            error = refused("conflict", "note", "RC-1", "--body-file", "-", stdin=body, operation="bpr1183-reused")
            assert "may have completed" in error["message"] and "nothing was applied again" in error["message"], error
        assert shown("RC-1") == settled, "a duplicate changes nothing"
        witnesses("after the duplicates")
        print("RECOVERY_DUPLICATES_OK")

        # ---------------------------------------------------------------- 8. case collisions
        with case_sensitive_folder(workspace) as sensitive:
            if sensitive is None:
                pass
            else:
                collided = sensitive / "collided"
                write_tree(collided, {"src/Cache.cpp": b"int upper;\n", "src/cache.cpp": b"int lower;\n"})
                before = tree_digest(collided)
                opened = ok("open", str(collided))
                cycle = opened["cycle"]
                assert opened["latest_entries"] == "2", f"both files are captured: {opened}"
                upper = ok("note", cycle, "--file", "src/Cache.cpp", "--body-file", "-", stdin=b"Upper.\n")["item"]
                lower = ok("note", cycle, "--file", "src/cache.cpp", "--body-file", "-", stdin=b"Lower.\n")["item"]
                draft = ok("review", cycle)
                draft = ok("review", cycle, "--context", "src/Cache.cpp", "--expected-revision", draft["draft_revision"])
                draft = ok("review", cycle, "--context", "src/cache.cpp", "--expected-revision", draft["draft_revision"])
                frozen = ok("freeze", cycle, "--expected-revision", draft["draft_revision"])
                document = exported(cycle, frozen["round"])
                assert b"src/Cache.cpp" in document and b"src/cache.cpp" in document, "both paths are carried, as written"
                bundle = exports / "collided.bpr-review.zip"
                ok("export", cycle, "--round", frozen["round"], "--format", "bundle", "--output", str(bundle))
                members = zipfile.ZipFile(bundle).namelist()
                assert len({name.casefold() for name in members}) == len(members), f"no two bundle members merge on a case-insensitive reader: {members}"
                ok("policy", "--classification", "unclassified", "--channel", "external_retrieval", "--permission", "allowed")
                ok("round", "grant", cycle, "--round", frozen["round"])
                listed = ok("round", "source", cycle, "--round", frozen["round"], extra={"BPR_CYCLE_ROLE": "consumer"})
                paths = sorted(entry["path"] for entry in listed["sources"])
                assert paths == ["src/Cache.cpp", "src/cache.cpp"], f"the round's sources are two: {paths}"
                assert {upper, lower} <= {note["item"] for note in shown(cycle)["notes"]}
                assert tree_digest(collided) == before, "the folder is unchanged"
                print("RECOVERY_CASE_COLLISIONS_OK")

        # ---------------------------------------------------------------- 9. mixed builds
        mixed(args, workspace, environment, bpr, ok, refused, exported, response_document, round_digest)

        assert tree_digest(beta) == beta_tree
        print("REVIEW_CYCLE_RECOVERY_CORPUS_OK")


@contextlib.contextmanager
def case_sensitive_folder(workspace):
    """A folder in which `A` and `a` are two names: the workspace itself where
    its volume is case-sensitive, a disk image on macOS, or None (skipped)."""
    probe = workspace / "case-probe"
    probe.mkdir()
    (probe / "A").write_bytes(b"")
    sensitive = not (probe / "a").exists()
    shutil.rmtree(probe)
    if sensitive:
        yield workspace
        return
    if sys.platform != "darwin" or shutil.which("hdiutil") is None:
        skipped("case collisions", "the volume is case-insensitive and the course cannot make one that is not")
        yield None
        return
    image = workspace / "case-sensitive.sparseimage"
    mount = workspace / "case-sensitive"
    mount.mkdir()
    created = subprocess.run(["hdiutil", "create", "-quiet", "-size", "16m", "-fs", "Case-sensitive APFS", "-volname", "bpr1183", "-type", "SPARSE", str(image)], capture_output=True, timeout=120)
    attached = created.returncode == 0 and subprocess.run(["hdiutil", "attach", "-quiet", "-nobrowse", "-mountpoint", str(mount), str(image)], capture_output=True, timeout=120).returncode == 0
    if not attached:
        skipped("case collisions", f"no case-sensitive disk image could be attached here: {created.stderr[-200:]!r}")
        yield None
        return
    try:
        yield mount
    finally:
        subprocess.run(["hdiutil", "detach", "-quiet", "-force", str(mount)], capture_output=True, timeout=120)


def mixed(args, workspace, environment, bpr, ok, refused, exported, response_document, round_digest):
    """Part 9: one profile through an older build, the current desktop, the
    current terminal and an agent-free build."""
    if not args.older_cycle_application and not args.agent_free_cycle_application:
        skipped("mixed builds", "no --older-cycle-application or --agent-free-cycle-application was given")
        return
    data = workspace / "mixed-data"
    make_root_private(data)
    project = workspace / "gamma-mixed"
    (project / "src").mkdir(parents=True)
    (project / "src" / "parser.cpp").write_bytes(b"int parse(const char* text);\n")
    writer = args.older_cycle_application or args.cycle_application

    def call(executable, *arguments, **options):
        return ok(*arguments, executable=executable, root=data, **options)

    opened = call(writer, "open", str(project))
    cycle = opened["cycle"]
    item = call(writer, "note", cycle, "--file", "src/parser.cpp", "--body-file", "-", stdin=b"Name the parameter.\n")["item"]
    draft = call(writer, "review", cycle)
    call(writer, "freeze", cycle, "--expected-revision", draft["draft_revision"])
    frozen = exported(cycle, "F1", executable=writer, root=data)
    digest = next(entry["digest"] for entry in call(writer, "show", cycle)["rounds"] if entry["round"] == "F1")
    reply = workspace / "mixed-response.json"
    reply.write_bytes(response_document(cycle, "F1", digest, "RESP-MIXED", item))
    call(writer, "response", "import", cycle, "--round", "F1", "--file", str(reply))
    written = {key: value for key, value in call(writer, "show", cycle).items() if key != "deliveries"}
    schema = user_version(data)

    # The current desktop opens the profile first.
    desktop = Desktop(args.application, data, workspace, 9, environment)
    try:
        resumed = desktop.ask("resume", title="gamma-mixed", notes=1)
        assert resumed["cycle"] == cycle and [note["body"] for note in resumed["notes"]] == ["Name the parameter.\n"] and resumed["responses"] == 1, f"the desktop reads what {Path(writer).name} wrote: {resumed}"
        assert exported(cycle, "F1", root=data) == frozen, "forwarded, the current terminal reads the round as frozen"
        if args.agent_free_cycle_application:
            assert exported(cycle, "F1", executable=args.agent_free_cycle_application, root=data) == frozen, "and so does an agent-free build"
            call(args.agent_free_cycle_application, "note", cycle, "--body-file", "-", stdin=b"From the agent-free terminal.\n")
            desktop.ask("wait-notes", count=2)
        if args.older_cycle_application:
            # An older terminal speaks to the current owner, which runs the command on its own store, or is refused.
            older = json.loads(bpr("show", cycle, executable=args.older_cycle_application, root=data).stdout)
            assert older["outcome"] == "ok" or older["error"]["code"] == "unsupported_version", f"an older terminal is served or refused, nothing else: {older}"
            print(f"RECOVERY_MIXED_OLDER_WITH_OWNER {older['outcome'] if older['outcome'] == 'ok' else older['error']['code']}")
        desktop.quit()
    finally:
        desktop.close()
    upgraded = user_version(data)
    now = {key: value for key, value in ok("show", cycle, root=data).items() if key != "deliveries"}
    assert now["rounds"] == written["rounds"] and now["responses"] == written["responses"], "rounds and responses survive the upgrade unchanged"
    assert exported(cycle, "F1", root=data) == frozen, "the current terminal reads the round byte for byte"
    if args.older_cycle_application and upgraded != schema:
        before = store_path(data).read_bytes()
        error = refused("unsupported_version", "list", executable=args.older_cycle_application, root=data)
        assert "different version" in error["message"], error
        assert store_path(data).read_bytes() == before, "the older build leaves the upgraded store as it is"
    if args.agent_free_cycle_application:
        assert exported(cycle, "F1", executable=args.agent_free_cycle_application, root=data) == frozen
    check_store(data, "mixed builds")
    print(f"RECOVERY_MIXED_BUILDS_OK schema={schema}->{upgraded} older={int(bool(args.older_cycle_application))} agent_free={int(bool(args.agent_free_cycle_application))}")


if __name__ == "__main__":
    main()
