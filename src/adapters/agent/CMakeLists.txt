# The agent process adapter exists only where a process can be started. It sits
# outside `src/adapters/qt` deliberately: that directory is an effect-free
# region the architecture scan enforces, because presentation starts no
# processes. `src/adapters/git` is the precedent.
if(NOT BPR_BUILD_QT)
    return()
endif()

add_library(
    better_pull_review_adapter_agent
    STATIC
        src/agent_process_launcher.cpp
)
add_library(
    BetterPullReview::AdapterAgent
    ALIAS
    better_pull_review_adapter_agent
)
target_include_directories(
    better_pull_review_adapter_agent
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    better_pull_review_adapter_agent
    PUBLIC
        BetterPullReview::Application
    PRIVATE
        Qt6::Core
)
target_compile_features(better_pull_review_adapter_agent PUBLIC cxx_std_23)
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_definitions(
        better_pull_review_adapter_agent
        PUBLIC
            _LIBCPP_ENABLE_EXPERIMENTAL
    )
endif()
bpr_enable_hardening(better_pull_review_adapter_agent)
bpr_enable_warnings(better_pull_review_adapter_agent)
