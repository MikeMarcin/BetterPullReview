add_library(
    better_pull_review_core
    STATIC
        src/diff_compute.cpp
        src/diff_model.cpp
        src/epoch_model.cpp
        src/epoch_transition.cpp
        src/github_model.cpp
        src/github_transition.cpp
        src/model.cpp
        src/sha256.cpp
        src/transition.cpp
)
add_library(BetterPullReview::Core ALIAS better_pull_review_core)

target_include_directories(
    better_pull_review_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_features(better_pull_review_core PUBLIC cxx_std_23)

bpr_enable_hardening(better_pull_review_core)
bpr_enable_warnings(better_pull_review_core)
