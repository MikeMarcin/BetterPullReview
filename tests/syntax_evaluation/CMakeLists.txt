# [[BPR-1442]]. RFC-BPR-0081's C++ identifier parser evaluation. Present only
# with BPR_BUILD_SYNTAX_EVALUATION=ON (see the root CMakeLists.txt); nothing
# here is installed, packaged or linked by a shipped target.
#
# For each tree-sitter-cpp pin (v0_23_4, master):
# - bpr_syntax_candidate_<pin>: the candidate refinement as object files that
#   define `refine_cpp_identifiers`. Linked ahead of the syntax adapter, they
#   replace the shipped refinement while the adapter's unchanged classifier
#   calls them.
# - bench_syntax_evaluation_<pin>: the harness, holding the shipped refinement
#   and the candidate side by side behind the classifier.
# - twins of the RFC's contract rows, the large-review benchmark and, in a
#   fuzz graph, the syntax fuzz target, each with the candidate compiled in.

include("${PROJECT_SOURCE_DIR}/cmake/BprTreeSitterProvider.cmake")
bpr_acquire_tree_sitter_evaluation()

set(bpr_eval_adapter_private "${PROJECT_SOURCE_DIR}/src/adapters/syntax/src")
set(bpr_eval_candidate_source "${CMAKE_CURRENT_SOURCE_DIR}/candidate/cpp_identifier_context_tree_sitter.cpp")

# The shipped refinement under the harness's name for it.
add_library(bpr_syntax_evaluation_baseline OBJECT "${PROJECT_SOURCE_DIR}/src/core/src/cpp_identifier_context.cpp")
target_link_libraries(bpr_syntax_evaluation_baseline PRIVATE BetterPullReview::Application)
target_include_directories(bpr_syntax_evaluation_baseline PRIVATE "${bpr_eval_adapter_private}")
target_compile_definitions(bpr_syntax_evaluation_baseline PRIVATE refine_cpp_identifiers=refine_cpp_identifiers_baseline)
target_compile_features(bpr_syntax_evaluation_baseline PRIVATE cxx_std_23)
bpr_enable_warnings(bpr_syntax_evaluation_baseline)

if(BPR_BUILD_FUZZERS)
    if(BPR_LIBFUZZER_RUNTIME AND EXISTS "${BPR_LIBFUZZER_RUNTIME}")
        set(bpr_eval_fuzz_driver "${BPR_LIBFUZZER_RUNTIME}")
    else()
        set(bpr_eval_fuzz_driver -fsanitize=fuzzer)
    endif()
endif()

set(bpr_eval_qt_environment "QT_QPA_PLATFORM=offscreen")
if(UNIX AND NOT APPLE)
    list(APPEND bpr_eval_qt_environment "QT_STYLE_OVERRIDE=Windows")
endif()

# bpr_eval_twin(<target> <pin> SOURCES ... LIBRARIES ... [INCLUDES ...] [DEFINITIONS ...])
# An executable built from another target's sources with the candidate's
# objects linked ahead of every library.
function(bpr_eval_twin target pin)
    cmake_parse_arguments(PARSE_ARGV 2 twin "" "" "SOURCES;LIBRARIES;INCLUDES;DEFINITIONS")
    bpr_add_executable(${target} ${twin_SOURCES} $<TARGET_OBJECTS:bpr_syntax_candidate_${pin}>)
    target_link_libraries(${target} PRIVATE ${twin_LIBRARIES} bpr_tree_sitter_cpp_${pin})
    if(twin_INCLUDES)
        target_include_directories(${target} PRIVATE ${twin_INCLUDES})
    endif()
    if(twin_DEFINITIONS)
        target_compile_definitions(${target} PRIVATE ${twin_DEFINITIONS})
    endif()
    target_compile_features(${target} PRIVATE cxx_std_23)
    bpr_enable_hardening(${target})
    bpr_enable_warnings(${target})
endfunction()

foreach(pin IN ITEMS v0_23_4 master)
    if(pin STREQUAL "v0_23_4")
        set(label "ts-v0.23.4")
    else()
        set(label "ts-master")
    endif()

    # The candidate exactly as it would replace the shipped refinement.
    add_library(bpr_syntax_candidate_${pin} OBJECT "${bpr_eval_candidate_source}")
    target_link_libraries(bpr_syntax_candidate_${pin} PRIVATE BetterPullReview::Application bpr_tree_sitter_cpp_${pin})
    target_include_directories(bpr_syntax_candidate_${pin} PRIVATE "${bpr_eval_adapter_private}")
    target_compile_features(bpr_syntax_candidate_${pin} PRIVATE cxx_std_23)
    bpr_enable_warnings(bpr_syntax_candidate_${pin})

    # The candidate under the harness's name for it.
    add_library(bpr_syntax_evaluation_candidate_${pin} OBJECT "${bpr_eval_candidate_source}")
    target_link_libraries(bpr_syntax_evaluation_candidate_${pin} PRIVATE BetterPullReview::Application bpr_tree_sitter_cpp_${pin})
    target_include_directories(bpr_syntax_evaluation_candidate_${pin} PRIVATE "${bpr_eval_adapter_private}")
    target_compile_definitions(bpr_syntax_evaluation_candidate_${pin} PRIVATE refine_cpp_identifiers=refine_cpp_identifiers_candidate)
    target_compile_features(bpr_syntax_evaluation_candidate_${pin} PRIVATE cxx_std_23)
    bpr_enable_warnings(bpr_syntax_evaluation_candidate_${pin})

    bpr_add_executable(
        bench_syntax_evaluation_${pin}
        harness/syntax_evaluation_harness.cpp
        $<TARGET_OBJECTS:bpr_syntax_evaluation_baseline>
        $<TARGET_OBJECTS:bpr_syntax_evaluation_candidate_${pin}>
    )
    target_link_libraries(bench_syntax_evaluation_${pin} PRIVATE BetterPullReview::AdapterSyntax Qt6::Core bpr_tree_sitter_cpp_${pin})
    target_compile_features(bench_syntax_evaluation_${pin} PRIVATE cxx_std_23)
    bpr_enable_warnings(bench_syntax_evaluation_${pin})

    # The same harness with the candidate's strict error-recovery policy, a
    # diagnostic: declarations inside tree-sitter ERROR nodes supply no facts.
    add_library(bpr_syntax_evaluation_candidate_${pin}_strict OBJECT "${bpr_eval_candidate_source}")
    target_link_libraries(bpr_syntax_evaluation_candidate_${pin}_strict PRIVATE BetterPullReview::Application bpr_tree_sitter_cpp_${pin})
    target_include_directories(bpr_syntax_evaluation_candidate_${pin}_strict PRIVATE "${bpr_eval_adapter_private}")
    target_compile_definitions(bpr_syntax_evaluation_candidate_${pin}_strict PRIVATE
        refine_cpp_identifiers=refine_cpp_identifiers_candidate BPR_TREE_SITTER_FACTS_IN_ERROR=0)
    target_compile_features(bpr_syntax_evaluation_candidate_${pin}_strict PRIVATE cxx_std_23)
    bpr_enable_warnings(bpr_syntax_evaluation_candidate_${pin}_strict)
    bpr_add_executable(
        bench_syntax_evaluation_${pin}_strict
        harness/syntax_evaluation_harness.cpp
        $<TARGET_OBJECTS:bpr_syntax_evaluation_baseline>
        $<TARGET_OBJECTS:bpr_syntax_evaluation_candidate_${pin}_strict>
    )
    target_link_libraries(bench_syntax_evaluation_${pin}_strict PRIVATE BetterPullReview::AdapterSyntax Qt6::Core bpr_tree_sitter_cpp_${pin})
    target_compile_features(bench_syntax_evaluation_${pin}_strict PRIVATE cxx_std_23)
    bpr_enable_warnings(bench_syntax_evaluation_${pin}_strict)

    # The contract rows with the candidate compiled in.
    bpr_eval_twin(test_syntax_classification_contract_${pin} ${pin}
        SOURCES "${PROJECT_SOURCE_DIR}/tests/syntax_contract/syntax_classification_contract_tests.cpp"
        LIBRARIES BetterPullReview::AdapterSyntax KF6::SyntaxHighlighting Qt6::Core
        DEFINITIONS "BPR_SYNTAX_FIXTURE_ROOT=\"${PROJECT_SOURCE_DIR}/tests/fixtures/syntax\"")
    if(WIN32)
        target_link_libraries(test_syntax_classification_contract_${pin} PRIVATE BetterPullReview::AdapterSQLite BetterPullReview::Platform)
    endif()
    bpr_add_network_isolated_test(NAME "syntax_classification_contract@${label}" COMMAND test_syntax_classification_contract_${pin})
    set_tests_properties("syntax_classification_contract@${label}" PROPERTIES
        PASS_REGULAR_EXPRESSION "syntax-classification-contract-tests:pass" TIMEOUT 120)

    bpr_eval_twin(test_syntax_refinement_cancellation_${pin} ${pin}
        SOURCES "${PROJECT_SOURCE_DIR}/tests/syntax_contract/syntax_refinement_cancellation_tests.cpp"
        LIBRARIES BetterPullReview::AdapterSyntax Qt6::Core)
    bpr_add_network_isolated_test(NAME "syntax_refinement_cancellation@${label}" COMMAND test_syntax_refinement_cancellation_${pin})
    set_tests_properties("syntax_refinement_cancellation@${label}" PROPERTIES
        PASS_REGULAR_EXPRESSION "syntax-refinement-cancellation-tests:pass" TIMEOUT 120)

    bpr_eval_twin(test_qt_syntax_pane_composition_${pin} ${pin}
        SOURCES "${PROJECT_SOURCE_DIR}/tests/adapters_qt/syntax_pane_composition_tests.cpp"
        LIBRARIES BetterPullReview::AdapterQt BetterPullReview::AdapterSyntax)
    bpr_add_network_isolated_test(NAME "qt_syntax_pane_composition@${label}" COMMAND test_qt_syntax_pane_composition_${pin})
    set_tests_properties("qt_syntax_pane_composition@${label}" PROPERTIES
        ENVIRONMENT "${bpr_eval_qt_environment}" PASS_REGULAR_EXPRESSION "syntax-pane-composition-tests:pass" TIMEOUT 120)

    bpr_eval_twin(test_cpp_identifier_appearance_${pin} ${pin}
        SOURCES "${PROJECT_SOURCE_DIR}/tests/end_to_end/cpp_identifier_appearance_public_path_tests.cpp"
        LIBRARIES BetterPullReview::Composition BetterPullReview::SQLite3
        DEFINITIONS BPR_BUILD_TESTING=1)
    bpr_add_network_isolated_test(NAME "application_cpp_identifier_appearance@${label}" COMMAND test_cpp_identifier_appearance_${pin})
    set_tests_properties("application_cpp_identifier_appearance@${label}" PROPERTIES
        ENVIRONMENT "${bpr_eval_qt_environment};BPR_CPP_IDENTIFIER_RENDER_ROOT=${CMAKE_CURRENT_BINARY_DIR}/cpp-identifier-renders-${pin}"
        TIMEOUT 240 PASS_REGULAR_EXPRESSION "CPP_IDENTIFIER_APPEARANCE_OK")

    bpr_eval_twin(bench_large_review_${pin} ${pin}
        SOURCES "${PROJECT_SOURCE_DIR}/tests/end_to_end/large_review_benchmark.cpp" "${PROJECT_SOURCE_DIR}/tests/end_to_end/large_review_native_memory.cpp"
        LIBRARIES BetterPullReview::Composition glaze::glaze
        INCLUDES "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
        DEFINITIONS BPR_BUILD_TESTING=1
            "BPR_LARGE_REVIEW_FIXTURE_ROOT=\"${PROJECT_SOURCE_DIR}/tests/fixtures/github/pull-requests/large-review-v1\""
            "BPR_BENCH_BUILD_CONFIGURATION=\"$<CONFIG>\"")
    if(WIN32)
        target_link_libraries(bench_large_review_${pin} PRIVATE psapi)
    endif()
    bpr_add_network_isolated_test(NAME "application_large_review_benchmark@${label}"
        COMMAND $<TARGET_FILE:bench_large_review_${pin}> $<TARGET_FILE:Git::Git>
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-measurement-${pin}.json"
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-window-${pin}.png")
    set_tests_properties("application_large_review_benchmark@${label}" PROPERTIES
        ENVIRONMENT "${bpr_eval_qt_environment}" RUN_SERIAL TRUE TIMEOUT 120
        PASS_REGULAR_EXPRESSION "LARGE_REVIEW_COMPOSED_BENCHMARK_OK")

    if(BPR_BUILD_FUZZERS)
        bpr_eval_twin(fuzz_syntax_${pin} ${pin}
            SOURCES "${PROJECT_SOURCE_DIR}/tests/fuzz/syntax_fuzz.cpp"
            LIBRARIES BetterPullReview::AdapterSyntax Qt6::Core ${bpr_eval_fuzz_driver})
        bpr_add_network_isolated_test(NAME "fuzz_syntax_corpus@${label}"
            COMMAND "$<TARGET_FILE:fuzz_syntax_${pin}>" -runs=0 -max_len=65536 -timeout=10 -rss_limit_mb=2048
                -malloc_limit_mb=512 -seed=970 "${PROJECT_SOURCE_DIR}/tests/fuzz/corpora/syntax")
        set_tests_properties("fuzz_syntax_corpus@${label}" PROPERTIES TIMEOUT 120)
    endif()
endforeach()

# The shipped classifier alone, for AC07's before-and-after span comparison.
# It links no candidate and no tree-sitter.
bpr_add_executable(bench_syntax_span_dump harness/syntax_span_dump.cpp)
target_link_libraries(bench_syntax_span_dump PRIVATE BetterPullReview::AdapterSyntax Qt6::Core)
target_compile_features(bench_syntax_span_dump PRIVATE cxx_std_23)
bpr_enable_warnings(bench_syntax_span_dump)

add_custom_target(syntax-evaluation)
add_dependencies(syntax-evaluation bench_syntax_evaluation_v0_23_4 bench_syntax_evaluation_master
    bench_syntax_evaluation_v0_23_4_strict bench_syntax_evaluation_master_strict bench_syntax_span_dump)
foreach(pin IN ITEMS v0_23_4 master)
    add_dependencies(syntax-evaluation
        test_syntax_classification_contract_${pin} test_syntax_refinement_cancellation_${pin}
        test_qt_syntax_pane_composition_${pin} test_cpp_identifier_appearance_${pin} bench_large_review_${pin})
    if(BPR_BUILD_FUZZERS)
        add_dependencies(syntax-evaluation fuzz_syntax_${pin})
    endif()
endforeach()
