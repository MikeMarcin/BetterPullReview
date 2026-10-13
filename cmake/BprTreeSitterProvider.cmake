include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/AitoCpm.cmake")
# bpr_fetch_verified_archive: SHA-256, size and licence digest, re-extracted.
include("${CMAKE_CURRENT_LIST_DIR}/BprRe2Provider.cmake")

# [[BPR-1442]]. RFC-BPR-0081's candidate parser, for the evaluation harness
# only: the tree-sitter v0.27.1 runtime and two tree-sitter-cpp pins, the
# v0.23.4 tag and a recorded `master` commit (open question Q1). Called only
# from tests/syntax_evaluation, which exists only with
# BPR_BUILD_SYNTAX_EVALUATION=ON, so no other configuration acquires,
# compiles or links any of it. Nothing here is installed, packaged or shipped,
# and nothing enters third_party/ or packaging/dependencies.json.
#
# The runtime is its single-file amalgamation lib/src/lib.c compiled as C11
# with WebAssembly support off (TREE_SITTER_FEATURE_WASM undefined). Each
# grammar is its generated parser.c and scanner.c.
function(bpr_acquire_tree_sitter_evaluation)
    foreach(owned IN ITEMS bpr_tree_sitter_runtime bpr_tree_sitter_cpp_v0_23_4 bpr_tree_sitter_cpp_master)
        if(TARGET ${owned})
            message(FATAL_ERROR "tree-sitter targets must be owned by the pinned evaluation provider")
        endif()
    endforeach()
    foreach(substitution IN ITEMS FETCHCONTENT_SOURCE_DIR_TREE_SITTER FETCHCONTENT_SOURCE_DIR_TREE_SITTER_CPP)
        if(DEFINED ${substitution})
            if(NOT "${${substitution}}" STREQUAL "")
                message(FATAL_ERROR "tree-sitter source substitution is forbidden")
            endif()
        endif()
    endforeach()
    bpr_load_cpm()

    set(stage "${CMAKE_BINARY_DIR}/_bpr/tree-sitter")
    set(master_commit "c009222808634c1014f82438d4883753516a2c24")
    bpr_fetch_verified_archive(
        "tree-sitter 0.27.1"
        "https://codeload.github.com/tree-sitter/tree-sitter/tar.gz/refs/tags/v0.27.1"
        "${CPM_SOURCE_CACHE}/tree-sitter-v0.27.1.tar.gz"
        "982cd3d4d9eb7be18c243240a622423fd2e4ecb4bec2cd98832c2f3fa1f0f333"
        1026138
        "${stage}/runtime/tree-sitter-0.27.1/LICENSE"
        "c5cfb43042b6b72045f4ba997834d0a7786d2793d91680868b5815b39f14fc78"
        "${stage}/runtime")
    bpr_fetch_verified_archive(
        "tree-sitter-cpp 0.23.4"
        "https://codeload.github.com/tree-sitter/tree-sitter-cpp/tar.gz/refs/tags/v0.23.4"
        "${CPM_SOURCE_CACHE}/tree-sitter-cpp-v0.23.4.tar.gz"
        "7a2c55afe3028f4105f25762ea58cc16537d1f5a1dcd9cca90410b3cd5d46051"
        1135921
        "${stage}/cpp-v0.23.4/tree-sitter-cpp-0.23.4/LICENSE"
        "2e0110e07abef7c2548b26ec9d6969775617ca539a0dc8dbeeb14d6452c711d1"
        "${stage}/cpp-v0.23.4")
    bpr_fetch_verified_archive(
        "tree-sitter-cpp master ${master_commit}"
        "https://codeload.github.com/tree-sitter/tree-sitter-cpp/tar.gz/${master_commit}"
        "${CPM_SOURCE_CACHE}/tree-sitter-cpp-${master_commit}.tar.gz"
        "6c6efaf024f5c02fee0403e4e2373346eeadc40828587bcfad9351df2f0a7986"
        1740811
        "${stage}/cpp-master/tree-sitter-cpp-${master_commit}/LICENSE"
        "2e0110e07abef7c2548b26ec9d6969775617ca539a0dc8dbeeb14d6452c711d1"
        "${stage}/cpp-master")

    set(runtime "${stage}/runtime/tree-sitter-0.27.1/lib")
    add_library(bpr_tree_sitter_runtime STATIC "${runtime}/src/lib.c")
    target_include_directories(bpr_tree_sitter_runtime SYSTEM PUBLIC "${runtime}/include" PRIVATE "${runtime}/src")
    # The upstream build's own definitions; WebAssembly stays off.
    target_compile_definitions(bpr_tree_sitter_runtime PRIVATE _POSIX_C_SOURCE=200112L _DEFAULT_SOURCE _BSD_SOURCE _DARWIN_C_SOURCE)
    set_target_properties(bpr_tree_sitter_runtime PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)

    foreach(pin IN ITEMS v0_23_4 master)
        if(pin STREQUAL "v0_23_4")
            set(grammar "${stage}/cpp-v0.23.4/tree-sitter-cpp-0.23.4/src")
        else()
            set(grammar "${stage}/cpp-master/tree-sitter-cpp-${master_commit}/src")
        endif()
        add_library(bpr_tree_sitter_cpp_${pin} STATIC "${grammar}/parser.c" "${grammar}/scanner.c")
        target_include_directories(bpr_tree_sitter_cpp_${pin} SYSTEM PRIVATE "${grammar}")
        target_link_libraries(bpr_tree_sitter_cpp_${pin} PUBLIC bpr_tree_sitter_runtime)
        set_target_properties(bpr_tree_sitter_cpp_${pin} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    endforeach()

    # The notices adoption would have to ship, kept beside the build only.
    file(READ "${stage}/runtime/tree-sitter-0.27.1/LICENSE" runtime_notice)
    file(READ "${stage}/cpp-v0.23.4/tree-sitter-cpp-0.23.4/LICENSE" grammar_notice)
    file(WRITE "${CMAKE_BINARY_DIR}/_bpr/TREE-SITTER-NOTICES.txt"
         "tree-sitter 0.27.1\n\n${runtime_notice}\n\ntree-sitter-cpp 0.23.4 (and ${master_commit})\n\n${grammar_notice}")
    set(BPR_TREE_SITTER_MASTER_COMMIT "${master_commit}" PARENT_SCOPE)
endfunction()
