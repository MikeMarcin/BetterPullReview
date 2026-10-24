#!/usr/bin/env python3
"""Prepare an unpublished standalone CLI with exact runtime and source evidence."""
from __future__ import annotations

import argparse
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys
import tarfile

import package_runtime as runtime


def deploy_windows(info: dict, root: Path) -> None:
    qt = Path(info['qt_root'])
    binary_root = root / 'bin'
    plugin = binary_root / 'plugins/tls/qschannelbackend.dll'
    plugin.parent.mkdir(parents=True)
    shutil.copy2(qt / 'plugins/tls/qschannelbackend.dll', plugin)
    (binary_root / 'qt.conf').write_text('[Paths]\nPlugins=plugins\n', encoding='utf-8')
    pending = list(binary_root.rglob('*.dll')) + list(binary_root.glob('*.exe'))
    processed = set()
    while pending:
        binary = pending.pop()
        if binary in processed:
            continue
        processed.add(binary)
        for name in re.findall(r'^\s+([\w.-]+\.dll)\s*$', runtime.run('dumpbin', '/DEPENDENTS', binary), re.M | re.I):
            destination = binary_root / name
            if destination.exists():
                pending.append(destination)
                continue
            if name.lower() in {'qt6core.dll', 'qt6network.dll'}:
                source = qt / 'bin' / name
            elif runtime.CRT.fullmatch(name):
                redist = os.environ.get('VCToolsRedistDir')
                if not redist:
                    raise ValueError('native MSVC redistribution directory required')
                sources = list(Path(redist).glob('x64/Microsoft.VC*.CRT/' + name))
                if len(sources) != 1:
                    raise ValueError('ambiguous or missing CRT: ' + name)
                source = sources[0]
            elif name.lower().startswith(('api-ms-win-', 'ext-ms-win-')) or (Path(os.environ['SystemRoot']) / 'System32' / name).is_file():
                continue
            else:
                raise ValueError('undeclared standalone CLI dependency: ' + name)
            shutil.copy2(source, destination)
            pending.append(destination)


def deploy_unix(build: Path, info: dict, root: Path) -> None:
    if info['platform'] == 'Darwin':
        for path in [root / 'bin/bpr', root / 'bin/better_pull_review_agent_supervisor',
                     *(root / name for name in info['private_libraries'])]:
            runtime.run('codesign', '--force', '--sign', '-', '--timestamp=none', path)
        return
    runtime.prepare_linux_icu(build, info)
    producers, receipt = runtime.linux_icu_producers(build, info)
    for name, path in producers.items():
        shutil.copy2(path, root / 'lib' / name)
    for path in root.rglob('*'):
        if not path.is_file() or path.read_bytes()[:4] != b'\x7fELF':
            continue
        relative = os.path.relpath(root / 'lib', path.parent)
        runtime.run('patchelf', '--set-rpath', '$ORIGIN' if relative == '.' else '$ORIGIN/' + relative, path)
    runtime.verify_linux_qt_companions(build, info, root / 'lib')
    runtime.write_json(runtime.legal_root(root, info) / 'icu-runtime-provenance.json', receipt)


def prepare(build: Path, output: Path) -> dict:
    info = runtime.inputs(build, tour_cli=True)
    if runtime.run('git', '-C', runtime.ROOT, 'status', '--porcelain').strip():
        raise ValueError('commit source before standalone CLI packaging')
    revision = runtime.run('git', '-C', runtime.ROOT, 'rev-parse', 'HEAD').strip()
    output.mkdir(parents=True, exist_ok=False)
    root = output / 'runtime'
    subprocess.run(['cmake', '--build', str(build), '--target', 'app_bpr', 'tool_better_pull_review_agent_supervisor'], check=True)
    subprocess.run(['cmake', '--install', str(build), '--component', 'TourCLI', '--prefix', str(root)], check=True)
    archive = output / runtime.source_asset_name(info)
    # The existing legal producer binds the CLI subset of dependencies and actual
    # Core/Network files. No source ZIP is embedded in the installed prefix.
    if info['platform'] == 'Windows':
        deploy_windows(info, root)
    runtime.notices(build, info, root, archive)
    if info['platform'] != 'Windows':
        deploy_unix(build, info, root)
    runtime.audit(build, info, root, root / 'sbom.spdx.json', archive)
    if runtime.run('git', '-C', runtime.ROOT, 'rev-parse', 'HEAD').strip() != revision or runtime.run('git', '-C', runtime.ROOT, 'status', '--porcelain').strip():
        raise ValueError('source changed during standalone CLI preparation')
    sys.path.insert(0, str(runtime.ROOT / 'scripts'))
    import verify_release_package as verifier
    evidence = verifier.audit_tour_cli(build, root, root / 'sbom.spdx.json', revision, source_archive=archive)
    name = f'BetterPullReview-{info["version"]}-{info["platform"].lower()}-tour-cli.tar.gz'
    payload = output / name
    with tarfile.open(payload, 'w:gz') as stream:
        stream.add(root, arcname='BetterPullReview')
    runtime.write_json(output / 'preparation.json', {'schema': 'bpr.tour-cli-preparation.v1', 'sourceRevision': revision,
                       'archive': payload.name, 'sha256': runtime.digest(payload), 'sourceAsset': archive.name,
                       'sourceSHA256': runtime.digest(archive), 'audit': evidence})
    return evidence


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    evidence = prepare(args.build.resolve(strict=True), args.output.resolve())
    print('TOUR_CLI_PREPARED', evidence)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        raise SystemExit('standalone CLI preparation refused: ' + str(error)) from error
