# [[RFC-BPR-0082]] [[BPR-1466]]. The opt-in diff-engine evaluation: pinned
# candidate sources, the components that wrap them, the hook that hands them to
# the ordinary entry points in this build only, and the measurement harness.
#
# Nothing here is part of a shipping configuration. BPR_DIFF_ENGINE_EVALUATION
# is off by default; when it is on, the core is compiled with the hook
# declaration and every executable of this build tree links the hook, so the
# existing suites run against whichever candidate the selection names. No
# candidate enters third_party/ or packaging/dependencies.json.

include("${PROJECT_SOURCE_DIR}/cmake/BprRe2Provider.cmake")

set(bpr_eval_stage "${CMAKE_BINARY_DIR}/_bpr/diff-evaluation")

# Each archive is verified by SHA-256 and size and re-extracted under the build
# tree, never trusted as an editable cached source (the RE2 provider's pattern).
bpr_fetch_verified_archive(
    "Git 2.56.0"
    "https://mirrors.edge.kernel.org/pub/software/scm/git/git-2.56.0.tar.xz"
    "${CPM_SOURCE_CACHE}/git-2.56.0.tar.xz"
    "26c56c296b38c0695b26fa95f475f1d01704d2d38e73465ca30b0b2f5dc789d3"
    8489656
    "${bpr_eval_stage}/git-2.56.0/git-2.56.0/COPYING"
    "5b2198d1645f767585e8a88ac0499b04472164c0d2da22e75ecf97ef443ab32e"
    "${bpr_eval_stage}/git-2.56.0")
bpr_fetch_verified_archive(
    "Git 2.39.5"
    "https://mirrors.edge.kernel.org/pub/software/scm/git/git-2.39.5.tar.xz"
    "${CPM_SOURCE_CACHE}/git-2.39.5.tar.xz"
    "c58da92c378df4a986ca33266897a7397e86c22ee266a284d8c2432c39066b59"
    7171400
    "${bpr_eval_stage}/git-2.39.5/git-2.39.5/COPYING"
    "5b2198d1645f767585e8a88ac0499b04472164c0d2da22e75ecf97ef443ab32e"
    "${bpr_eval_stage}/git-2.39.5")
bpr_fetch_verified_archive(
    "dtl 1.21"
    "https://codeload.github.com/cubicdaiya/dtl/tar.gz/refs/tags/v1.21"
    "${CPM_SOURCE_CACHE}/dtl-1.21.tar.gz"
    "90ed2dbf4e6d687737fe25f118bbcb6aed778cecc3f2115d191a032bf8643dbd"
    26321
    "${bpr_eval_stage}/dtl/dtl-1.21/COPYING"
    "7331165008d2c296d7cd0fc65ed752cd169fd7e4604a6413f9a1eef8c0e14fec"
    "${bpr_eval_stage}/dtl")
bpr_fetch_verified_archive(
    "rapidfuzz-cpp 3.3.4"
    "https://codeload.github.com/rapidfuzz/rapidfuzz-cpp/tar.gz/refs/tags/v3.3.4"
    "${CPM_SOURCE_CACHE}/rapidfuzz-cpp-3.3.4.tar.gz"
    "a0dd2ef361cac165e12076696e7c7e8d069a2908abd9599ad4bd190de33f9881"
    314599
    "${bpr_eval_stage}/rapidfuzz/rapidfuzz-cpp-3.3.4/LICENSE"
    "529ac8f069bd6aa633a2da2bea8de4299f43dec428aa407585f2b460dd98944f"
    "${bpr_eval_stage}/rapidfuzz")
bpr_fetch_verified_archive(
    "diff-match-patch-cpp-stl ba3bbe3"
    "https://codeload.github.com/leutloff/diff-match-patch-cpp-stl/tar.gz/ba3bbe3cdba5d461e241b756a50085c3602a3b5a"
    "${CPM_SOURCE_CACHE}/diff-match-patch-cpp-stl-ba3bbe3.tar.gz"
    "11b071222bcb441dcd40bd3fe6189ca0a323708a0650f3671e53194737a7ebc0"
    49024
    "${bpr_eval_stage}/dmp/diff-match-patch-cpp-stl-ba3bbe3cdba5d461e241b756a50085c3602a3b5a/LICENSE"
    "73ba74dfaa520b49a401b5d21459a8523a146f3b7518a833eea5efa85130bf68"
    "${bpr_eval_stage}/dmp")

# xdiff's own licence is LGPL-2.1-or-later; xhistogram.c carries the Eclipse
# Distribution License notice of its JGit origin. Both are pinned.
file(SHA256 "${bpr_eval_stage}/git-2.56.0/git-2.56.0/LGPL-2.1" bpr_eval_lgpl)
if(NOT bpr_eval_lgpl STREQUAL "1922f45d2c49e390032c9c0ba6d7cac904087f7cec51af30c2b2ad022ce0e76a")
    message(FATAL_ERROR "Git 2.56.0 LGPL-2.1 notice does not match its pin")
endif()

set(bpr_eval_git_source_2_56 "${bpr_eval_stage}/git-2.56.0/git-2.56.0")
set(bpr_eval_git_source_2_39 "${bpr_eval_stage}/git-2.39.5/git-2.39.5")
set(bpr_eval_dtl "${bpr_eval_stage}/dtl/dtl-1.21")
set(bpr_eval_rapidfuzz "${bpr_eval_stage}/rapidfuzz/rapidfuzz-cpp-3.3.4")
set(bpr_eval_dmp "${bpr_eval_stage}/dmp/diff-match-patch-cpp-stl-ba3bbe3cdba5d461e241b756a50085c3602a3b5a")

# The process shape runs Git releases built from the pinned sources, beside the
# installed Git. Only the `git` binary is built: diff, hash-object and init are
# builtins. The outputs live outside the extracted trees, so a reconfigure's
# re-extraction does not force a rebuild; the command depends on the archive.
find_program(BPR_EVAL_MAKE NAMES gmake make REQUIRED)
set(bpr_eval_git_flags
    NO_GETTEXT=YesPlease NO_TCLTK=YesPlease NO_PERL=YesPlease NO_PYTHON=YesPlease
    NO_CURL=YesPlease NO_EXPAT=YesPlease NO_OPENSSL=YesPlease NO_INSTALL_HARDLINKS=YesPlease)
foreach(version IN ITEMS 2.39.5 2.56.0)
    string(REPLACE "." "_" token "${version}")
    set(source "${bpr_eval_stage}/git-${version}/git-${version}")
    set(binary "${bpr_eval_stage}/bin-${version}/git")
    add_custom_command(
        OUTPUT "${binary}"
        COMMAND "${BPR_EVAL_MAKE}" -C "${source}" -j8 ${bpr_eval_git_flags} git
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${bpr_eval_stage}/bin-${version}"
        COMMAND "${CMAKE_COMMAND}" -E copy "${source}/git" "${binary}"
        DEPENDS "${CPM_SOURCE_CACHE}/git-${version}.tar.xz"
        COMMENT "Building the evaluation's Git ${version}"
        VERBATIM)
    add_custom_target(tool_diff_evaluation_git_${token} DEPENDS "${binary}")
    set(BPR_EVAL_GIT_${token} "${binary}")
endforeach()
find_program(BPR_EVAL_INSTALLED_GIT NAMES git REQUIRED)

# Linked shape: Git 2.56.0's xdiff, compiled with a first-party shim for the
# few git-compat-util.h facilities it uses, as its own shared library that the
# candidates link dynamically. The owner's Q1 answer (2026-10-13) accepts LGPL
# xdiff only "if xdiff is dynamically linked as its own shared library, as Qt
# is"; static linking stays refused, so the evaluation measures that linkage.
file(GLOB bpr_eval_xdiff_sources "${bpr_eval_git_source_2_56}/xdiff/*.c")
add_library(bpr_eval_xdiff SHARED ${bpr_eval_xdiff_sources})
# Only the xdiff directory is ever on an include path: Git's top level holds a
# file named `version`, which a case-insensitive file system would find for
# the standard <version> header.
target_include_directories(bpr_eval_xdiff PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/xdiff_shim")
target_include_directories(bpr_eval_xdiff SYSTEM PUBLIC "${bpr_eval_git_source_2_56}/xdiff")
target_compile_options(bpr_eval_xdiff PRIVATE -w)
set_target_properties(bpr_eval_xdiff PROPERTIES
    C_STANDARD 11
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET default
    OUTPUT_NAME bpr_eval_xdiff)

add_library(bpr_eval_headers INTERFACE)
target_include_directories(bpr_eval_headers SYSTEM INTERFACE
    "${bpr_eval_dtl}"
    "${bpr_eval_rapidfuzz}"
    "${bpr_eval_dmp}")

# The candidates and the hook. An object library, so its definitions are in
# every executable that links it whatever the archive order.
add_library(bpr_diff_evaluation_hook OBJECT
    candidates/candidates.cpp
    candidates/git_process.cpp
    candidates/xdiff_linked.cpp
    candidates/onp_matcher.cpp
    candidates/histogram_matcher.cpp
    candidates/slide_post_pass.cpp
    candidates/pair_scores.cpp
    candidates/intraline_candidates.cpp
    candidates/hook.cpp)
target_include_directories(bpr_diff_evaluation_hook PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(bpr_diff_evaluation_hook PUBLIC BetterPullReview::Core bpr_eval_xdiff bpr_eval_headers)
target_compile_definitions(bpr_diff_evaluation_hook
    PUBLIC BPR_DIFF_ENGINE_EVALUATION
    PRIVATE
        BPR_EVAL_GIT_2_39="${BPR_EVAL_GIT_2_39_5}"
        BPR_EVAL_GIT_2_56="${BPR_EVAL_GIT_2_56_0}"
        BPR_EVAL_GIT_INSTALLED="${BPR_EVAL_INSTALLED_GIT}"
        BPR_EVAL_SELECTION_FILE="${CMAKE_BINARY_DIR}/diff-evaluation-candidate.txt")
target_compile_features(bpr_diff_evaluation_hook PUBLIC cxx_std_23)
add_dependencies(bpr_diff_evaluation_hook tool_diff_evaluation_git_2_39_5 tool_diff_evaluation_git_2_56_0)

# The core declares the hook only in this build.
target_compile_definitions(better_pull_review_core PRIVATE BPR_DIFF_ENGINE_EVALUATION)

# Every executable of this tree links the hook: the existing suites then run
# against the selected candidate. Deferred to the end of the top-level
# configure, when every target exists.
function(bpr_link_diff_evaluation_hook)
    set(pending "${PROJECT_SOURCE_DIR}")
    while(pending)
        list(POP_FRONT pending directory)
        get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
        get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
        list(APPEND pending ${children})
        foreach(target IN LISTS targets)
            get_target_property(type "${target}" TYPE)
            get_target_property(imported "${target}" IMPORTED)
            if(type STREQUAL "EXECUTABLE" AND NOT imported)
                target_link_libraries("${target}" PRIVATE bpr_diff_evaluation_hook)
            endif()
        endforeach()
    endwhile()
endfunction()
cmake_language(DEFER DIRECTORY "${PROJECT_SOURCE_DIR}" CALL bpr_link_diff_evaluation_hook)

# The measurement harness: corpus runs, stops, process overhead and oracle
# agreement. It uses the Git adapter's process supervision for the process
# shape's launches, kills and reaps.
if(BPR_BUILD_QT)
    bpr_add_executable(bench_better_pull_review_diff_engine_evaluation
        harness/diff_engine_evaluation.cpp
        harness/supervised_git.cpp
        harness/oracle.cpp)
    target_link_libraries(bench_better_pull_review_diff_engine_evaluation
        PRIVATE BetterPullReview::Core BetterPullReview::ProcessSupport BetterPullReview::Platform Qt6::Core)
    target_include_directories(bench_better_pull_review_diff_engine_evaluation PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# The fixed intraline render set an attended review judges (AC06), drawn by
# the product's own diff workbench offscreen.
if(TARGET BetterPullReview::AdapterQt)
    bpr_add_executable(tool_diff_intraline_render_set harness/intraline_render_set.cpp)
    target_link_libraries(tool_diff_intraline_render_set PRIVATE BetterPullReview::AdapterQt Qt6::Widgets)
    target_include_directories(tool_diff_intraline_render_set PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

bpr_add_executable(tool_diff_baseline_digest baseline_digest.cpp)
target_link_libraries(tool_diff_baseline_digest PRIVATE BetterPullReview::Core)

# The diff fuzz target's corpus, replayed by a plain driver: the libFuzzer graph
# instruments everything and is a separate configuration.
bpr_add_executable(tool_diff_fuzz_replay
    "${PROJECT_SOURCE_DIR}/tests/fuzz/diff_fuzz.cpp"
    harness/fuzz_replay_main.cpp)
target_link_libraries(tool_diff_fuzz_replay PRIVATE BetterPullReview::Core)

# Expectation inventories: copies of the suites whose binaries stop at their
# first failed expectation, generated into the build tree with that stop
# replaced by a count, so a candidate's run lists every changed expectation
# (each FAILED line) instead of the first. The sources are not copied into the
# repository; the copy is regenerated whenever the suite changes.
foreach(inventory IN ITEMS
        "diff_compute_inventory|tests/core/diff_compute_tests.cpp|BetterPullReview::Core"
        "diff_host_inventory|tests/application/diff_application_host_tests.cpp|BetterPullReview::Application")
    string(REPLACE "|" ";" fields "${inventory}")
    list(GET fields 0 name)
    list(GET fields 1 source)
    list(GET fields 2 library)
    if(NOT TARGET "${library}")
        continue()
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/${source}")
    file(READ "${PROJECT_SOURCE_DIR}/${source}" content)
    string(REPLACE "std::exit(EXIT_FAILURE);" "++bpr_eval_inventory_failures;" content "${content}")
    string(PREPEND content
        "#include <cstdio>\n"
        "inline int bpr_eval_inventory_failures = 0;\n"
        "namespace { struct bpr_eval_inventory_report { ~bpr_eval_inventory_report() { std::fprintf(stderr, \"INVENTORY_FAILURES %d\\n\", bpr_eval_inventory_failures); } } const bpr_eval_inventory_reporter; }\n")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/inventory/${name}.cpp" "${content}")
    get_filename_component(source_directory "${PROJECT_SOURCE_DIR}/${source}" DIRECTORY)
    bpr_add_executable(tool_${name} "${CMAKE_CURRENT_BINARY_DIR}/inventory/${name}.cpp")
    target_include_directories(tool_${name} PRIVATE "${source_directory}")
    target_link_libraries(tool_${name} PRIVATE "${library}")
endforeach()
