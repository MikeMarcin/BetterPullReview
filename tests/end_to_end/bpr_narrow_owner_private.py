#!/usr/bin/env python3
"""The owner-private descriptor a Python course writes, in one place.

[[BPR-0900]]. Named to match `bpr_narrow_owner_private.cmake` beside it, so a
reader who finds one finds the other. The two are not the same descriptor and
that difference is the card's subject: the CMake helper writes one inheritable
full-access grant for the current user and stops, while this one also grants the
process's own AppContainer SID when contained and removes the SYSTEM,
Administrators and OWNER RIGHTS entries that CPython's `mkdir(mode=0o700)`
leaves behind — entries the profile registry refuses.

Promoted rather than written: this was byte-identical in
`epoch_ordinary_update_public_path.py` and `epoch_portable_occurrence_public_path.py`,
and a third spelling of one descriptor is the outcome worth avoiding.
"""

import os
import subprocess


def make_root_private(root):
    """[[BPR-0834]]. Creates the data root owner-private by the stores' definition.

    POSIX: mode 0700. Windows: exactly one inheritable full-access grant for the
    current user, plus the process's own AppContainer SID when contained, as the
    product's `make_storage_root_owner_private` writes. CPython's `mkdir(mode=0o700)`
    is not that shape: it writes explicit SYSTEM, Administrators and OWNER RIGHTS
    entries the profile registry refuses, and inside a container it excludes the
    container from its own directory. So Windows creates the directory without a
    mode and narrows it with icacls.
    """
    if os.name == "posix":
        root.mkdir(mode=0o700, exist_ok=True)
        return
    root.mkdir(exist_ok=True)
    grants = ["/grant:r", os.environ["USERDOMAIN"] + "\\" + os.environ["USERNAME"] + ":(OI)(CI)F"]
    container = appcontainer_sid()
    if container is not None:
        grants += ["/grant:r", f"*{container}:(OI)(CI)F"]
    subprocess.run(["icacls", str(root), "/inheritance:r", *grants,
                    "/remove:g", "*S-1-5-18", "*S-1-5-32-544", "*S-1-3-4"],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def appcontainer_sid():
    """The string SID of this process's AppContainer, or None outside one."""
    import ctypes
    from ctypes import wintypes
    advapi32 = ctypes.WinDLL("advapi32", use_last_error=True)
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.GetCurrentProcess.restype = wintypes.HANDLE
    advapi32.OpenProcessToken.argtypes = [wintypes.HANDLE, wintypes.DWORD, ctypes.POINTER(wintypes.HANDLE)]
    advapi32.GetTokenInformation.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD,
                                             ctypes.POINTER(wintypes.DWORD)]
    advapi32.ConvertSidToStringSidW.argtypes = [ctypes.c_void_p, ctypes.POINTER(wintypes.LPWSTR)]
    kernel32.LocalFree.argtypes = [ctypes.c_void_p]
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    token = wintypes.HANDLE()
    if not advapi32.OpenProcessToken(kernel32.GetCurrentProcess(), 0x0008, ctypes.byref(token)):
        raise OSError(f"OpenProcessToken ({ctypes.get_last_error()})")
    try:
        is_container = wintypes.DWORD(0)
        size = wintypes.DWORD(0)
        # TokenIsAppContainer = 29, TokenAppContainerSid = 31
        if not advapi32.GetTokenInformation(token, 29, ctypes.byref(is_container), ctypes.sizeof(is_container),
                                            ctypes.byref(size)):
            raise OSError(f"TokenIsAppContainer ({ctypes.get_last_error()})")
        if not is_container.value:
            return None
        advapi32.GetTokenInformation(token, 31, None, 0, ctypes.byref(size))
        buffer = ctypes.create_string_buffer(size.value)
        if not advapi32.GetTokenInformation(token, 31, buffer, size, ctypes.byref(size)):
            raise OSError(f"TokenAppContainerSid ({ctypes.get_last_error()})")
        sid = ctypes.cast(buffer, ctypes.POINTER(ctypes.c_void_p)).contents.value
        text = wintypes.LPWSTR()
        if not advapi32.ConvertSidToStringSidW(sid, ctypes.byref(text)):
            raise OSError(f"ConvertSidToStringSidW ({ctypes.get_last_error()})")
        try:
            return text.value
        finally:
            kernel32.LocalFree(text)
    finally:
        kernel32.CloseHandle(token)
