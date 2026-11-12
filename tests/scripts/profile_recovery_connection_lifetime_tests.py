"""[[BPR-2137]]. Real SQLite handles remain retained after fixture helpers exit.

The forced-leak control must fail the same closed-handle oracle; collection
cannot hide a missing close. This requires no application, Git or network.
"""
from contextlib import contextmanager
import importlib.util
from pathlib import Path
import sqlite3
import sys
from unittest.mock import patch

sys.dont_write_bytecode = True
SOURCE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(SOURCE))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

spec = importlib.util.spec_from_file_location("recovery_course", SOURCE / "tests/end_to_end/profile_recovery_course.py")
course = importlib.util.module_from_spec(spec)
spec.loader.exec_module(course)
real_connect = sqlite3.connect


@contextmanager
def retained_connections(factory=sqlite3.Connection):
    retained = []

    def connect(*args, **kwargs):
        connection = real_connect(*args, factory=factory, **kwargs)
        retained.append(connection)
        return connection

    try:
        with patch.object(course.sqlite3, "connect", connect):
            yield retained
    finally:
        for connection in retained:
            sqlite3.Connection.close(connection)


def require_closed(retained):
    assert retained, "the actual helpers must have opened a connection"
    for connection in retained:
        try:
            sqlite3.Connection.execute(connection, "SELECT 1")
        except sqlite3.ProgrammingError:
            continue
        raise AssertionError("a retained fixture connection remains live")


class ExecuteFailure(sqlite3.Connection):
    def execute(self, *args, **kwargs):
        raise sqlite3.OperationalError("injected statement failure")


class ForcedLeak(sqlite3.Connection):
    def close(self):
        pass


def main():
    with contained_temporary_directory(prefix="bpr-reset-connections-") as temporary:
        root = Path(temporary)
        course.make_root_private(root)
        ledger = root / "ledger.sqlite3"
        marker = "fixture-owned-sentinel"
        with retained_connections() as retained:
            course.write_reset_sentinel(ledger, marker)
            require_closed(retained)
            committed = real_connect(ledger)
            try:
                assert committed.execute("SELECT marker FROM bpr_reset_course_sentinel").fetchall() == [(marker,)]
            finally:
                committed.close()
            assert course.has_reset_sentinel(ledger, root / "present", marker)
            assert not course.has_reset_sentinel(ledger, root / "wrong-marker", "wrong")
            empty = root / "empty.sqlite3"
            real_connect(empty).close()
            assert not course.has_reset_sentinel(empty, root / "no-table", marker)
            require_closed(retained)
            assert len(retained) == 4
        for operation in ("write", "read"):
            with retained_connections(ExecuteFailure) as retained:
                try:
                    if operation == "write":
                        course.write_reset_sentinel(root / "write-error.sqlite3", marker)
                    else:
                        course.has_reset_sentinel(ledger, root / "read-error", marker)
                except sqlite3.OperationalError as error:
                    assert str(error) == "injected statement failure"
                else:
                    raise AssertionError("the injected statement error must propagate")
                require_closed(retained)
        with retained_connections(ForcedLeak) as retained:
            leaking = root / "forced-leak.sqlite3"
            course.write_reset_sentinel(leaking, marker)
            assert course.has_reset_sentinel(leaking, root / "forced-leak-copy", marker)
            assert len(retained) == 2
            for connection in retained:
                try:
                    require_closed([connection])
                except AssertionError as error:
                    assert str(error) == "a retained fixture connection remains live"
                else:
                    raise AssertionError("the forced-leak negative escaped the lifecycle oracle")
        print("PROFILE_RECOVERY_CONNECTION_LIFETIME_OK: retained-positive=6 forced-leak-rejected=2")


if __name__ == "__main__":
    main()
