# [[BPR-1458]]. Candidate L: libgit2 answering RFC-BPR-0083's C1-C6 questions
# behind the harness's candidate interface. No libgit2 type crosses
# libgit2_candidate.hpp.
option(BPR_EVALUATE_LIBGIT2_SHA256 "Build the evaluation libgit2 with its experimental SHA-256 support (H3 variant)" OFF)
include("${PROJECT_SOURCE_DIR}/cmake/BprLibgit2Provider.cmake")
bpr_acquire_libgit2()

add_library(bpr_libgit2_candidate STATIC libgit2_candidate.cpp)
target_include_directories(
    bpr_libgit2_candidate
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/tests/workspace_contract"
)
target_link_libraries(bpr_libgit2_candidate PRIVATE BetterPullReview::Libgit2Evaluation)
target_compile_definitions(bpr_libgit2_candidate PUBLIC BPR_HAVE_LIBGIT2_CANDIDATE=1)
target_compile_features(bpr_libgit2_candidate PUBLIC cxx_std_23)
bpr_enable_warnings(bpr_libgit2_candidate)

# The macOS evidence course's driver (libgit2_course.py).
bpr_add_executable(tool_libgit2_probe libgit2_probe.cpp)
target_link_libraries(tool_libgit2_probe PRIVATE bpr_libgit2_candidate)
bpr_enable_warnings(tool_libgit2_probe)

# The cost record's stripped binary-size pair: K alone, then K and L.
bpr_add_executable(tool_libgit2_size_k libgit2_size_probe.cpp)
target_link_libraries(tool_libgit2_size_k PRIVATE BetterPullReview::Core)
bpr_add_executable(tool_libgit2_size_kl libgit2_size_probe.cpp)
target_link_libraries(tool_libgit2_size_kl PRIVATE BetterPullReview::Core bpr_libgit2_candidate)
target_compile_definitions(tool_libgit2_size_kl PRIVATE BPR_SIZE_WITH_LIBGIT2=1)

# [[BPR-1459]]. Candidate V as a test-only oracle: libgit2's wildmatch, taken
# from the evaluation archive, against core::wildcard_match.
bpr_add_executable(tool_wildmatch_differential wildmatch_differential.cpp)
target_link_libraries(tool_wildmatch_differential PRIVATE BetterPullReview::Core BetterPullReview::Libgit2Evaluation)
bpr_enable_warnings(tool_wildmatch_differential)
