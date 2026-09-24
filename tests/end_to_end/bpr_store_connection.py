"""A SQLite connection through which a course reads or edits a store.

[[BPR-1039]]. `with sqlite3.connect(...) as connection:` ends the transaction
when the block ends, but it does not close the connection. On CPython 3.14 the
connection then lives until the cyclic garbage collector finds it, because it
sits in a reference cycle, and on Windows an open connection keeps its file
from being deleted. A course that read its store this way held the file
itself when the contained temporary support removed its root, which failed
with WinError 32 on `device-bindings.sqlite3` and `profile-registry.sqlite3`.
The Restart Manager named the course's own Python process as the holder.
"""

import contextlib
import sqlite3


@contextlib.contextmanager
def store_connection(*arguments, **keywords):
    """Yield a connection that commits when the block succeeds, rolls back
    when it raises, and is closed either way."""
    connection = sqlite3.connect(*arguments, **keywords)
    try:
        with connection:
            yield connection
    finally:
        connection.close()
