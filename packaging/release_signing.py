"""Native signing and verification; never discover credentials or enroll keys."""
from __future__ import annotations

import base64
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


def command(*args: str | Path) -> str:
    return subprocess.run([str(arg) for arg in args], check=True, text=True,
                          capture_output=True).stdout


def authenticode(path: Path, signtool: Path, publisher: str) -> dict:
    if sys.platform != "win32":
        raise ValueError("Authenticode verification requires native Windows")
    command(signtool, "verify", "/pa", "/all", "/v", path)
    # Values are JSON data passed through base64, never PowerShell source.
    data = base64.b64encode(json.dumps({"path": str(path.resolve()), "publisher": publisher}).encode()).decode()
    # A PowerShell 7 parent can put incompatible modules first in PSModulePath.
    # Load this child's own Windows PowerShell module by its absolute manifest.
    script = "$ErrorActionPreference = 'Stop'\n"
    script += "Import-Module (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Security/Microsoft.PowerShell.Security.psd1') -ErrorAction Stop\n"
    script += f"$inputData = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('{data}')) | ConvertFrom-Json\n"
    script += """$s = Get-AuthenticodeSignature -LiteralPath $inputData.path
if ($s.Status -ne 'Valid' -or !$s.SignerCertificate -or !$s.TimeStamperCertificate) {
  throw 'Valid publisher signature and timestamp required'
}
if ($s.SignerCertificate.GetNameInfo([Security.Cryptography.X509Certificates.X509NameType]::SimpleName, $false) -cne $inputData.publisher) {
  throw 'Unexpected Authenticode publisher'
}
@{publisher=$inputData.publisher; certificateSHA1=$s.SignerCertificate.Thumbprint;
  timestampCertificateSHA1=$s.TimeStamperCertificate.Thumbprint} | ConvertTo-Json -Compress
"""
    encoded = base64.b64encode(script.encode("utf-16le")).decode()
    return json.loads(command("powershell.exe", "-NoProfile", "-NonInteractive", "-EncodedCommand", encoded))


def sign_windows(path: Path, signtool: Path, dlib: Path, metadata: Path, publisher: str) -> dict:
    if sys.platform != "win32":
        raise ValueError("Azure signing requires native Windows")
    command(signtool, "sign", "/v", "/fd", "SHA256", "/td", "SHA256",
            "/tr", "http://timestamp.acs.microsoft.com", "/dlib", dlib, "/dmdf", metadata, path)
    return authenticode(path, signtool, publisher)


def fingerprint(value: str) -> str:
    value = value.upper()
    if not re.fullmatch(r"(?:[0-9A-F]{40}|[0-9A-F]{64})", value):
        raise ValueError("an approved full OpenPGP fingerprint is required")
    return value


def verify_detached(path: Path, signature: Path, approved_key: str, *, gpg_home: Path | None = None) -> dict:
    approved_key = fingerprint(approved_key)
    prefix = ["/usr/bin/gpg", "--homedir", str(gpg_home), "--no-options"] if gpg_home else ["gpg"]
    result = subprocess.run([*prefix, "--batch", "--status-fd", "1", "--verify", str(signature), str(path)],
                            check=True, text=True, capture_output=True)
    valid = [line.split() for line in result.stdout.splitlines() if line.startswith("[GNUPG:] VALIDSIG ")]
    if len(valid) != 1 or len(valid[0]) < 11 or valid[0][9] != "8" or (valid[0][2] != approved_key and valid[0][-1] != approved_key):
        raise ValueError("detached signature has an unexpected signing fingerprint")
    return {"keyFingerprint": approved_key, "signingFingerprint": valid[0][2]}


def sign_detached(path: Path, approved_key: str, *, gpg_home: Path | None = None) -> tuple[Path, dict]:
    approved_key = fingerprint(approved_key)
    signature = path.with_name(path.name + ".asc")
    if signature.exists():
        raise ValueError(f"signature already exists: {signature.name}")
    prefix = ["/usr/bin/gpg", "--homedir", gpg_home, "--no-options", "--batch", "--pinentry-mode", "error"] if gpg_home else ["gpg"]
    command(*prefix, "--armor", "--digest-algo", "SHA256", "--local-user", approved_key + ("!" if gpg_home else ""),
            "--output", signature, "--detach-sign", path)
    return signature, verify_detached(path, signature, approved_key, gpg_home=gpg_home)


def sign_rpm(path: Path, approved_key: str) -> None:
    if not sys.platform.startswith("linux"):
        raise ValueError("RPM signing requires the native Linux producer")
    gpg = shutil.which("gpg")
    if not gpg:
        raise ValueError("GnuPG executable is required for RPM signing")
    # Some producers default __gpg to a nonexistent gpg2 compatibility path.
    command("rpmsign", "--define", f"__gpg {gpg}", "--define", f"_gpg_name {fingerprint(approved_key)}", "--addsign", path)


def verify_rpm(path: Path, approved_key: str) -> None:
    # Verify against only the approved public key in a disposable RPM database.
    # Never change the host's trust database or use short key IDs as authority.
    approved_key = fingerprint(approved_key)
    with tempfile.TemporaryDirectory(prefix="bpr-rpm-verification-") as temporary:
        root = Path(temporary)
        key = root / "approved.asc"
        key.write_text(command("gpg", "--batch", "--armor", "--export", approved_key), encoding="utf-8")
        command("rpmkeys", "--dbpath", root / "db", "--import", key)
        output = command("rpmkeys", "--dbpath", root / "db", "--checksig", "--verbose", path)
    if any(word in output.upper() for word in ("NOKEY", "NOT OK", "BAD", "NOTTRUSTED")):
        raise ValueError("RPM signature or digest verification failed")
    keys = command("gpg", "--batch", "--with-colons", "--list-keys", fingerprint(approved_key))
    allowed = {line.split(":")[4].lower() for line in keys.splitlines() if line.startswith(("pub:", "sub:"))}
    observed = re.findall(r"signature.*key ID ([0-9a-f]+): OK", output, re.I)
    if not observed or any(not any(key.endswith(item.lower()) for key in allowed) for item in observed):
        raise ValueError("RPM signature does not identify the approved key")
