#!/usr/bin/env python3
"""Disposable macOS ZIP staging course; never touches the installed application."""
from __future__ import annotations
import argparse
import plistlib
import stat
import struct
import subprocess
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory


def archive(path: Path, scenario: str):
    info = {"CFBundleIdentifier": "com.gamecoretech.betterpullreview", "CFBundleShortVersionString": "0.1.3", "CFBundleExecutable": "BetterPullReview", "LSMinimumSystemVersion": "11.0"}
    if scenario.startswith("numbered"):
        info.update(CFBundleVersion="54", BPRReleaseBuild="54", BPRReleaseConfiguration="a" * 64, BPRReleaseCohort="b" * 64)
        if scenario == "numbered-zero":
            info.update(CFBundleVersion="0", BPRReleaseBuild="0")
        if scenario == "numbered-mismatch":
            info["CFBundleVersion"] = "55"
        if scenario == "numbered-missing-seal":
            del info["BPRReleaseCohort"]
    if scenario == "wrong-id":
        info["CFBundleIdentifier"] = "com.example.other"
    if scenario == "wrong-version":
        info["CFBundleShortVersionString"] = "0.1.2"
    if scenario == "future-os":
        info["LSMinimumSystemVersion"] = "999.0"
    def macho(cpu):
        return struct.pack("<8I", 0xFEEDFACF, cpu, 0, 2, 1, 16, 0, 0) + struct.pack("<4I", 0x24, 16, 0x000B0000, 0x000B0000)
    binary = struct.pack(">12I", 0xCAFEBABE, 2, 0x01000007, 3, 48, 48, 0, 0x0100000C, 0, 96, 48, 0) + macho(0x01000007) + macho(0x0100000C)
    if scenario == "wrong-architecture":
        binary = b"not a universal executable"
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as output:
        if scenario == "entry-limit":
            for index in range(50001):
                output.writestr(f"__MACOSX/{index}", b"")
        output.writestr("BetterPullReview.app/Contents/Info.plist", plistlib.dumps(info))
        output.writestr("BetterPullReview.app/Contents/MacOS/BetterPullReview", binary)
        if scenario in ("traversal", "absolute", "wrong-top"):
            name = {"traversal": "../escape", "absolute": "/escape", "wrong-top": "Other.app/file"}[scenario]
            output.writestr(name, b"fixture")
        if scenario in ("symlink-escape", "symlink-write", "device"):
            entry = zipfile.ZipInfo("BetterPullReview.app/Contents/linked")
            entry.create_system = 3
            entry.external_attr = ((stat.S_IFLNK if scenario != "device" else stat.S_IFCHR) | 0o777) << 16
            output.writestr(entry, b"../../../escape" if scenario == "symlink-escape" else b"MacOS")
            if scenario == "symlink-write":
                output.writestr("BetterPullReview.app/Contents/linked/file", b"fixture")
        if scenario == "duplicate":
            output.writestr("BetterPullReview.app/Contents/Info.plist", plistlib.dumps(info))
    if scenario == "expanded-size":
        data = bytearray(path.read_bytes())
        struct.pack_into("<I", data, 22, 2 * 1024**3 + 1)
        struct.pack_into("<I", data, data.index(b"PK\x01\x02") + 24, 2 * 1024**3 + 1)
        path.write_bytes(data)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--signed-archive", type=Path)
    args = parser.parse_args()
    with contained_temporary_directory(prefix="bpr-release-staging-") as temporary:
        base = Path(temporary).resolve()
        installed = base / "installed"
        installed.mkdir(mode=0o700)
        scenarios = ("numbered", "numbered-zero", "numbered-mismatch", "numbered-missing-seal", "unsigned", "traversal", "absolute", "symlink-escape", "symlink-write", "duplicate", "device", "wrong-top", "wrong-id", "wrong-version", "future-os", "wrong-architecture", "digest", "changed-archive", "cancel", "expanded-size", "entry-limit")
        if args.signed_archive:
            scenarios = ("signed",)
            subprocess.run(["/usr/bin/ditto", "-x", "-k", str(args.signed_archive.resolve()), str(installed)], check=True, timeout=90, capture_output=True)
        for scenario in scenarios:
            root = base / scenario
            root.mkdir(mode=0o700)
            path = args.signed_archive.resolve() if args.signed_archive else base / f"{scenario}.zip"
            if not args.signed_archive:
                archive(path, scenario)
            result = subprocess.run([str(args.executable), str(root), str(installed / "BetterPullReview.app"), str(path), scenario, "0.1.3"], capture_output=True, timeout=300)
            print(result.stdout.decode(), end="")
            if result.returncode:
                print(result.stderr.decode(), file=sys.stderr)
                raise SystemExit(result.returncode)
        assert not (base / "escape").exists()
    print("release-update-native-staging:pass")


if __name__ == "__main__":
    main()
