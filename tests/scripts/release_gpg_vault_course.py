#!/usr/bin/env python3
"""Opt-in native Windows/WSL vault course using only a disposable encrypted key.

The supplied configuration contributes the producer/user/tool routing only.
This course never reads or overwrites the approved key's Windows credential.
"""
from __future__ import annotations

import argparse
import ctypes
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "packaging"))
import release_gpg_vault as vault

SETUP = r'''
import json,os,pathlib,subprocess,sys,tempfile
base=pathlib.Path.home()/'.local/share/bpr-release-signing'
root=pathlib.Path(tempfile.mkdtemp(prefix='vault-course-',dir=base))
home=root/'gnupg';home.mkdir(mode=0o700)
(home/'gpg-agent.conf').write_text('pinentry-program /usr/bin/pinentry-curses\ndefault-cache-ttl 600\nmax-cache-ttl 7200\nno-allow-external-cache\nallow-preset-passphrase\n')
(home/'gpg-agent.conf').chmod(0o600)
phrase=sys.stdin.buffer.read(7680)
cmd=['/usr/bin/gpg','--homedir',str(home),'--no-options','--batch']
result=subprocess.run(cmd+['--pinentry-mode','loopback','--passphrase-fd','0','--quick-generate-key','BetterPullReview Release Signing','ed25519','sign','1d'],input=phrase,capture_output=True)
if result.returncode: raise SystemExit('synthetic key generation failed')
rows=subprocess.run(cmd+['--with-colons','--with-keygrip','--list-secret-keys'],check=True,capture_output=True,text=True).stdout.splitlines()
fields=[row.split(':') for row in rows]
fpr=next(row[9] for row in fields if row[0]=='fpr');grip=next(row[9] for row in fields if row[0]=='grp')
subprocess.run(['/usr/bin/gpgconf','--homedir',str(home),'--kill','gpg-agent'],check=True,capture_output=True)
print(json.dumps({'gnupgHome':str(home),'keyFingerprint':fpr,'keygrip':grip}))
'''

SIGN = r'''
import json,pathlib,subprocess,sys
sys.path.insert(0,sys.argv[1]+'/packaging')
import release_gpg_vault as v,release_signing as s
configpath=pathlib.Path(sys.argv[2]);c=v.prepare_linux_signing(configpath,None)
root=pathlib.Path(c['gnupgHome']).parent
sample=root/'sample.txt';sample.write_text('synthetic vault course content\n')
sig,receipt=s.sign_detached(sample,c['keyFingerprint'],gpg_home=pathlib.Path(c['gnupgHome']))
public=root/'public';public.mkdir(mode=0o700)
key=v.run_fixed(v.gpg_args(c,'/usr/bin/gpg','--no-options','--batch','--export',c['keyFingerprint']))
v.run_fixed(['/usr/bin/gpg','--homedir',str(public),'--no-options','--batch','--import'],data=key)
s.verify_detached(sample,sig,c['keyFingerprint'],gpg_home=public)
sample.write_text('tampered content\n')
try:s.verify_detached(sample,sig,c['keyFingerprint'],gpg_home=public)
except (ValueError,subprocess.CalledProcessError):pass
else:raise SystemExit('tampered sample accepted')
v.run_fixed(['/usr/bin/gpgconf','--homedir',str(public),'--kill','all'])
print(json.dumps({'linuxInteropUnlock':True,'publicOnlyVerification':True,'tamperRejected':True}))
'''

CLEANUP = r'''
import pathlib,shutil,subprocess,sys
home=pathlib.Path(sys.argv[1]);root=home.parent
base=pathlib.Path.home()/'.local/share/bpr-release-signing'
if (root.parent!=base or not root.name.startswith('vault-course-') or root.resolve()!=root or home.name!='gnupg'):
 raise SystemExit('synthetic cleanup root refused')
subprocess.run(['/usr/bin/gpgconf','--homedir',str(home),'--kill','all'],check=True,capture_output=True)
shutil.rmtree(root)
print('synthetic-root-removed')
'''


def require(value: bool, message: str) -> None:
    if not value:
        raise vault.VaultError(message)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, required=True, help="absolute nonsecret producer configuration")
    args = parser.parse_args()
    require(sys.platform == "win32", "native course requires Windows")
    routing = vault.load_config(args.config)
    wsl = str(Path(os.environ["SystemRoot"]) / "System32/wsl.exe")
    prefix = [wsl, "--distribution", routing["wslDistribution"], "--user", routing["wslUser"], "--exec"]
    phrase = secrets.token_urlsafe(32) + "-\u03bb-\U0001f512"
    encoded = vault.passphrase_bytes(phrase, "utf-8")
    temporary_home = None
    stored = False
    store = None
    report = {}
    try:
        with tempfile.TemporaryDirectory(prefix="bpr-vault-native-course-") as temporary:
            root = Path(temporary)
            scripts = {}
            for name, source in (("setup", SETUP), ("sign", SIGN), ("cleanup", CLEANUP)):
                path = root / (name + ".py")
                path.write_text(source, encoding="utf-8")
                scripts[name] = vault.run_fixed(prefix + ["/usr/bin/wslpath", "-u", str(path)]).decode().strip()
            info = json.loads(vault.run_fixed(prefix + ["/usr/bin/python3", scripts["setup"]], data=encoded))
            temporary_home = info["gnupgHome"]
            binding = vault.validate_config(routing | info)
            path = root / "synthetic-vault.json"
            path.write_text(json.dumps(binding), encoding="utf-8")
            helper = str(ROOT / "packaging/release_gpg_vault.py")
            command = [sys.executable, helper, "unlock", "--config", str(path)]
            # Destination checks must refuse a nonprivate home before vault access.
            vault.run_fixed(prefix + ["/usr/bin/chmod", "0755", temporary_home])
            try:
                unsafe = subprocess.run(command, capture_output=True)
                require(unsafe.returncode != 0, "nonprivate synthetic GPG home accepted")
                report["nonprivateGpgHomeRejected"] = True
            finally:
                vault.run_fixed(prefix + ["/usr/bin/chmod", "0700", temporary_home])
            store = vault.WindowsCredential(binding)
            try:
                store.read(metadata_only=True)
            except vault.VaultError as error:
                require("is missing;" in str(error), "unexpected synthetic credential read refusal")
            else:
                raise vault.VaultError("synthetic credential name collided; no overwrite performed")
            missing = subprocess.run(command, capture_output=True)
            require(missing.returncode != 0 and b"is missing;" in missing.stderr, "missing synthetic credential was not refused")
            report["missingCredentialRejected"] = True
            store.write("synthetic-wrong-passphrase")
            stored = True
            wrong = subprocess.run(command, capture_output=True)
            require(wrong.returncode != 0 and b"no interactive fallback" in wrong.stderr, "wrong synthetic passphrase was not refused")
            report["wrongPassphraseRejected"] = True
            cache_check = subprocess.run(prefix + vault.gpg_args(binding, "/usr/bin/gpg", "--no-options", "--batch", "--pinentry-mode", "error",
                                                               "--yes", "--local-user", binding["keyFingerprint"] + "!", "--output", "/dev/null", "--detach-sign"),
                                         input=vault.PROBE, capture_output=True)
            require(cache_check.returncode != 0, "failed unlock left a usable signing cache")
            report["failedUnlockCacheCleared"] = True
            store.write(phrase)
            check = store.read()
            try:
                require(check == encoded, "native UTF-16LE/UTF-8 credential round-trip failed")
            finally:
                vault.wipe(check)
            report["nativeUnicodeCredentialRoundTrip"] = True
            # Wrong metadata must be rejected without passing a blob to WSL.
            blob = vault.passphrase_bytes(phrase, "utf-16le")
            try:
                buffer = (ctypes.c_ubyte * len(blob)).from_buffer(blob)
                credential = vault.Credential(Type=1, TargetName=store.target, UserName="wrong-user", Persist=2,
                                              CredentialBlobSize=len(blob), CredentialBlob=buffer)
                require(bool(store.api.CredWriteW(ctypes.byref(credential), 0)), "synthetic metadata fixture write failed")
            finally:
                vault.wipe(blob)
            metadata = subprocess.run(command, capture_output=True)
            require(metadata.returncode != 0 and b"binding or encoding is invalid" in metadata.stderr, "wrong credential metadata accepted")
            report["wrongCredentialMetadataRejected"] = True
            store.write(phrase)
            vault.run_fixed(prefix + vault.gpg_args(binding, "/usr/bin/gpgconf", "--kill", "gpg-agent"))
            unlocked = json.loads(vault.run_fixed(command))
            require(unlocked.get("unlocked") is True and unlocked.get("pinentryDisabledProbe") is True,
                    "cold synthetic agent unlock failed")
            report["coldAgentUnlockPinentryDisabled"] = True
            # Repeat from the Linux public consumer through Windows interop.
            linux_root = vault.run_fixed(prefix + ["/usr/bin/wslpath", "-u", str(ROOT)]).decode().strip()
            linux_config = vault.run_fixed(prefix + ["/usr/bin/wslpath", "-u", str(path)]).decode().strip()
            report.update(json.loads(vault.run_fixed(prefix + ["/usr/bin/python3", scripts["sign"], linux_root, linux_config])))
            vault.run_fixed(prefix + ["/usr/bin/python3", scripts["cleanup"], temporary_home])
            temporary_home = None
    finally:
        vault.wipe(encoded)
        del phrase
        if stored and store is not None:
            store.api.CredDeleteW.argtypes = [ctypes.c_wchar_p, ctypes.c_ulong, ctypes.c_ulong]
            store.api.CredDeleteW.restype = ctypes.c_int
            require(bool(store.api.CredDeleteW(store.target, 1, 0)), "synthetic exact-credential cleanup failed")
            try:
                store.read(metadata_only=True)
            except vault.VaultError as error:
                require("is missing;" in str(error), "synthetic credential cleanup could not be confirmed")
            else:
                raise vault.VaultError("synthetic credential remained after cleanup")
            report["exactCredentialRemoved"] = True
        if temporary_home is not None:
            vault.run_fixed(prefix + ["/usr/bin/python3", "-c", CLEANUP, temporary_home])
    report["schema"] = "bpr.release-gpg-vault-course.v1"
    report["realCredentialAccessed"] = False
    report["releaseSigned"] = False
    print(json.dumps(report, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except vault.VaultError as error:
        print("synthetic vault course refused: " + str(error), file=sys.stderr)
        raise SystemExit(1) from None
