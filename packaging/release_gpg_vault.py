#!/usr/bin/env python3
"""Enroll one Windows credential and unlock only its bound WSL release GPG key.

No command returns a secret. Configuration is public producer-local data;
passphrases travel only in process memory and the fixed preset child's stdin.
"""
from __future__ import annotations

import argparse
import base64
import ctypes
from ctypes import wintypes
import getpass
import json
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import stat
import subprocess
import sys
import time
import warnings

SCHEMA = "bpr.release-gpg-vault.v1"
TARGET_PREFIX = "BetterPullReview/ReleaseSigning/OpenPGP/"
USERNAME = "BetterPullReviewReleaseSigning"
KEY_UID = "BetterPullReview Release Signing"
MAX_BLOB = 2560
PROBE = b"BetterPullReview release credential unlock check\n"


class VaultError(ValueError):
    """A fixed, secret-free refusal suitable for operator output."""


def load_config(path: Path) -> dict:
    if not path.is_absolute():
        raise VaultError("an absolute, bounded producer vault configuration is required")
    try:
        if path.stat().st_size > 8192:
            raise VaultError("producer vault configuration exceeds its bound")
        value = json.loads(path.read_text(encoding="utf-8"))
    except OSError:
        raise VaultError("producer vault configuration is absent or inaccessible in this process") from None
    except (ValueError, UnicodeError):
        raise VaultError("invalid producer vault configuration") from None
    return validate_config(value)


def validate_config(value: dict) -> dict:
    fields = {"schema", "approved", "keyFingerprint", "keygrip", "gnupgHome",
              "wslDistribution", "wslUser", "windowsPython"}
    if (not isinstance(value, dict) or set(value) != fields or value.get("schema") != SCHEMA
            or value.get("approved") is not True):
        raise VaultError("an explicitly approved producer vault configuration is required")
    for name in fields - {"approved"}:
        if not isinstance(value[name], str) or not value[name] or any(c in value[name] for c in "\0\r\n"):
            raise VaultError("invalid producer vault configuration field")
    if not re.fullmatch(r"(?:[0-9A-F]{40}|[0-9A-F]{64})", value["keyFingerprint"]):
        raise VaultError("an approved full uppercase OpenPGP fingerprint is required")
    if not re.fullmatch(r"[0-9A-F]{40}", value["keygrip"]):
        raise VaultError("an exact uppercase GPG keygrip is required")
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,63}", value["wslDistribution"]):
        raise VaultError("invalid WSL distribution")
    if not re.fullmatch(r"[a-z_][a-z0-9_-]{0,31}", value["wslUser"]):
        raise VaultError("invalid WSL user")
    home = PurePosixPath(value["gnupgHome"])
    prefix = PurePosixPath("/home") / value["wslUser"] / ".local/share/bpr-release-signing"
    if (not home.is_absolute() or ".." in home.parts or str(home) != value["gnupgHome"]
            or not home.is_relative_to(prefix) or home == prefix):
        raise VaultError("GPG home must be inside this user's dedicated release-signing root")
    python = PureWindowsPath(value["windowsPython"])
    if not python.is_absolute() or python.drive.startswith("\\") or ".." in python.parts or python.suffix.lower() != ".exe":
        raise VaultError("an absolute native Windows Python executable is required")
    return dict(value)


def credential_target(config: dict) -> str:
    return TARGET_PREFIX + config["keyFingerprint"]


def wipe(value: bytearray) -> None:
    value[:] = b"\0" * len(value)


def passphrase_bytes(value: str, encoding: str) -> bytearray:
    if not value or any(c in value for c in "\0\r\n"):
        raise VaultError("passphrase must be nonempty and contain no NUL or line break")
    try:
        encoded = bytearray(value.encode(encoding))
    except UnicodeError:
        raise VaultError("passphrase encoding is invalid") from None
    if len(value.encode("utf-16le")) > MAX_BLOB:
        wipe(encoded)
        raise VaultError("passphrase exceeds the Windows credential limit")
    return encoded


class Credential(ctypes.Structure):
    _fields_ = [("Flags", wintypes.DWORD), ("Type", wintypes.DWORD),
                ("TargetName", wintypes.LPWSTR), ("Comment", wintypes.LPWSTR),
                ("LastWritten", wintypes.FILETIME), ("CredentialBlobSize", wintypes.DWORD),
                ("CredentialBlob", ctypes.POINTER(ctypes.c_ubyte)), ("Persist", wintypes.DWORD),
                ("AttributeCount", wintypes.DWORD), ("Attributes", ctypes.c_void_p),
                ("TargetAlias", wintypes.LPWSTR), ("UserName", wintypes.LPWSTR)]


class WindowsCredential:
    """Exact-name generic credential access; no enumeration or secret-print API."""
    def __init__(self, config: dict):
        if sys.platform != "win32":
            raise VaultError("credential access requires the local signing Windows account")
        self.target = credential_target(config)
        self.api = ctypes.WinDLL("advapi32.dll", use_last_error=True, winmode=0x800)
        self.api.CredReadW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                      ctypes.POINTER(ctypes.POINTER(Credential))]
        self.api.CredReadW.restype = wintypes.BOOL
        self.api.CredWriteW.argtypes = [ctypes.POINTER(Credential), wintypes.DWORD]
        self.api.CredWriteW.restype = wintypes.BOOL
        self.api.CredFree.argtypes = [ctypes.c_void_p]
        self.api.CredFree.restype = None

    def read(self, *, metadata_only: bool = False) -> bytearray | None:
        pointer = ctypes.POINTER(Credential)()
        if not self.api.CredReadW(self.target, 1, 0, ctypes.byref(pointer)):
            error = ctypes.get_last_error()
            if error == 1168:
                raise VaultError("release credential is missing; enroll once in the local signing account")
            raise VaultError(f"release credential unavailable in this Windows logon session (Win32 {error})")
        blob = bytearray()
        try:
            value = pointer.contents
            if (value.Type != 1 or value.TargetName != self.target or value.UserName != USERNAME
                    or value.Persist != 2 or value.Flags != 0 or value.AttributeCount != 0
                    or not value.CredentialBlob or not 0 < value.CredentialBlobSize <= MAX_BLOB
                    or value.CredentialBlobSize % 2):
                raise VaultError("release credential binding or encoding is invalid")
            if metadata_only:
                return None
            blob = bytearray(ctypes.string_at(value.CredentialBlob, value.CredentialBlobSize))
            try:
                return passphrase_bytes(blob.decode("utf-16le"), "utf-8")
            except UnicodeError:
                raise VaultError("release credential encoding is invalid") from None
        finally:
            wipe(blob)
            if pointer:
                value = pointer.contents
                if value.CredentialBlob and value.CredentialBlobSize <= MAX_BLOB:
                    ctypes.memset(value.CredentialBlob, 0, value.CredentialBlobSize)
                self.api.CredFree(pointer)

    def write(self, phrase: str) -> None:
        blob = passphrase_bytes(phrase, "utf-16le")
        try:
            buffer = (ctypes.c_ubyte * len(blob)).from_buffer(blob)
            value = Credential(Type=1, TargetName=self.target, UserName=USERNAME,
                               Persist=2, CredentialBlobSize=len(blob), CredentialBlob=buffer,
                               Comment="BetterPullReview scoped WSL release signing")
            if not self.api.CredWriteW(ctypes.byref(value), 0):
                raise VaultError(f"release credential storage refused (Win32 {ctypes.get_last_error()})")
        finally:
            wipe(blob)


def run_fixed(args: list[str], *, data: bytes | bytearray | None = None) -> bytes:
    try:
        result = subprocess.run(args, input=data, capture_output=True, timeout=45)
    except (OSError, subprocess.TimeoutExpired):
        raise VaultError("release signing prerequisite command unavailable or timed out") from None
    if result.returncode:
        # Never echo child output: a credential failure may come from native code.
        raise VaultError("release signing prerequisite command refused; no interactive fallback")
    if len(result.stdout) > 65536:
        raise VaultError("release signing prerequisite output exceeded its bound")
    return result.stdout


def gpg_args(config: dict, tool: str, *args: str) -> list[str]:
    return [tool, "--homedir", config["gnupgHome"], *args]


def validate_key_listing(output: str, config: dict) -> None:
    rows = [line.split(":") for line in output.splitlines()]
    keys = [row for row in rows if row[0] == "sec"]
    if (len(keys) != 1 or len(keys[0]) < 15 or keys[0][1] in ("r", "e", "d")
            or "s" not in keys[0][11] or keys[0][14] != "+"
            or (keys[0][6] and int(keys[0][6]) <= time.time())
            or any(row[0] == "ssb" and "s" in row[11] for row in rows)):
        raise VaultError("one usable local primary signing key with no signing subkey is required")
    fingerprints = [row[9] for row in rows if row[0] == "fpr"]
    grips = [row[9] for row in rows if row[0] == "grp"]
    uids = [row[9] for row in rows if row[0] == "uid" and row[1] not in ("r", "e", "d")]
    if (not fingerprints or fingerprints[0] != config["keyFingerprint"]
            or not grips or grips[0] != config["keygrip"] or uids != [KEY_UID]):
        raise VaultError("GPG fingerprint, keygrip or release UID does not match the approved binding")


def validate_local(config: dict) -> dict:
    if not sys.platform.startswith("linux") or os.getuid() == 0:
        raise VaultError("GPG operations require the ordinary WSL signing user")
    home = Path(config["gnupgHome"])
    for path in (home, home.parent):
        info = path.lstat()
        if (not stat.S_ISDIR(info.st_mode) or info.st_uid != os.getuid()
                or stat.S_IMODE(info.st_mode) != 0o700 or path.resolve(strict=True) != path):
            raise VaultError("GPG home and parent must be canonical, owned and private (0700)")
    listing = run_fixed(gpg_args(config, "/usr/bin/gpg", "--no-options", "--batch", "--with-colons",
                                 "--with-keygrip", "--list-secret-keys", config["keyFingerprint"]))
    validate_key_listing(listing.decode("utf-8"), config)
    info = run_fixed(gpg_args(config, "/usr/bin/gpg-connect-agent", "KEYINFO " + config["keygrip"], "/bye"))
    rows = [line.split() for line in info.decode("ascii").splitlines() if line.startswith("S KEYINFO ")]
    if len(rows) != 1 or len(rows[0]) < 8 or rows[0][2] != config["keygrip"] or rows[0][3] != "D" or rows[0][7] != "P":
        raise VaultError("the bound GPG key must remain passphrase protected")
    agent = home / "gpg-agent.conf"
    agent_info = agent.lstat()
    if (not stat.S_ISREG(agent_info.st_mode) or agent_info.st_uid != os.getuid()
            or stat.S_IMODE(agent_info.st_mode) & 0o077):
        raise VaultError("the scoped GPG agent configuration must be owned and private")
    lines = [line.strip() for line in agent.read_text(encoding="utf-8").splitlines()
             if line.strip() and not line.lstrip().startswith("#")]
    required = {"default-cache-ttl 600", "max-cache-ttl 7200", "no-allow-external-cache"}
    allowed = required | {"pinentry-program /usr/bin/pinentry-curses", "allow-preset-passphrase"}
    if not required.issubset(lines) or len(lines) != len(set(lines)) or set(lines) - allowed:
        raise VaultError("unexpected scoped GPG agent options; preserve bounded cache and external-cache refusal")
    return {"keyFingerprint": config["keyFingerprint"], "keygrip": config["keygrip"],
            "gnupgHome": config["gnupgHome"], "passphraseProtected": True,
            "presetEnabled": "allow-preset-passphrase" in lines}


def local_action(config: dict, operation: str) -> dict:
    result = validate_local(config)
    if operation == "configure-agent":
        if not result["presetEnabled"]:
            agent = Path(config["gnupgHome"]) / "gpg-agent.conf"
            with agent.open("a", encoding="utf-8") as stream:
                stream.write("\nallow-preset-passphrase\n")
        run_fixed(gpg_args(config, "/usr/bin/gpgconf", "--reload", "gpg-agent"))
        result = validate_local(config)
    elif operation == "unlock":
        if not result["presetEnabled"]:
            raise VaultError("enable presetting only in the scoped GPG agent before enrollment/unlock")
        phrase = bytearray(sys.stdin.buffer.read(7681))
        try:
            if not 0 < len(phrase) <= 7680 or any(c in phrase for c in (0, 10, 13)):
                raise VaultError("invalid passphrase transport")
            # No shell, secret argument, environment variable or temporary file.
            try:
                run_fixed(gpg_args(config, "/usr/lib/gnupg/gpg-preset-passphrase", "--preset", config["keygrip"]), data=phrase)
                run_fixed(gpg_args(config, "/usr/bin/gpg", "--no-options", "--batch", "--pinentry-mode", "error",
                                   "--yes", "--local-user", config["keyFingerprint"] + "!", "--output", "/dev/null",
                                   "--detach-sign"), data=PROBE)
            except VaultError:
                run_fixed(gpg_args(config, "/usr/lib/gnupg/gpg-preset-passphrase", "--forget", config["keygrip"]))
                raise VaultError("stored passphrase did not unlock the bound signing key; no interactive fallback") from None
        finally:
            wipe(phrase)
        result["unlocked"] = True
        result["pinentryDisabledProbe"] = True
    elif operation != "validate":
        raise VaultError("unsupported scoped GPG operation")
    return result


def wsl_action(config: dict, operation: str, *, phrase: bytearray | None = None) -> dict:
    executable = Path(os.environ["SystemRoot"]) / "System32/wsl.exe"
    prefix = [str(executable), "--distribution", config["wslDistribution"], "--user", config["wslUser"], "--exec"]
    helper = run_fixed(prefix + ["/usr/bin/wslpath", "-u", str(Path(__file__).resolve())]).decode().strip()
    binding = base64.b64encode(json.dumps(config).encode()).decode()
    output = run_fixed(prefix + ["/usr/bin/python3", helper, "_wsl", "--binding", binding, "--operation", operation], data=phrase)
    try:
        result = json.loads(output)
    except ValueError:
        raise VaultError("invalid scoped WSL validation receipt") from None
    if (result.get("keyFingerprint") != config["keyFingerprint"] or result.get("keygrip") != config["keygrip"]
            or result.get("gnupgHome") != config["gnupgHome"] or result.get("passphraseProtected") is not True
            or (operation == "unlock" and (result.get("unlocked") is not True or result.get("pinentryDisabledProbe") is not True))):
        raise VaultError("scoped WSL receipt does not match the approved key")
    return result


def unlock_windows(config: dict) -> dict:
    result = wsl_action(config, "validate")
    if not result["presetEnabled"]:
        raise VaultError("scoped GPG presetting is not configured")
    phrase = WindowsCredential(config).read()
    try:
        return wsl_action(config, "unlock", phrase=phrase)
    finally:
        wipe(phrase)


def prepare_linux_signing(path: Path, approved_key: str | None) -> dict:
    if not sys.platform.startswith("linux"):
        raise VaultError("vault-backed package signing requires the WSL Linux producer")
    config = load_config(path)
    if approved_key and approved_key.upper() != config["keyFingerprint"]:
        raise VaultError("package signer differs from the approved vault binding")
    validate_local(config)
    python = run_fixed(["/usr/bin/wslpath", "-u", config["windowsPython"]]).decode().strip()
    helper = run_fixed(["/usr/bin/wslpath", "-w", str(Path(__file__).resolve())]).decode().strip()
    windows_config = run_fixed(["/usr/bin/wslpath", "-w", str(path.resolve(strict=True))]).decode().strip()
    output = run_fixed([python, helper, "unlock", "--config", windows_config])
    result = json.loads(output)
    if (result.get("keyFingerprint") != config["keyFingerprint"] or result.get("keygrip") != config["keygrip"]
            or result.get("gnupgHome") != config["gnupgHome"] or result.get("passphraseProtected") is not True
            or result.get("unlocked") is not True or result.get("pinentryDisabledProbe") is not True):
        raise VaultError("Windows credential unlock receipt does not match the package signer")
    return config


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    for action in ("validate", "configure-agent", "enroll", "unlock"):
        child = sub.add_parser(action)
        child.add_argument("--config", required=True, type=Path)
        if action == "enroll":
            child.add_argument("--replace", action="store_true", help="intentionally replace this exact named credential")
    internal = sub.add_parser("_wsl", help=argparse.SUPPRESS)
    internal.add_argument("--binding", required=True)
    internal.add_argument("--operation", choices=("validate", "configure-agent", "unlock"), required=True)
    args = parser.parse_args()
    if args.action == "_wsl":
        config = validate_config(json.loads(base64.b64decode(args.binding, validate=True)))
        result = local_action(config, args.operation)
    else:
        config = load_config(args.config)
        if sys.platform != "win32":
            raise VaultError("run enrollment/unlock through native Python as the local signing Windows account")
        if args.action == "unlock":
            result = unlock_windows(config)
        else:
            result = wsl_action(config, "configure-agent" if args.action == "configure-agent" else "validate")
            if args.action == "enroll":
                credential = WindowsCredential(config)
                if not args.replace:
                    try:
                        credential.read(metadata_only=True)
                    except VaultError as error:
                        if "is missing;" not in str(error):
                            raise
                    else:
                        raise VaultError("this credential already exists; use --replace only for an intended rotation")
                if not sys.stdin.isatty():
                    raise VaultError("passphrase enrollment requires a visible local console")
                with warnings.catch_warnings():
                    warnings.simplefilter("error", getpass.GetPassWarning)
                    phrase = getpass.getpass("Release GPG passphrase (hidden): ")
                    confirm = getpass.getpass("Confirm passphrase (hidden): ")
                if phrase != confirm:
                    raise VaultError("passphrase confirmation did not match; nothing stored")
                credential.write(phrase)
                del phrase, confirm
                credential.read(metadata_only=True)
                result["credentialStored"] = True
    print(json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except VaultError as error:
        print("release vault refused: " + str(error), file=sys.stderr)
        raise SystemExit(1) from None
    except (ValueError, OSError, KeyError, IndexError, getpass.GetPassWarning):
        # Native output and exceptions never include secret input or child logs.
        print("release vault refused; check approved config, private GPG binding and local Windows credential session", file=sys.stderr)
        raise SystemExit(1) from None
