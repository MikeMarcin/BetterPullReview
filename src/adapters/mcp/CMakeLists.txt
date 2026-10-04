# The MCP context helper lives here rather than in `src/adapters/agent`
# because the two are opposite directions of the same boundary. The agent
# adapter is a narrow launch port for a program BetterPullReview does not
# implement and only starts; this is a program BetterPullReview implements and
# ships, and it serves its own frozen projection outward to somebody else's
# agent. Putting a server inside a launch port would make one directory answer
# for both directions, and the target that starts an agent would link the
# target that answers one.
#
# It links `BetterPullReview::Core` and nothing else. Not the application host,
# not Qt, not the GitHub, Git, or SQLite adapters: a helper whose whole claim is
# that it reaches no credential and no network is best served by a link line a
# reviewer can read in one line.
#
# [[BPR-1179]]. It serves somebody else's agent, so it is an agent
# integration: an agent-free graph builds neither the library nor the helper.
if(NOT BPR_BUILD_AGENT_INTEGRATIONS)
    return()
endif()

add_library(
    better_pull_review_adapter_mcp
    STATIC
        src/mcp_context_helper.cpp
)
add_library(BetterPullReview::AdapterMcp ALIAS better_pull_review_adapter_mcp)
target_include_directories(
    better_pull_review_adapter_mcp
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    better_pull_review_adapter_mcp
    PUBLIC
        BetterPullReview::Core
)
target_compile_features(better_pull_review_adapter_mcp PUBLIC cxx_std_23)
bpr_enable_hardening(better_pull_review_adapter_mcp)
bpr_enable_warnings(better_pull_review_adapter_mcp)

bpr_add_executable(
    tool_better_pull_review_mcp_context_helper
    src/mcp_context_helper_main.cpp
    src/mcp_terminal_diagnostic.hpp
)
bpr_add_windows_version_resource(tool_better_pull_review_mcp_context_helper "BetterPullReview MCP context helper")
target_link_libraries(
    tool_better_pull_review_mcp_context_helper
    PRIVATE
        BetterPullReview::AdapterMcp
        Threads::Threads
)
bpr_enable_hardening(tool_better_pull_review_mcp_context_helper)
bpr_enable_warnings(tool_better_pull_review_mcp_context_helper)
