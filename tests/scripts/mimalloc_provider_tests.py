#!/usr/bin/env python3
"""Exercise the pinned allocator provider and executable policy offline."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(sys.argv[1]).resolve()
CACHE = Path(sys.argv[2]).resolve()
ARCHIVE = 'mimalloc-3.5.3.tar.gz'


def configure(root, name, text, arguments=(), refusal=None):
    source = root / name
    source.mkdir()
    (source / 'CMakeLists.txt').write_text(
        'cmake_minimum_required(VERSION 4.4.2)\nproject(AllocatorContract C CXX)\n'
        f'include("{ROOT}/cmake/BprMimallocProvider.cmake")\n' + text)
    command = ['cmake', '-S', str(source), '-B', str(source / 'build'), '-G', 'Ninja',
               f'-DCPM_SOURCE_CACHE={CACHE}', '-DBPR_DEPENDENCY_OFFLINE=ON', *arguments]
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
with tempfile.TemporaryDirectory(prefix='bpr-mimalloc-provider-') as directory:
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
'''
    build = configure(root, 'offline-product-graph', graph, ['-DMI_XMALLOC=ON'])
    config = (build / '_bpr/mimalloc-source/mimalloc-3.5.3/include/mimalloc.h')
    config.write_text('editable cached source must never be trusted')
    result = subprocess.run(['cmake', '-S', str(build.parent), '-B', str(build)], text=True, capture_output=True, timeout=180)
    assert result.returncode == 0 and 'MI_MALLOC_VERSION 30503' in config.read_text(), result.stdout + result.stderr
    print('edited-extraction-restored:pass')
print('mimalloc-provider:pass')
