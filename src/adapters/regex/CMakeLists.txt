# [[BPR-1431]]. Source search's regex engine: the pinned Google RE2 behind the
# core `source_regex_engine` interface. It links core and RE2 and nothing
# else: no Qt, no provider, no process, file or network effect.
add_library(
    better_pull_review_adapter_regex
    STATIC
        src/re2_source_regex_engine.cpp
)
add_library(
    BetterPullReview::AdapterRegex
    ALIAS
    better_pull_review_adapter_regex
)
target_include_directories(
    better_pull_review_adapter_regex
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    better_pull_review_adapter_regex
    PUBLIC
        BetterPullReview::Core
    PRIVATE
        BetterPullReview::RE2
)
target_compile_features(
    better_pull_review_adapter_regex
    PUBLIC
        cxx_std_23
)
bpr_enable_hardening(better_pull_review_adapter_regex)
bpr_enable_warnings(better_pull_review_adapter_regex)
