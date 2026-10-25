# [[BPR-1148]]. Review Cycles workspace contracts: the read-only local source
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

# [[BPR-1149]] and [[BPR-1150]]. Capture against real roots into the real store.
bpr_add_executable(test_review_cycle_capture review_cycle_capture_tests.cpp)
target_link_libraries(test_review_cycle_capture PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::Application)
bpr_enable_hardening(test_review_cycle_capture)
if(WIN32)
    target_link_libraries(test_review_cycle_capture PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_capture)
bpr_add_network_isolated_test(
    NAME review_cycle_capture
    WINDOWS_UNCONTAINED
    COMMAND test_review_cycle_capture ${bpr_workspace_git_arguments}
)

# [[BPR-1151]]. Comparison views over retained cycle material.
bpr_add_executable(test_review_cycle_comparison review_cycle_comparison_tests.cpp)
target_link_libraries(test_review_cycle_comparison PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::Application BetterPullReview::SQLite3)
bpr_enable_hardening(test_review_cycle_comparison)
if(WIN32)
    target_link_libraries(test_review_cycle_comparison PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_comparison)
bpr_add_network_isolated_test(NAME review_cycle_comparison WINDOWS_UNCONTAINED COMMAND test_review_cycle_comparison)

# [[BPR-1151]]. Scale measurement: capture, recapture, paging and diffs over a
# synthetic tree. The row runs a small tree and asserts only correctness and
# bounds; `--files 100000` is the LIMITS measurement, run by hand.
bpr_add_executable(bench_review_cycle_scale review_cycle_scale_bench.cpp)
target_link_libraries(bench_review_cycle_scale PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::Application)
bpr_enable_hardening(bench_review_cycle_scale)
if(WIN32)
    target_link_libraries(bench_review_cycle_scale PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(bench_review_cycle_scale)
bpr_add_network_isolated_test(NAME review_cycle_scale WINDOWS_UNCONTAINED COMMAND bench_review_cycle_scale --files 2000)
# [[BPR-1457]]. The Git metadata readers' scale and adversarial (H6) cases at a
# small size; `--git-index-entries 250000` is the RFC-BPR-0083 measurement, run
# by hand. Without Git the index half is skipped and says so.
bpr_add_network_isolated_test(
    NAME review_cycle_git_reader_scale
    WINDOWS_UNCONTAINED
    COMMAND bench_review_cycle_scale ${bpr_workspace_git_arguments} --git-index-entries 2000 --adversarial
)

# [[BPR-1152]]. Capture recovery and retained-material budgets. The kill course
# forks and delivers SIGKILL, so it runs on POSIX only; the rest runs everywhere.
bpr_add_executable(test_review_cycle_recovery review_cycle_recovery_tests.cpp)
target_link_libraries(test_review_cycle_recovery PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::Application BetterPullReview::SQLite3)
bpr_enable_hardening(test_review_cycle_recovery)
if(WIN32)
    target_link_libraries(test_review_cycle_recovery PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_recovery)
bpr_add_network_isolated_test(NAME review_cycle_recovery WINDOWS_UNCONTAINED COMMAND test_review_cycle_recovery)

# [[BPR-1153]]. Private cycle feedback on retained material through the runtime.
bpr_add_executable(test_review_cycle_feedback review_cycle_feedback_tests.cpp)
target_link_libraries(test_review_cycle_feedback PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::Application)
bpr_enable_hardening(test_review_cycle_feedback)
if(WIN32)
    target_link_libraries(test_review_cycle_feedback PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_feedback)
bpr_add_network_isolated_test(NAME review_cycle_feedback WINDOWS_UNCONTAINED COMMAND test_review_cycle_feedback)

# [[BPR-1155]]. Exact suggestion records, previews and preflight on retained material.
bpr_add_executable(test_review_cycle_suggestion review_cycle_suggestion_tests.cpp)
target_link_libraries(test_review_cycle_suggestion PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::Application)
bpr_enable_hardening(test_review_cycle_suggestion)
if(WIN32)
    target_link_libraries(test_review_cycle_suggestion PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_suggestion)
bpr_add_network_isolated_test(NAME review_cycle_suggestion WINDOWS_UNCONTAINED COMMAND test_review_cycle_suggestion)

# [[BPR-1157]]. The desktop's local cycle host: Open Project to a first note.
# [[BPR-1163]]. The manual iteration course: Capture revision, named views, checkpoints, Git chronology.
# [[BPR-1161]]. Compose feedback and Share feedback through the host, the store and chosen files.
bpr_add_executable(test_review_cycle_workbench review_cycle_workbench_tests.cpp)
target_link_libraries(test_review_cycle_workbench PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterExport BetterPullReview::AdapterSQLite BetterPullReview::Application)
bpr_enable_hardening(test_review_cycle_workbench)
if(WIN32)
    target_link_libraries(test_review_cycle_workbench PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_workbench)
bpr_add_network_isolated_test(NAME review_cycle_workbench WINDOWS_UNCONTAINED COMMAND test_review_cycle_workbench ${bpr_workspace_git_arguments})
# [[BPR-1161]]. It renders a 16 MiB round. Slowest isolated run: Windows Debug
# 38.13 s (MSVC 14.51, Qt 6.11.2, dbe6966ea); Linux about 31 s, macOS about 18 s.
# Three times the slowest, rounded up (docs/development/build-and-test-reference.md).
set_tests_properties(review_cycle_workbench PROPERTIES TIMEOUT 120)

# [[BPR-1158]]. Round drafts frozen into one canonical payload through the runtime and store.
bpr_add_executable(test_review_cycle_round review_cycle_round_tests.cpp)
target_link_libraries(test_review_cycle_round PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::Application)
bpr_enable_hardening(test_review_cycle_round)
if(WIN32)
    target_link_libraries(test_review_cycle_round PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_round)
bpr_add_network_isolated_test(NAME review_cycle_round WINDOWS_UNCONTAINED COMMAND test_review_cycle_round)

# [[BPR-1159]]. Copy and Save of a frozen round to a chosen file, with conflicts refused.
bpr_add_executable(test_review_cycle_export review_cycle_export_tests.cpp)
target_link_libraries(test_review_cycle_export PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterExport BetterPullReview::AdapterSQLite BetterPullReview::Application)
bpr_enable_hardening(test_review_cycle_export)
if(WIN32)
    target_link_libraries(test_review_cycle_export PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_export)
bpr_add_network_isolated_test(NAME review_cycle_export WINDOWS_UNCONTAINED COMMAND test_review_cycle_export)

# [[BPR-1160]]. A round saved as a bundle, read back and opened into another profile's quarantine.
bpr_add_executable(test_review_cycle_bundle review_cycle_bundle_tests.cpp)
target_link_libraries(test_review_cycle_bundle PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterExport BetterPullReview::AdapterSQLite BetterPullReview::Application)
bpr_enable_hardening(test_review_cycle_bundle)
if(WIN32)
    target_link_libraries(test_review_cycle_bundle PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_bundle)
bpr_add_network_isolated_test(NAME review_cycle_bundle WINDOWS_UNCONTAINED COMMAND test_review_cycle_bundle "${PROJECT_SOURCE_DIR}/docs/rfcs/assets/bpr-review-cycles-v1/examples")

# [[BPR-1162]]. Imported rounds attached through explicit mappings, forked, and prose-only replies.
bpr_add_executable(test_review_cycle_attach review_cycle_attach_tests.cpp)
target_link_libraries(test_review_cycle_attach PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::Application)
bpr_enable_hardening(test_review_cycle_attach)
if(WIN32)
    target_link_libraries(test_review_cycle_attach PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_attach)
bpr_add_network_isolated_test(NAME review_cycle_attach WINDOWS_UNCONTAINED COMMAND test_review_cycle_attach)

# [[BPR-1164]]. Recipient statements appended to frozen rounds through the runtime and store.
bpr_add_executable(test_review_cycle_response review_cycle_response_tests.cpp)
target_link_libraries(test_review_cycle_response PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::Application)
bpr_enable_hardening(test_review_cycle_response)
if(WIN32)
    target_link_libraries(test_review_cycle_response PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_response)
bpr_add_network_isolated_test(NAME review_cycle_response WINDOWS_UNCONTAINED COMMAND test_review_cycle_response)

# [[BPR-1165]]. Reconciliation, explicit verification and the next round's draft through the runtime and store.
bpr_add_executable(test_review_cycle_verification review_cycle_verification_tests.cpp)
target_link_libraries(test_review_cycle_verification PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::Application)
bpr_enable_hardening(test_review_cycle_verification)
if(WIN32)
    target_link_libraries(test_review_cycle_verification PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_verification)
bpr_add_network_isolated_test(NAME review_cycle_verification WINDOWS_UNCONTAINED COMMAND test_review_cycle_verification)

# [[BPR-1172]]. Planning outgoing content over an explicit Git base. Unlike the
# rows above, the product runs Git here: the installed-Git publication catalog
# reads P and T through its own private store, never the checkout's repository.
if(TARGET Git::Git)
    bpr_add_executable(test_review_cycle_outgoing review_cycle_outgoing_tests.cpp)
    target_link_libraries(test_review_cycle_outgoing PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::AdapterGit BetterPullReview::Application)
    bpr_enable_hardening(test_review_cycle_outgoing)
    if(WIN32)
        target_link_libraries(test_review_cycle_outgoing PRIVATE advapi32 BetterPullReview::Platform)
    endif()
    bpr_enable_warnings(test_review_cycle_outgoing)
    bpr_add_network_isolated_test(NAME review_cycle_outgoing WINDOWS_UNCONTAINED COMMAND test_review_cycle_outgoing ${bpr_workspace_git_arguments})
endif()

# [[BPR-1454]] and [[BPR-1461]]. The Git metadata readers against repositories
# real Git made: format refusals, config syntax, ignoreCase, POSIX classes, the
# index trailer and extensions, HEAD resolution and reftable.
if(TARGET Git::Git)
    bpr_add_executable(test_review_cycle_git_readers review_cycle_git_reader_tests.cpp)
    target_link_libraries(test_review_cycle_git_readers PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::AdapterGit BetterPullReview::Application)
    bpr_enable_hardening(test_review_cycle_git_readers)
    if(WIN32)
        target_link_libraries(test_review_cycle_git_readers PRIVATE advapi32 BetterPullReview::Platform)
    endif()
    bpr_enable_warnings(test_review_cycle_git_readers)
    bpr_add_network_isolated_test(NAME review_cycle_git_readers WINDOWS_UNCONTAINED COMMAND test_review_cycle_git_readers ${bpr_workspace_git_arguments})
endif()

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

# [[BPR-1173]]. Isolated squashed candidates: canaries, exact read-back, external patch.
if(TARGET Git::Git)
    bpr_add_executable(test_review_cycle_candidate review_cycle_candidate_tests.cpp)
    target_link_libraries(test_review_cycle_candidate PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::AdapterGit BetterPullReview::Application)
    bpr_enable_hardening(test_review_cycle_candidate)
    if(WIN32)
        target_link_libraries(test_review_cycle_candidate PRIVATE advapi32 BetterPullReview::Platform)
    endif()
    bpr_enable_warnings(test_review_cycle_candidate)
    bpr_add_network_isolated_test(NAME review_cycle_candidate WINDOWS_UNCONTAINED COMMAND test_review_cycle_candidate ${bpr_workspace_git_arguments})
endif()

# [[BPR-1174]]. The finite publication plan, its authorization and recovery journal.
if(TARGET Git::Git)
    bpr_add_executable(test_review_cycle_publication review_cycle_publication_tests.cpp)
    target_link_libraries(test_review_cycle_publication PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::AdapterGit BetterPullReview::Application)
    bpr_enable_hardening(test_review_cycle_publication)
    if(WIN32)
        target_link_libraries(test_review_cycle_publication PRIVATE advapi32 BetterPullReview::Platform)
    endif()
    bpr_enable_warnings(test_review_cycle_publication)
    bpr_add_network_isolated_test(NAME review_cycle_publication WINDOWS_UNCONTAINED COMMAND test_review_cycle_publication ${bpr_workspace_git_arguments})
endif()

# [[BPR-1175]]. Publishing exact objects and a new branch through the catalog port.
if(TARGET Git::Git)
    bpr_add_executable(test_review_cycle_publisher review_cycle_publisher_tests.cpp)
    target_link_libraries(test_review_cycle_publisher PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::AdapterGit BetterPullReview::Application)
    bpr_enable_hardening(test_review_cycle_publisher)
    if(WIN32)
        target_link_libraries(test_review_cycle_publisher PRIVATE advapi32 BetterPullReview::Platform)
    endif()
    bpr_enable_warnings(test_review_cycle_publisher)
    bpr_add_network_isolated_test(NAME review_cycle_publisher WINDOWS_UNCONTAINED COMMAND test_review_cycle_publisher ${bpr_workspace_git_arguments})
endif()

# [[BPR-1177]]. Publish as Pull Request through the desktop's cycle host:
# the preview's baselines, the finite plan, the association, privacy, an
# externally published link and history after the squash.
if(TARGET Git::Git)
    bpr_add_executable(test_review_cycle_publish_desk review_cycle_publish_desk_tests.cpp)
    target_link_libraries(test_review_cycle_publish_desk PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterExport BetterPullReview::AdapterSQLite BetterPullReview::AdapterGit BetterPullReview::Application)
    bpr_enable_hardening(test_review_cycle_publish_desk)
    if(WIN32)
        target_link_libraries(test_review_cycle_publish_desk PRIVATE advapi32 BetterPullReview::Platform)
    endif()
    bpr_enable_warnings(test_review_cycle_publish_desk)
    bpr_add_network_isolated_test(NAME review_cycle_publish_desk WINDOWS_UNCONTAINED COMMAND test_review_cycle_publish_desk ${bpr_workspace_git_arguments})
    set_tests_properties(review_cycle_publish_desk PROPERTIES TIMEOUT 180)

    # [[BPR-1178]]. RC59-07's crash, drift and recovery matrix as real
    # processes: the desktop's cycle host publishing to a simulated GitHub
    # whose state is a file, killed at every object, branch and pull request
    # boundary and restarted, with another actor's changes while nothing ran.
    bpr_add_executable(test_review_cycle_publication_matrix review_cycle_publication_matrix.cpp)
    target_link_libraries(test_review_cycle_publication_matrix PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::AdapterGit BetterPullReview::Application)
    bpr_enable_hardening(test_review_cycle_publication_matrix)
    if(WIN32)
        target_link_libraries(test_review_cycle_publication_matrix PRIVATE advapi32 BetterPullReview::Platform)
    endif()
    bpr_enable_warnings(test_review_cycle_publication_matrix)
    bpr_add_network_isolated_test(
        NAME review_cycle_publication_matrix
        WINDOWS_UNCONTAINED
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/review_cycle_publication_matrix_tests.py"
            --executable "$<TARGET_FILE:test_review_cycle_publication_matrix>"
            ${bpr_workspace_git_arguments}
    )
    set_tests_properties(review_cycle_publication_matrix PROPERTIES TIMEOUT 600 PASS_REGULAR_EXPRESSION "review-cycle-publication-matrix-tests:pass")
endif()

# [[BPR-1176]]. RC59-10's live publication course against the one owner-
# designated disposable repository. Not a test row and not built by default:
# it reaches GitHub.com, so only an explicit configure builds it, and only a
# person runs it (tests/scripts/run_live_publication_course.sh).
option(BPR_BUILD_LIVE_PUBLICATION_COURSE "Build the RC59-10 live publication course (reaches github.com/MikeMarcin/BPR-Test)" OFF)
if(BPR_BUILD_LIVE_PUBLICATION_COURSE AND BPR_BUILD_QT)
    bpr_add_executable(tool_review_cycle_live_publication review_cycle_live_publication.cpp)
    target_link_libraries(
        tool_review_cycle_live_publication
        PRIVATE
            BetterPullReview::AdapterWorkspace
            BetterPullReview::AdapterSQLite
            BetterPullReview::AdapterGit
            BetterPullReview::AdapterGitHub
            BetterPullReview::Application
            Qt6::Core
            Qt6::Network
    )
    target_include_directories(tool_review_cycle_live_publication PRIVATE "${PROJECT_SOURCE_DIR}/src/adapters/github/src")
    bpr_enable_hardening(tool_review_cycle_live_publication)
    if(WIN32)
        target_link_libraries(tool_review_cycle_live_publication PRIVATE advapi32 BetterPullReview::Platform)
    endif()
    bpr_enable_warnings(tool_review_cycle_live_publication)
endif()

# [[BPR-1180]]. Send to Agent through the host and the real store, the adapter scripted.
bpr_add_executable(test_review_cycle_send review_cycle_send_tests.cpp)
target_link_libraries(test_review_cycle_send PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterExport BetterPullReview::AdapterSQLite BetterPullReview::Application)
bpr_enable_hardening(test_review_cycle_send)
if(WIN32)
    target_link_libraries(test_review_cycle_send PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_send)
bpr_add_network_isolated_test(NAME review_cycle_send WINDOWS_UNCONTAINED COMMAND test_review_cycle_send)

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
