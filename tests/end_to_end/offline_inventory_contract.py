"""BPR-0969 inventory: actual restarts, private stores, synthetic vault in pipes."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from bpr_narrow_owner_private import make_root_private
from bpr_store_connection import store_connection
from scripts.contained_temporary_support import contained_temporary_directory


def retained_objects(root):
    return {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in root.rglob("*.pack") if "objects" in path.parts}


def retained_epochs(root):
    catalogs = list(root.rglob("epoch-catalog.sqlite3"))
    if len(catalogs) != 1:
        raise SystemExit("expected one private retained epoch catalog")
    with store_connection(catalogs[0].as_uri() + "?mode=ro", uri=True) as connection:
        authority = connection.execute(
            "SELECT incarnation, cache_scope, profile_id, profile_revision, host_id, "
            "host_authority, account_id, access_mode, repository_id, object_format "
            "FROM repository_scope ORDER BY incarnation, cache_scope").fetchall()
        scope = connection.execute(
            "SELECT incarnation, pr_scope, cache_scope, viewer_id, access_mode, "
            "pull_request_id, pull_request_number FROM pull_request_scope "
            "ORDER BY incarnation, pr_scope").fetchall()
        epochs = connection.execute(
            "SELECT incarnation, pr_scope, ordinal, material_base_algorithm, "
            "material_base_digest, material_head_algorithm, material_head_digest "
            "FROM epoch ORDER BY incarnation, pr_scope, ordinal").fetchall()
        anchors = connection.execute(
            "SELECT incarnation, pr_scope, ordinal, endpoint, object_algorithm, "
            "object_digest, ref_format, availability FROM epoch_anchor "
            "ORDER BY incarnation, pr_scope, ordinal, endpoint").fetchall()
    if not authority or not scope or not epochs or len(anchors) != 2 * len(epochs):
        raise SystemExit("retained epochs lack authority or exact endpoint anchors")
    expected_base = bytes.fromhex("a61a212aa0484929bc5f20a26e9a7c1c6541c726")
    expected_head = bytes.fromhex("3b60c8324eee117ce72e6446fe640d85a60b37a2")
    # Material digests identify trees; the endpoint anchors identify the commits
    # published by the accepted snapshot and selected comparison.
    if any(row[5] != {1: expected_base, 2: expected_head}[row[3]] for row in anchors):
        raise SystemExit("retained epoch anchors differ from accepted source revision")
    return authority, scope, epochs, anchors


def legacy_overview_payload(payload, keep_catalogue=False):
    # Original payload prefix: key version + fingerprint bytes encoded as u64,
    # three numeric identities, locator strings, PR number/repository/PR ids,
    # seven text fields, then four overview scalars. No catalogue extension.
    offset = 8 * (1 + 32 + 3)
    for _ in range(2):
        size = int.from_bytes(payload[offset:offset + 8], "big")
        offset += 8 + size
    offset += 3 * 8
    for _ in range(7):
        size = int.from_bytes(payload[offset:offset + 8], "big")
        offset += 8 + size
    offset += 4 * 8
    if not 0 < offset < len(payload):
        raise SystemExit("catalogue fixture lacks versioned extension")
    if keep_catalogue:
        version = int.from_bytes(payload[offset:offset + 8], "big")
        available = int.from_bytes(payload[offset + 8:offset + 16], "big")
        offset += 16
        if version != 2 or available not in (0, 1):
            raise SystemExit("invalid catalogue extension in safe fixture")
        if available:
            count = int.from_bytes(payload[offset:offset + 8], "big")
            offset += 8
            if count > 1024:
                raise SystemExit("fixture catalogue count exceeded bound")
            for _ in range(count):
                size = int.from_bytes(payload[offset:offset + 8], "big")
                offset += 8 + size
                previous = int.from_bytes(payload[offset:offset + 8], "big")
                offset += 8
                if previous:
                    size = int.from_bytes(payload[offset:offset + 8], "big")
                    offset += 8 + size
                offset += 3 * 8
        if not 0 < offset < len(payload):
            raise SystemExit("safe fixture lacks thread extension")
    return payload[:offset]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True)
    parser.add_argument("--identity", action="store_true")
    arguments = parser.parse_args()
    with contained_temporary_directory(prefix="bpr-0969-offline-") as temporary:
        parent = Path(temporary).resolve()
        os.chmod(parent, 0o700)
        make_root_private(parent)
        root = parent / "data"
        make_root_private(root)
        vault = b""
        objects = None
        catalog = None
        phases = ("seed", "restart-read", "restart-offline", "legacy") if arguments.identity else ("seed", "restart-read", "restart-mismatched-source", "restart-missing-source", "restart-catalogue", "restart-legacy", "restart-corrupt", "restart-offline")
        for phase in phases:
            hidden_packs = []
            if phase == "restart-missing-source":
                for pack in root.rglob("*.pack"):
                    hidden = pack.with_suffix(".offline-fixture-held")
                    pack.rename(hidden)
                    hidden_packs.append((pack, hidden))
            if phase == "restart-mismatched-source":
                databases = list(root.rglob("review-overviews.sqlite3"))
                if len(databases) != 1:
                    raise SystemExit("expected private overview fixture")
                with store_connection(databases[0]) as connection:
                    for sequence, original in connection.execute("SELECT sequence,payload FROM overviews").fetchall():
                        payload = bytearray(original)
                        offset = 8 * (1 + 32 + 3)
                        for _ in range(2):
                            offset += 8 + int.from_bytes(payload[offset:offset + 8], "big")
                        offset += 3 * 8
                        for digit in (b"1", b"2"):
                            size = int.from_bytes(payload[offset:offset + 8], "big")
                            if size != 40:
                                raise SystemExit("expected fixture SHA-1 endpoint")
                            offset += 8
                            payload[offset:offset + size] = digit * size
                            offset += size
                        connection.execute("UPDATE overviews SET payload=?,digest=? WHERE sequence=?",
                                           (payload, hashlib.sha256(payload).digest(), sequence))
            if phase in ("restart-catalogue", "restart-legacy"):
                databases = list(root.rglob("review-overviews.sqlite3"))
                if len(databases) != 1:
                    raise SystemExit("expected one private overview database")
                with store_connection(databases[0]) as connection:
                    rows = connection.execute("SELECT sequence,payload FROM overviews").fetchall()
                    for sequence, payload in rows:
                        legacy = legacy_overview_payload(payload, keep_catalogue=phase == "restart-catalogue")
                        connection.execute("UPDATE overviews SET payload=?,digest=? WHERE sequence=?",
                                           (legacy, hashlib.sha256(legacy).digest(), sequence))
            if phase == "restart-corrupt":
                databases = list(root.rglob("review-overviews.sqlite3"))
                if len(databases) != 1:
                    raise SystemExit("expected one private overview database")
                with store_connection(databases[0]) as connection:
                    connection.execute("UPDATE overviews SET digest=zeroblob(32)")
            if phase == "legacy":
                databases = list(root.rglob("window-restoration.sqlite3"))
                if len(databases) != 1:
                    raise SystemExit("expected one profile restoration store")
                with store_connection(databases[0]) as connection:
                    # Version four predates both numeric account ownership and Draft.
                    # Keeping a newer column makes the migration correctly refuse
                    # the fixture's mismatched legacy shape.
                    connection.executescript("ALTER TABLE recent_reviews DROP COLUMN draft; "
                                             "ALTER TABLE restoration_records DROP COLUMN account; "
                                             "ALTER TABLE recent_reviews DROP COLUMN account; "
                                             "PRAGMA user_version=4;")
            if arguments.identity:
                phase = "identity-" + phase
            result = subprocess.run([arguments.application, phase, str(root)], input=vault,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    timeout=100, check=False)
            diagnostic = result.stderr.decode("utf-8", errors="replace")
            sys.stderr.write(diagnostic)
            # stdout is only synthetic vault material, never diagnostics.
            if result.returncode or f"offline-inventory:{phase}:pass" not in diagnostic:
                raise SystemExit(f"{phase} failed ({result.returncode})")
            if len(result.stdout) > 2048:
                raise SystemExit("vault output exceeded bound")
            vault = result.stdout
            if arguments.identity:
                databases = list(root.rglob("window-restoration.sqlite3"))
                if len(databases) != 1:
                    raise SystemExit("expected one private Recent database")
                with store_connection(databases[0].as_uri() + "?mode=ro", uri=True) as connection:
                    recent = connection.execute("SELECT account,number,title,hex(position) FROM recent_reviews ORDER BY ordinal").fetchall()
                expected = {("910001", "17"), ("910002", "18")} if phase != "identity-legacy" else {("910001", "17"), ("0", "17"), ("0", "18")}
                if {(row[0], row[1]) for row in recent} != expected or any(not row[3] for row in recent):
                    raise SystemExit("Recent persistence lost exact account/destination/reading identity")
            for pack, hidden in hidden_packs:
                if pack.exists():
                    raise SystemExit("historical reading unexpectedly fetched missing source")
                hidden.rename(pack)
            current = retained_objects(root)
            if not current:
                raise SystemExit("no retained Git pack survived orderly process exit")
            if objects is not None and current != objects:
                raise SystemExit("offline restart changed retained source packs")
            objects = current
            current_catalog = retained_epochs(root)
            if catalog is not None and current_catalog != catalog:
                raise SystemExit("restart changed retained authority, revision or anchors")
            catalog = current_catalog
            print(f"phase={phase} retained_packs={len(current)} "
                  f"retained_epochs={len(catalog[2])} retained_anchors={len(catalog[3])} "
                  "unchanged=True", flush=True)
    print("OFFLINE_INVENTORY_OK")


if __name__ == "__main__":
    main()
