# [[BPR-1148]]. Workspace contracts: the read-only local source
# against disposable roots on the native filesystem. When the build found Git,
# that Git builds repository fixtures; the product under test never runs it.
set(bpr_workspace_git_arguments "")
if(TARGET Git::Git)
    set(bpr_workspace_git_arguments --git "$<TARGET_FILE:Git::Git>")
endif()

bpr_add_executable(test_workspace_source workspace_source_tests.cpp)
target_link_libraries(test_workspace_source PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite)
bpr_enable_hardening(test_workspace_source)
if(WIN32)
    # [[BPR-1126]]. scratch_root.hpp makes a Windows root with the platform
    # target's owner-private descriptor.
    target_link_libraries(test_workspace_source PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_workspace_source)
bpr_add_network_isolated_test(
    NAME workspace_source
    WINDOWS_UNCONTAINED
    COMMAND test_workspace_source ${bpr_workspace_git_arguments}
)

# [[BPR-1457]]. The Git metadata readers' scale and adversarial (H6) cases at a
# small size; `--git-index-entries 250000` is the RFC-BPR-0083 measurement, run
# by hand. Without Git the index half is skipped and says so. [[BPR-2308]]: the
# Review Cycle capture half of the old scale bench retired with Review Cycles.
bpr_add_executable(bench_git_metadata_scale git_metadata_scale_bench.cpp)
target_link_libraries(bench_git_metadata_scale PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::Application)
bpr_enable_hardening(bench_git_metadata_scale)
if(WIN32)
    target_link_libraries(bench_git_metadata_scale PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(bench_git_metadata_scale)
bpr_add_network_isolated_test(
    NAME review_cycle_git_reader_scale
    WINDOWS_UNCONTAINED
    COMMAND bench_git_metadata_scale ${bpr_workspace_git_arguments} --git-index-entries 2000 --adversarial
)

# [[BPR-1455]] and [[BPR-1456]]. RFC-BPR-0083's differential harness: the
# installed Git, isolated from system and global files, and each candidate
# implementation of the Git metadata components answer the same questions
# over the corpus; every divergence is classified under H1, and the canary
# course checks containment, no execution and no writes. `--large` adds the
# 250,000-entry case (BPR-1457 owns its timing). libgit2 (L) joins when the
# evaluation-only target exists, wherever it is defined.
if(TARGET Git::Git)
    bpr_add_executable(test_git_metadata_harness git_metadata_harness.cpp git_metadata_first_party.cpp)
    target_link_libraries(test_git_metadata_harness PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::Application
        "$<$<TARGET_EXISTS:bpr_libgit2_candidate>:bpr_libgit2_candidate>")
    target_compile_definitions(test_git_metadata_harness PRIVATE "$<$<TARGET_EXISTS:bpr_libgit2_candidate>:BPR_HAVE_LIBGIT2_CANDIDATE=1>")
    bpr_enable_hardening(test_git_metadata_harness)
    if(WIN32)
        target_link_libraries(test_git_metadata_harness PRIVATE advapi32 BetterPullReview::Platform)
    endif()
    bpr_enable_warnings(test_git_metadata_harness)
    bpr_add_network_isolated_test(NAME git_metadata_harness WINDOWS_UNCONTAINED COMMAND test_git_metadata_harness ${bpr_workspace_git_arguments})
endif()

# [[BPR-1916]] AC2. Repository skills are read at the pull request's base,
# never its head, by the product's installed-Git skill reader.
if(TARGET Git::Git)
    bpr_add_executable(test_repository_skill_reader repository_skill_reader_tests.cpp)
    target_link_libraries(test_repository_skill_reader PRIVATE BetterPullReview::AdapterSQLite BetterPullReview::AdapterGit BetterPullReview::Application)
    bpr_enable_hardening(test_repository_skill_reader)
    if(WIN32)
        target_link_libraries(test_repository_skill_reader PRIVATE advapi32 BetterPullReview::Platform)
    endif()
    bpr_enable_warnings(test_repository_skill_reader)
    bpr_add_network_isolated_test(NAME repository_skill_reader WINDOWS_UNCONTAINED COMMAND test_repository_skill_reader ${bpr_workspace_git_arguments})
    set_tests_properties(repository_skill_reader PROPERTIES TIMEOUT 120)
endif()

# [[BPR-1750]]. RFC-BPR-0092 D2: the clone folder scan on real folders.
bpr_add_executable(test_clone_folder_scan clone_folder_scan_tests.cpp)
target_link_libraries(test_clone_folder_scan PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite)
bpr_enable_hardening(test_clone_folder_scan)
if(WIN32)
    target_link_libraries(test_clone_folder_scan PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_clone_folder_scan)
bpr_add_network_isolated_test(NAME workspace_clone_folder_scan COMMAND test_clone_folder_scan)
set_tests_properties(workspace_clone_folder_scan PROPERTIES PASS_REGULAR_EXPRESSION "clone-folder-scan:pass" TIMEOUT 60)

# RFC-BPR-0108 ([[BPR-2019]], [[BPR-2020]]). A local PR draft from real
# worktrees: the remote chain, the comparison, working edits, the store.
if(TARGET Git::Git)
    bpr_add_executable(test_local_draft local_draft_tests.cpp)
    target_link_libraries(test_local_draft PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::AdapterGit BetterPullReview::Application BetterPullReview::SQLite3)
    bpr_enable_hardening(test_local_draft)
    if(WIN32)
        target_link_libraries(test_local_draft PRIVATE advapi32 BetterPullReview::Platform)
    endif()
    bpr_enable_warnings(test_local_draft)
    bpr_add_network_isolated_test(NAME local_draft WINDOWS_UNCONTAINED COMMAND test_local_draft ${bpr_workspace_git_arguments})
    set_tests_properties(local_draft PROPERTIES TIMEOUT 300)
    # RFC-BPR-0111 ([[BPR-2029]], [[BPR-2030]]).
    bpr_add_executable(test_local_draft_publication local_draft_publication_tests.cpp)
    target_link_libraries(test_local_draft_publication PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::AdapterGit BetterPullReview::Application)
    bpr_enable_hardening(test_local_draft_publication)
    if(WIN32)
        target_link_libraries(test_local_draft_publication PRIVATE advapi32 BetterPullReview::Platform)
    endif()
    bpr_enable_warnings(test_local_draft_publication)
    bpr_add_network_isolated_test(NAME local_draft_publication WINDOWS_UNCONTAINED COMMAND test_local_draft_publication ${bpr_workspace_git_arguments})
    set_tests_properties(local_draft_publication PROPERTIES TIMEOUT 300)
    # RFC-BPR-0113 and RFC-BPR-0114 as amended 2026-11-12 ([[BPR-2062]], [[BPR-2065]], [[BPR-2066]]).
    bpr_add_executable(test_local_changes local_changes_tests.cpp)
    target_link_libraries(test_local_changes PRIVATE BetterPullReview::AdapterGit BetterPullReview::AdapterSQLite BetterPullReview::Application)
    bpr_enable_hardening(test_local_changes)
    if(WIN32)
        target_link_libraries(test_local_changes PRIVATE advapi32 BetterPullReview::Platform)
    endif()
    bpr_enable_warnings(test_local_changes)
    bpr_add_network_isolated_test(NAME local_changes COMMAND test_local_changes ${bpr_workspace_git_arguments})
    set_tests_properties(local_changes PROPERTIES TIMEOUT 300 PASS_REGULAR_EXPRESSION "local-changes:(pass|skipped)")
endif()
