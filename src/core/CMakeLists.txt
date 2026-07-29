add_library(
    better_pull_review_core
    STATIC
        src/model.cpp
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
