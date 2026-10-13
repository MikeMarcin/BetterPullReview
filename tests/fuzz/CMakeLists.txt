# [[BPR-0970]]. libFuzzer targets for the custom parser and mapping
# boundaries, one per family. The top-level BPR_BUILD_FUZZERS block has
# already instrumented every first-party unit; a target here adds only the
# libFuzzer driver at link time. docs/verification/BPR-0970-parser-fuzzing.md
# holds the inventory these targets answer, and
# docs/development/build-and-test.md the configure and campaign commands.
#
# The release harnesses beside these files (release_*_fuzz.cpp) keep their own
# release-policy recipe and are not registered here.

# The driver: Clang links libFuzzer with -fsanitize=fuzzer. Apple clang
# instruments for it (fuzzer-no-link) but ships no runtime, so on macOS the
# runtime archive of an LLVM installation is named instead, for example
# /opt/homebrew/opt/llvm/lib/clang/<version>/lib/darwin/libclang_rt.fuzzer_osx.a.
# The production code is then compiled by the same compiler as every other
# graph on the host.
set(BPR_LIBFUZZER_RUNTIME "" CACHE FILEPATH "libFuzzer runtime archive, for a compiler that cannot link -fsanitize=fuzzer itself")
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-fsanitize=fuzzer,address,undefined")
check_cxx_source_compiles(
    "#include <cstddef>\n#include <cstdint>\nextern \"C\" int LLVMFuzzerTestOneInput(const std::uint8_t*, std::size_t) { return 0; }"
    BPR_COMPILER_LINKS_LIBFUZZER
)
unset(CMAKE_REQUIRED_FLAGS)
if(BPR_COMPILER_LINKS_LIBFUZZER)
    set(bpr_fuzz_driver_link -fsanitize=fuzzer)
elseif(BPR_LIBFUZZER_RUNTIME AND EXISTS "${BPR_LIBFUZZER_RUNTIME}")
    set(bpr_fuzz_driver_link "${BPR_LIBFUZZER_RUNTIME}")
else()
    message(
        FATAL_ERROR
        "This compiler cannot link libFuzzer. Set BPR_LIBFUZZER_RUNTIME to an LLVM "
        "libclang_rt.fuzzer archive for this platform"
    )
endif()

# The adapter families need the Git, GitHub and MCP adapters, which exist only
# when the Qt or the tour graph is built.
foreach(bpr_fuzz_required IN ITEMS better_pull_review_adapter_github better_pull_review_adapter_git better_pull_review_adapter_mcp)
    if(NOT TARGET ${bpr_fuzz_required})
        message(FATAL_ERROR "BPR_BUILD_FUZZERS needs ${bpr_fuzz_required}: configure with BPR_BUILD_TOUR_CLI=ON or BPR_BUILD_QT=ON")
    endif()
endforeach()

set(bpr_fuzz_targets "")

# bpr_add_fuzz_target(<family> MAX_LEN <bytes> LIBRARIES <targets...> [INCLUDES <dirs...>] [DEFINITIONS <defs...>])
#
# Builds fuzz_<family> from <family>_fuzz.cpp and registers
# fuzz_<family>_corpus, which replays the checked-in corpus once with the same
# limits a campaign uses. The replay mutates nothing and writes nothing back to
# the corpus directory.
function(bpr_add_fuzz_target family)
    cmake_parse_arguments(PARSE_ARGV 1 bpr_fuzz "" "MAX_LEN" "LIBRARIES;INCLUDES;DEFINITIONS")
    if(bpr_fuzz_UNPARSED_ARGUMENTS OR NOT bpr_fuzz_MAX_LEN OR NOT bpr_fuzz_LIBRARIES)
        message(FATAL_ERROR "bpr_add_fuzz_target(${family}) needs MAX_LEN and LIBRARIES")
    endif()
    set(target "fuzz_${family}")
    bpr_add_executable(${target} "${family}_fuzz.cpp")
    target_link_libraries(${target} PRIVATE ${bpr_fuzz_LIBRARIES})
    if(bpr_fuzz_INCLUDES)
        target_include_directories(${target} PRIVATE ${bpr_fuzz_INCLUDES})
    endif()
    if(bpr_fuzz_DEFINITIONS)
        target_compile_definitions(${target} PRIVATE ${bpr_fuzz_DEFINITIONS})
    endif()
    target_compile_features(${target} PRIVATE cxx_std_23)
    target_link_libraries(${target} PRIVATE ${bpr_fuzz_driver_link})
    bpr_enable_warnings(${target})
    bpr_enable_hardening(${target})
    bpr_add_network_isolated_test(
        NAME "fuzz_${family}_corpus"
        COMMAND "$<TARGET_FILE:${target}>"
            -runs=0
            -max_len=${bpr_fuzz_MAX_LEN}
            -timeout=10
            -rss_limit_mb=2048
            -malloc_limit_mb=512
            -seed=970
            "${CMAKE_CURRENT_SOURCE_DIR}/corpora/${family}"
    )
    set_tests_properties("fuzz_${family}_corpus" PROPERTIES TIMEOUT 120)
    # Appended, so the row keeps the network-isolation label the manifest checks.
    set_property(TEST "fuzz_${family}_corpus" APPEND PROPERTY LABELS bpr_fuzz_corpus)
    set(bpr_fuzz_targets ${bpr_fuzz_targets} ${target} PARENT_SCOPE)
endfunction()

bpr_add_fuzz_target(diff MAX_LEN 8192 LIBRARIES BetterPullReview::Core)
bpr_add_fuzz_target(markdown MAX_LEN 16384 LIBRARIES BetterPullReview::Core)
bpr_add_fuzz_target(settings_theme MAX_LEN 20000
    LIBRARIES BetterPullReview::AdapterSQLite BetterPullReview::SQLite3)
bpr_add_fuzz_target(review_cycle MAX_LEN 65536 LIBRARIES BetterPullReview::Core)
bpr_add_fuzz_target(agent_output MAX_LEN 65536 LIBRARIES BetterPullReview::Application BetterPullReview::AdapterMcp)
bpr_add_fuzz_target(git_output MAX_LEN 65536
    LIBRARIES BetterPullReview::Application BetterPullReview::AdapterGit
    INCLUDES "${PROJECT_SOURCE_DIR}/src/adapters/git/src")
bpr_add_fuzz_target(github_api MAX_LEN 262144
    LIBRARIES BetterPullReview::AdapterGitHub glaze::glaze
    INCLUDES "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
    DEFINITIONS "BPR_GITHUB_CONTRACT_FIXTURE_ROOT=\"${PROJECT_SOURCE_DIR}/tests/fixtures/github/contracts/v1\"")

# The rendered Markdown view exists only in a Qt graph.
if(TARGET better_pull_review_adapter_qt)
    bpr_add_fuzz_target(markdown_view MAX_LEN 70000 LIBRARIES BetterPullReview::AdapterQt)
    if(UNIX AND NOT APPLE)
        # [[BPR-1260]]. Linux runs LeakSanitizer with AddressSanitizer, and
        # Qt's font caches outlive the process; see the suppression file.
        set_tests_properties(fuzz_markdown_view_corpus PROPERTIES ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/markdown_view_lsan.supp:print_suppressions=0")
    else()
        set_tests_properties(fuzz_markdown_view_corpus PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endif()

# [[BPR-1441]]. The C++ syntax classifier and its identifier refinement exist
# only in a Qt graph, beside the packaged KSyntaxHighlighting definitions.
if(TARGET better_pull_review_adapter_syntax)
    bpr_add_fuzz_target(syntax MAX_LEN 65536 LIBRARIES BetterPullReview::AdapterSyntax Qt6::Core)
endif()

add_custom_target(parser-fuzzers DEPENDS ${bpr_fuzz_targets})
# [[BPR-1260]]. On Linux each corpus row runs under the network sandbox tool,
# so the documented build-then-ctest sequence needs it built as well.
if(TARGET tool_better_pull_review_network_sandbox)
    add_dependencies(parser-fuzzers tool_better_pull_review_network_sandbox)
endif()
