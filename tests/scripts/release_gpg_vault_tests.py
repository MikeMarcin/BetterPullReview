#!/usr/bin/env python3
"""Bounded vault/WSL transport, key identity and polluted native module contracts."""
from __future__ import annotations

import base64
import json
import os
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import MagicMock, patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from contained_temporary_support import ContainedTemporaryDirectory
from skip_report import SkipReportingTestRunner
sys.path.insert(0, str(ROOT / "packaging"))
import release_gpg_vault as vault
import release_signing as signing


def config() -> dict:
    return {"schema": vault.SCHEMA, "approved": True, "keyFingerprint": "A" * 40,
            "keygrip": "B" * 40, "gnupgHome": "/home/tester/.local/share/bpr-release-signing/gnupg",
            "wslDistribution": "Ubuntu", "wslUser": "tester", "windowsPython": "C:/Python/python.exe"}


def listing(value: dict) -> str:
    return ("sec:u:255:22:AAAAAAAAAAAAAAAA:1:4102444800::u:::scSC:::+::ed25519:::0:\n"
            "fpr:::::::::" + value["keyFingerprint"] + ":\n"
            "grp:::::::::" + value["keygrip"] + ":\n"
            "uid:u::::1::ID::BetterPullReview Release Signing::::::::::0:\n")


class VaultTests(unittest.TestCase):
    def test_config_is_exact_approved_and_home_scoped(self):
        self.assertEqual(vault.validate_config(config()), config())
        changes = [{"approved": False}, {"keyFingerprint": "A" * 16}, {"keygrip": "X" * 40},
                   {"wslDistribution": "--exec"}, {"wslUser": "root"},
                   {"gnupgHome": "/home/tester/.gnupg"}, {"gnupgHome": "/home/tester/.local/share/bpr-release-signing/../gnupg"},
                   {"windowsPython": "python.exe"}, {"windowsPython": "//server/python.exe"}, {"command": "arbitrary"}]
        for change in changes:
            with self.subTest(change=change), self.assertRaises(vault.VaultError):
                vault.validate_config(config() | change)

    def test_binding_rejects_wrong_grip_uid_expiry_and_signing_subkey(self):
        value = config()
        good = listing(value)
        vault.validate_key_listing(good, value)
        for output in (good.replace("B" * 40, "C" * 40), good.replace("A" * 40, "D" * 40),
                       good.replace("BetterPullReview Release Signing", "Some other key"),
                       good.replace("4102444800", "1"), good.replace("sec:u:", "sec:r:"),
                       good.replace(":::+::", ":::#::"), good + "ssb:u:255:22:ID:1:4102444800::u:::s:::+:\n"):
            with self.subTest(output=output), self.assertRaises(vault.VaultError):
                vault.validate_key_listing(output, value)

    def test_secret_encoding_rejects_empty_linebreak_nul_and_oversize(self):
        for phrase in ("", "bad\nphrase", "bad\rphrase", "bad\0phrase", "x" * 1281):
            with self.assertRaises(vault.VaultError):
                vault.passphrase_bytes(phrase, "utf-16le")
        value = vault.passphrase_bytes("synthetic-\u03bb-\U0001f512", "utf-16le")
        self.assertEqual(value.decode("utf-16le"), "synthetic-\u03bb-\U0001f512")
        vault.wipe(value)
        self.assertTrue(all(byte == 0 for byte in value))

    def test_wrong_destination_is_refused_before_credential_access(self):
        with patch.object(vault, "wsl_action", side_effect=vault.VaultError("wrong key")), patch.object(vault, "WindowsCredential") as store:
            with self.assertRaises(vault.VaultError):
                vault.unlock_windows(config())
            store.assert_not_called()

    def test_missing_preset_is_refused_before_credential_access(self):
        with patch.object(vault, "wsl_action", return_value={"presetEnabled": False}), patch.object(vault, "WindowsCredential") as store:
            with self.assertRaises(vault.VaultError):
                vault.unlock_windows(config())
            store.assert_not_called()

    def test_missing_credential_never_presets_or_prompts(self):
        with patch.object(vault, "wsl_action", return_value={"presetEnabled": True}) as wsl, patch.object(vault, "WindowsCredential") as store:
            store.return_value.read.side_effect = vault.VaultError("missing")
            with self.assertRaises(vault.VaultError):
                vault.unlock_windows(config())
            self.assertEqual(wsl.call_count, 1)

    def test_unlock_wipes_secret_on_success_and_failure(self):
        for fail in (False, True):
            phrase = bytearray(b"synthetic-not-a-real-credential")
            observed = []
            def action(value, operation, **kwargs):
                if operation == "validate":
                    return {"presetEnabled": True}
                observed.append(bytes(kwargs["phrase"]))
                if fail:
                    raise vault.VaultError("wrong secret")
                return {"unlocked": True}
            with patch.object(vault, "wsl_action", side_effect=action), patch.object(vault, "WindowsCredential") as store:
                store.return_value.read.return_value = phrase
                if fail:
                    with self.assertRaises(vault.VaultError):
                        vault.unlock_windows(config())
                else:
                    self.assertTrue(vault.unlock_windows(config())["unlocked"])
                self.assertTrue(observed == [b"synthetic-not-a-real-credential"])
                self.assertTrue(all(byte == 0 for byte in phrase))

    def test_wsl_secret_is_only_stdin_to_fixed_python_helper(self):
        value = config()
        receipt = {"keyFingerprint": value["keyFingerprint"], "keygrip": value["keygrip"],
                   "gnupgHome": value["gnupgHome"], "passphraseProtected": True,
                   "unlocked": True, "pinentryDisabledProbe": True}
        phrase = bytearray(b"synthetic-stdin-only")
        with patch.dict(os.environ, {"SystemRoot": "C:/Windows"}), patch.object(vault, "run_fixed", side_effect=[b"/mnt/c/repo/packaging/release_gpg_vault.py\n", json.dumps(receipt).encode()]) as command:
            vault.wsl_action(value, "unlock", phrase=phrase)
            args, kwargs = command.call_args
            self.assertEqual(kwargs["data"], phrase)
            self.assertNotIn(phrase.decode(), " ".join(args[0]))
            self.assertIn("/usr/bin/python3", args[0])
            self.assertEqual(args[0][-1], "unlock")
            self.assertEqual(json.loads(base64.b64decode(args[0][-3])), value)

    def test_native_child_failures_never_echo_child_output(self):
        error = subprocess.CompletedProcess([], 1, b"synthetic sensitive child output", b"other sensitive output")
        with patch.object(vault.subprocess, "run", return_value=error), self.assertRaises(vault.VaultError) as caught:
            vault.run_fixed(["fixed"])
        self.assertNotIn("sensitive", str(caught.exception))
        self.assertIn("no interactive fallback", str(caught.exception))

    def test_wrong_passphrase_forgets_cache_and_disables_pinentry(self):
        value = config()
        input_stream = MagicMock()
        input_stream.buffer.read.return_value = b"synthetic-wrong-passphrase"
        calls = []
        def run(args, **kwargs):
            calls.append((args, kwargs))
            if "/usr/bin/gpg" in args:
                raise vault.VaultError("native signing refused")
            return b""
        with patch.object(vault, "validate_local", return_value={"presetEnabled": True}), patch.object(vault.sys, "stdin", input_stream), patch.object(vault, "run_fixed", side_effect=run):
            with self.assertRaisesRegex(vault.VaultError, "stored passphrase.*no interactive fallback"):
                vault.local_action(value, "unlock")
        self.assertEqual(calls[0][0], vault.gpg_args(value, "/usr/lib/gnupg/gpg-preset-passphrase", "--preset", value["keygrip"]))
        self.assertIn("error", calls[1][0])
        self.assertEqual(calls[1][1]["data"], vault.PROBE)
        self.assertEqual(calls[-1][0][-2:], ["--forget", value["keygrip"]])

    def test_linux_package_signer_mismatch_refuses_before_unlock(self):
        with patch.object(vault.sys, "platform", "linux"), patch.object(vault, "load_config", return_value=config()), patch.object(vault, "run_fixed") as command:
            with self.assertRaisesRegex(vault.VaultError, "package signer differs"):
                vault.prepare_linux_signing(Path("/config.json"), "C" * 40)
            command.assert_not_called()

    def test_vault_backed_detached_signing_uses_only_scoped_home_and_no_prompt(self):
        with ContainedTemporaryDirectory() as temporary:
            artifact = Path(temporary) / "sample.txt"
            artifact.write_text("synthetic")
            with patch.object(signing, "command") as command, patch.object(signing, "verify_detached", return_value={}) as verify:
                signing.sign_detached(artifact, "A" * 40, gpg_home=Path("/scoped/gnupg"))
            args = [str(arg) for arg in command.call_args.args]
            self.assertEqual(args[:8], ["/usr/bin/gpg", "--homedir", str(Path("/scoped/gnupg")), "--no-options", "--batch", "--pinentry-mode", "error", "--armor"])
            self.assertIn("A" * 40 + "!", args)
            self.assertEqual(verify.call_args.kwargs, {"gpg_home": Path("/scoped/gnupg")})

    def test_authenticode_import_binds_child_module_manifest(self):
        receipt = {"publisher": "Fixture", "certificateSHA1": "C" * 40, "timestampCertificateSHA1": "D" * 40}
        with patch.object(signing.sys, "platform", "win32"), patch.object(signing, "command", side_effect=["verified", json.dumps(receipt)]) as command:
            signing.authenticode(Path("fixture.exe"), Path("signtool.exe"), "Fixture")
            args = command.call_args.args
            self.assertEqual(args[:4], ("powershell.exe", "-NoProfile", "-NonInteractive", "-EncodedCommand"))
            source = base64.b64decode(args[-1]).decode("utf-16le")
            self.assertIn("Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Security/Microsoft.PowerShell.Security.psd1'", source)
            self.assertLess(source.index("Import-Module"), source.index("Get-AuthenticodeSignature"))
            self.assertIn("$ErrorActionPreference = 'Stop'", source)

    @unittest.skipUnless(sys.platform == "win32", "native Windows PowerShell pollution regression")
    def test_authenticode_ignores_poisoned_psmodulepath(self):
        with ContainedTemporaryDirectory() as temporary:
            root = Path(temporary)
            module = root / "modules/Microsoft.PowerShell.Security"
            module.mkdir(parents=True)
            marker = root / "POISONED.txt"
            script = "[IO.File]::WriteAllText($env:BPR_MODULE_TEST_MARKER,'poisoned'); throw 'POISONED MODULE LOADED'"
            (module / "Microsoft.PowerShell.Security.psm1").write_text("function Get-AuthenticodeSignature { " + script + " }; Export-ModuleMember -Function Get-AuthenticodeSignature")
            (module / "Microsoft.PowerShell.Security.psd1").write_text("@{RootModule='Microsoft.PowerShell.Security.psm1';ModuleVersion='99.0';FunctionsToExport=@('Get-AuthenticodeSignature')}")
            unsigned = root / "unsigned.ps1"
            unsigned.write_text("# deliberately unsigned fixture\n")
            native = Path(os.environ["SystemRoot"]) / "System32/WindowsPowerShell/v1.0/Modules"
            with patch.dict(os.environ, {"PSModulePath": str(root / "modules") + ";" + str(native), "BPR_MODULE_TEST_MARKER": str(marker)}):
                control = subprocess.run(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", "Get-AuthenticodeSignature -LiteralPath $env:BPR_MODULE_TEST_MARKER"], capture_output=True)
                self.assertTrue(marker.exists(), "the fixture must actually poison native autoload")
                marker.unlink()
                original = signing.command
                def command(*args):
                    return "stubbed SignTool verification of unsigned fixture" if str(args[0]) == "fixture-signtool" else original(*args)
                with patch.object(signing, "command", side_effect=command):
                    with self.assertRaises(subprocess.CalledProcessError) as caught:
                        signing.authenticode(unsigned, Path("fixture-signtool"), "Fixture")
                self.assertFalse(marker.exists(), "the inherited module must not execute")
                self.assertIn("Valid publisher signature and timestamp required", caught.exception.stderr)
                self.assertNotIn("POISONED", caught.exception.stderr)


if __name__ == "__main__":
    unittest.main(testRunner=SkipReportingTestRunner)
