# [[BPR-1446]]. RFC-BPR-0080's evaluation harness. Configure with
# -DBPR_HASH_EVALUATION=ON; run_hash_evaluation.py drives it and writes the raw
# results the report in docs/verification/RFC-BPR-0080.md cites.
include("${PROJECT_SOURCE_DIR}/cmake/BprHashEvaluationProvider.cmake")
bpr_acquire_hash_evaluation(bpr_hash_evaluation_vectors bpr_hash_evaluation_collisions)

bpr_add_executable(bench_hash_evaluation hash_evaluation.cpp hash_arms.cpp)
target_link_libraries(
    bench_hash_evaluation
    PRIVATE
        BetterPullReview::Core
        BetterPullReview::Sha1dc
        BetterPullReview::HaclSha2
)
if(TARGET Qt6::Core)
    target_link_libraries(bench_hash_evaluation PRIVATE Qt6::Core)
    target_compile_definitions(bench_hash_evaluation PRIVATE BPR_HASH_EVALUATION_QT)
endif()
bpr_enable_hardening(bench_hash_evaluation)
bpr_enable_warnings(bench_hash_evaluation)

bpr_add_network_isolated_test(
    NAME hash_evaluation_known_answers
    COMMAND bench_hash_evaluation kat "${bpr_hash_evaluation_vectors}"
)
bpr_add_network_isolated_test(
    NAME hash_evaluation_chunking
    COMMAND bench_hash_evaluation chunking
)
bpr_add_network_isolated_test(
    NAME hash_evaluation_collisions
    COMMAND bench_hash_evaluation collision "${bpr_hash_evaluation_collisions}"
)
