#!/usr/bin/env python3
"""[[BPR-2215]] Print the TMPDIR Linux test rows need, or nothing.

The installed Git cache refuses durable artifacts on a filesystem without
inode generations (BPR-2095). When the system temporary directory is such a
filesystem (Ubuntu 26.04 mounts /tmp as tmpfs) and /var/tmp is not, this prints
/var/tmp, which cmake/BprTestIsolation.cmake gives every isolated row. Anywhere
else it prints nothing and exits 0. It writes only a probe file it removes.
"""

from __future__ import annotations

import errno
import os
import sys
import tempfile

# FS_IOC_GETVERSION, _IOR('v', 1, long) on LP64 Linux.
FS_IOC_GETVERSION = 0x80087601


def answers_generation(directory: str) -> bool:
    import fcntl

    try:
        probe = tempfile.mkdtemp(prefix="bpr-generation-probe-", dir=directory)
    except OSError:
        return False
    path = os.path.join(probe, "probe")
    try:
        descriptor = os.open(path, os.O_RDWR | os.O_CREAT | os.O_EXCL, 0o600)
        try:
            fcntl.ioctl(descriptor, FS_IOC_GETVERSION, b"\0" * 8)
            return True
        except OSError as error:
            if error.errno in (errno.ENOTTY, errno.EOPNOTSUPP, errno.EINVAL):
                return False
            raise
        finally:
            os.close(descriptor)
    finally:
        if os.path.exists(path):
            os.unlink(path)
        os.rmdir(probe)


def main() -> int:
    if not sys.platform.startswith("linux"):
        return 0
    if answers_generation(tempfile.gettempdir()):
        return 0
    if os.path.isdir("/var/tmp") and answers_generation("/var/tmp"):
        print("/var/tmp")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
