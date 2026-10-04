# The agent process adapter exists only where a process can be started. It sits
# outside `src/adapters/qt` deliberately: that directory is an effect-free
# region the architecture scan enforces, because presentation starts no
# processes. `src/adapters/git` is the precedent.
if(NOT BPR_BUILD_QT AND NOT BPR_BUILD_TOUR_CLI)
    return()
endif()

# [[BPR-1179]]. Two parts with different owners. The bundled supervisor and
# the header-only Windows workspace guard are the process owner a guarded Git
# launch runs inside ([[BPR-0622]]), so every graph with Git builds them. The
# launch adapter and its protocol loop start agents, and exist only with
# BPR_BUILD_AGENT_INTEGRATIONS: an agent-free graph has no target that can.
add_library(better_pull_review_workspace_guard INTERFACE)
add_library(BetterPullReview::WorkspaceGuard ALIAS better_pull_review_workspace_guard)
target_include_directories(better_pull_review_workspace_guard INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
# [[BPR-0825]]. The guard builds and checks its private objects with the
# platform target's one owner-private definition.
target_link_libraries(better_pull_review_workspace_guard INTERFACE BetterPullReview::Platform)
if(UNIX)
    bpr_add_executable(tool_better_pull_review_agent_supervisor src/unix_agent_supervisor.cpp)
    target_compile_features(tool_better_pull_review_agent_supervisor PRIVATE cxx_std_23)
    bpr_enable_hardening(tool_better_pull_review_agent_supervisor)
    bpr_enable_warnings(tool_better_pull_review_agent_supervisor)
endif()
if(WIN32)
    # [[BPR-0622]]. The Windows owner of a guarded child's job; see
    # windows_agent_supervisor.cpp. It links no Qt.
    bpr_add_executable(tool_better_pull_review_agent_supervisor src/windows_agent_supervisor.cpp)
    bpr_add_windows_version_resource(tool_better_pull_review_agent_supervisor "BetterPullReview agent supervisor")
    target_compile_features(tool_better_pull_review_agent_supervisor PRIVATE cxx_std_23)
    target_include_directories(tool_better_pull_review_agent_supervisor PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(tool_better_pull_review_agent_supervisor PRIVATE Kernel32 Advapi32)
    target_link_libraries(tool_better_pull_review_agent_supervisor PRIVATE BetterPullReview::Platform)
    bpr_enable_hardening(tool_better_pull_review_agent_supervisor)
    bpr_enable_warnings(tool_better_pull_review_agent_supervisor)
endif()
if(NOT BPR_BUILD_AGENT_INTEGRATIONS)
    return()
endif()

add_library(
    better_pull_review_adapter_agent
    STATIC
        src/agent_process_launcher.cpp
        src/agent_protocol_loop.cpp
)
if(UNIX)
    add_dependencies(better_pull_review_adapter_agent tool_better_pull_review_agent_supervisor)
    if(BUILD_TESTING)
        target_compile_definitions(better_pull_review_adapter_agent PRIVATE
            "BPR_AGENT_SUPERVISOR_EXECUTABLE=\"$<TARGET_FILE:tool_better_pull_review_agent_supervisor>\"")
    endif()
endif()
if(WIN32)
    target_sources(
        better_pull_review_adapter_agent
        PRIVATE
            src/windows_agent_process.cpp
    )
    add_dependencies(better_pull_review_adapter_agent tool_better_pull_review_agent_supervisor)
    if(BUILD_TESTING)
        target_compile_definitions(better_pull_review_adapter_agent PRIVATE
            "BPR_AGENT_SUPERVISOR_EXECUTABLE_W=L\"$<TARGET_FILE:tool_better_pull_review_agent_supervisor>\"")
    endif()
    target_link_libraries(
        better_pull_review_adapter_agent
        PRIVATE
            Kernel32
            Advapi32
    )
endif()
add_library(
    BetterPullReview::AdapterAgent
    ALIAS
    better_pull_review_adapter_agent
)
target_link_libraries(
    better_pull_review_adapter_agent
    PUBLIC
        BetterPullReview::Application
        BetterPullReview::WorkspaceGuard
    PRIVATE
        Qt6::Core
)
# [[BPR-0825]]. The public Windows workspace guard builds and checks its
# private objects with the platform target's one owner-private definition.
target_link_libraries(better_pull_review_adapter_agent PUBLIC BetterPullReview::Platform)
target_compile_features(better_pull_review_adapter_agent PUBLIC cxx_std_23)
if(BUILD_TESTING)
    target_compile_definitions(
        better_pull_review_adapter_agent
        PRIVATE
            BPR_BUILD_TESTING=1
    )
endif()
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_definitions(
        better_pull_review_adapter_agent
        PUBLIC
            _LIBCPP_ENABLE_EXPERIMENTAL
    )
endif()
bpr_enable_hardening(better_pull_review_adapter_agent)
bpr_enable_warnings(better_pull_review_adapter_agent)
