#!/usr/bin/env python3
"""Exercise the pinned allocator provider and executable policy offline."""
import os
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(sys.argv[1]).resolve()
CACHE = Path(sys.argv[2]).resolve()
ARCHIVE = 'mimalloc-3.5.3.tar.gz'
sys.path.insert(0, str(ROOT / 'scripts'))
from contained_temporary_support import ContainedTemporaryDirectory


def configure(root, name, text, arguments=(), refusal=None):
    source = root / name
    source.mkdir()
    (source / 'CMakeLists.txt').write_text(
        'cmake_minimum_required(VERSION 4.4.2)\nproject(AllocatorContract C CXX)\n'
        f'include("{ROOT.as_posix()}/cmake/BprMimallocProvider.cmake")\n' + text)
    command = ['cmake', '-S', str(source), '-B', str(source / 'build'), '-G', 'Ninja',
               f'-DCPM_SOURCE_CACHE={CACHE}', '-DBPR_DEPENDENCY_OFFLINE=ON',
               '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', *arguments]
    result = subprocess.run(command, text=True, capture_output=True, timeout=180)
    output = result.stdout + result.stderr
    normalized = " ".join(output.split())
    if refusal:
        assert result.returncode != 0 and refusal in normalized, (name, output)
    else:
        assert result.returncode == 0, (name, output)
    print(f'{name}:pass')
    return source / 'build'


PRODUCT = 'set(BPR_BUILD_QT ON)\nbpr_configure_mimalloc()\n'
with ContainedTemporaryDirectory() as directory:
    root = Path(directory)
    configure(root, 'core', 'bpr_configure_mimalloc()\nif(TARGET mimalloc OR BPR_MIMALLOC_ACTIVE)\nmessage(FATAL_ERROR "core acquired allocator")\nendif()\n')
    configure(root, 'baseline', PRODUCT + 'if(TARGET mimalloc OR BPR_MIMALLOC_ACTIVE)\nmessage(FATAL_ERROR "baseline acquired allocator")\nendif()\n', ['-DBPR_USE_MIMALLOC=OFF'])
    configure(root, 'distribution-baseline', PRODUCT, ['-DBPR_USE_MIMALLOC=OFF', '-DBPR_DISTRIBUTION_BUILD=ON'], 'Distribution builds require BPR_USE_MIMALLOC=ON')
    configure(root, 'sanitizer', PRODUCT + 'if(TARGET mimalloc OR BPR_MIMALLOC_ACTIVE)\nmessage(FATAL_ERROR "instrumentation acquired allocator")\nendif()\n', ['-DCMAKE_CXX_FLAGS=-fsanitize=address'])
    configure(root, 'fuzzer', PRODUCT + 'if(TARGET mimalloc OR BPR_MIMALLOC_ACTIVE)\nmessage(FATAL_ERROR "fuzzer acquired allocator")\nendif()\n', ['-DBPR_BUILD_FUZZERS=ON'])
    configure(root, 'distribution-sanitizer', PRODUCT, ['-DCMAKE_CXX_FLAGS=-fsanitize=address', '-DBPR_DISTRIBUTION_BUILD=ON'], 'cannot use sanitizer/fuzzer instrumentation')
    for target in ('mimalloc', 'mimalloc-static', 'mimalloc-obj', 'BetterPullReview::Mimalloc'):
        configure(root, 'collision-' + target.replace(':', '_'), f'add_library({target} INTERFACE IMPORTED)\n' + PRODUCT, refusal='must be owned by the pinned provider')
    for variable in ('FETCHCONTENT_SOURCE_DIR_MIMALLOC', 'FETCHCONTENT_SOURCE_DIR_BPRMIMALLOC', 'CPM_mimalloc_SOURCE', 'CPM_BprMimalloc_SOURCE'):
        configure(root, variable, PRODUCT, [f'-D{variable}={root}'], 'source substitution is forbidden')
    if os.name == 'nt':
        configure(root, 'static-crt', PRODUCT, ['-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded'], 'requires the dynamic CRT')
        configure(root, 'static-crt-flags', PRODUCT, ['-DCMAKE_CXX_FLAGS=/MT'], 'refuses static CRT compiler flags')
    private_cache = root / 'cache'
    (private_cache / 'cpm').mkdir(parents=True)
    shutil.copyfile(CACHE / 'cpm/CPM-0.43.1.cmake', private_cache / 'cpm/CPM-0.43.1.cmake')
    configure(root, 'missing-offline', PRODUCT, [f'-DCPM_SOURCE_CACHE={private_cache}'], 'Offline mode requires cached mimalloc')
    (private_cache / ARCHIVE).write_bytes(b'unverified bytes')
    configure(root, 'wrong-offline', PRODUCT, [f'-DCPM_SOURCE_CACHE={private_cache}'], 'archive does not match pinned')
    graph = PRODUCT + '''
if(NOT BPR_MIMALLOC_ACTIVE OR NOT TARGET mimalloc)
    message(FATAL_ERROR "allocator absent")
endif()
if(TARGET mimalloc-static OR TARGET mimalloc-obj OR TARGET mimalloc-test-api)
    message(FATAL_ERROR "unused allocator variants")
endif()
file(WRITE "${CMAKE_BINARY_DIR}/main.cpp" "int main() { return 0; }\\n")
foreach(product IN ITEMS app_better_pull_review app_bpr tool_better_pull_review_mcp_context_helper tool_better_pull_review_agent_supervisor tool_bpr_update_helper)
    add_executable(${product} "${CMAKE_BINARY_DIR}/main.cpp")
    bpr_attach_product_allocator(${product})
    get_target_property(enabled ${product} BPR_MIMALLOC_ENABLED)
    if(NOT enabled)
        message(FATAL_ERROR "missing product allocator: ${product}")
    endif()
endforeach()
add_executable(test_baseline "${CMAKE_BINARY_DIR}/main.cpp")
bpr_attach_product_allocator(test_baseline)
get_target_property(enabled test_baseline BPR_MIMALLOC_ENABLED)
if(enabled)
    message(FATAL_ERROR "non-product attached allocator")
endif()
add_executable(bench_measurement "${CMAKE_BINARY_DIR}/main.cpp")
bpr_attach_selected_allocator(bench_measurement)
get_target_property(enabled bench_measurement BPR_MIMALLOC_ENABLED)
if(NOT enabled)
    message(FATAL_ERROR "measurement allocator absent")
endif()
get_property(products GLOBAL PROPERTY BPR_PRODUCT_EXECUTABLES)
if(bench_measurement IN_LIST products)
    message(FATAL_ERROR "measurement became a shipped product")
endif()
'''
    build = configure(root, 'offline-product-graph', graph, ['-DMI_XMALLOC=ON'])
    # Exercise the actual first-configure library, not only its target name.
    # Upstream string cache initialization previously discarded Linux's C++
    # selection, so ordinary throwing new aborted instead of throwing.
    (build.parent / 'oom.cpp').write_text(
        '#include <cstddef>\n#include <limits>\n#include <new>\n'
        'int main() { volatile auto size = std::numeric_limits<std::size_t>::max(); '
        'try { auto* p = ::operator new(size); ::operator delete(p); return 1; } '
        'catch (const std::bad_alloc&) { return 0; } }\n')
    # The original graph has already configured. A separate fresh graph must
    # use these choices before it has any upstream cache entries to reuse.
    fresh = configure(root, 'fresh-allocation-failure', PRODUCT +
        f'add_executable(app_bpr "{(build.parent / "oom.cpp").as_posix()}")\n'
        'bpr_attach_product_allocator(app_bpr)\n',
        ['-DMI_USE_CXX=OFF', '-DMI_OPT_ARCH=ON'])
    subprocess.run(['cmake', '--build', str(fresh), '--target', 'app_bpr'],
                   check=True, capture_output=True, timeout=180)
    image = fresh / ('app_bpr.exe' if os.name == 'nt' else 'app_bpr')
    result = subprocess.run([str(image)], capture_output=True, timeout=30)
    assert result.returncode == 0, ('fresh throwing-new failure semantics', result.returncode, result.stderr)
    import json
    commands = json.loads((fresh / 'compile_commands.json').read_text())
    allocator_commands = [entry.get('command', ' '.join(entry.get('arguments', [])))
                          for entry in commands if 'mimalloc-3.5.3/src/' in entry['file'].replace('\\', '/')]
    assert allocator_commands and all(not any(flag in command.lower() for flag in ('-march=', '-mavx', '/arch:'))
                                      for command in allocator_commands), allocator_commands
    print('fresh-allocation-failure:pass')
    redirect = build / '_bpr/mimalloc-source/mimalloc-3.5.3/bin/mimalloc-redirect.dll'
    redirect_digest = hashlib.sha256(redirect.read_bytes()).hexdigest()
    assert redirect_digest == 'bc3dfdf51fbc80b88cb62cdb6a6784ecc894164bdd4ae7f3885048d870809491'
    assert redirect_digest in (ROOT / 'scripts/verify_pe_release.py').read_text()
    print('pinned-redirect-bytes:pass')
    if os.name == 'nt':
        command = ['cmake', '--build', str(build), '--target', 'app_bpr']
        subprocess.run(command, check=True, capture_output=True, timeout=180)
        producers = [path for path in (build / '_bpr/mimalloc').glob('mimalloc*.dll')
                     if path.name != 'mimalloc-redirect.dll']
        assert len(producers) == 1, producers
        deployed = build / producers[0].name
        expected = hashlib.sha256(producers[0].read_bytes()).digest()
        assert hashlib.sha256(deployed.read_bytes()).digest() == expected
        executable = build / 'app_bpr.exe'
        before = executable.stat().st_mtime_ns
        deployed.write_bytes(b'stale runtime copy')
        subprocess.run(command, check=True, capture_output=True, timeout=180)
        assert executable.stat().st_mtime_ns == before, 'regression control relinked the executable'
        assert hashlib.sha256(deployed.read_bytes()).digest() == expected, 'runtime copy stayed stale without a relink'
        print('runtime-refresh-without-executable-relink:pass')
    config = (build / '_bpr/mimalloc-source/mimalloc-3.5.3/include/mimalloc.h')
    config.write_text('editable cached source must never be trusted')
    result = subprocess.run(['cmake', '-S', str(build.parent), '-B', str(build)], text=True, capture_output=True, timeout=180)
    assert result.returncode == 0 and 'MI_MALLOC_VERSION 30503' in config.read_text(), result.stdout + result.stderr
    print('edited-extraction-restored:pass')
print('mimalloc-provider:pass')
