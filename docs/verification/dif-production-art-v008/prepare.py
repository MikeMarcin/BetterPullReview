#!/usr/bin/env python3
"""Rebuild the d10 Dif structural candidate and its review previews."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

from PIL import Image


EVIDENCE_ROOT = Path(__file__).resolve().parent
REPOSITORY_ROOT = EVIDENCE_ROOT.parents[2]
INTAKE_MANIFEST = EVIDENCE_ROOT / "intake-manifest.json"
CANDIDATE_MANIFEST = EVIDENCE_ROOT / "candidate" / "dif-sprite-manifest.json"
DIF_ASSETS_PATH = REPOSITORY_ROOT / "scripts" / "dif_assets.py"


def load_dif_assets():
    spec = importlib.util.spec_from_file_location("bpr_d10_dif_assets", DIF_ASSETS_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {DIF_ASSETS_PATH}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def require_empty(path: Path) -> None:
    if path.exists() and any(path.iterdir()):
        raise RuntimeError(f"output directory is not empty: {path}")
    path.mkdir(parents=True, exist_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    output_root = args.out.resolve()
    require_empty(output_root)

    dif = load_dif_assets()
    intake = json.loads(INTAKE_MANIFEST.read_text(encoding="utf-8"))
    expected = {
        item["path"]: item for item in intake["files"] if item["path"].endswith(".png")
    }
    assets = output_root / "assets"
    preview = output_root / "preview"
    assets.mkdir()
    (assets / "README.md").write_text(
        "# d10 structural candidate\n\n"
        "This disposable compiler harness does not establish art or derivative rights.\n",
        encoding="utf-8",
    )

    source_files: list[dict[str, str]] = []

    def write_source(incoming_relative: str, source_relative: str) -> None:
        source = EVIDENCE_ROOT / "incoming" / incoming_relative
        raw = source.read_bytes()
        record = expected[incoming_relative]
        if sha256(raw) != record["sha256"]:
            raise RuntimeError(f"incoming digest changed: {incoming_relative}")
        image = Image.open(source).convert("RGBA")
        rgba = dif.RgbaImage(image.width, image.height, image.tobytes())
        canonical = dif.encode_png(rgba)
        if sha256(canonical) != record["canonical_png_sha256"]:
            raise RuntimeError(f"canonical digest changed: {incoming_relative}")
        destination = assets / source_relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(canonical)
        source_files.append({"path": source_relative, "sha256": sha256(canonical)})

    write_source("poses/rest/frame-000.png", "source/dif-neutral-master.png")
    contract = json.loads(
        (
            REPOSITORY_ROOT
            / "docs/rfcs/assets/bpr-dif-integration-v1/specs/CLIP-CONTRACT.json"
        ).read_text(encoding="utf-8")
    )
    for clip in contract["clips"]:
        clip_id = clip["id"]
        directory = clip_id.replace("_", "-")
        for index in range(clip["frame_count"]):
            name = f"frame-{index:03d}.png"
            write_source(
                f"poses/{clip_id}/{name}", f"source/{directory}/{name}"
            )

    manifest_bytes = CANDIDATE_MANIFEST.read_bytes()
    (assets / "dif-sprite-manifest.json").write_bytes(manifest_bytes)
    production_provenance = json.loads(
        (REPOSITORY_ROOT / "src/adapters/qt/assets/dif/provenance.json").read_text(
            encoding="utf-8"
        )
    )
    production_provenance["creation"] = {
        "creator": "External generator operator (identity not supplied)",
        "method_record": "docs/verification/dif-production-art-v008/README.md",
        "source_authority": "checked_in_source_bytes",
        "tools": [
            "external image generator (provider, model, seed, and version unknown)",
            "supplied catchlight removal described by incoming/eye-fix-log.json",
            "scripts/dif_assets.py canonical PNG codec",
        ],
    }
    production_provenance["rights"] = {
        "basis": "STRUCTURAL HARNESS ONLY; exact external derivative disposition pending",
        "derivative_scope": "No production authority established by this file",
        "distribution_scope": "Disposable local structural evaluation only",
        "foreign_identity_exclusion": ["AllTheThings", "Mel", "Nib"],
        "notice_disposition": "Pending attended exact-byte derivative decision",
        "notice_required": False,
    }
    production_provenance["source_files"] = sorted(
        source_files, key=lambda item: item["path"]
    )
    (assets / "provenance.json").write_bytes(
        dif.canonical_json_bytes(production_provenance)
    )

    dif._write_products(dif.generated_products(assets), assets / "generated")
    dif.validate_assets(assets)
    dif.verify_assets(assets)
    dif.render_previews(assets, preview)
    print(f"DIF_D10_CANDIDATE_REPRODUCIBLE {output_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
