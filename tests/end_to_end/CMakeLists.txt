if(APPLE OR WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(bpr_native_platform_vault_self_test_result "true")
else()
    set(bpr_native_platform_vault_self_test_result "false")
endif()

# RFC-BPR-0016, as BPR-0515 left it. What the shipped device-authorization
# selection answers in the dev-tree matrix, on one mechanism for every platform
# and every row that uses it. `DEVICE_FLOW_REQUESTS` counts submissions that
# reached the refusing transport, never HTTP requests, so zero means the
# capability gate held and is the one value every platform shares.
#
# macOS is registered under the recorded distributable identity since BPR-0318,
# Linux since BPR-0310, and Windows since BPR-0311, so on all three the real
# flow starts and the refusing transport is never touched. What it starts
# against is the difference this expression exists to remove. macOS used to get
# its terminal from sandbox-exec and Linux from a network namespace whose only
# interface is loopback, while Windows carries WINDOWS_UNCONTAINED - BPR-0362
# exempts these rows from the AppContainer because a lowbox token cannot give
# them the drive-letter link, the mount-point manager, or the pipe namespace -
# so on Windows nothing stood in front of the network at all, the flow reached
# `awaiting_authorization` against github.com, and a device code was minted once
# per run per row.
#
# `--network-denied` replaces all three with the application denying its own
# egress: the device-authorization wire is composed against a closed loopback
# port rather than the discovered system proxy, so the connect is refused inside
# the process before anything is dialled, whatever the harness or the station
# can reach. It denies at the socket, which is what these rows actually needed
# and the one thing containment could never have given them, so
# WINDOWS_UNCONTAINED stays and is now beside the point.
#
# One mechanism, so one value: `failed`, which macOS and Linux already recorded
# and which no longer depends on anything being reachable. An unregistered
# platform composes no wire for the override to apply to, so it keeps answering
# the typed refusal.
if(APPLE OR WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(bpr_device_selection_denied_self_test_expression
        "DEVICE_FLOW_STAGE:failed[\r\n]+DEVICE_FLOW_REQUESTS:0[\r\n]+DEVICE_FLOW_REGISTERED_SELECTION_OK")
else()
    set(bpr_device_selection_denied_self_test_expression
        "DEVICE_FLOW_STAGE:unavailable[\r\n]+DEVICE_FLOW_REQUESTS:0[\r\n]+DEVICE_FLOW_UNAVAILABLE_OK")
endif()

# An automated application run bounds itself in wall clock so that it always
# reports its stage, verdict, and evidence before CTest kills the process. A
# run killed by the harness reports nothing at all: that is what made a stalled
# stage unattributable, and it is the whole reason these two numbers exist.
#
# They only mean anything together, so this states the relationship once and
# refuses a pair that does not hold. `RUNS` is how many times the test starts
# the application under a budget. `RESERVE_MS` is everything the budget does
# not cover: process start, shutdown, writing evidence, and any invocation that
# exits during argument parsing before a run is ever scheduled.
#
# The rule is that the worst case the application permits itself must fit
# inside three quarters of the harness allowance. The remaining quarter belongs
# to CTest and is never meant to be spent.
#
# Setting `TIMEOUT` only through this function is deliberate. A timeout raised
# or a budget lowered on its own is exactly the drift that produced the
# original failure, and here it cannot be done without restating both.
function(bpr_bound_automated_run bpr_test_name)
    cmake_parse_arguments(
        PARSE_ARGV
        1
        arg
        ""
        "TIMEOUT;BUDGET_MS;RUNS;RESERVE_MS"
        ""
    )
    foreach(required IN ITEMS TIMEOUT BUDGET_MS RUNS RESERVE_MS)
        if(NOT DEFINED arg_${required})
            message(
                FATAL_ERROR
                "${bpr_test_name}: ${required} is required to bound the run"
            )
        endif()
    endforeach()

    math(
        EXPR
        bpr_worst_case_ms
        "${arg_RUNS} * ${arg_BUDGET_MS} + ${arg_RESERVE_MS}"
    )
    math(EXPR bpr_harness_ms "${arg_TIMEOUT} * 1000")
    math(EXPR bpr_permitted_ms "${bpr_harness_ms} * 3 / 4")
    if(bpr_worst_case_ms GREATER bpr_permitted_ms)
        message(
            FATAL_ERROR
            "${bpr_test_name}: the application would permit itself "
            "${bpr_worst_case_ms} ms (${arg_RUNS} run(s) of ${arg_BUDGET_MS} ms "
            "plus ${arg_RESERVE_MS} ms outside the budget), which exceeds the "
            "${bpr_permitted_ms} ms three-quarter share of its ${arg_TIMEOUT} s "
            "CTest timeout. A stalled stage would be killed by the harness with "
            "no stage, verdict, or evidence reported."
        )
    endif()

    math(EXPR bpr_headroom_ms "${bpr_harness_ms} - ${bpr_worst_case_ms}")
    message(
        STATUS
        "bpr: ${bpr_test_name} bounds itself at ${bpr_worst_case_ms} ms inside "
        "a ${bpr_harness_ms} ms timeout (${bpr_headroom_ms} ms headroom)"
    )
    set_tests_properties("${bpr_test_name}" PROPERTIES TIMEOUT ${arg_TIMEOUT})
endfunction()

# Sizing and failure-reporting rule: docs/development/build-and-test.md,
# "Run budgets and row timeouts" (BPR-0905).
# Budgets, each read together with the timeout it sits inside. See
# `bpr_bound_automated_run` above and `default_automated_run_budget` in
# `src/app/main.cpp`.
#
# Each is set against what the run actually takes on an idle host, with room
# for a loaded one. Measured on macOS 15.7.7 arm64, Qt 6.11.1:
#
#   self test                       0.5 s   budget 8 s     16x
#   fixture render and workbench   12-13 s  budget 40 s     3x
#   native appearance probe        ~11 s    budget 30 s   2.7x
#   epoch phase                     25 s    budget 120 s  4.8x
#
# A budget is not a target. It is the point past which waiting has stopped
# being useful and the run should say what it was waiting for.
#
# The epoch phase is the one whose headroom was measured against a host doing
# something else, because that is where it kept running out (BPR-0555). Four
# rows failed the RFC-BPR-0016 closeout matrix on 2026-09-03 at 60 s, and were
# reproduced failing at the head that session started from, so the seed was not
# slow by construction; the host was reindexing. Re-measured on 2026-09-03 on
# macOS 15.7.7 arm64, Qt 6.11.1, three consecutive seed phases driven directly:
#
#   quiet host, previously recorded            25.0 s
#   load average 5.1 (browser, an Android      34.7 s, 33.3 s, 33.4 s
#     emulator, WindowServer)
#   the hosts that failed, from their own      60.0 s to 68.6 s
#     PUBLIC_PATH_BUDGET lines
#
# 120 s is 4.8x the quiet measurement, 3.5x the loaded one, and 1.75x the worst
# a real host has actually taken. The seed itself was left alone: its cost is
# writing a Git object cache and a SQLite catalog for real, which is the point
# of the fixture, and making it cheaper means changing what these rows drive.
set(bpr_self_test_budget_ms 8000)
set(bpr_fixture_run_budget_ms 40000)
set(bpr_native_appearance_budget_ms 30000)
set(bpr_epoch_phase_budget_ms 120000)
set(bpr_stall_budget_ms 5000)

set(bpr_test_qt_root "")
if(DEFINED Qt6_ROOT)
    set(bpr_test_qt_root "${Qt6_ROOT}")
endif()
set(bpr_test_qt_dir "")
if(DEFINED BPR_PRODUCER_QT6_DIR)
    set(bpr_test_qt_dir "${BPR_PRODUCER_QT6_DIR}")
endif()

bpr_add_network_isolated_test(
    NAME application_public_path
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_public_path
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+NATIVE_PLATFORM_VAULT_SELECTED:${bpr_native_platform_vault_self_test_result}[\r\n]+CLI_ENTRY_ABSENT_OK[\r\n]+CLI_ENTRY_CARDS:1"
)
bpr_bound_automated_run(
    application_public_path
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

bpr_add_network_isolated_test(
    NAME application_public_path_ghes_cli_unavailable
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --ghes-host
        ghe.example.invalid
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_public_path_ghes_cli_unavailable
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+NATIVE_PLATFORM_VAULT_SELECTED:${bpr_native_platform_vault_self_test_result}[\r\n]+CLI_ENTRY_ABSENT_OK[\r\n]+CLI_ENTRY_CARDS:2"
)
bpr_bound_automated_run(
    application_public_path_ghes_cli_unavailable
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# RFC-BPR-0016. Three selectors, because "the choice exists", "GitHub.com
# refuses it", and "GHES refuses it too" fail for different reasons and one
# combined row would hide which. Each drives the shipped binary, so the
# evidence is the composed application rather than a helper that links the same
# symbols. The tests-disabled Release absence scan is a separate closeout
# command in docs/development/build-and-test.md, exactly as the CLI one is: a
# tests-enabled graph cannot prove what a tests-disabled binary omits.
bpr_add_network_isolated_test(
    NAME application_device_authorization_public_path
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --device-authorization-probe
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_device_authorization_public_path
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+NATIVE_PLATFORM_VAULT_SELECTED:${bpr_native_platform_vault_self_test_result}[\r\n]+CLI_ENTRY_ABSENT_OK[\r\n]+CLI_ENTRY_CARDS:1[\r\n]+DEVICE_FLOW_HOST_COMPOSED:true[\r\n]+DEVICE_FLOW_PRESENTED:true"
)
bpr_bound_automated_run(
    application_device_authorization_public_path
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# BPR-0515. `WINDOWS_UNCONTAINED` stays: BPR-0362's reasoning is unchanged, and
# this test still needs the drive-letter link, the mount-point manager, and the
# pipe namespace that a lowbox token cannot reach. `--network-denied` does not
# contain the process; it denies at the socket, which is what this row actually
# needed and the one thing the container could never have given it.
# The selector's name predates the registered selection: on an unregistered
# platform its stage is `unavailable`, and on every shipped platform it is now
# the dev-tree device-authorization gate under local denial (BPR-0515), landing
# on `failed`. The name stays because six gates bind it by string and the closed
# cards record it (BPR-0285); read it as "the dev-tree device gate".
bpr_add_network_isolated_test(
    NAME application_device_authorization_unavailable
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --ghes-host
        ghe.example.invalid
        --device-authorization-probe
        --network-denied
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_device_authorization_unavailable
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "${bpr_device_selection_denied_self_test_expression}"
)
bpr_bound_automated_run(
    application_device_authorization_unavailable
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# The registered course, and the only place RFC-BPR-0016's headline claim can
# be observed rather than argued: the recorded BPR-0276 grant runs through the
# real host, the real enrollment boundary, and the real vault, and the token is
# then looked for in every file the application wrote. A registered capability
# exists nowhere else - BPR-0276's identity is private and owner-only and a
# distributable one is BPR-0318's - so this course needs the fixture, and it
# ends at the grant's terminal rather than continuing into the fixture's own
# pull-request stages.
#
# BPR-0539. The count alone did not make that claim. The course connected the
# fixture account before it began, `github_connect_action` enrolled the
# fixture's own 32 random bytes, and the one store this row counted was that
# credential rather than the grant - whose own enrollment then arrived as a
# second establishment the fixture vault refused. The course now runs the
# grant as its first session, and the row reads the identity of what was
# stored rather than only how often: `DEVICE_FLOW_VAULT_STORED_TOKEN` is the
# leading 16 hex digits of the SHA-256 of the bytes the vault accepted, and
# the value pinned here is that digest for the recorded grant's own access
# token - `printf '%s' fixture-access-token-0276 | shasum -a 256` - so the row
# is anchored to a value outside the process rather than to the fixture's own
# say-so. `DEVICE_FLOW_STORED_IS_GRANT` is the comparison the fixture makes
# between the two, and the connect credential is random per run, so it can
# match neither.
bpr_add_network_isolated_test(
    NAME application_device_authorization_registered_course
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --github-fixture
        github-com
        --device-authorization-probe
        --device-authorization-registered-course
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_device_authorization_registered_course
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "DEVICE_FLOW_STAGE:authorized[\r\n]+DEVICE_FLOW_REQUESTS:0[\r\n]+DEVICE_FLOW_VAULT_STORED_TOKEN:f5505a4b89c07e21[\r\n]+DEVICE_FLOW_STORED_IS_GRANT:true[\r\n]+DEVICE_FLOW_REFRESH_RETAINED:false[\r\n]+DEVICE_FLOW_CLEANUP_FAILED:false[\r\n]+DEVICE_FLOW_ENROLLMENTS:1[\r\n]+DEVICE_FLOW_VAULT_STORES:1[\r\n]+DEVICE_FLOW_DURABLE_COPIES:0[\r\n]+DEVICE_FLOW_TRANSPORT_SUBMISSIONS:2[\r\n]+DEVICE_FLOW_ENROLLED_OK"
)
bpr_bound_automated_run(
    application_device_authorization_registered_course
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# BPR-0536. The registered course carried one step past its terminal, and the
# only place RFC-BPR-0016's claim-matrix row 9 can be observed through the
# composed graph rather than at the host: the recorded grant enrolls, the
# composition binds the refresh authority from the accepted session, and the
# renewal rotates the access and refresh material under that same vault
# authority before the access token expires. Two vault stores, because the
# rotation enrolls the rotated access token under the authority the grant
# established - that is the rotation being durable rather than a second
# enrollment - and `DEVICE_FLOW_DURABLE_COPIES` stays zero because nothing the
# renewal writes goes anywhere but the vault. `WINDOWS_UNCONTAINED` and the
# isolation keyword are the registered course's, unchanged: this row runs the
# same graph with one more flag.
#
# BPR-0547. `DEVICE_FLOW_REFRESH_DELETES:1` is which erasure the retained flag
# beside it reports. The rotation ends by enrolling the rotated access token,
# which supersedes the session the grant established; a build that carries the
# refresh slot's erasure along with that supersession destroys the material the
# rotation stored a moment earlier, and the orderly shutdown's own erasure
# afterwards leaves `RETAINED:false` looking exactly the same. One erasure is
# the shutdown's. Two is the renewal chain destroying itself, and the next
# renewal answering `refresh_absent`.
#
# BPR-0547. And `DEVICE_FLOW_ROTATIONS:2`, because one rotation is satisfied by
# a chain that has already destroyed itself: nothing has had to spend the
# material the first rotation wrote. The second renewal is the one that reads
# the slot the first filled, so the row now drives it -- the composed clock
# steps once per renewal the host arms rather than once, which is the only
# input a course cannot otherwise supply. Every count moves with it: a third
# vault store for the second rotation's access token, a third refresh store for
# its renewable material, and `DEVICE_FLOW_REFRESH_STORED_TOKEN` naming which
# rotation last wrote the slot, since the fixture's renewal answers carry the
# renewal's number. Read beside the single erasure, that digest is the second
# rotation's material being durable until the orderly shutdown took it. The
# erasure count does not move: retention is what makes the second rotation
# possible, so the shutdown is still the only occasion that erases.
#
# The bounds are raised, and for a measured reason. Each of the two renewals is
# made due one second from the moment it is armed rather than at once: a
# rotation arms the next renewal in the same turn that enrolls its new access
# token, while that enrollment is still travelling through the profile
# activation and the vault operations that decide what the refresh slot holds.
# A renewal due in that same instant outruns them and reads a slot nothing has
# touched yet, which is a race the shipped product never runs -- there the next
# renewal is eight hours after the supersession settled. So the row costs two
# seconds of deliberate waiting it did not cost before, and it is measured
# rather than assumed: 8.5 s beside one other test and 10.1 s alone on a
# JaegAW macOS station with another build running, against 5.6 s when both
# renewals were due the instant they were armed. The two seconds are a fixed
# cost rather than variance, which is why the budget moves with them.
set(bpr_refresh_course_budget_ms 12000)
bpr_add_network_isolated_test(
    NAME application_device_authorization_refresh_course
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --github-fixture
        github-com
        --device-authorization-probe
        --device-authorization-refresh-course
        --automated-run-budget-ms
        ${bpr_refresh_course_budget_ms}
)
set_tests_properties(
    application_device_authorization_refresh_course
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "DEVICE_FLOW_STAGE:authorized[\r\n]+DEVICE_FLOW_REQUESTS:0[\r\n]+DEVICE_FLOW_ROTATIONS:2[\r\n]+DEVICE_FLOW_REFRESH_STATE:scheduled[\r\n]+DEVICE_FLOW_VAULT_STORES:3[\r\n]+DEVICE_FLOW_DURABLE_COPIES:0[\r\n]+DEVICE_FLOW_REFRESH_BOUND:true[\r\n]+DEVICE_FLOW_REFRESH_RESUMES:0[\r\n]+DEVICE_FLOW_REFRESH_RETAINED:false[\r\n]+DEVICE_FLOW_REFRESH_STORES:3[\r\n]+DEVICE_FLOW_REFRESH_DELETES:1[\r\n]+DEVICE_FLOW_REFRESH_STORED_TOKEN:e2e8c030e3a107e9[\r\n]+DEVICE_FLOW_CLEANUP_FAILED:false[\r\n]+DEVICE_FLOW_REFRESH_OK"
)
bpr_bound_automated_run(
    application_device_authorization_refresh_course
    TIMEOUT 25
    BUDGET_MS ${bpr_refresh_course_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# BPR-0540. The renewal course's composition driven the other way round, and
# the only place the review's finding can be observed rather than argued: the
# refresh slot already holds a past grant's material when the run starts - the
# state an orderly shutdown used to leave behind, keyed by authority and
# outliving the process that earned it - and the session this run establishes
# is the credential the reviewer pasted. Presence of that material is not
# permission to spend it. A build that answers renewability from the slot
# alone resumes here, rotates, and supersedes the pasted session with a device
# one nobody asked for; the pinned counters are each half of that sequence
# refusing to start, and `DEVICE_FLOW_STORED_IS_PASTED` is the load-bearing
# one, because only the digest can tell the two candidate sessions apart. The
# digests are random per run - the connect credential is 32 fresh bytes - so
# the row pins their equality rather than their value. `REFRESH_RETAINED:false`
# is the other half of the same slice: the orphan is erased at shutdown rather
# than merely left unspent. `WINDOWS_UNCONTAINED` and the isolation keyword are
# the renewal course's, unchanged: this row runs the same graph, seeded, with
# one flag more.
bpr_add_network_isolated_test(
    NAME application_device_authorization_orphan_course
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --github-fixture
        github-com
        --device-authorization-probe
        --device-authorization-orphan-course
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_device_authorization_orphan_course
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "DEVICE_FLOW_REFRESH_RESUMES:0[\r\n]+DEVICE_FLOW_ROTATIONS:0[\r\n]+DEVICE_FLOW_VAULT_STORES:1[\r\n]+DEVICE_FLOW_REFRESH_STORES:0[\r\n]+DEVICE_FLOW_REFRESH_RETAINED:false[\r\n]+DEVICE_FLOW_VAULT_STORED_TOKEN:[0-9a-f]+[\r\n]+DEVICE_FLOW_PASTED_TOKEN:[0-9a-f]+[\r\n]+DEVICE_FLOW_STORED_IS_PASTED:true[\r\n]+DEVICE_FLOW_ORPHAN_OK"
)
bpr_bound_automated_run(
    application_device_authorization_orphan_course
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# RFC-BPR-0016's browser handoff, and its construction cut. Both drive the
# registered course, because `awaiting_authorization` is the only stage that
# has an address to open and the recorded grant is the only composition that
# reaches it without a human. Neither starts a browser: a tests-enabled graph
# binds the recording opener rather than `system_browser_opener`, so what these
# prove is that the action reaches a bound capability and what the host makes
# of the answer. That the shipped opener is the one bound in a shipped graph is
# proven by linkage (`composed_application_hosts`) and by construction; that it
# reaches a real browser is a target-native observation the stations record.
bpr_add_network_isolated_test(
    NAME application_device_authorization_browser_handoff
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --github-fixture
        github-com
        --device-authorization-probe
        --device-authorization-registered-course
        --device-authorization-browser-probe
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_device_authorization_browser_handoff
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "DEVICE_FLOW_BROWSER_COMPOSED:true[\r\n]+DEVICE_FLOW_BROWSER_OPENS:1[\r\n]+DEVICE_FLOW_HANDOFF:opened[\r\n]+DEVICE_FLOW_ENROLLMENTS:1"
)
bpr_bound_automated_run(
    application_device_authorization_browser_handoff
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# The construction cut. One construction removed and nothing else: the same
# course, the same flow, the same grant reaching the same vault once, with no
# browser capability bound. The action still answers - `unavailable`, which is
# what every build did before the opener was composed - and the positive marker
# is false, so a run that quietly stopped composing the opener could not pass
# the handoff contract above by accident.
bpr_add_network_isolated_test(
    NAME application_device_authorization_browser_absent
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --github-fixture
        github-com
        --device-authorization-probe
        --device-authorization-registered-course
        --device-authorization-browser-probe
        --device-authorization-browser-absent
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_device_authorization_browser_absent
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "DEVICE_FLOW_PRESENTED:true[\r\n]+DEVICE_FLOW_STAGE:authorized[\r\n]+DEVICE_FLOW_REQUESTS:0[\r\n]+DEVICE_FLOW_BROWSER_COMPOSED:false[\r\n]+DEVICE_FLOW_BROWSER_OPENS:0[\r\n]+DEVICE_FLOW_HANDOFF:unavailable[\r\n]+DEVICE_FLOW_ENROLLMENTS:1"
)
bpr_bound_automated_run(
    application_device_authorization_browser_absent
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# BPR-0797. Which capabilities the runnable application composes, asserted by
# identity, in the two configurations it is run in.
#
# Every other row here either injects its own opener or asserts widgets and
# sentences, so every one of them passed while the button a person presses
# reached `recording_device_authorization_browser` -- whose `open()` is
# `++Opens; return true;` -- and the account row said "Opened in your browser."
# about a browser nothing had asked to open. The owner reported exactly that.
# `device_browser_opens()` had no reader in tests/ at all.
#
# The pair differs in one thing: whether the automated test boundary marker
# that `bpr_add_network_isolated_test` injects is present. The first row is an
# ordinary run and must compose the recorder, which is the guarantee that no
# suite can start a browser. The second takes the marker away, which is the
# configuration a person's own build runs in, and must compose
# `system_browser_opener` and offer the real GitHub CLI read.
#
# Neither opens anything. The probe composes, reports and shuts down at the
# start stage: it selects no device path, so no verification address exists to
# hand anywhere, and it runs no CLI course, so it needs no platform vault and
# passes on a station whose keychain is locked (the limitation BPR-0775
# recorded for every fixture-less self-test row). `--network-denied` composes
# the device wire against a closed loopback port besides, and on macOS the
# sandbox denies egress. Independent reasons, because the row that gets this
# wrong opens a browser on whoever is running the suite.
# [[BPR-0982]]. Since [[BPR-0909]] the CLI way in is the Use GitHub CLI entry,
# wired only where the composed run offers the read; no card carries a
# precomposed GitHub CLI label any more.
if(APPLE)
    set(bpr_product_run_cli_read "available")
    set(bpr_product_run_cli_entry "offered")
else()
    # RFC-BPR-0015 enables the CLI read on macOS only, so off macOS the product
    # run composes the same answer as a suite does, for a different reason.
    set(bpr_product_run_cli_read "unavailable")
    set(bpr_product_run_cli_entry "absent")
endif()
bpr_add_network_isolated_test(
    NAME application_composition_capabilities_inside_boundary
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --composition-capability-probe
        --network-denied
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_composition_capabilities_inside_boundary
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "COMPOSITION_TEST_BOUNDARY:inside[\r\n]+COMPOSITION_BROWSER:recording[\r\n]+COMPOSITION_CLI_READ:unavailable[\r\n]+COMPOSITION_CLI_ENTRY:absent"
)
bpr_bound_automated_run(
    application_composition_capabilities_inside_boundary
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

bpr_add_network_isolated_test(
    NAME application_composition_capabilities_product_run
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --composition-capability-probe
        --network-denied
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_composition_capabilities_product_run
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "COMPOSITION_TEST_BOUNDARY:outside[\r\n]+COMPOSITION_BROWSER:system[\r\n]+COMPOSITION_CLI_READ:${bpr_product_run_cli_read}[\r\n]+COMPOSITION_CLI_ENTRY:${bpr_product_run_cli_entry}"
)
# The one row in the tree that stands outside the boundary on purpose. Appended
# after the seam's own `set:1`, and `ENVIRONMENT_MODIFICATION` is applied in
# list order, so this erases it for this row and no other.
set_property(
    TEST application_composition_capabilities_product_run
    APPEND
    PROPERTY ENVIRONMENT_MODIFICATION "BPR_AUTOMATED_TEST_BOUNDARY=unset:"
)
bpr_bound_automated_run(
    application_composition_capabilities_product_run
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# [[BPR-0526]]'s mount, and its construction cut, on the same terms as the
# browser pair above: one course, one difference. The first starts the flow by
# pressing the begin control on the panel the shipped window mounts, found by
# the accessible name it announces rather than through any accessor the
# composition holds, and then makes the identical registered-course claim -
# same terminal, same single enrollment, same single vault store, same two
# recorded submissions. A window that stopped mounting the panel, or a
# composition that stopped wiring it, reports `DEVICE_FLOW_PANEL_BEGAN:false`
# and never reaches that claim.
#
# The second removes exactly one thing: composition adopts neither edge of the
# panel. The window still draws it, `select_device_authorization` still starts
# the same flow to the same terminal, and the positive marker is false - so a
# graph that quietly stopped composing the panel cannot pass the course above
# by accident.
bpr_add_network_isolated_test(
    NAME application_device_authorization_panel_course
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --github-fixture
        github-com
        --device-authorization-probe
        --device-authorization-registered-course
        --device-authorization-panel-course
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_device_authorization_panel_course
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "DEVICE_FLOW_ENROLLMENTS:1[\r\n]+DEVICE_FLOW_VAULT_STORES:1[\r\n]+DEVICE_FLOW_DURABLE_COPIES:0[\r\n]+DEVICE_FLOW_TRANSPORT_SUBMISSIONS:2[\r\n]+DEVICE_FLOW_ENROLLED_OK[\r\n]+DEVICE_FLOW_PANEL_BEGAN:true[\r\n]+DEVICE_FLOW_PANEL_COMPOSED:true"
)
bpr_bound_automated_run(
    application_device_authorization_panel_course
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

bpr_add_network_isolated_test(
    NAME application_device_authorization_panel_absent
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --github-fixture
        github-com
        --device-authorization-probe
        --device-authorization-registered-course
        --device-authorization-panel-absent
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_device_authorization_panel_absent
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "DEVICE_FLOW_ENROLLMENTS:1[\r\n]+DEVICE_FLOW_VAULT_STORES:1[\r\n]+DEVICE_FLOW_DURABLE_COPIES:0[\r\n]+DEVICE_FLOW_TRANSPORT_SUBMISSIONS:2[\r\n]+DEVICE_FLOW_ENROLLED_OK[\r\n]+DEVICE_FLOW_PANEL_COMPOSED:false"
)
bpr_bound_automated_run(
    application_device_authorization_panel_absent
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# BPR-0535. The enterprise negative the row above could never make: every
# course beside it begins for the front profile, and the picker is the only
# thing that can bind the mounted panel to another one. The capability is
# composed for GitHub.com, so a begin pressed while a GHES profile is selected
# has to end in the typed refusal with nothing built - on every platform,
# registered or not, which is why the expression names no platform. The wire is
# composed under local denial like the gate above, so a run that stopped
# refusing reaches a closed loopback port rather than github.com.
bpr_add_network_isolated_test(
    NAME application_device_authorization_ghes_refusal
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --ghes-host
        ghe.example.invalid
        --device-authorization-probe
        --device-authorization-ghes-course
        --network-denied
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_device_authorization_ghes_refusal
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "DEVICE_FLOW_STAGE:unavailable[\r\n]+DEVICE_FLOW_REQUESTS:0[\r\n]+DEVICE_FLOW_GHES_BEGIN_REFUSED:true[\r\n]+DEVICE_FLOW_GHES_REFUSAL_OK"
)
bpr_bound_automated_run(
    application_device_authorization_ghes_refusal
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

bpr_add_network_isolated_test(
    NAME application_device_authorization_release_contract
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --device-authorization-probe
        --network-denied
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_device_authorization_release_contract
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        # The whole shipped chain in one expression, in order: the real
        # platform vault rather than a fixture, the existing CLI refusal
        # unchanged beside it, and then the device path composed, presented,
        # and answering the platform's shipped selection — the typed refusal
        # where unregistered, the started-then-locally-refused real flow where
        # registered. This row runs the same tests-enabled binary against the
        # same registered selection as the gate above, so BPR-0515 gives it the
        # same `--network-denied` and the same terminal: the dev-tree matrix
        # makes no real GitHub connection on any platform. What the override
        # does not change is what this row is for — the shipped composition,
        # the real vault, and the shipped capability are exactly what a
        # tests-disabled Release inherits. What it cannot check here is symbol
        # absence, which is the separate closeout scan.
        PASS_REGULAR_EXPRESSION
            "NATIVE_PLATFORM_VAULT_SELECTED:${bpr_native_platform_vault_self_test_result}[\r\n]+CLI_ENTRY_ABSENT_OK[\r\n]+CLI_ENTRY_CARDS:1[\r\n]+DEVICE_FLOW_HOST_COMPOSED:true[\r\n]+DEVICE_FLOW_PRESENTED:true[\r\n]+${bpr_device_selection_denied_self_test_expression}"
)
bpr_bound_automated_run(
    application_device_authorization_release_contract
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# RFC-BPR-0017's independent expected platform selection. Activating a mode
# sends nothing. The historical unavailable row explicitly names GHES, which
# remains refused on enabled macOS and unavailable on the other platforms.
set(bpr_public_preview_selection_self_test_expression
    "PUBLIC_PREVIEW_COMPOSED:false[\r\n]+PUBLIC_PREVIEW_STAGE:unavailable[\r\n]+PUBLIC_PREVIEW_REQUESTS:0[\r\n]+PUBLIC_PREVIEW_HEADER_EFFECTS:0[\r\n]+PUBLIC_PREVIEW_VAULT_EFFECTS:0[\r\n]+PUBLIC_PREVIEW_SIGN_IN_REQUESTS:0[\r\n]+PUBLIC_PREVIEW_UNAVAILABLE_OK"
)
set(bpr_public_preview_enterprise_self_test_expression
    "${bpr_public_preview_selection_self_test_expression}"
)
if(APPLE)
    set(bpr_public_preview_selection_self_test_expression
        "PUBLIC_PREVIEW_COMPOSED:true[\r\n]+PUBLIC_PREVIEW_STAGE:enabled[\r\n]+PUBLIC_PREVIEW_REQUESTS:0[\r\n]+PUBLIC_PREVIEW_HEADER_EFFECTS:0[\r\n]+PUBLIC_PREVIEW_VAULT_EFFECTS:0[\r\n]+PUBLIC_PREVIEW_SIGN_IN_REQUESTS:0[\r\n]+PUBLIC_PREVIEW_ENABLED_OK"
    )
    set(bpr_public_preview_enterprise_self_test_expression
        "PUBLIC_PREVIEW_COMPOSED:true[\r\n]+PUBLIC_PREVIEW_STAGE:refused[\r\n]+PUBLIC_PREVIEW_REQUESTS:0[\r\n]+PUBLIC_PREVIEW_HEADER_EFFECTS:0[\r\n]+PUBLIC_PREVIEW_VAULT_EFFECTS:0[\r\n]+PUBLIC_PREVIEW_SIGN_IN_REQUESTS:0[\r\n]+PUBLIC_PREVIEW_REFUSED_OK"
    )
endif()

bpr_add_network_isolated_test(
    NAME application_anonymous_public_preview_unavailable
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --ghes-host
        ghe.example.invalid
        --public-preview-probe
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_anonymous_public_preview_unavailable
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+NATIVE_PLATFORM_VAULT_SELECTED:${bpr_native_platform_vault_self_test_result}[\r\n]+CLI_ENTRY_ABSENT_OK[\r\n]+CLI_ENTRY_CARDS:2[\r\n]+${bpr_public_preview_enterprise_self_test_expression}"
)
bpr_bound_automated_run(
    application_anonymous_public_preview_unavailable
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# The tests-disabled-equivalent probe. `--public-preview-probe` is
# unconditional for exactly this reason: the claim a released binary has to be
# able to make is its selected mode and zero-request activation, and a flag
# that existed only in a tests-enabled build could not make it. What this row
# cannot check is symbol absence, which is the separate closeout scan.
bpr_add_network_isolated_test(
    NAME application_anonymous_public_preview_release_contract
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --public-preview-probe
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_anonymous_public_preview_release_contract
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "NATIVE_PLATFORM_VAULT_SELECTED:${bpr_native_platform_vault_self_test_result}[\r\n]+CLI_ENTRY_ABSENT_OK[\r\n]+CLI_ENTRY_CARDS:1[\r\n]+${bpr_public_preview_selection_self_test_expression}"
)
bpr_bound_automated_run(
    application_anonymous_public_preview_release_contract
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# [[BPR-0316]]. The verdict a shipped build whose selection is enabled has to
# be able to print. An enabled selection activates the mode from the composed
# authority and reads nothing until a reviewer asks for something, so the
# passing shape here is the opposite of the fixture course's: the mode entered
# with zero requests, no authorization header, and no vault operation. The
# selection is forced from a tests-enabled flag because no platform ships it
# enabled yet; the markers this asserts are the shipped ones.
bpr_add_network_isolated_test(
    NAME application_anonymous_public_preview_enabled_probe
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --public-preview-selection-enabled-probe
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_anonymous_public_preview_enabled_probe
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "NATIVE_PLATFORM_VAULT_SELECTED:${bpr_native_platform_vault_self_test_result}[\r\n]+CLI_ENTRY_ABSENT_OK[\r\n]+CLI_ENTRY_CARDS:1[\r\n]+PUBLIC_PREVIEW_COMPOSED:true[\r\n]+PUBLIC_PREVIEW_STAGE:enabled[\r\n]+PUBLIC_PREVIEW_REQUESTS:0[\r\n]+PUBLIC_PREVIEW_HEADER_EFFECTS:0[\r\n]+PUBLIC_PREVIEW_VAULT_EFFECTS:0[\r\n]+PUBLIC_PREVIEW_SIGN_IN_REQUESTS:0[\r\n]+PUBLIC_PREVIEW_ENABLED_OK"
)
bpr_bound_automated_run(
    application_anonymous_public_preview_enabled_probe
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# [[BPR-0316]]. The live candidate's wiring, proven without reaching the
# network. The dry run composes exactly what the live read would compose --
# the shipped anonymous REST factory, the shipped anonymous smart factory, no
# fixture -- enters the mode, and stops before opening anything. The
# production-factories marker is a pointer identity between what the anonymous
# provider was constructed with and the shipped factory this composition
# built, so a graph that routed the signed-out read through any other factory
# turns it false. The row is network-isolated, which is what makes "issued
# nothing" a fact rather than a promise; the read itself is an authorized
# operator step and has no selector.
#
# [[BPR-0316]] part two. The regex is contiguous, so `WIRE_REQUESTS:0`
# following `PRODUCTION_FACTORIES:true` asserts that no per-request
# `PUBLIC_PREVIEW_WIRE_OPERATION` line was printed between them, which is what
# a run that issued nothing must report. `WIRE_SINGLE_ATTEMPT:true` is the
# in-process check that the counting transport refuses a second request for a
# read that already failed -- the mechanism that bounds an authorized read to
# one provider attempt, proven here rather than against the live service --
# and `LIVE_ELAPSED_MS` is the whole course's wall time.
bpr_add_network_isolated_test(
    NAME application_anonymous_public_preview_live_candidate_wiring
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --public-preview-live-candidate
        octocat
        Hello-World
        1
        --public-preview-live-candidate-dry-run
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_anonymous_public_preview_live_candidate_wiring
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PREVIEW_COMPOSED:true[\r\n]+PUBLIC_PREVIEW_STAGE:enabled[\r\n]+PUBLIC_PREVIEW_REQUESTS:0[\r\n]+PUBLIC_PREVIEW_HEADER_EFFECTS:0[\r\n]+PUBLIC_PREVIEW_VAULT_EFFECTS:0[\r\n]+PUBLIC_PREVIEW_SIGN_IN_REQUESTS:0[\r\n]+PUBLIC_PREVIEW_REVIEW_COMMENT_GROUPS:0[\r\n]+PUBLIC_PREVIEW_WRITE_REFUSALS:0[\r\n]+PUBLIC_PREVIEW_LIVE_CANDIDATE_PRODUCTION_FACTORIES:true[\r\n]+PUBLIC_PREVIEW_WIRE_REQUESTS:0[\r\n]+PUBLIC_PREVIEW_WIRE_CAPABILITY_PROBES:0[\r\n]+PUBLIC_PREVIEW_WIRE_CREDENTIALED_REFUSALS:0[\r\n]+PUBLIC_PREVIEW_WIRE_SINGLE_ATTEMPT:true[\r\n]+PUBLIC_PREVIEW_BUDGET:0/0/0[\r\n]+PUBLIC_PREVIEW_SNAPSHOT_MODE:authenticated[\r\n]+PUBLIC_PREVIEW_CANDIDATE:octocat/Hello-World#1 [\r\n]+PUBLIC_PREVIEW_LIVE_ELAPSED_MS:[0-9]+[\r\n]+PUBLIC_PREVIEW_LIVE_CANDIDATE_OK"
)
bpr_bound_automated_run(
    application_anonymous_public_preview_live_candidate_wiring
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)


bpr_add_executable(
    test_better_pull_review_worker_executor_tests
    worker_executor_tests.cpp
)

bpr_add_executable(
    test_better_pull_review_queue_composition_public_path
    queue_composition_public_path.cpp
)
target_link_libraries(
    test_better_pull_review_queue_composition_public_path
    PRIVATE
        BetterPullReview::Composition
)
target_compile_definitions(
    test_better_pull_review_queue_composition_public_path
    PRIVATE
        BPR_BUILD_TESTING=1
)
target_compile_features(
    test_better_pull_review_queue_composition_public_path
    PRIVATE
        cxx_std_23
)
bpr_enable_hardening(test_better_pull_review_queue_composition_public_path)
bpr_enable_warnings(test_better_pull_review_queue_composition_public_path)

target_link_libraries(
    test_better_pull_review_worker_executor_tests
    PRIVATE
        BetterPullReview::Composition
)
target_compile_features(
    test_better_pull_review_worker_executor_tests
    PRIVATE
        cxx_std_23
)
bpr_enable_hardening(test_better_pull_review_worker_executor_tests)
bpr_enable_warnings(test_better_pull_review_worker_executor_tests)

bpr_add_network_isolated_test(
    NAME application_worker_executor
    COMMAND test_better_pull_review_worker_executor_tests
)
set_tests_properties(
    application_worker_executor
    PROPERTIES
        PASS_REGULAR_EXPRESSION "worker-executor-tests:pass"
        TIMEOUT 15
)

# BPR-0381. What a timed-out public-path wait says, asserted directly rather than
# only when something else is already broken. No Qt: the report formatting is
# standard C++ precisely so this can compile it without an application or an event
# loop, and the polling half that needs Qt is not under test here.
bpr_add_executable(
    test_better_pull_review_public_path_wait_tests
    public_path_wait_tests.cpp
)
target_compile_features(
    test_better_pull_review_public_path_wait_tests
    PRIVATE
        cxx_std_23
)
bpr_enable_hardening(test_better_pull_review_public_path_wait_tests)
bpr_enable_warnings(test_better_pull_review_public_path_wait_tests)

bpr_add_network_isolated_test(
    NAME application_public_path_wait
    COMMAND test_better_pull_review_public_path_wait_tests
)
set_tests_properties(
    application_public_path_wait
    PROPERTIES
        PASS_REGULAR_EXPRESSION "public-path-wait-tests:pass"
        TIMEOUT 15
)

# BPR-0537. The composed system-browser opener's address validation, asked
# directly. The three device-authorization courses observe a browser that
# opened; this asks what the opener refuses, which is the claim, and drives
# `open` with the platform launch substituted for a counting stand-in so no
# test ever hands an address to the desktop. Links the composition for the
# opener itself and needs no window, no event loop, and no network.
bpr_add_executable(
    test_better_pull_review_system_browser_opener_tests
    system_browser_opener_tests.cpp
)
target_link_libraries(
    test_better_pull_review_system_browser_opener_tests
    PRIVATE
        BetterPullReview::Composition
)
target_compile_definitions(
    test_better_pull_review_system_browser_opener_tests
    PRIVATE
        BPR_BUILD_TESTING=1
)
target_compile_features(
    test_better_pull_review_system_browser_opener_tests
    PRIVATE
        cxx_std_23
)
bpr_enable_hardening(test_better_pull_review_system_browser_opener_tests)
bpr_enable_warnings(test_better_pull_review_system_browser_opener_tests)

bpr_add_network_isolated_test(
    NAME application_device_authorization_browser_address
    COMMAND test_better_pull_review_system_browser_opener_tests
)
set_tests_properties(
    application_device_authorization_browser_address
    PROPERTIES
        PASS_REGULAR_EXPRESSION "system-browser-opener-tests:pass"
        TIMEOUT 15
)

set(bpr_application_composition_contract_command
    Python3::Interpreter
    "${CMAKE_CURRENT_SOURCE_DIR}/composition_contract_tests.py"
    --cmake
    "${CMAKE_COMMAND}"
    --cxx
    "${CMAKE_CXX_COMPILER}"
)
if(bpr_test_qt_root)
    list(
        APPEND
        bpr_application_composition_contract_command
        --qt-root
        "${bpr_test_qt_root}"
    )
endif()
if(bpr_test_qt_dir)
    list(
        APPEND
        bpr_application_composition_contract_command
        --qt-dir
        "${bpr_test_qt_dir}"
    )
endif()
list(
    APPEND
    bpr_application_composition_contract_command
    --cpm-source-cache
    "${CPM_SOURCE_CACHE}"
)
bpr_add_network_isolated_test(
    NAME application_composition_contract
    WINDOWS_UNCONTAINED
    COMMAND ${bpr_application_composition_contract_command}
)
# No PASS_REGULAR_EXPRESSION. This is a `unittest` suite and its exit code is
# already the verdict; setting the property makes CTest ignore that exit code
# and pass on a substring match instead. "OK" is satisfied by any incidental
# `..._OK` marker in captured output, which is how a failing suite reported
# Passed.
set_tests_properties(
    application_composition_contract
    PROPERTIES
        TIMEOUT 600
)
if(WIN32)
    # Its nested MSVC configures and builds took 823 s on the Windows station
    # (2026-09-30), and every case passed; 600 s cut it off.
    set_tests_properties(application_composition_contract PROPERTIES TIMEOUT 1800)
elseif(UNIX AND NOT APPLE)
    # [[BPR-1258]]. Alone on Linux (bpr-main-linux: WSL2, 20 cores, GCC 15.2,
    # load about 2, its own build, 2026-10-01) the eight cases passed in 700.4 s,
    # about 658 s of it the nested tests-disabled Release configure and
    # `--parallel 2` build; 600 s cut it off. Three times that, rounded up to a
    # minute, per the run-budget rule in docs/development/build-and-test.md.
    set_tests_properties(application_composition_contract PROPERTIES TIMEOUT 2160)
endif()

set(bpr_public_path_fixture_environment "QT_QPA_PLATFORM=offscreen")
if(UNIX AND NOT APPLE)
    list(APPEND
        bpr_public_path_fixture_environment
        "QT_STYLE_OVERRIDE=Windows"
    )
endif()

bpr_add_executable(bench_better_pull_review_large_review large_review_benchmark.cpp large_review_native_memory.cpp)
target_link_libraries(bench_better_pull_review_large_review PRIVATE BetterPullReview::Composition glaze::glaze)
if(WIN32)
    # [[BPR-0967]]: GetProcessMemoryInfo for the native memory courses.
    target_link_libraries(bench_better_pull_review_large_review PRIVATE psapi)
endif()
target_include_directories(bench_better_pull_review_large_review PRIVATE "${PROJECT_SOURCE_DIR}/src/adapters/github/src")
target_compile_definitions(bench_better_pull_review_large_review PRIVATE
    BPR_BUILD_TESTING=1
    "BPR_LARGE_REVIEW_FIXTURE_ROOT=\"${PROJECT_SOURCE_DIR}/tests/fixtures/github/pull-requests/large-review-v1\""
    "BPR_BENCH_BUILD_CONFIGURATION=\"$<CONFIG>\"")
bpr_enable_hardening(bench_better_pull_review_large_review)
bpr_enable_warnings(bench_better_pull_review_large_review)
# [[BPR-0874]]. The three composed courses below run uncontained on Windows:
# contained, narrowing the private data root parent needs WRITE_DAC the
# container lacks (dos-volume-name, as BPR-0833).
bpr_add_network_isolated_test(
    NAME application_large_review_benchmark
    WINDOWS_UNCONTAINED
    COMMAND $<TARGET_FILE:bench_better_pull_review_large_review>
        $<TARGET_FILE:Git::Git>
        "${CMAKE_CURRENT_BINARY_DIR}/large-review-measurement.json"
        "${CMAKE_CURRENT_BINARY_DIR}/large-review-window.png")
set_tests_properties(application_large_review_benchmark PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" RUN_SERIAL TRUE TIMEOUT 120
    PASS_REGULAR_EXPRESSION "LARGE_REVIEW_COMPOSED_BENCHMARK_OK")

# BPR-0967: control the timing probe with known synchronous and terminal stalls.
bpr_add_network_isolated_test(
    NAME application_large_review_timing_control
    WINDOWS_UNCONTAINED
    COMMAND $<TARGET_FILE:bench_better_pull_review_large_review>
        $<TARGET_FILE:Git::Git>
        "${CMAKE_CURRENT_BINARY_DIR}/large-review-timing-control.json"
        "${CMAKE_CURRENT_BINARY_DIR}/large-review-timing-control.png"
        --timing-control)
set_tests_properties(application_large_review_timing_control PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" RUN_SERIAL TRUE TIMEOUT 90
    PASS_REGULAR_EXPRESSION "LARGE_REVIEW_TIMING_CONTROL_OK")

bpr_add_network_isolated_test(
    NAME application_unavailable_diff_public_path
    WINDOWS_UNCONTAINED
    COMMAND $<TARGET_FILE:bench_better_pull_review_large_review>
        $<TARGET_FILE:Git::Git>
        "${CMAKE_CURRENT_BINARY_DIR}/unavailable-diff-measurement.json"
        "${CMAKE_CURRENT_BINARY_DIR}/unavailable-diff-window.png"
        --unavailable)
set_tests_properties(application_unavailable_diff_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" RUN_SERIAL TRUE TIMEOUT 120
    PASS_REGULAR_EXPRESSION "UNAVAILABLE_DIFF_COMPOSED_OK")

# [[BPR-0858]]: BPR-0719's approximate alignment through the same public path.
bpr_add_network_isolated_test(
    NAME application_approximate_diff_public_path
    WINDOWS_UNCONTAINED
    COMMAND $<TARGET_FILE:bench_better_pull_review_large_review>
        $<TARGET_FILE:Git::Git>
        "${CMAKE_CURRENT_BINARY_DIR}/approximate-diff-measurement.json"
        "${CMAKE_CURRENT_BINARY_DIR}/approximate-diff-window.png"
        --approximate)
set_tests_properties(application_approximate_diff_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" RUN_SERIAL TRUE TIMEOUT 120
    PASS_REGULAR_EXPRESSION "APPROXIMATE_DIFF_COMPOSED_OK")

# [[BPR-0891]]'s sweep: a measurement gate, not a security one. The cache-bytes,
# cache-entries, loaded-close and resources rows sample the process through
# `large_review_native_memory.cpp`, which has macOS, Windows and Linux counters
# since [[BPR-0967]].
# [[BPR-1196]]. A short public-path control before the complete byte-pressure run.
bpr_add_network_isolated_test(
    NAME application_first_tab_comparison
    WINDOWS_UNCONTAINED
    COMMAND $<TARGET_FILE:bench_better_pull_review_large_review>
        $<TARGET_FILE:Git::Git>
        "${CMAKE_CURRENT_BINARY_DIR}/first-tab-comparison.json"
        "${CMAKE_CURRENT_BINARY_DIR}/first-tab-comparison.png"
        --first-tab)
set_tests_properties(application_first_tab_comparison PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" RUN_SERIAL TRUE TIMEOUT 120
    PASS_REGULAR_EXPRESSION "FIRST_TAB_COMPARISON_OK")

# [[BPR-0967]]. Measurements and exact public outcomes on every platform. The
# numeric limits are justified by optimized Mac measurements alone, so on
# Windows and Linux the benchmark reports them (`BudgetsEnforced: false`,
# `UnenforcedLimitMisses`) and enforces only the exact outcomes. The Windows
# rows run uncontained for the reason [[BPR-0874]] records above.
if(APPLE OR WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    bpr_add_network_isolated_test(
        NAME application_large_review_responsiveness
        WINDOWS_UNCONTAINED
        COMMAND $<TARGET_FILE:bench_better_pull_review_large_review>
            $<TARGET_FILE:Git::Git>
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-responsiveness.json"
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-responsiveness.png"
            --responsiveness)
    set_tests_properties(application_large_review_responsiveness PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}" RUN_SERIAL TRUE TIMEOUT 120
        PASS_REGULAR_EXPRESSION "LARGE_REVIEW_RESPONSIVENESS_MEASURED")
    bpr_add_network_isolated_test(
        NAME application_large_review_launch
        WINDOWS_UNCONTAINED
        COMMAND Python3::Interpreter -I -S "${CMAKE_CURRENT_SOURCE_DIR}/large_review_launch.py"
            "--enforce=$<IF:$<OR:$<PLATFORM_ID:Darwin>,$<NOT:$<CONFIG:Debug>>>,1,0>"
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-launch.json"
            $<TARGET_FILE:bench_better_pull_review_large_review>
            $<TARGET_FILE:Git::Git>
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-launch-course.json"
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-launch.png"
            --responsiveness)
    set_tests_properties(application_large_review_launch PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}" RUN_SERIAL TRUE TIMEOUT 120
        PASS_REGULAR_EXPRESSION "LARGE_REVIEW_LAUNCH_OBSERVED")
    bpr_add_network_isolated_test(
        NAME application_large_review_active_work
        WINDOWS_UNCONTAINED
        COMMAND $<TARGET_FILE:bench_better_pull_review_large_review>
            $<TARGET_FILE:Git::Git>
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-active-work.json"
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-active-work.png"
            --active-work)
    set_tests_properties(application_large_review_active_work PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}" RUN_SERIAL TRUE TIMEOUT 120
        PASS_REGULAR_EXPRESSION "LARGE_REVIEW_ACTIVE_WORK_MEASURED")
    foreach(course IN ITEMS pathological provider-cancel)
        string(REPLACE "-" "_" course_name "${course}")
        bpr_add_network_isolated_test(
            NAME application_large_review_${course_name}
            WINDOWS_UNCONTAINED
            COMMAND $<TARGET_FILE:bench_better_pull_review_large_review>
                $<TARGET_FILE:Git::Git>
                "${CMAKE_CURRENT_BINARY_DIR}/large-review-${course}.json"
                "${CMAKE_CURRENT_BINARY_DIR}/large-review-${course}.png"
                --${course})
        set_tests_properties(application_large_review_${course_name} PROPERTIES
            ENVIRONMENT "${bpr_public_path_fixture_environment}" RUN_SERIAL TRUE TIMEOUT 120)
    endforeach()
    set_tests_properties(application_large_review_pathological PROPERTIES
        PASS_REGULAR_EXPRESSION "UNAVAILABLE_DIFF_COMPOSED_OK")
    set_tests_properties(application_large_review_provider_cancel PROPERTIES
        PASS_REGULAR_EXPRESSION "LARGE_REVIEW_PROVIDER_CANCEL_OK")
    bpr_add_network_isolated_test(
        NAME application_large_review_cache_bytes
        WINDOWS_UNCONTAINED
        COMMAND $<TARGET_FILE:bench_better_pull_review_large_review>
            $<TARGET_FILE:Git::Git>
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-cache-bytes.json"
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-cache-bytes.png"
            --cache-bytes)
    # [[BPR-1265]]. The byte-pressure course runs only in an optimized build; a
    # Debug build exits 77 with LARGE_REVIEW_CACHE_BYTES_SKIPPED and its reason,
    # and the row reports Skipped rather than a pass.
    set_tests_properties(application_large_review_cache_bytes PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}" RUN_SERIAL TRUE TIMEOUT 900
        PASS_REGULAR_EXPRESSION "LARGE_REVIEW_CACHE_BYTES_OK"
        SKIP_RETURN_CODE 77)
    # [[BPR-0967]]. Contained on Windows: measured 2026-10-02, it passes inside
    # the container (LARGE_REVIEW_HISTORY_SESSION_RACE_OK); it opens no data root.
    bpr_add_network_isolated_test(
        NAME application_large_review_history_session_race
        COMMAND $<TARGET_FILE:bench_better_pull_review_large_review>
            $<TARGET_FILE:Git::Git>
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-history-session-race.json"
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-history-session-race.png"
            --history-session-race)
    set_tests_properties(application_large_review_history_session_race PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}" RUN_SERIAL TRUE TIMEOUT 90
        PASS_REGULAR_EXPRESSION "LARGE_REVIEW_HISTORY_SESSION_RACE_OK")
    bpr_add_network_isolated_test(
        NAME application_large_review_cache_entries
        WINDOWS_UNCONTAINED
        COMMAND $<TARGET_FILE:bench_better_pull_review_large_review>
            $<TARGET_FILE:Git::Git>
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-cache-entries.json"
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-cache-entries.png"
            --cache-entries)
    set_tests_properties(application_large_review_cache_entries PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}" RUN_SERIAL TRUE TIMEOUT 240
        PASS_REGULAR_EXPRESSION "LARGE_REVIEW_CACHE_ENTRIES_OK")
    bpr_add_network_isolated_test(
        NAME application_large_review_loaded_close
        WINDOWS_UNCONTAINED
        COMMAND $<TARGET_FILE:bench_better_pull_review_large_review>
            $<TARGET_FILE:Git::Git>
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-loaded-close.json"
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-loaded-close.png"
            --loaded-close)
    set_tests_properties(application_large_review_loaded_close PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}" RUN_SERIAL TRUE TIMEOUT 120
        PASS_REGULAR_EXPRESSION "LARGE_REVIEW_LOADED_CLOSE_OK")
    bpr_add_network_isolated_test(
        NAME application_large_review_history
        WINDOWS_UNCONTAINED
        COMMAND $<TARGET_FILE:bench_better_pull_review_large_review>
            $<TARGET_FILE:Git::Git>
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-history.json"
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-history.png"
            --history)
    set_tests_properties(application_large_review_history PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}" RUN_SERIAL TRUE TIMEOUT 600
        PASS_REGULAR_EXPRESSION "LARGE_REVIEW_HISTORY_OK")
    bpr_add_network_isolated_test(
        NAME application_large_review_resources
        WINDOWS_UNCONTAINED
        COMMAND $<TARGET_FILE:bench_better_pull_review_large_review>
            $<TARGET_FILE:Git::Git>
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-resources.json"
            "${CMAKE_CURRENT_BINARY_DIR}/large-review-resources.png"
            --resources)
    set_tests_properties(application_large_review_resources PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}" RUN_SERIAL TRUE TIMEOUT 120
        PASS_REGULAR_EXPRESSION "LARGE_REVIEW_RESOURCES_OK")
endif()

# [[BPR-0967]]. Windows row timeouts for the large-review courses. A Windows
# Debug build triples each course clock (the benchmark's run budget: 90 s, or
# 180 s for cache entries, 540 s for history, 840 s for byte pressure). On
# bpr-main-win (i9-10900KF, MSVC 19.51, Qt 6.11.2, load 3-10%) the Debug
# resources course outran 90 s at 682ea1392, and at f61340e17 the byte course
# took 1,626.01 s of the doubled 1,680 s clock. Each timeout keeps
# clock + 30 s reserve <= 0.75 * TIMEOUT, rounded up to a minute. Measured
# Debug completions at f61340e17 against them: resources 110.80 s and loaded
# close 89.39 (420 s, 3.8x and more), cache entries 61.07 (780 s), history
# 187.44 (2,220 s), byte pressure 1,626.01 (3,420 s, 2.1x: three times would
# be 81 minutes, and the clock itself is 1.55x the completion). macOS and Linux
# keep the timeouts above: every Linux Debug row finishes in under a third of
# them (byte pressure 62.64 s of 900, history 45.35 of 600, the rest under 9 s).
if(WIN32)
    set_tests_properties(
        application_large_review_benchmark
        application_large_review_timing_control
        application_unavailable_diff_public_path
        application_approximate_diff_public_path
        application_first_tab_comparison
        application_large_review_responsiveness
        application_large_review_launch
        application_large_review_active_work
        application_large_review_pathological
        application_large_review_provider_cancel
        application_large_review_loaded_close
        application_large_review_resources
        PROPERTIES TIMEOUT 420)
    set_tests_properties(application_large_review_cache_entries PROPERTIES TIMEOUT 780)
    set_tests_properties(application_large_review_history PROPERTIES TIMEOUT 2220)
    set_tests_properties(application_large_review_cache_bytes PROPERTIES TIMEOUT 3420)
endif()

bpr_add_executable(test_better_pull_review_large_review_replay large_review_replay_tests.cpp)
target_link_libraries(test_better_pull_review_large_review_replay PRIVATE BetterPullReview::AdapterGitHub glaze::glaze)
target_include_directories(test_better_pull_review_large_review_replay PRIVATE "${PROJECT_SOURCE_DIR}/src/adapters/github/src")
target_compile_definitions(test_better_pull_review_large_review_replay PRIVATE
    BPR_BUILD_TESTING=1
    "BPR_LARGE_REVIEW_FIXTURE_ROOT=\"${PROJECT_SOURCE_DIR}/tests/fixtures/github/pull-requests/large-review-v1\"")
bpr_enable_hardening(test_better_pull_review_large_review_replay)
bpr_enable_warnings(test_better_pull_review_large_review_replay)
bpr_add_network_isolated_test(NAME large_review_replay_contract COMMAND $<TARGET_FILE:test_better_pull_review_large_review_replay>)
set_tests_properties(large_review_replay_contract PROPERTIES TIMEOUT 30)
if(WIN32)
    # [[BPR-0967]]. bpr-main-win Debug at f61340e17: 33.23 s (30 s timed out
    # at 682ea1392); 120 s is 3.6 times that.
    set_tests_properties(large_review_replay_contract PROPERTIES TIMEOUT 120)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # [[BPR-0967]]. bpr-main-linux Debug at f61340e17: 16.32 s; 60 s is 3.7
    # times that (Release 1.68 s).
    set_tests_properties(large_review_replay_contract PROPERTIES TIMEOUT 60)
endif()

bpr_add_executable(
    test_better_pull_review_git_diagnostic_wrapper
    "${PROJECT_SOURCE_DIR}/tests/fixtures/git/git_diagnostic_wrapper_main.cpp"
)
target_compile_features(
    test_better_pull_review_git_diagnostic_wrapper
    PRIVATE
        cxx_std_23
)
target_compile_definitions(
    test_better_pull_review_git_diagnostic_wrapper
    PRIVATE
        BPR_REAL_GIT_EXECUTABLE="$<TARGET_FILE:Git::Git>"
)
set_target_properties(
    test_better_pull_review_git_diagnostic_wrapper
    PROPERTIES
        OUTPUT_NAME git
        RUNTIME_OUTPUT_DIRECTORY
            "${CMAKE_CURRENT_BINARY_DIR}/git-diagnostic-wrapper"
)
bpr_enable_hardening(test_better_pull_review_git_diagnostic_wrapper)
bpr_enable_warnings(test_better_pull_review_git_diagnostic_wrapper)

if(BPR_BUILD_AGENT_INTEGRATIONS) # [[BPR-1179]]
bpr_add_executable(
    test_better_pull_review_agent_invocation_composition_public_path
    agent_invocation_composition_public_path.cpp
)
target_link_libraries(
    test_better_pull_review_agent_invocation_composition_public_path
    PRIVATE
        BetterPullReview::Composition
)
target_compile_definitions(
    test_better_pull_review_agent_invocation_composition_public_path
    PRIVATE
        BPR_BUILD_TESTING=1
)
add_dependencies(
    test_better_pull_review_agent_invocation_composition_public_path
    test_better_pull_review_agent_canary
    test_better_pull_review_git_diagnostic_wrapper
)
bpr_enable_hardening(
    test_better_pull_review_agent_invocation_composition_public_path
)
bpr_enable_warnings(
    test_better_pull_review_agent_invocation_composition_public_path
)

# [[BPR-0622]]. Windows runs both courses with handle-based liveness and
# NtSuspendProcess in place of SIGSTOP; see the scripts.
if(UNIX OR WIN32)
    bpr_add_network_isolated_test(
        NAME application_agent_crash_recovery
        WINDOWS_UNCONTAINED
        COMMAND Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/agent_composition_kill_recovery_tests.py"
            --executable "$<TARGET_FILE:test_better_pull_review_agent_invocation_composition_public_path>"
            --canary "$<TARGET_FILE:test_better_pull_review_agent_canary>"
            --git "$<TARGET_FILE:Git::Git>"
    )
    set_tests_properties(application_agent_crash_recovery PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        TIMEOUT 360)
    bpr_add_network_isolated_test(
        NAME application_agent_materialization_crash_recovery
        WINDOWS_UNCONTAINED
        COMMAND Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/agent_materialization_kill_recovery_tests.py"
            --executable "$<TARGET_FILE:test_better_pull_review_agent_invocation_composition_public_path>"
            --canary "$<TARGET_FILE:test_better_pull_review_agent_canary>"
            --git "$<TARGET_FILE:Git::Git>"
            --wrapper "$<TARGET_FILE:test_better_pull_review_git_diagnostic_wrapper>"
    )
    set_tests_properties(application_agent_materialization_crash_recovery PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        TIMEOUT 360)
endif()

# RFC-BPR-0012. A fixture agent that anchors its finding at the identity the
# composed context disclosed, rather than at a fixed repeated hex digit. The
# existing canary proves an unanchored answer still reaches the reviewer; this
# one proves a correctly placed answer validates against the frozen snapshot.
bpr_add_executable(
    test_better_pull_review_agent_finding_canary
    agent_finding_canary_main.cpp
)
target_compile_features(
    test_better_pull_review_agent_finding_canary
    PRIVATE
        cxx_std_23
)
bpr_enable_hardening(test_better_pull_review_agent_finding_canary)
bpr_enable_warnings(test_better_pull_review_agent_finding_canary)

bpr_add_executable(
    test_better_pull_review_agent_finding_composition_public_path
    agent_finding_composition_public_path.cpp
)
target_link_libraries(
    test_better_pull_review_agent_finding_composition_public_path
    PRIVATE
        BetterPullReview::Composition
)
target_compile_definitions(
    test_better_pull_review_agent_finding_composition_public_path
    PRIVATE
        BPR_BUILD_TESTING=1
        "BPR_COMPOSITION_SOURCE=\"${PROJECT_SOURCE_DIR}/src/app/src/github_application.cpp\""
)
add_dependencies(
    test_better_pull_review_agent_finding_composition_public_path
    test_better_pull_review_agent_finding_canary
)
bpr_enable_hardening(
    test_better_pull_review_agent_finding_composition_public_path
)
bpr_enable_warnings(
    test_better_pull_review_agent_finding_composition_public_path
)

# RFC-BPR-0012 (BPR-0514, BPR-0517). The side-chat session, driven through the
# shipped panel: one send reaches one turn on the launch path that already
# existed and the typed message reaches the agent's frame exactly once, a stale
# send is refused by the core's verdict and named on screen, `start_new_session`
# starts exactly one session, and `continue_against_old_revision` starts none
# and takes one further turn under the session the reviewer kept.
bpr_add_executable(
    test_better_pull_review_agent_chat_composition_public_path
    agent_chat_composition_public_path.cpp
)
target_link_libraries(
    test_better_pull_review_agent_chat_composition_public_path
    PRIVATE
        BetterPullReview::Composition
)
target_compile_definitions(
    test_better_pull_review_agent_chat_composition_public_path
    PRIVATE
        BPR_BUILD_TESTING=1
        "BPR_COMPOSITION_SOURCE=\"${PROJECT_SOURCE_DIR}/src/app/src/github_application.cpp\""
)
add_dependencies(
    test_better_pull_review_agent_chat_composition_public_path
    test_better_pull_review_agent_finding_canary
)
bpr_enable_hardening(
    test_better_pull_review_agent_chat_composition_public_path
)
bpr_enable_warnings(
    test_better_pull_review_agent_chat_composition_public_path
)

foreach(
    course
    IN ITEMS
        positive
        stale
        continued
        anchored-continuation
        construction-cut
)
    if(course STREQUAL "positive")
        set(chat_test_name application_agent_chat_composition_contract)
        set(chat_pass_marker "agent-chat-composition:pass")
    else()
        string(REPLACE "-" "_" chat_course_name "${course}")
        set(chat_test_name "application_agent_chat_${chat_course_name}_contract")
        set(chat_pass_marker "agent-chat-${course}:pass")
    endif()
    bpr_add_network_isolated_test(
        NAME "${chat_test_name}"
        WINDOWS_UNCONTAINED
        COMMAND
            test_better_pull_review_agent_chat_composition_public_path
            "${course}"
            $<TARGET_FILE:test_better_pull_review_agent_finding_canary>
    )
    set_tests_properties(
        "${chat_test_name}"
        PROPERTIES
            ENVIRONMENT "${bpr_public_path_fixture_environment};GITHUB_TOKEN=must-not-cross-agent-boundary"
            PASS_REGULAR_EXPRESSION "${chat_pass_marker}"
            TIMEOUT 120
    )
endforeach()

foreach(
    course
    IN ITEMS
        positive
        full-agent
        finding-host-construction-cut
        workshop-press
        workshop-construction-cut
        mcp-construction-cut
        chat-panel-construction-cut
        queue-construction-cut
        dif-context
)
    if(course STREQUAL "positive")
        set(finding_test_name application_agent_finding_composition_contract)
        set(finding_pass_marker "agent-finding-composition:pass")
    else()
        string(REPLACE "-" "_" finding_course_name "${course}")
        set(finding_test_name "application_agent_finding_${finding_course_name}_contract")
        set(finding_pass_marker "agent-finding-${course}:pass")
    endif()
    bpr_add_network_isolated_test(
        NAME "${finding_test_name}"
        WINDOWS_UNCONTAINED
        COMMAND
            test_better_pull_review_agent_finding_composition_public_path
            "${course}"
            $<TARGET_FILE:test_better_pull_review_agent_finding_canary>
    )
    set_tests_properties(
        "${finding_test_name}"
        PROPERTIES
            ENVIRONMENT "${bpr_public_path_fixture_environment};GITHUB_TOKEN=must-not-cross-agent-boundary"
            PASS_REGULAR_EXPRESSION "${finding_pass_marker}"
            TIMEOUT 120
    )
endforeach()

foreach(
    course
    IN ITEMS
        positive
        construction-cut
        launcher-construction-cut
        gesture-cut
        cancellation
        staleness
        cleanup-retry
        workspace-diagnostic
)
	if(course STREQUAL "positive")
		set(test_name application_agent_invocation_composition_contract)
	else()
        string(REPLACE "-" "_" course_name "${course}")
        set(test_name "application_agent_invocation_${course_name}_contract")
        set(pass_marker "agent-invocation-${course}:pass")
    endif()
	if(course STREQUAL "positive")
		bpr_add_network_isolated_test(
			NAME "${test_name}"
			WINDOWS_UNCONTAINED
			COMMAND
				"${CMAKE_COMMAND}"
				"-DBPR_APPLICATION=$<TARGET_FILE:test_better_pull_review_agent_invocation_composition_public_path>"
				"-DBPR_APPLICATION_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/agent_invocation_composition_public_path.cpp"
				"-DBPR_CANARY=$<TARGET_FILE:test_better_pull_review_agent_canary>"
				"-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/agent-invocation-composition-contract"
				-P
				"${CMAKE_CURRENT_SOURCE_DIR}/application_agent_invocation_composition_contract.cmake"
		)
	else()
		if(course STREQUAL "workspace-diagnostic")
			set(
				course_extra_argument
				$<TARGET_FILE:test_better_pull_review_git_diagnostic_wrapper>
			)
		else()
			set(course_extra_argument "")
		endif()
		bpr_add_network_isolated_test(
			NAME "${test_name}"
			WINDOWS_UNCONTAINED
			COMMAND
				test_better_pull_review_agent_invocation_composition_public_path
				"${course}"
				$<TARGET_FILE:test_better_pull_review_agent_canary>
				${course_extra_argument}
		)
	endif()
    set_tests_properties(
        "${test_name}"
        PROPERTIES
            ENVIRONMENT "${bpr_public_path_fixture_environment};GITHUB_TOKEN=must-not-cross-agent-boundary"
            TIMEOUT 90
    )
	if(NOT course STREQUAL "positive")
		set_tests_properties(
			"${test_name}"
			PROPERTIES PASS_REGULAR_EXPRESSION "${pass_marker}"
		)
	endif()
endforeach()
endif()

bpr_add_network_isolated_test(
    NAME application_queue_composition_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:test_better_pull_review_queue_composition_public_path>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/queue-composition-contract"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_queue_composition_contract.cmake"
)
set_tests_properties(
    application_queue_composition_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "QUEUE_COMPOSITION_CONTRACT_OK"
)
# Four courses: seed, reopen, request-cut and activation-cut.
bpr_bound_automated_run(
    application_queue_composition_contract
    TIMEOUT 150
    BUDGET_MS 25000
    RUNS 4
    RESERVE_MS 4000
)

bpr_add_network_isolated_test(
    NAME application_native_notification_attended_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/native-notification-attended-contract"
        "-DBPR_SOURCE=${CMAKE_SOURCE_DIR}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_native_notification_attended_contract.cmake"
)
if(APPLE)
    set_tests_properties(
        application_native_notification_attended_contract
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=cocoa"
    )
else()
    set_tests_properties(
        application_native_notification_attended_contract
        PROPERTIES ENVIRONMENT "${bpr_public_path_fixture_environment}"
    )
endif()
# Three delivery courses plus the signal-observation cut.
bpr_bound_automated_run(
    application_native_notification_attended_contract
    TIMEOUT 150
    BUDGET_MS 25000
    RUNS 4
    RESERVE_MS 4000
)

bpr_add_executable(
    test_better_pull_review_github_application_liveness_tests
    github_application_liveness_tests.cpp
)
target_link_libraries(
    test_better_pull_review_github_application_liveness_tests
    PRIVATE
        BetterPullReview::Composition
)
target_compile_features(
    test_better_pull_review_github_application_liveness_tests
    PRIVATE
        cxx_std_23
)
bpr_enable_hardening(test_better_pull_review_github_application_liveness_tests)
bpr_enable_warnings(test_better_pull_review_github_application_liveness_tests)

# Constructs the real application, so it needs the same offscreen platform the
# fixture runs use. Nothing is shown and no fixture is selected: the subjects are
# the destructor and, since BPR-0802, that an in-process composition root reads
# the automated-test boundary marker itself: the binary sets the marker before
# anything composes, and every composition it starts binds the recorder and
# offers no GitHub CLI read.
bpr_add_network_isolated_test(
    NAME application_poller_liveness
    WINDOWS_UNCONTAINED
    COMMAND test_better_pull_review_github_application_liveness_tests
)
set_tests_properties(
    application_poller_liveness
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "github-application-liveness-tests:pass"
        TIMEOUT 60
)

bpr_add_network_isolated_test(
    NAME application_icon
    COMMAND
        app_better_pull_review
        --application-icon-probe
)
set_tests_properties(
    application_icon
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION "APPLICATION_ICON_OK"
        TIMEOUT 15
)

# [[BPR-1270]]. The real startup registers the bundled code font, and a fresh
# profile's code font is that family, resolved fixed-pitch by this process's
# font database. Exits before composition, like the icon probe.
bpr_add_network_isolated_test(
    NAME application_code_font
    COMMAND
        app_better_pull_review
        --code-font-probe
)
set_tests_properties(
    application_code_font
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION "CODE_FONT_OK"
        TIMEOUT 15
)

bpr_add_network_isolated_test(
    NAME application_native_appearance_probe_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_BEFORE_RENDER=${CMAKE_CURRENT_BINARY_DIR}/native-appearance-before.png"
        "-DBPR_AFTER_RENDER=${CMAKE_CURRENT_BINARY_DIR}/native-appearance-after.png"
        "-DBPR_RUN_BUDGET_MS=${bpr_native_appearance_budget_ms}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/native_appearance_probe_contract.cmake"
)
set_tests_properties(
    application_native_appearance_probe_contract
    PROPERTIES
        PASS_REGULAR_EXPRESSION "NATIVE_APPEARANCE_PROBE_CONTRACT_OK"
)
# Two full-budget runs — the offscreen real-selection course and its cut wire.
# The reserve covers the 1 ms startup-budget course plus the eight
# configuration-failure invocations; each starts Qt but does not consume a full
# automated-run budget.
bpr_bound_automated_run(
    application_native_appearance_probe_contract
    TIMEOUT 100
    BUDGET_MS ${bpr_native_appearance_budget_ms}
    RUNS 2
    RESERVE_MS 12000
)

bpr_add_network_isolated_test(
    NAME application_github_public_path_github_com
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --github-fixture
        github-com
        --render-evidence
        "${CMAKE_CURRENT_BINARY_DIR}/github-public-path-github-com.png"
        --automated-run-budget-ms
        ${bpr_fixture_run_budget_ms}
)
set_tests_properties(
    application_github_public_path_github_com
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+GITHUB_FIXTURE_OK:github.com[\r\n]+RENDER_OK"
)
bpr_bound_automated_run(
    application_github_public_path_github_com
    TIMEOUT 60
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# The recorded public pull request, served through the same public path as the
# synthetic fixtures. A green run is the conjunction at main.cpp's evidence
# gate: all ten recorded interactions consumed in order, the capability probe
# answered once, one smart acquire/discover/ls-refs/fetch, and the pack
# imported through `git index-pack --strict
# --check-self-contained-and-connected` followed by `fsck --connectivity-only`.
bpr_add_network_isolated_test(
    NAME application_github_public_path_uiforetw_74
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --github-fixture
        uiforetw-74
        --render-evidence
        "${CMAKE_CURRENT_BINARY_DIR}/github-public-path-uiforetw-74.png"
        --automated-run-budget-ms
        ${bpr_fixture_run_budget_ms}
)
set_tests_properties(
    application_github_public_path_uiforetw_74
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+GITHUB_FIXTURE_OK:uiforetw-74[\r\n]+RENDER_OK"
)
bpr_bound_automated_run(
    application_github_public_path_uiforetw_74
    TIMEOUT 60
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# [[BPR-1213]]. The constructed nested-folders pull request, served through
# the same public path and the same evidence gate as the recorded one.
bpr_add_network_isolated_test(
    NAME application_github_public_path_nested_folders_v1
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --github-fixture
        nested-folders-v1
        --render-evidence
        "${CMAKE_CURRENT_BINARY_DIR}/github-public-path-nested-folders-v1.png"
        --automated-run-budget-ms
        ${bpr_fixture_run_budget_ms}
)
set_tests_properties(
    application_github_public_path_nested_folders_v1
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+GITHUB_FIXTURE_OK:nested-folders-v1[\r\n]+RENDER_OK"
)
bpr_bound_automated_run(
    application_github_public_path_nested_folders_v1
    TIMEOUT 60
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

bpr_add_network_isolated_test(
    NAME application_uiforetw_selected_diff_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/uiforetw-selected-diff-contract"
        "-DBPR_RUN_BUDGET_MS=${bpr_fixture_run_budget_ms}"
        "-DBPR_SOURCE=${PROJECT_SOURCE_DIR}/src/app/src/github_application.cpp"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_uiforetw_selected_diff_contract.cmake"
)
set_tests_properties(
    application_uiforetw_selected_diff_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "UIFORETW_SELECTED_DIFF_CONTRACT_OK"
)
bpr_bound_automated_run(
    application_uiforetw_selected_diff_contract
    TIMEOUT 300
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 5
    RESERVE_MS 10000
)

bpr_add_network_isolated_test(
    NAME application_diff_workbench_public_path
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --github-fixture
        github-com
        --diff-workbench-probe
        --diff-interaction-evidence
        "${CMAKE_CURRENT_BINARY_DIR}/bpr-0083-diff-interaction"
        --render-evidence
        "${CMAKE_CURRENT_BINARY_DIR}/diff-workbench-public-path.png"
        # BPR-0414's owner asked for the files drawer out of the way so more
        # source is visible in the attended capture. Collapsed through the
        # shipped toggle action after the course's assertions have run.
        --render-evidence-collapse-files
        --automated-run-budget-ms
        ${bpr_fixture_run_budget_ms}
)
set_tests_properties(
    application_diff_workbench_public_path
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION
            # One contiguous block, so every line the status dump prints between
            # the first and last marker has to be named here. An inserted line is
            # a mismatch rather than extra output, which is how BPR-0368's two
            # new markers left this test red from 9d63c4d until BPR-0393.
            #
            # SYNTAX_DISPLAYED is matched by shape on purpose. It reads 0:0 on
            # this course today, which is BPR-0390's defect; binding it to 1:1
            # here would take BPR-0390's own deterministic check away from it.
            "PUBLIC_PATH_OK[\r\n]+GITHUB_FIXTURE_OK:github.com[\r\n]+DIFF_WORKBENCH_OK[\r\n]+DIFF_INTERACTION_EVIDENCE_OK[\r\n]+CONVERSATION_ANCHOR_EVIDENCE_OK[\r\n]+SYNTAX_THEME_REPAINT_OK[\r\n]+RENDER_OK[\r\n]+REVIEW_LEDGER_PRESENT_ON_ENTRY:0[\r\n]+REVIEW_LEDGER_OK[\r\n]+WORKBENCH_HOST_COMPOSED_OK[\r\n]+APPEARANCE_HOST_COMPOSED_OK[\r\n]+SYNTAX_CLASSIFIER_COMPOSED_OK[\r\n]+SYNTAX_CLASSIFICATION:[1-9][0-9]*:[1-9][0-9]*[\r\n]+SYNTAX_DELIVERY:[0-9]+:[0-9]+:[0-9]+:[0-9]+:[0-9]+[\r\n]+SYNTAX_DISPLAYED:[01]:[01][\r\n]+SYNTAX_DISPLAYED_SPANS:[0-9]+:[0-9]+:[0-9]+:[01][\r\n]+APPEARANCE_SETTINGS:[a-z_]+:[0-9]+[\r\n]+DIF_PREFERENCES:[a-z]+:[a-z]+:[a-z]+[\r\n]+NOTIFICATION_DELIVERY_COMPOSED_OK[\r\n]+NOTIFICATION_PERMISSION:[a-z-]+[\r\n]+NOTIFICATION_BUNDLE_IDENTITY:[A-Za-z0-9.-]+[\r\n]+APPEARANCE_DIVERGENCE:0[\r\n]+APPEARANCE_OBSERVED:[a-z]+:[a-z-]+:[0-9]+[\r\n]+APPEARANCE_APPLIED:#[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]:#[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]:from-profile"
)
bpr_bound_automated_run(
    application_diff_workbench_public_path
    TIMEOUT 60
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

bpr_add_network_isolated_test(
    NAME application_workbench_composition_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
		"-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/workbench-composition-contract"
		"-DBPR_RUN_BUDGET_MS=${bpr_fixture_run_budget_ms}"
		"-DBPR_SOURCE=${PROJECT_SOURCE_DIR}/src/app/src/github_application.cpp"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_workbench_composition_contract.cmake"
)
set_tests_properties(
    application_workbench_composition_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "WORKBENCH_COMPOSITION_CONTRACT_OK"
)
# The normal selection course and its cut-selection-wire counterexample.
bpr_bound_automated_run(
    application_workbench_composition_contract
    TIMEOUT 120
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 2
    RESERVE_MS 2000
)

# BPR-0905: these epoch seed/reuse courses use the measured epoch allowance.
# The card recorded quiet phases of 18.4 s (ledger), 18.0 s (defect), and
# 23.7 s (saved server): 120 s supplies 6.5x, 6.7x, and 5.1x respectively.
# Ledger keeps its unavailable/single phase at 40 s; using the 120 s maximum
# for all RUNS is conservative. Timeouts satisfy (RUNS * 120 s + 4 s) / 0.75, rounded
# upward: three runs -> 500 s, four -> 660 s. No other budget is raised.
bpr_add_network_isolated_test(
    NAME application_review_ledger_composition_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/review-ledger-composition-contract"
        "-DBPR_RUN_BUDGET_MS=${bpr_epoch_phase_budget_ms}"
        "-DBPR_SINGLE_RUN_BUDGET_MS=${bpr_fixture_run_budget_ms}"
        "-DBPR_SOURCE=${PROJECT_SOURCE_DIR}/src/app/src/github_application.cpp"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_review_ledger_composition_contract.cmake"
)
set_tests_properties(
    application_review_ledger_composition_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "REVIEW_LEDGER_COMPOSITION_CONTRACT_OK"
)
bpr_bound_automated_run(
    application_review_ledger_composition_contract
    TIMEOUT 500
    BUDGET_MS ${bpr_epoch_phase_budget_ms}
    RUNS 3
    RESERVE_MS 4000
)

bpr_add_network_isolated_test(
    NAME application_review_round_trip_composition_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_APP_SOURCE=${PROJECT_SOURCE_DIR}/src/app/src/github_application.cpp"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/review-round-trip-composition-contract"
        "-DBPR_RUN_BUDGET_MS=${bpr_fixture_run_budget_ms}"
        "-DBPR_SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_review_round_trip_composition_contract.cmake"
)
set_tests_properties(
    application_review_round_trip_composition_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
)
bpr_bound_automated_run(
    application_review_round_trip_composition_contract
    TIMEOUT 75
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 1
    RESERVE_MS 10000
)

bpr_add_network_isolated_test(
    NAME application_defect_composition_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/defect-composition-contract"
        "-DBPR_RUN_BUDGET_MS=${bpr_epoch_phase_budget_ms}"
        "-DBPR_SOURCE=${PROJECT_SOURCE_DIR}/src/app/src/github_application.cpp"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_defect_composition_contract.cmake"
)
set_tests_properties(
    application_defect_composition_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "DEFECT_COMPOSITION_CONTRACT_OK"
)
# Seed and reopen for each of the GitHub.com and GHES fixtures.
bpr_bound_automated_run(
    application_defect_composition_contract
    TIMEOUT 660
    BUDGET_MS ${bpr_epoch_phase_budget_ms}
    RUNS 4
    RESERVE_MS 4000
)

bpr_add_network_isolated_test(
    NAME application_overview_since_review_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/overview-since-review-contract"
        # [[BPR-0901]]. An epoch fixture phase, not a fixture render: this row
        # drives `--epoch-fixture-phase` four times and pays the seed's Git
        # object cache and SQLite catalog each time. On the render budget it
        # had 40 s for a phase measured at 28 s driven directly on a quiet
        # host, a 1.4x margin, and it has been running out of that clock for
        # dispatches. `build/r2-logs/dsp6-0890-sweep.log:340`, the full macOS
        # sweep, on the ghes-3.18 reuse:
        #   RUN_BUDGET_EXPIRED:elapsed_ms=40035 budget_ms=40000 over_ms=35 stage=12
        # and `build/r2-logs/dsp2-0818-siblings-a445866d.log:17`, on the
        # github.com seed, `elapsed_ms=40049`. The headline the driver printed
        # was `EPOCH_FIXTURE_REUSE_FAILED`, so it read as an epoch defect.
        # [[BPR-0555]] moved the epoch-phase rows it knew about to this budget;
        # this one and `application_review_behaviour_contract` were not in that
        # sweep. 120000 ms is 4.3x the 28 s quiet measurement, which is what
        # makes these two ordinary among the epoch-phase rows rather than a
        # pair with a bound of their own.
        "-DBPR_RUN_BUDGET_MS=${bpr_epoch_phase_budget_ms}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_overview_since_review_contract.cmake"
)
set_tests_properties(
    application_overview_since_review_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "OVERVIEW_SINCE_REVIEW_CONTRACT_OK"
)
# Seed and reuse for each of the GitHub.com and GHES fixtures. Only the
# timeout moves with the budget, and it has to: `bpr_bound_automated_run`
# refuses 4 x 120000 + 4000 ms inside 240 s, and 660 s is the first round
# number whose three-quarter share (495000 ms) covers the 484000 ms worst
# case. The reserve is unchanged; nothing outside the budget changed.
bpr_bound_automated_run(
    application_overview_since_review_contract
    TIMEOUT 660
    BUDGET_MS ${bpr_epoch_phase_budget_ms}
    RUNS 4
    RESERVE_MS 4000
)

bpr_add_network_isolated_test(
    NAME application_review_behaviour_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/review-behaviour-contract"
        # [[BPR-0901]]. The same epoch fixture phase its sibling above drives,
        # on the same render budget, and the clearest evidence that the budget
        # is the fault rather than anything this dispatch changed: it expires
        # on the github.com seed at `4a2be1b1`, four days before the data root
        # moved. `build/r2-logs/dsp2-0818-contract-4a2be1b1.log:17`:
        #   RUN_BUDGET_EXPIRED:elapsed_ms=40048 budget_ms=40000 over_ms=48 stage=4
        # and again at `build/r2-logs/dsp6-merge2-ctest.log:27`,
        # `elapsed_ms=40052`, which is the failure [[BPR-0901]] was raised for.
        # Nine expiries of this row are on disk across four dispatches, every
        # one of them within 53 ms of the bound. See the note above.
        "-DBPR_RUN_BUDGET_MS=${bpr_epoch_phase_budget_ms}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_review_behaviour_contract.cmake"
)
set_tests_properties(
    application_review_behaviour_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "REVIEW_BEHAVIOUR_CONTRACT_OK"
)
# Seed and reuse for each of the GitHub.com and GHES fixtures.
bpr_bound_automated_run(
    application_review_behaviour_contract
    TIMEOUT 660
    BUDGET_MS ${bpr_epoch_phase_budget_ms}
    RUNS 4
    RESERVE_MS 4000
)

bpr_add_network_isolated_test(
    NAME application_saved_server_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/saved-server-contract"
        "-DBPR_RUN_BUDGET_MS=${bpr_epoch_phase_budget_ms}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_saved_server_contract.cmake"
)
set_tests_properties(
    application_saved_server_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "SAVED_SERVER_CONTRACT_OK"
)
bpr_bound_automated_run(
    application_saved_server_contract
    TIMEOUT 660
    BUDGET_MS ${bpr_epoch_phase_budget_ms}
    RUNS 4
    RESERVE_MS 4000
)

bpr_add_network_isolated_test(
    NAME application_threaded_reply_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_RUN_BUDGET_MS=${bpr_fixture_run_budget_ms}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_threaded_reply_contract.cmake"
)
set_tests_properties(
    application_threaded_reply_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "THREADED_REPLY_CONTRACT_OK"
)
bpr_bound_automated_run(
    application_threaded_reply_contract
    TIMEOUT 120
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 2
    RESERVE_MS 4000
)

# [[BPR-1083]]. Labels written, chosen and shown in the composed application.
bpr_add_network_isolated_test(
    NAME application_label_authoring_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_RUN_BUDGET_MS=${bpr_fixture_run_budget_ms}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_label_authoring_contract.cmake"
)
set_tests_properties(
    application_label_authoring_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "LABEL_AUTHORING_CONTRACT_OK"
)
bpr_bound_automated_run(
    application_label_authoring_contract
    TIMEOUT 120
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 1
    RESERVE_MS 4000
)

set(bpr_appearance_theme_budget_ms 50000)
bpr_add_network_isolated_test(
    NAME application_appearance_theme_customization_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/appearance-theme-customization-root"
        "-DBPR_THEME=${CMAKE_SOURCE_DIR}/tests/fixtures/appearance/contrast-safe-theme.bprtheme"
        "-DBPR_RUN_BUDGET_MS=${bpr_appearance_theme_budget_ms}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_appearance_theme_customization_contract.cmake"
)
set_tests_properties(
    application_appearance_theme_customization_contract
    PROPERTIES
        PASS_REGULAR_EXPRESSION "APPEARANCE_THEME_CUSTOMIZATION_CONTRACT_OK"
)
bpr_bound_automated_run(
    application_appearance_theme_customization_contract
    TIMEOUT 240
    BUDGET_MS ${bpr_appearance_theme_budget_ms}
    RUNS 3
    RESERVE_MS 10000
)

bpr_add_network_isolated_test(
    NAME application_fixture_verdict_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/fixture_verdict_contract.py"
        $<TARGET_FILE:app_better_pull_review>
)
set_tests_properties(
    application_fixture_verdict_contract
    PROPERTIES
        TIMEOUT 300
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "FIXTURE_VERDICT_OK"
)

bpr_add_network_isolated_test(
    NAME application_attended_launch_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/attended_launch_contract.py"
        $<TARGET_FILE:app_better_pull_review>
)
set_tests_properties(
    application_attended_launch_contract
    PROPERTIES
        TIMEOUT 180
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "ATTENDED_LAUNCH_OK"
)

bpr_add_network_isolated_test(
    NAME application_diff_workbench_shutdown_public_path
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --github-fixture
        github-com
        --diff-workbench-shutdown-probe
        --automated-run-budget-ms
        ${bpr_fixture_run_budget_ms}
)
set_tests_properties(
    application_diff_workbench_shutdown_public_path
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+GITHUB_FIXTURE_OK:github.com[\r\n]+DIFF_WORKBENCH_SHUTDOWN_OK"
)
bpr_bound_automated_run(
    application_diff_workbench_shutdown_public_path
    TIMEOUT 60
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# The run's own bound, exercised. `--automated-stall-probe` holds the `open`
# stage forever, which is the shape of a hang: nothing crashes, nothing errors,
# a readiness condition simply never becomes true. What has to happen is that
# the application notices and reports the stage it was in, the verdict, and the
# evidence — rather than being killed with nothing to show.
#
# The 5 s budget inside a 30 s timeout is deliberately lopsided. An application
# that failed to bound itself would be killed by the harness at 30 s, print
# nothing, and fail this test; there is no arrangement of the numbers under
# which a missing bound still passes.
# The one row whose subject is a failed run, so the one row `PUBLIC_PATH_FAILED`
# must not fail: it asserts the stall probe's verdict block, which prints only
# after `passed` is false. BPR-0552 makes that marker an error expression for
# every other row registered through `bpr_add_network_isolated_test`.
bpr_add_network_isolated_test(
    NAME application_automated_run_budget
    WINDOWS_UNCONTAINED
    EXPECTS_FAILED_RUN
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_RUN_BUDGET_MS=${bpr_stall_budget_ms}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/automated_run_budget_contract.cmake"
)
set_tests_properties(
    application_automated_run_budget
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "AUTOMATED_RUN_BUDGET_CONTRACT_OK"
)
bpr_bound_automated_run(
    application_automated_run_budget
    TIMEOUT 30
    BUDGET_MS ${bpr_stall_budget_ms}
    RUNS 2
    RESERVE_MS 2000
)

bpr_add_network_isolated_test(
    NAME application_github_public_path_ghes_3_18
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --github-fixture
        ghes-3.18
        --render-evidence
        "${CMAKE_CURRENT_BINARY_DIR}/github-public-path-ghes-3.18.png"
        --automated-run-budget-ms
        ${bpr_fixture_run_budget_ms}
)
set_tests_properties(
    application_github_public_path_ghes_3_18
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+GITHUB_FIXTURE_OK:ghes-3.18[\r\n]+RENDER_OK"
)
bpr_bound_automated_run(
    application_github_public_path_ghes_3_18
    TIMEOUT 60
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# The enterprise host is refused by the core policy rather than by the
# composition, so it needs a course where the authority *is* composed: a row
# that withheld it would prove the composition refusal twice and the policy
# refusal never. Tests-enabled only, because forcing the shipped selection is
# the one thing a released binary must not be able to do from a command line.
bpr_add_network_isolated_test(
    NAME application_anonymous_public_preview_ghes_refusal
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --public-preview-probe
        --public-preview-ghes-course
        --github-fixture
        ghes-3.18
        --automated-run-budget-ms
        ${bpr_fixture_run_budget_ms}
)
set_tests_properties(
    application_anonymous_public_preview_ghes_refusal
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+PUBLIC_PREVIEW_COMPOSED:true[\r\n]+PUBLIC_PREVIEW_STAGE:refused[\r\n]+PUBLIC_PREVIEW_REQUESTS:0[\r\n]+PUBLIC_PREVIEW_HEADER_EFFECTS:0[\r\n]+PUBLIC_PREVIEW_VAULT_EFFECTS:0[\r\n]+PUBLIC_PREVIEW_SIGN_IN_REQUESTS:0[\r\n]+PUBLIC_PREVIEW_REFUSED_OK"
)
bpr_bound_automated_run(
    application_anonymous_public_preview_ghes_refusal
    TIMEOUT 60
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# The signed-out read, end to end against the recorded public path. The fixture
# replays the nine baseline credential-free operations followed by the
# independent update timeline, so what this row proves is not
# that a recording exists but that the composed host reaches it: the mode is
# entered, the pull request opens through the credential-free authority, the
# inline conversation the signed-out REST surface carries is observed, and
# neither an authorization header nor the vault was involved in any of it.
# Tests-enabled only, for the same reason the enterprise row above is.
bpr_add_network_isolated_test(
    NAME application_anonymous_public_preview_public_path
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --public-preview-probe
        --public-preview-enabled-course
        --github-fixture
        github-com
        --automated-run-budget-ms
        ${bpr_epoch_phase_budget_ms}
)
set_tests_properties(
    application_anonymous_public_preview_public_path
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        # Every term of the claim, in the order the run prints them. The two
        # zeroes are the ones a passing read could most easily have bought:
        # a header the transport attached, or a vault the read touched. The
        # two refusals are the write and triage intents the mode was offered
        # while the session was live, and they sit beside a request count of
        # one, which is where "refused before a request" is legible.
        # [[BPR-0829]]. Four smart-Git leases: the live epoch's pack, then one
        # background retention import per force-push occurrence on the
        # fixture timeline (events 91701, 91702 and 91703). The run waits for
        # all three to be retained before it offers the writes; shutdown
        # cancels an import in flight, so reporting earlier read 1 to 3.
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PREVIEW_COMPOSED:true[\r\n]+PUBLIC_PREVIEW_STAGE:enabled[\r\n]+PUBLIC_PREVIEW_REQUESTS:1[\r\n]+PUBLIC_PREVIEW_HEADER_EFFECTS:0[\r\n]+PUBLIC_PREVIEW_VAULT_EFFECTS:0[\r\n]+PUBLIC_PREVIEW_SIGN_IN_REQUESTS:0[\r\n]+PUBLIC_PREVIEW_REVIEW_COMMENT_GROUPS:1[\r\n]+PUBLIC_PREVIEW_WRITE_REFUSALS:2[\r\n]+PUBLIC_PREVIEW_EPOCH_STATUS:6[\r\n]+PUBLIC_PREVIEW_OCCURRENCES:[34][\r\n]+PUBLIC_PREVIEW_HISTORY_INCOMPLETE:true[\r\n]+PUBLIC_PREVIEW_EPOCH_LINE_HISTORY:[^\r\n]*[\r\n]+PUBLIC_PREVIEW_SMART_ACQUISITIONS:4[\r\n]+PUBLIC_PREVIEW_ENABLED_OK"
)
bpr_bound_automated_run(
    application_anonymous_public_preview_public_path
    TIMEOUT 170
    BUDGET_MS ${bpr_epoch_phase_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# The same A→B→A timeline travels through independent application opens with
# real SQLite catalogues. Additional opens exercise failed, refused, capped,
# and disconnected history without treating Qt row counts as identity proof.
bpr_add_network_isolated_test(
    NAME application_epoch_portable_occurrence_two_catalogues
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/epoch_portable_occurrence_public_path.py"
        --application
        $<TARGET_FILE:app_better_pull_review>
        --budget-ms
        ${bpr_fixture_run_budget_ms}
)
set_tests_properties(
    application_epoch_portable_occurrence_two_catalogues
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "EPOCH_PORTABLE_TWO_CATALOGUES_OK:3"
)
bpr_bound_automated_run(
    application_epoch_portable_occurrence_two_catalogues
    TIMEOUT 700
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 12
    RESERVE_MS 10000
)

foreach(
    fixture_case
    IN ITEMS
        "github-com|github.com"
        "ghes-3.18|ghes-3.18"
)
    string(REPLACE "|" ";" fixture_fields "${fixture_case}")
    list(GET fixture_fields 0 fixture_name)
    list(GET fixture_fields 1 fixture_label)
    string(REPLACE "." "_" fixture_test_name "${fixture_name}")
    bpr_add_network_isolated_test(
        NAME application_epoch_persistence_${fixture_test_name}
        WINDOWS_UNCONTAINED
        COMMAND
            "${CMAKE_COMMAND}"
            "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
            "-DBPR_FIXTURE=${fixture_name}"
            "-DBPR_FIXTURE_LABEL=${fixture_label}"
            "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/epoch-persistence-${fixture_test_name}"
            "-DBPR_RUN_BUDGET_MS=${bpr_epoch_phase_budget_ms}"
            -P
            "${CMAKE_CURRENT_SOURCE_DIR}/epoch_persistence_contract.cmake"
    )
    set_tests_properties(
        application_epoch_persistence_${fixture_test_name}
        PROPERTIES
            ENVIRONMENT "${bpr_public_path_fixture_environment}"
            PASS_REGULAR_EXPRESSION
                "EPOCH_PERSISTENCE_CONTRACT_OK:${fixture_label}"
    )
    # Two budgeted runs: the seed phase and the reuse phase.
    bpr_bound_automated_run(
        application_epoch_persistence_${fixture_test_name}
        TIMEOUT 340
        BUDGET_MS ${bpr_epoch_phase_budget_ms}
        RUNS 2
        RESERVE_MS 4000
    )
endforeach()

# [[BPR-0843]]. The linked spelling needs an unprivileged symbolic link, so this
# is registered on macOS and Linux only. Windows has none. Its contained runs
# also refuse startup with `registry` for a reason of their own ([[BPR-0834]]),
# so the same pin could pass there for the wrong reason.
if(UNIX)
    bpr_add_network_isolated_test(
        NAME application_epoch_fixture_noncanonical_root_cause
        COMMAND
            "${CMAKE_COMMAND}"
            "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
            "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/epoch-fixture-noncanonical-root"
            "-DBPR_RUN_BUDGET_MS=${bpr_epoch_phase_budget_ms}"
            -P
            "${CMAKE_CURRENT_SOURCE_DIR}/epoch_fixture_noncanonical_root_contract.cmake"
    )
    set_tests_properties(
        application_epoch_fixture_noncanonical_root_cause
        PROPERTIES
            ENVIRONMENT "${bpr_public_path_fixture_environment}"
            PASS_REGULAR_EXPRESSION
                "EPOCH_FIXTURE_NONCANONICAL_ROOT_CAUSE_OK:registry"
    )
    # Two budgeted runs: the refused linked spelling and the canonical seed.
    bpr_bound_automated_run(
        application_epoch_fixture_noncanonical_root_cause
        TIMEOUT 340
        BUDGET_MS ${bpr_epoch_phase_budget_ms}
        RUNS 2
        RESERVE_MS 4000
    )
endif()

bpr_add_executable(test_startup_diagnostic_bounds startup_diagnostic_bounds_tests.cpp)
target_link_libraries(test_startup_diagnostic_bounds PRIVATE BetterPullReview::Composition)
bpr_enable_hardening(test_startup_diagnostic_bounds)
bpr_enable_warnings(test_startup_diagnostic_bounds)
bpr_add_network_isolated_test(NAME application_startup_diagnostic_bounds
    COMMAND test_startup_diagnostic_bounds)
set_tests_properties(application_startup_diagnostic_bounds PROPERTIES
    TIMEOUT 30 PASS_REGULAR_EXPRESSION "startup-diagnostic-bounds:pass")

# [[BPR-0882]]. A startup refused as `unavailable` names the arm that refused
# and what it threw. The run removes Git by emptying the search path, so it is
# registered on macOS and Linux only. On Windows the build tree loads Qt through
# that same search path ([[BPR-0430]]), and the process would exit before
# `main` instead of reaching the arm under test.
if(UNIX)
    bpr_add_network_isolated_test(
        NAME application_startup_failure_source
        COMMAND
            "${CMAKE_COMMAND}"
            "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
            "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/startup-failure-source"
            "-DBPR_RUN_BUDGET_MS=${bpr_self_test_budget_ms}"
            -P
            "${CMAKE_CURRENT_SOURCE_DIR}/startup_failure_source_contract.cmake"
    )
    set_tests_properties(
        application_startup_failure_source
        PROPERTIES
            ENVIRONMENT "${bpr_public_path_fixture_environment}"
            PASS_REGULAR_EXPRESSION
                "STARTUP_FAILURE_SOURCE_OK:profile-runtime-construction"
    )
    bpr_bound_automated_run(
        application_startup_failure_source
        TIMEOUT 60
        BUDGET_MS ${bpr_self_test_budget_ms}
        RUNS 1
        RESERVE_MS 4000
    )
endif()

bpr_add_network_isolated_test(
    NAME application_epoch_maintenance_public_path
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/epoch-maintenance-public-path"
        "-DBPR_RUN_BUDGET_MS=${bpr_epoch_phase_budget_ms}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/epoch_maintenance_public_path_contract.cmake"
)
set_tests_properties(
    application_epoch_maintenance_public_path
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "EPOCH_MAINTENANCE_PUBLIC_PATH_CONTRACT_OK"
)
# Four budgeted runs: seed, prune, verify-pruned-delete, verify-deleted.
bpr_bound_automated_run(
    application_epoch_maintenance_public_path
    TIMEOUT 660
    BUDGET_MS ${bpr_epoch_phase_budget_ms}
    RUNS 4
    RESERVE_MS 8000
)

bpr_add_network_isolated_test(
    NAME application_appearance_gesture_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/appearance-gesture-contract"
        "-DBPR_RUN_BUDGET_MS=${bpr_epoch_phase_budget_ms}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/appearance_gesture_contract.cmake"
)
set_tests_properties(
    application_appearance_gesture_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "APPEARANCE_GESTURE_CONTRACT_OK"
)
# Three budgeted runs: the fresh composition, the gesture, and the restart.
bpr_bound_automated_run(
    application_appearance_gesture_contract
    TIMEOUT 500
    BUDGET_MS ${bpr_epoch_phase_budget_ms}
    RUNS 3
    RESERVE_MS 6000
)

bpr_add_network_isolated_test(
    NAME application_appearance_store_fallback_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/appearance-store-fallback-contract"
        # An epoch fixture seed, not a fixture render: this row drives
        # `--epoch-fixture-phase` and was one of the four BPR-0555 named. Under
        # the render budget it had 40 s for work measured at 33-35 s on a
        # loaded host, which is the thinnest margin in the file and was read as
        # a seed defect every time it ran out.
        "-DBPR_RUN_BUDGET_MS=${bpr_epoch_phase_budget_ms}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/appearance_store_fallback_contract.cmake"
)
set_tests_properties(
    application_appearance_store_fallback_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "APPEARANCE_STORE_FALLBACK_CONTRACT_OK"
)
# Two budgeted real-executable runs: the session switch with a classified diff,
# then a restart against the same obstructed appearance-store path.
bpr_bound_automated_run(
    application_appearance_store_fallback_contract
    TIMEOUT 340
    BUDGET_MS ${bpr_epoch_phase_budget_ms}
    RUNS 2
    RESERVE_MS 4000
)

bpr_add_network_isolated_test(
    NAME binary_dependency_surface
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_binary_dependencies.py"
        $<TARGET_FILE:app_better_pull_review>
)
set_tests_properties(
    binary_dependency_surface
    PROPERTIES
        PASS_REGULAR_EXPRESSION "BINARY_DEPENDENCIES_OK"
        TIMEOUT 15
)

bpr_add_network_isolated_test(
    NAME runtime_process_surface
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_runtime_surface.py"
        # BPR-0820. The inspected launch has no automated-run flag, so without
        # its own root it would compose the default profile and, while a
        # person's application holds that profile, hand it an activation.
        --private-data-root
        $<TARGET_FILE:app_better_pull_review>
)
set_tests_properties(
    runtime_process_surface
    PROPERTIES
        PASS_REGULAR_EXPRESSION "RUNTIME_SURFACE_OK"
        TIMEOUT 15
)

# [[BPR-1179]]. A graph without a removable family must carry none of its anchors.
set(bpr_composed_hosts_agent_option "")
if(NOT BPR_BUILD_AGENT_INTEGRATIONS)
    list(APPEND bpr_composed_hosts_agent_option --without-agent-integrations)
endif()
if(NOT BPR_BUILD_LOCAL_AUTOMATION)
    list(APPEND bpr_composed_hosts_agent_option --without-local-automation)
endif()
bpr_add_network_isolated_test(
    NAME composed_application_hosts
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_composed_hosts.py"
        --profile
        development-fixture
        ${bpr_composed_hosts_agent_option}
        $<TARGET_FILE:app_better_pull_review>
)
set_tests_properties(
    composed_application_hosts
    PROPERTIES
        TIMEOUT 30
)

bpr_add_network_isolated_test(
    NAME application_render
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --render-evidence
        "${CMAKE_CURRENT_BINARY_DIR}/foundation-window.png"
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_render
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=1.25"
        PASS_REGULAR_EXPRESSION "RENDER_OK"
)
bpr_bound_automated_run(
    application_render
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

bpr_add_executable(test_device_binding_restart device_binding_restart_tests.cpp)
target_link_libraries(test_device_binding_restart PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_device_binding_restart PRIVATE BPR_BUILD_TESTING=1)
bpr_enable_hardening(test_device_binding_restart)
bpr_enable_warnings(test_device_binding_restart)
bpr_add_network_isolated_test(NAME application_device_binding_restart WINDOWS_UNCONTAINED COMMAND test_device_binding_restart)
set_tests_properties(application_device_binding_restart PROPERTIES ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 60 PASS_REGULAR_EXPRESSION "device-binding-restart-tests:pass")

# BPR-0644. A launch after the access token expired renews the stored grant
# with its refresh material instead of forgetting it. A launch that cannot
# renew it (offline, refresh-expired or refused) keeps the grant and offers the
# device sign-in beside Sign out. [[BPR-1116]]: so does a renewal that gives up
# while the application runs.
bpr_add_executable(test_device_binding_launch_renewal device_binding_launch_renewal_tests.cpp)
target_link_libraries(test_device_binding_launch_renewal PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_device_binding_launch_renewal PRIVATE BPR_BUILD_TESTING=1)
bpr_enable_hardening(test_device_binding_launch_renewal)
bpr_enable_warnings(test_device_binding_launch_renewal)
# [[BPR-0876]]. Uncontained on Windows: contained it stops at its first step,
# narrowing its private desktop test root (dos-volume-name, as BPR-0833).
bpr_add_network_isolated_test(NAME application_device_binding_launch_renewal WINDOWS_UNCONTAINED COMMAND test_device_binding_launch_renewal)
# [[BPR-1116]]. The in-process courses add about 30 s on macOS, most of it
# the device host's three offline retries, five seconds apart.
set_tests_properties(application_device_binding_launch_renewal PROPERTIES ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 180 PASS_REGULAR_EXPRESSION "device-binding-launch-renewal-tests:pass")

# BPR-0644. A restored grant with hours of access left survives an orderly
# quit and a termination signal, each followed by a real relaunch; the
# driver reads the metadata row from the store after every process exits.
bpr_add_executable(test_device_binding_relaunch device_binding_relaunch_tests.cpp)
target_link_libraries(test_device_binding_relaunch PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_device_binding_relaunch PRIVATE BPR_BUILD_TESTING=1)
bpr_enable_hardening(test_device_binding_relaunch)
bpr_enable_warnings(test_device_binding_relaunch)
# [[BPR-0876]]. Uncontained on Windows: contained its seed stops at
# SEED_STARTUP_PROFILE_REFUSAL:registry_unavailable, the data root's ancestor
# walk the container cannot make (dos-volume-name).
bpr_add_network_isolated_test(NAME application_device_binding_relaunch WINDOWS_UNCONTAINED
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/device_binding_relaunch_contract.py"
    --application $<TARGET_FILE:test_device_binding_relaunch>)
set_tests_properties(application_device_binding_relaunch PROPERTIES ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 240 PASS_REGULAR_EXPRESSION "DEVICE_BINDING_RELAUNCH_OK")

# [[BPR-0803]]. Choosing GitHub CLI while a device sign-in waits keeps the code
# beside the GitHub CLI choice; each of the two endings runs to a session and
# leaves no flow behind. The GitHub CLI way in is composed only on macOS: the
# composition's discovery is `__APPLE__` code, as the CLI courses of
# `application_setup_destination_public_path` are. So the row is registered
# only there; elsewhere the course cannot reach a CLI session, and on Linux it
# failed at "one discovered account connects at once and signs in". The
# executable still builds everywhere, so its source stays portable.
bpr_add_executable(test_device_flow_cli_choice device_flow_cli_choice_tests.cpp)
target_link_libraries(test_device_flow_cli_choice PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_device_flow_cli_choice PRIVATE BPR_BUILD_TESTING=1)
bpr_enable_hardening(test_device_flow_cli_choice)
bpr_enable_warnings(test_device_flow_cli_choice)
if(APPLE)
    bpr_add_network_isolated_test(NAME application_device_flow_cli_choice COMMAND test_device_flow_cli_choice)
    set_tests_properties(application_device_flow_cli_choice PROPERTIES ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 120 PASS_REGULAR_EXPRESSION "device-flow-cli-choice-tests:pass")
endif()

# The same native sink and canary course now run on Windows; no platform privacy waiver.
bpr_add_executable(test_application_logging application_logging_tests.cpp)
target_link_libraries(test_application_logging PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_application_logging PRIVATE BPR_BUILD_TESTING=1)
bpr_enable_hardening(test_application_logging)
bpr_enable_warnings(test_application_logging)
bpr_add_network_isolated_test(NAME application_logging WINDOWS_UNCONTAINED COMMAND test_application_logging)
bpr_add_network_isolated_test(NAME application_logging_cancel_startup WINDOWS_UNCONTAINED COMMAND test_application_logging --cancel-startup)
set_tests_properties(application_logging application_logging_cancel_startup PROPERTIES ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 30)
foreach(scenario full unavailable capability-failure)
    bpr_add_network_isolated_test(NAME application_logging_${scenario} WINDOWS_UNCONTAINED COMMAND test_application_logging --${scenario}-log)
    set_tests_properties(application_logging_${scenario} PROPERTIES ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 30)
endforeach()

# Real normal controls, typed fake server, real SQLite; no live effects.
bpr_add_executable(test_review_tour_composition_public_path review_tour_composition_public_path.cpp)
target_link_libraries(test_review_tour_composition_public_path PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_review_tour_composition_public_path PRIVATE BPR_BUILD_TESTING=1)
bpr_enable_hardening(test_review_tour_composition_public_path)
bpr_enable_warnings(test_review_tour_composition_public_path)
bpr_add_network_isolated_test(NAME application_review_tour_composition_public_path
    WINDOWS_UNCONTAINED
    COMMAND test_review_tour_composition_public_path "${CMAKE_CURRENT_BINARY_DIR}/review-tour-composition.png")
set_tests_properties(application_review_tour_composition_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}"
    TIMEOUT 90 PASS_REGULAR_EXPRESSION "review-tour-composition:pass")
foreach(scale IN ITEMS 125 200)
    if(scale EQUAL 125)
        set(tour_scale_factor "1.25")
    else()
        set(tour_scale_factor "2")
    endif()
    bpr_add_network_isolated_test(NAME application_review_tour_composition_scale_${scale}
        WINDOWS_UNCONTAINED
        COMMAND test_review_tour_composition_public_path "${CMAKE_CURRENT_BINARY_DIR}/review-tour-${scale}.png")
    set_tests_properties(application_review_tour_composition_scale_${scale} PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment};QT_SCALE_FACTOR=${tour_scale_factor}"
        TIMEOUT 90 PASS_REGULAR_EXPRESSION "review-tour-composition:pass")
endforeach()

bpr_add_executable(test_better_pull_review_review_submission_composition_public_path review_submission_composition_public_path.cpp)
target_link_libraries(test_better_pull_review_review_submission_composition_public_path PRIVATE BetterPullReview::Composition BetterPullReview::SQLite3)
target_compile_definitions(test_better_pull_review_review_submission_composition_public_path PRIVATE BPR_BUILD_TESTING=1)
bpr_enable_hardening(test_better_pull_review_review_submission_composition_public_path)
bpr_enable_warnings(test_better_pull_review_review_submission_composition_public_path)
bpr_add_network_isolated_test(NAME application_review_submission_composition_public_path
    WINDOWS_UNCONTAINED
    COMMAND test_better_pull_review_review_submission_composition_public_path "${CMAKE_CURRENT_BINARY_DIR}/review-submission-composition.png")
set_tests_properties(application_review_submission_composition_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}"
    TIMEOUT 90 PASS_REGULAR_EXPRESSION "review-submission-composition:pass")
foreach(scale IN ITEMS 125 200)
    if(scale EQUAL 125)
        set(scale_factor "1.25")
    else()
        set(scale_factor "2")
    endif()
    bpr_add_network_isolated_test(NAME application_review_submission_composition_scale_${scale}
        WINDOWS_UNCONTAINED
        COMMAND test_better_pull_review_review_submission_composition_public_path "${CMAKE_CURRENT_BINARY_DIR}/review-submission-composition-${scale}.png")
    set_tests_properties(application_review_submission_composition_scale_${scale} PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment};QT_SCALE_FACTOR=${scale_factor}"
        TIMEOUT 90 PASS_REGULAR_EXPRESSION "review-submission-composition:pass")
endforeach()

bpr_add_executable(
    test_setup_destination_public_path
    setup_destination_public_path_tests.cpp
)
target_link_libraries(test_setup_destination_public_path PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_setup_destination_public_path PRIVATE BPR_BUILD_TESTING=1)
target_compile_features(test_setup_destination_public_path PRIVATE cxx_std_23)
bpr_enable_hardening(test_setup_destination_public_path)
bpr_enable_warnings(test_setup_destination_public_path)
bpr_add_network_isolated_test(
    NAME application_setup_destination_public_path
    WINDOWS_UNCONTAINED
    COMMAND test_setup_destination_public_path
)
set_tests_properties(
    application_setup_destination_public_path
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        TIMEOUT 60
        PASS_REGULAR_EXPRESSION "setup-destination-public-path:pass"
)
if(APPLE)
    # [[BPR-1200]]. A refused first read, signed out and signed in, stays on
    # Home and says why in a card; the readable pull request is its control.
    bpr_add_network_isolated_test(
        NAME application_refused_first_read_public_path
        COMMAND test_setup_destination_public_path --refused-first-read
    )
    set_tests_properties(application_refused_first_read_public_path PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 60
        PASS_REGULAR_EXPRESSION "refused-first-read-public-path:pass")
endif()
bpr_add_network_isolated_test(
    NAME application_selected_file_safety_public_path
    WINDOWS_UNCONTAINED
    COMMAND test_setup_destination_public_path --selected-file-safety
)
set_tests_properties(application_selected_file_safety_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 60
    PASS_REGULAR_EXPRESSION "selected-file-safety-public-path:pass")
# [[BPR-1213]]. The constructed nested-folders pull request on Overview: flat
# folder rows, and a click or Left/Right hiding only that folder's own files.
bpr_add_network_isolated_test(
    NAME application_nested_overview_folders_public_path
    WINDOWS_UNCONTAINED
    COMMAND test_setup_destination_public_path --nested-overview-folders
)
set_tests_properties(application_nested_overview_folders_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 60
    PASS_REGULAR_EXPRESSION "nested-overview-folders-public-path:pass")
# [[BPR-1089]]. The Conventional Comments pull request on Overview: the composed
# label counts, Settings' chips, the exact drill-down into Discuss by pointer and
# keyboard, and the list following a resolve on the fixture server.
bpr_add_network_isolated_test(
    NAME application_overview_feedback_public_path
    WINDOWS_UNCONTAINED
    COMMAND test_setup_destination_public_path --overview-feedback
)
set_tests_properties(application_overview_feedback_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 90
    PASS_REGULAR_EXPRESSION "overview-feedback-public-path:pass")
# [[BPR-1089]]. overview-labels-v1, the pull request the owner's Overview look
# opens: its counts in Open and All, Settings' chips in order, the ? count, and
# the Label key, a Comments menu and Discuss's caption in the page's theme.
bpr_add_network_isolated_test(
    NAME application_overview_labels_public_path
    WINDOWS_UNCONTAINED
    COMMAND test_setup_destination_public_path --overview-labels
)
set_tests_properties(application_overview_labels_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 90
    PASS_REGULAR_EXPRESSION "overview-labels-public-path:pass")

bpr_add_executable(test_attended_recent_replay attended_recent_replay_tests.cpp)
target_link_libraries(test_attended_recent_replay PRIVATE BetterPullReview::Composition BetterPullReview::AdapterGitHub)
target_include_directories(test_attended_recent_replay PRIVATE "${PROJECT_SOURCE_DIR}/src/adapters/github/src")
target_compile_definitions(test_attended_recent_replay PRIVATE BPR_BUILD_TESTING=1)
target_compile_features(test_attended_recent_replay PRIVATE cxx_std_23)
bpr_enable_hardening(test_attended_recent_replay)
bpr_enable_warnings(test_attended_recent_replay)
bpr_add_network_isolated_test(NAME application_attended_recent_replay WINDOWS_UNCONTAINED COMMAND test_attended_recent_replay)
set_tests_properties(application_attended_recent_replay PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 120
    PASS_REGULAR_EXPRESSION "ATTENDED_RECENT_REPLAY_OK")

bpr_add_executable(test_profile_startup_public_path profile_startup_public_path_tests.cpp)
target_link_libraries(test_profile_startup_public_path PRIVATE BetterPullReview::Composition BetterPullReview::AdapterSQLite)
target_compile_definitions(test_profile_startup_public_path PRIVATE BPR_BUILD_TESTING=1)
target_compile_features(test_profile_startup_public_path PRIVATE cxx_std_23)
bpr_enable_hardening(test_profile_startup_public_path)
bpr_enable_warnings(test_profile_startup_public_path)
bpr_add_network_isolated_test(NAME application_profile_startup_public_path WINDOWS_UNCONTAINED COMMAND test_profile_startup_public_path)
set_tests_properties(application_profile_startup_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 90
    PASS_REGULAR_EXPRESSION "profile-startup-public-path:pass")

bpr_add_executable(test_profile_management_public_path profile_management_public_path_tests.cpp)
target_link_libraries(test_profile_management_public_path PRIVATE BetterPullReview::Composition BetterPullReview::AdapterSQLite)
target_compile_definitions(test_profile_management_public_path PRIVATE BPR_BUILD_TESTING=1)
target_compile_features(test_profile_management_public_path PRIVATE cxx_std_23)
bpr_enable_hardening(test_profile_management_public_path)
bpr_enable_warnings(test_profile_management_public_path)
# [[BPR-1050]]. The course's `shared-parent` section asserts [[BPR-0817]]'s
# refusal of a group-writable parent, so the refusal line is its subject there.
# The section is compiled out on Windows, where the row keeps the refusal entry
# for its main root.
set(bpr_profile_management_refusal "")
if(NOT WIN32)
    set(bpr_profile_management_refusal EXPECTS_WORKSPACE_RECOVERY_REFUSAL)
endif()
bpr_add_network_isolated_test(NAME application_profile_management_public_path WINDOWS_UNCONTAINED ${bpr_profile_management_refusal} COMMAND test_profile_management_public_path)
# [[BPR-0777]]. The course now drives both empty-desktop policies and three
# reopening gestures, so it starts several more profile runtimes than it did at 90 s.
set_tests_properties(application_profile_management_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 300
    PASS_REGULAR_EXPRESSION "profile-management-public-path:pass")

if(WIN32)
    # [[BPR-0806]]. The tests-disabled Windows image could not start its profile
    # desktop. This holds the Qt-boundary path conversion, the strict storage root
    # validator and the private-object admission of an existing store file on the
    # target itself.
    bpr_add_executable(test_windows_profile_storage_startup_contract windows_profile_storage_startup_contract_tests.cpp)
    target_link_libraries(test_windows_profile_storage_startup_contract PRIVATE BetterPullReview::Composition BetterPullReview::AdapterSQLite advapi32)
    target_compile_features(test_windows_profile_storage_startup_contract PRIVATE cxx_std_23)
    bpr_enable_hardening(test_windows_profile_storage_startup_contract)
    bpr_enable_warnings(test_windows_profile_storage_startup_contract)
    bpr_add_network_isolated_test(NAME application_windows_profile_storage_startup_contract WINDOWS_UNCONTAINED COMMAND test_windows_profile_storage_startup_contract)
    set_tests_properties(application_windows_profile_storage_startup_contract PROPERTIES
        TIMEOUT 60
        PASS_REGULAR_EXPRESSION "windows-profile-storage-startup-contract:pass")
    set_property(TEST application_windows_profile_storage_startup_contract APPEND PROPERTY LABELS "BPR-0806;target-native-windows")

    # [[BPR-0653]]. The strip as the Windows title row, on the real windows
    # platform: the host's hit test, the removed title bar, maximise and restore
    # by double-click and by the caption buttons, minimise and close. The
    # offscreen window courses cannot see a frame or a non-client message.
    bpr_add_executable(test_better_pull_review_windows_title_row_host_tests windows_title_row_host_tests.cpp)
    target_link_libraries(test_better_pull_review_windows_title_row_host_tests PRIVATE BetterPullReview::Composition)
    target_compile_features(test_better_pull_review_windows_title_row_host_tests PRIVATE cxx_std_23)
    bpr_enable_hardening(test_better_pull_review_windows_title_row_host_tests)
    bpr_enable_warnings(test_better_pull_review_windows_title_row_host_tests)
    bpr_add_network_isolated_test(NAME application_windows_title_row COMMAND test_better_pull_review_windows_title_row_host_tests)
    set_tests_properties(application_windows_title_row PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=windows"
        TIMEOUT 60
        PASS_REGULAR_EXPRESSION "windows-title-row-host-tests:pass")
    set_property(TEST application_windows_title_row APPEND PROPERTY LABELS "BPR-0653;target-native-windows")

    # [[BPR-0974]]. The tests-disabled image keeps its Qt resources in a bundle
    # beside it and has no tests of its own. This holds the same bundle, built by
    # the same rule here, and the image's own registration code, compiled as the
    # image compiles it: the compiled digest, every refusal, the bundle beside the
    # executable and every resource the application reads from it. It links Qt
    # Core and nothing that embeds a resource.
    get_target_property(bpr_resource_bundle better_pull_review_resource_bundle BPR_BUNDLE_FILE)
    get_target_property(bpr_resource_bundle_header better_pull_review_resource_bundle BPR_BUNDLE_HEADER_DIRECTORY)
    get_property(bpr_syntax_collection GLOBAL PROPERTY BPR_KSYNTAXHIGHLIGHTING_SYNTAX_COLLECTION)
    get_property(bpr_syntax_index GLOBAL PROPERTY BPR_KSYNTAXHIGHLIGHTING_SYNTAX_INDEX)
    get_target_property(bpr_code_font_directory BetterPullReview::CodeFont BPR_CODE_FONT_DIRECTORY)
    bpr_add_executable(
        test_better_pull_review_installed_resources_contract
        installed_resources_contract_tests.cpp
        "${PROJECT_SOURCE_DIR}/src/app/src/installed_resources.cpp"
    )
    target_link_libraries(test_better_pull_review_installed_resources_contract PRIVATE Qt6::Core)
    target_include_directories(test_better_pull_review_installed_resources_contract PRIVATE "${bpr_resource_bundle_header}")
    target_compile_definitions(
        test_better_pull_review_installed_resources_contract
        PRIVATE
            BPR_EXTERNAL_QT_RESOURCES=1
            "BPR_TEST_RESOURCE_BUNDLE=\"${bpr_resource_bundle}\""
            "BPR_TEST_IMAGE_BUNDLE=\"$<TARGET_FILE_DIR:test_better_pull_review_installed_resources_contract>/${BPR_QT_RESOURCE_BUNDLE_NAME}\""
            # The source root only: the Dif boundary verifier admits no graph
            # reference to the Dif asset tree but the accepted products
            # themselves, so the test names that directory in its source.
            "BPR_TEST_SOURCE_ROOT=\"${PROJECT_SOURCE_DIR}\""
            "BPR_TEST_SYNTAX_COLLECTION=\"${bpr_syntax_collection}\""
            "BPR_TEST_SYNTAX_INDEX=\"${bpr_syntax_index}\""
            "BPR_TEST_CODE_FONT_DIRECTORY=\"${bpr_code_font_directory}\""
    )
    target_compile_features(test_better_pull_review_installed_resources_contract PRIVATE cxx_std_23)
    bpr_enable_hardening(test_better_pull_review_installed_resources_contract)
    bpr_enable_warnings(test_better_pull_review_installed_resources_contract)
    add_dependencies(test_better_pull_review_installed_resources_contract better_pull_review_resource_bundle)
    add_custom_command(TARGET test_better_pull_review_installed_resources_contract POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${bpr_resource_bundle}"
            "$<TARGET_FILE_DIR:test_better_pull_review_installed_resources_contract>/${BPR_QT_RESOURCE_BUNDLE_NAME}"
        VERBATIM)
    bpr_add_network_isolated_test(NAME application_installed_resources_contract COMMAND test_better_pull_review_installed_resources_contract)
    set_tests_properties(application_installed_resources_contract PROPERTIES
        TIMEOUT 60
        PASS_REGULAR_EXPRESSION "installed-resources-contract:pass")
    set_property(TEST application_installed_resources_contract APPEND PROPERTY LABELS "BPR-0974;target-native-windows")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # [[BPR-1024]]. Which X11 events the activation host keeps from Qt, and that
    # it does nothing off the xcb platform. The window manager's refusal and
    # where keys then go are the native WSLg course's.
    bpr_add_executable(test_better_pull_review_xcb_activation_host_tests xcb_activation_host_tests.cpp)
    target_link_libraries(test_better_pull_review_xcb_activation_host_tests PRIVATE BetterPullReview::Composition PkgConfig::BPR_XCB)
    target_compile_features(test_better_pull_review_xcb_activation_host_tests PRIVATE cxx_std_23)
    bpr_enable_hardening(test_better_pull_review_xcb_activation_host_tests)
    bpr_enable_warnings(test_better_pull_review_xcb_activation_host_tests)
    bpr_add_network_isolated_test(NAME application_xcb_activation_host COMMAND test_better_pull_review_xcb_activation_host_tests)
    set_tests_properties(application_xcb_activation_host PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 30
        PASS_REGULAR_EXPRESSION "xcb-activation-host-tests:pass")
    set_property(TEST application_xcb_activation_host APPEND PROPERTY LABELS "BPR-1024")
endif()

if(APPLE)
    # RFC-BPR-0033 B1 and B2, the native half. The owner's attended review of the
    # shipped build found the macOS menu bar emptied by closing the last window,
    # which the offscreen course above cannot see. This course runs the same
    # desktop composition on the cocoa platform and reads `[NSApp mainMenu]`
    # through AppKit at every event-loop turn of the close, then reopens through
    # the Dock's reopen message, native activation, Command-N and the Profiles
    # menu. It needs the owner's Aqua session; there is no offscreen substitute.
    enable_language(OBJCXX)
    bpr_add_executable(test_profile_empty_desktop_native_menu_bar
        profile_empty_desktop_native_menu_bar_tests.cpp
        native_main_menu_probe_mac.mm)
    set_source_files_properties(native_main_menu_probe_mac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    find_library(BPR_TEST_APPKIT_FRAMEWORK AppKit REQUIRED)
    # QtGui's private QPA headers for the focus-window delivery the cocoa plugin
    # itself uses, taken only when the session refuses to activate the process.
    target_link_libraries(test_profile_empty_desktop_native_menu_bar PRIVATE
        BetterPullReview::Composition "${BPR_TEST_APPKIT_FRAMEWORK}" objc)
    file(GLOB bpr_qtgui_private_headers LIST_DIRECTORIES true
        "${Qt6Gui_DIR}/../../QtGui.framework/Headers/[0-9]*.[0-9]*.[0-9]*")
    list(LENGTH bpr_qtgui_private_headers bpr_qtgui_private_header_count)
    if(NOT bpr_qtgui_private_header_count EQUAL 1)
        message(FATAL_ERROR "Expected one versioned QtGui private header directory, found: ${bpr_qtgui_private_headers}")
    endif()
    target_include_directories(test_profile_empty_desktop_native_menu_bar SYSTEM PRIVATE
        "${bpr_qtgui_private_headers}" "${bpr_qtgui_private_headers}/QtGui")
    target_compile_definitions(test_profile_empty_desktop_native_menu_bar PRIVATE BPR_BUILD_TESTING=1)
    target_compile_features(test_profile_empty_desktop_native_menu_bar PRIVATE cxx_std_23)
    bpr_enable_hardening(test_profile_empty_desktop_native_menu_bar)
    bpr_enable_warnings(test_profile_empty_desktop_native_menu_bar)
    bpr_add_network_isolated_test(NAME application_profile_empty_desktop_native_menu_bar COMMAND test_profile_empty_desktop_native_menu_bar)
    set_tests_properties(application_profile_empty_desktop_native_menu_bar PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=cocoa" RUN_SERIAL TRUE TIMEOUT 300
        PASS_REGULAR_EXPRESSION "profile-empty-desktop-native-menu-bar:pass")
endif()

if(APPLE)
    # [[BPR-1295]]. Hide Drawers' Command-B on the native menu bar: an editable
    # note keeps the key, a list gives it to the command. AppKit's View menu
    # carries the key equivalent, which the offscreen window course cannot see.
    # It needs the owner's Aqua session, as the course above does.
    bpr_add_executable(test_hide_drawers_native_menu_key
        hide_drawers_native_menu_key_tests.cpp
        native_main_menu_probe_mac.mm)
    target_link_libraries(test_hide_drawers_native_menu_key PRIVATE
        BetterPullReview::Composition "${BPR_TEST_APPKIT_FRAMEWORK}" objc)
    target_include_directories(test_hide_drawers_native_menu_key SYSTEM PRIVATE
        "${bpr_qtgui_private_headers}" "${bpr_qtgui_private_headers}/QtGui")
    target_compile_features(test_hide_drawers_native_menu_key PRIVATE cxx_std_23)
    bpr_enable_hardening(test_hide_drawers_native_menu_key)
    bpr_enable_warnings(test_hide_drawers_native_menu_key)
    bpr_add_network_isolated_test(NAME qt_hide_drawers_native_menu_key COMMAND test_hide_drawers_native_menu_key)
    set_tests_properties(qt_hide_drawers_native_menu_key PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=cocoa" RUN_SERIAL TRUE TIMEOUT 120
        PASS_REGULAR_EXPRESSION "hide-drawers-native-menu:pass")
endif()

# [[BPR-0891]]'s sweep. The in-place updater is macOS only. Its installation
# transaction and helper are compiled only there, and elsewhere
# `release_update_staging.cpp` answers `unsupported_platform` and stages
# nothing (`src/adapters/update`). Windows and Linux keep manual distribution
# (`docs/architecture/release-readiness.md`), so this is not a waiver: the
# other platforms have nothing of this to run.
if(APPLE)
    bpr_add_executable(test_release_update_transaction release_update_transaction_tests.cpp)
    target_link_libraries(test_release_update_transaction PRIVATE BetterPullReview::AdapterUpdate Qt6::Core)
    bpr_enable_hardening(test_release_update_transaction)
    bpr_enable_warnings(test_release_update_transaction)
    bpr_add_network_isolated_test(NAME release_update_transaction COMMAND test_release_update_transaction)
    set_tests_properties(release_update_transaction PROPERTIES TIMEOUT 120)
    bpr_add_executable(test_release_update_staging release_update_staging_tests.cpp)
    target_link_libraries(test_release_update_staging PRIVATE BetterPullReview::AdapterUpdate Qt6::Core)
    bpr_enable_hardening(test_release_update_staging)
    bpr_enable_warnings(test_release_update_staging)
    bpr_add_network_isolated_test(NAME release_update_native_staging
        COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tests/scripts/verify_release_update_staging.py"
        --executable "$<TARGET_FILE:test_release_update_staging>")
    set_tests_properties(release_update_native_staging PROPERTIES TIMEOUT 90)
endif()

bpr_add_executable(test_release_update_public_path release_update_public_path_tests.cpp)
bpr_add_executable(test_release_maintenance_public_path release_maintenance_public_path_tests.cpp)
target_link_libraries(test_release_maintenance_public_path PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_release_maintenance_public_path PRIVATE BPR_BUILD_TESTING=1)
target_compile_features(test_release_maintenance_public_path PRIVATE cxx_std_23)
bpr_enable_hardening(test_release_maintenance_public_path)
bpr_enable_warnings(test_release_maintenance_public_path)
bpr_add_network_isolated_test(NAME application_release_maintenance_public_path WINDOWS_UNCONTAINED
    COMMAND test_release_maintenance_public_path)
set_tests_properties(application_release_maintenance_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment};BPR_MAINTENANCE_PUBLIC_RENDER=${CMAKE_CURRENT_BINARY_DIR}/maintenance-public.png"
    RUN_SERIAL TRUE TIMEOUT 90 PASS_REGULAR_EXPRESSION "release-maintenance-public-path:pass")
bpr_add_executable(test_release_update_recovery release_update_recovery_tests.cpp)
target_link_libraries(test_release_update_recovery PRIVATE BetterPullReview::AdapterSQLite Qt6::Core)
bpr_enable_hardening(test_release_update_recovery)
bpr_enable_warnings(test_release_update_recovery)
# [[BPR-1031]]. Uncontained on Windows: its first step narrows a QTemporaryDir
# the container cannot open for WRITE_DAC (dos-volume-name, as BPR-0833).
bpr_add_network_isolated_test(NAME release_update_recovery WINDOWS_UNCONTAINED COMMAND test_release_update_recovery)
target_link_libraries(test_release_update_public_path PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_release_update_public_path PRIVATE BPR_BUILD_TESTING=1)
target_compile_features(test_release_update_public_path PRIVATE cxx_std_23)
bpr_enable_hardening(test_release_update_public_path)
bpr_enable_warnings(test_release_update_public_path)
bpr_add_network_isolated_test(NAME application_release_update_public_path WINDOWS_UNCONTAINED COMMAND test_release_update_public_path)
set_tests_properties(application_release_update_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 90
    PASS_REGULAR_EXPRESSION "release-update-public-path:pass")

bpr_add_executable(test_profile_multi_window_public_path profile_multi_window_public_path_tests.cpp)
target_link_libraries(test_profile_multi_window_public_path PRIVATE BetterPullReview::Composition BetterPullReview::SQLite3)
target_compile_definitions(test_profile_multi_window_public_path PRIVATE BPR_BUILD_TESTING=1)
target_compile_features(test_profile_multi_window_public_path PRIVATE cxx_std_23)
bpr_enable_hardening(test_profile_multi_window_public_path)
bpr_enable_warnings(test_profile_multi_window_public_path)
bpr_add_network_isolated_test(NAME application_profile_multi_window_public_path WINDOWS_UNCONTAINED COMMAND test_profile_multi_window_public_path)
set_tests_properties(application_profile_multi_window_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 180
    PASS_REGULAR_EXPRESSION "profile-multi-window-public-path:pass")

# [[BPR-1027]]. A New Window window closed while the first stays open: its
# close is the last reference to its session, so the session's whole
# destructor runs from the OnClosed erase.
bpr_add_executable(test_profile_second_window_close_public_path profile_second_window_close_public_path_tests.cpp)
target_link_libraries(test_profile_second_window_close_public_path PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_profile_second_window_close_public_path PRIVATE BPR_BUILD_TESTING=1)
target_compile_features(test_profile_second_window_close_public_path PRIVATE cxx_std_23)
bpr_enable_hardening(test_profile_second_window_close_public_path)
bpr_enable_warnings(test_profile_second_window_close_public_path)
bpr_add_network_isolated_test(NAME application_profile_second_window_close_public_path WINDOWS_UNCONTAINED COMMAND test_profile_second_window_close_public_path)
set_tests_properties(application_profile_second_window_close_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 180
    PASS_REGULAR_EXPRESSION "profile-second-window-close:pass")

# [[BPR-0990]]. Personal discovery through the composed application: a saved
# repository with thousands of unrelated open pull requests, two windows, and
# only the bounded personal reads.
bpr_add_executable(test_personal_discovery_public_path personal_discovery_public_path_tests.cpp)
target_link_libraries(test_personal_discovery_public_path PRIVATE BetterPullReview::Composition BetterPullReview::SQLite3)
target_compile_definitions(test_personal_discovery_public_path PRIVATE BPR_BUILD_TESTING=1)
target_compile_features(test_personal_discovery_public_path PRIVATE cxx_std_23)
bpr_enable_hardening(test_personal_discovery_public_path)
bpr_enable_warnings(test_personal_discovery_public_path)
bpr_add_network_isolated_test(NAME application_personal_discovery_public_path WINDOWS_UNCONTAINED COMMAND test_personal_discovery_public_path)
set_tests_properties(application_personal_discovery_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 180
    PASS_REGULAR_EXPRESSION "personal-discovery-public-path:pass")

# [[BPR-0978]]. Two concurrent profiles, two accounts on one fixture server.
bpr_add_executable(test_profile_two_account_isolation_public_path profile_two_account_isolation_public_path_tests.cpp)
target_link_libraries(test_profile_two_account_isolation_public_path PRIVATE BetterPullReview::Composition BetterPullReview::AdapterSQLite BetterPullReview::SQLite3)
target_compile_definitions(test_profile_two_account_isolation_public_path PRIVATE BPR_BUILD_TESTING=1)
target_compile_features(test_profile_two_account_isolation_public_path PRIVATE cxx_std_23)
bpr_enable_hardening(test_profile_two_account_isolation_public_path)
bpr_enable_warnings(test_profile_two_account_isolation_public_path)
bpr_add_network_isolated_test(NAME application_profile_two_account_isolation_public_path WINDOWS_UNCONTAINED COMMAND test_profile_two_account_isolation_public_path)
set_tests_properties(application_profile_two_account_isolation_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 240
    PASS_REGULAR_EXPRESSION "profile-two-account-isolation-public-path:pass")

bpr_add_executable(test_profile_owner_admission_public_path profile_owner_admission_public_path_tests.cpp)
# [[BPR-0979]]. The activation course runs on Windows too, over the named pipe;
# the Windows row is uncontained for the same private-root reason as the
# admission course below.
bpr_add_executable(test_profile_activation_public_path profile_activation_public_path_tests.cpp)
target_link_libraries(test_profile_activation_public_path PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_profile_activation_public_path PRIVATE BPR_BUILD_TESTING=1)
target_compile_features(test_profile_activation_public_path PRIVATE cxx_std_23)
bpr_enable_hardening(test_profile_activation_public_path)
bpr_enable_warnings(test_profile_activation_public_path)
# [[BPR-1234]]. Without `app_bpr` the pair is absent, not two empty arguments,
# which the script refuses.
set(bpr_activation_tour_arguments "")
if(TARGET app_bpr)
    set(bpr_activation_tour_arguments --tour-application $<TARGET_FILE:app_bpr>)
endif()
bpr_add_network_isolated_test(NAME application_profile_activation_public_path WINDOWS_UNCONTAINED
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/profile_activation_contract.py"
    --application $<TARGET_FILE:test_profile_activation_public_path>
    --desktop-application $<TARGET_FILE:app_better_pull_review>
    ${bpr_activation_tour_arguments})
set_tests_properties(application_profile_activation_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 260)
target_link_libraries(test_profile_owner_admission_public_path PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_profile_owner_admission_public_path PRIVATE BPR_BUILD_TESTING=1)
target_compile_features(test_profile_owner_admission_public_path PRIVATE cxx_std_23)
bpr_enable_hardening(test_profile_owner_admission_public_path)
bpr_enable_warnings(test_profile_owner_admission_public_path)
bpr_add_network_isolated_test(NAME application_profile_owner_admission_public_path WINDOWS_UNCONTAINED
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/profile_owner_admission_contract.py"
    --application $<TARGET_FILE:test_profile_owner_admission_public_path>)
set_tests_properties(application_profile_owner_admission_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 240)

# [[BPR-1179]]. The reviewer the two-round, parity and large-project courses
# drive is built wherever one of them runs: beside the shipped bpr, or in a
# graph without local automation, whose course reaches no bpr at all.
if(TARGET app_bpr OR NOT BPR_BUILD_LOCAL_AUTOMATION)
    # [[BPR-1166]]. The complete manual two-round course in the application's
    # own profile desktop, restarted twice on one profile: Open Project, eight
    # notes with two suggestions, Copy, Save and the bundle, the shipped
    # `bpr cycle` export and a granted harness's read, edits made outside, a
    # restart, Capture revision, comparisons, verification, Save next round and
    # another capture. No agent component is reachable and no callback is read.
    # Uncontained on Windows (dos-volume-name): contained, the course's first
    # `git init` in its AC\Temp project exits 128 with "unable to get current
    # working directory: Permission denied", Git's getcwd, before any desktop
    # step. Measured on JaegAW 2026-10-03.
    # [[BPR-1184]] measures large projects through the same reviewer, with
    # [[BPR-0967]]'s native process-memory counters.
    bpr_add_executable(test_review_cycle_two_round_public_path review_cycle_two_round_public_path_tests.cpp large_review_native_memory.cpp)
    target_link_libraries(test_review_cycle_two_round_public_path PRIVATE BetterPullReview::Composition)
    if(WIN32)
        target_link_libraries(test_review_cycle_two_round_public_path PRIVATE psapi)
    endif()
    target_compile_definitions(test_review_cycle_two_round_public_path PRIVATE BPR_BUILD_TESTING=1)
    target_compile_features(test_review_cycle_two_round_public_path PRIVATE cxx_std_23)
    bpr_enable_hardening(test_review_cycle_two_round_public_path)
    bpr_enable_warnings(test_review_cycle_two_round_public_path)
    if(APPLE)
        # QtGui's QPA focus-window entry, which the cocoa plugin itself calls when
        # AppKit makes a window key: a native run from a terminal while someone
        # uses another application gets no key window otherwise (as
        # test_profile_empty_desktop_native_menu_bar does).
        file(GLOB bpr_two_round_qtgui_private LIST_DIRECTORIES true
            "${Qt6Gui_DIR}/../../QtGui.framework/Headers/[0-9]*.[0-9]*.[0-9]*")
        list(LENGTH bpr_two_round_qtgui_private bpr_two_round_qtgui_private_count)
        if(NOT bpr_two_round_qtgui_private_count EQUAL 1)
            message(FATAL_ERROR "Expected one versioned QtGui private header directory, found: ${bpr_two_round_qtgui_private}")
        endif()
        target_include_directories(test_review_cycle_two_round_public_path SYSTEM PRIVATE
            "${bpr_two_round_qtgui_private}" "${bpr_two_round_qtgui_private}/QtGui")
    endif()
endif()
if(NOT BPR_BUILD_LOCAL_AUTOMATION)
    # [[BPR-1179]]. The same complete manual course in a desktop built without
    # local automation: no bpr exists, the window's own Copy, Save and bundle
    # agree with the canonical payload, a fourth launch reads the cycle back,
    # and on POSIX no owner command endpoint is bound beside the activation one.
    bpr_add_network_isolated_test(NAME application_review_cycle_automation_off_two_round WINDOWS_UNCONTAINED
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/review_cycle_two_round_contract.py"
        --application $<TARGET_FILE:test_review_cycle_two_round_public_path>
        --without-local-automation)
    set_tests_properties(application_review_cycle_automation_off_two_round PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "REVIEW_CYCLE_TWO_ROUND_OK"
        TIMEOUT 600)
endif()

# [[BPR-1062]]. A real desktop author and the shipped CLI share one live draft.
if(TARGET app_bpr)
    bpr_add_executable(test_profile_tour_handoff_public_path profile_tour_handoff_public_path_tests.cpp)
    target_link_libraries(test_profile_tour_handoff_public_path PRIVATE BetterPullReview::Composition)
    target_compile_definitions(test_profile_tour_handoff_public_path PRIVATE BPR_BUILD_TESTING=1)
    target_compile_features(test_profile_tour_handoff_public_path PRIVATE cxx_std_23)
    bpr_enable_hardening(test_profile_tour_handoff_public_path)
    bpr_enable_warnings(test_profile_tour_handoff_public_path)
    bpr_add_network_isolated_test(NAME application_profile_tour_handoff_public_path WINDOWS_UNCONTAINED
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/profile_tour_handoff_contract.py"
        --application $<TARGET_FILE:test_profile_tour_handoff_public_path>
        --tour-application $<TARGET_FILE:app_bpr>)
    set_tests_properties(application_profile_tour_handoff_public_path PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 240)
    # [[BPR-1168]]. A real desktop owner with a local cycle open and the shipped
    # `bpr cycle`, in both directions: forwarded commands, the reviewer's unsaved
    # and composing text, duplicates, concurrent callers, owner death and
    # replacement, and an owner of another protocol version. Registered contained:
    # Windows has not measured it yet; a measured refusal is pinned, not guessed.
    bpr_add_executable(test_profile_cycle_handoff_public_path profile_cycle_handoff_public_path_tests.cpp)
    target_link_libraries(test_profile_cycle_handoff_public_path PRIVATE BetterPullReview::Composition)
    target_compile_definitions(test_profile_cycle_handoff_public_path PRIVATE BPR_BUILD_TESTING=1)
    target_compile_features(test_profile_cycle_handoff_public_path PRIVATE cxx_std_23)
    bpr_enable_hardening(test_profile_cycle_handoff_public_path)
    bpr_enable_warnings(test_profile_cycle_handoff_public_path)
    bpr_add_network_isolated_test(NAME application_profile_cycle_handoff_public_path WINDOWS_UNCONTAINED
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/profile_cycle_handoff_contract.py"
        --application $<TARGET_FILE:test_profile_cycle_handoff_public_path>
        --cycle-application $<TARGET_FILE:app_bpr>)
    set_tests_properties(application_profile_cycle_handoff_public_path PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 300)
    # [[BPR-1181]]. Review Cycles' security boundaries attacked through real
    # processes, the shipped `bpr` and the reviewer's desktop: a hostile
    # checkout whose Git canaries never fire, hostile note text, forged
    # responses, one policy for every channel with and without the desktop, a
    # same-user peer and impostor owners at the owner's endpoint, and credential
    # and content canaries kept out of every file, log and refusal. Uncontained
    # on Windows (dos-volume-name): contained, the course's first `git init`
    # fails before any attack runs. Measured on JaegAW 2026-10-04.
    bpr_add_network_isolated_test(NAME application_review_cycle_security_qualification WINDOWS_UNCONTAINED
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/review_cycle_security_qualification.py"
        --application $<TARGET_FILE:test_review_cycle_two_round_public_path>
        --cycle-application $<TARGET_FILE:app_bpr>)
    set_tests_properties(application_review_cycle_security_qualification PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "REVIEW_CYCLE_SECURITY_QUALIFICATION_OK"
        TIMEOUT 600)
    # [[BPR-1183]]. The hostile-input, data-loss and mixed-cycle recovery
    # corpus through the shipped `bpr` and the reviewer's desktop: kill points
    # in capture, notes, goals, freezes, responses and saves; the owner killed
    # mid-command; a full disk; damaged and missing retained material; a
    # damaged and a lost store; stale drafts and replaced and moved roots;
    # duplicates; case collisions where a case-sensitive folder can be made.
    # Older and agent-free builds join when the course is given them.
    # Uncontained on Windows (dos-volume-name): contained, its first `bpr cycle
    # open` exits 7, "The application root is unavailable or is not
    # owner-private." Measured on JaegAW 2026-10-05.
    bpr_add_network_isolated_test(NAME application_review_cycle_recovery_corpus WINDOWS_UNCONTAINED
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/review_cycle_recovery_corpus.py"
        --application $<TARGET_FILE:test_review_cycle_two_round_public_path>
        --cycle-application $<TARGET_FILE:app_bpr>)
    set_tests_properties(application_review_cycle_recovery_corpus PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "REVIEW_CYCLE_RECOVERY_CORPUS_OK"
        TIMEOUT 900)
    bpr_add_network_isolated_test(NAME application_review_cycle_two_round_public_path WINDOWS_UNCONTAINED
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/review_cycle_two_round_contract.py"
        --application $<TARGET_FILE:test_review_cycle_two_round_public_path>
        --cycle-application $<TARGET_FILE:app_bpr>)
    set_tests_properties(application_review_cycle_two_round_public_path PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "REVIEW_CYCLE_TWO_ROUND_OK"
        TIMEOUT 600)
    # [[BPR-1171]]. One cycle through the desktop, the human terminal, a declared
    # harness and a recipient who cannot reach bpr, and back, with the reviewer
    # the two-round course drives: one golden payload in every interface; what
    # the terminal and the harness are told with the desktop open equal to what
    # they are told with it closed; the window's unsaved note and Home kept while
    # other interfaces write; a draft the terminal changed restored on the next
    # launch. Uncontained on Windows (dos-volume-name): contained, the course's
    # first `bpr cycle list` exits 7 with "The application root is unavailable
    # or is not owner-private.", as cycle_cli_public_path does. Measured on
    # JaegAW 2026-10-04.
    bpr_add_network_isolated_test(NAME application_review_cycle_parity_public_path WINDOWS_UNCONTAINED
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/review_cycle_parity_contract.py"
        --application $<TARGET_FILE:test_review_cycle_two_round_public_path>
        --cycle-application $<TARGET_FILE:app_bpr>)
    set_tests_properties(application_review_cycle_parity_public_path PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "REVIEW_CYCLE_PARITY_OK"
        TIMEOUT 600)
    # [[BPR-1184]]. Large-project responsiveness through the same reviewer and
    # the shipped `bpr cycle`: named corpora of 20,000 and 100,000 files of
    # 1,000,000 lines each, a 514-item stress round and a 250,001-entry
    # refusal, reported as p50/p95 with peak memory in the report file. The
    # 100 ms typing and navigation targets while a capture runs are enforced in
    # optimized macOS builds, whose measurements justify them; elsewhere they
    # are reported until the stations' measurements do. [[BPR-1274]]: so are the
    # stress round's, no gesture holding the window past 100 ms and a note
    # change, Copy and Save each inside the 250 ms past which progress and
    # cancel are owed; a note change in the round within seven times the
    # cycle's first is enforced in every optimized build. A Debug build runs a
    # 2,000-file correctness pass and claims no measurement. Uncontained on
    # Windows (dos-volume-name): contained, the first desktop exits 1 with
    # "FAILED: the desktop starts with a window" before any `bpr cycle` step.
    # Measured on JaegAW 2026-10-04 in the Release tree.
    # Timeout: macOS Release ran the row in 225.31 s (M4 Pro, 2026-10-04); three
    # times that, rounded up. Windows and Linux are provisional at 2,400 s
    # until the stations measure them (build-and-test.md, run budgets).
    if(APPLE)
        set(bpr_large_project_timeout 720)
    else()
        set(bpr_large_project_timeout 2400)
    endif()
    bpr_add_network_isolated_test(NAME application_review_cycle_large_project WINDOWS_UNCONTAINED
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/review_cycle_large_project_contract.py"
        --application $<TARGET_FILE:test_review_cycle_two_round_public_path>
        --cycle-application $<TARGET_FILE:app_bpr>
        --build-type $<CONFIG>
        --source-root "${PROJECT_SOURCE_DIR}"
        --report "${CMAKE_CURRENT_BINARY_DIR}/bpr1184-large-project-report.json"
        "--enforce=$<IF:$<AND:$<PLATFORM_ID:Darwin>,$<NOT:$<CONFIG:Debug>>>,1,0>"
        "--quick=$<IF:$<CONFIG:Debug>,1,0>"
        --limit-probe)
    set_tests_properties(application_review_cycle_large_project PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "REVIEW_CYCLE_LARGE_PROJECT_OK"
        RUN_SERIAL TRUE
        TIMEOUT ${bpr_large_project_timeout})
    # [[BPR-1274]]. The row's targets judged on reports it wrote, launching
    # nothing: the macOS Release report measured before the fix misses the
    # stress round's four timed targets, the one measured after it meets
    # them all, and a note change made to cost what the old build's did fails
    # the scaling check in an optimized build and is only reported in Debug.
    bpr_add_network_isolated_test(NAME review_cycle_large_project_targets
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/review_cycle_large_project_targets_tests.py")
    set_tests_properties(review_cycle_large_project_targets PROPERTIES TIMEOUT 60)
endif()

if(BPR_BUILD_QT AND TARGET Git::Git)
    # [[BPR-1177]]. Publish as Pull Request in the application's own profile
    # desktop: Open Project on a Git checkout, a private note, Capture revision,
    # the publication sheet's baselines, Review Plan, Publish Pull Request to an
    # in-memory GitHub behind the publication catalog's port, a link made by
    # hand, Open Pull Request into the existing workflow, and the cycle resumed
    # with its history. No live GitHub. Uncontained on Windows
    # (dos-volume-name): contained, it exits 3 with no output in 0.08 s, the
    # unchecked std::filesystem::canonical of its scratch root aborting.
    # Measured on JaegAW 2026-10-04.
    bpr_add_executable(test_review_cycle_publish_public_path review_cycle_publish_public_path_tests.cpp)
    target_link_libraries(test_review_cycle_publish_public_path PRIVATE BetterPullReview::Composition)
    target_compile_definitions(test_review_cycle_publish_public_path PRIVATE BPR_BUILD_TESTING=1)
    target_compile_features(test_review_cycle_publish_public_path PRIVATE cxx_std_23)
    bpr_enable_hardening(test_review_cycle_publish_public_path)
    if(WIN32)
        target_link_libraries(test_review_cycle_publish_public_path PRIVATE advapi32 BetterPullReview::Platform)
    endif()
    bpr_enable_warnings(test_review_cycle_publish_public_path)
    if(APPLE)
        file(GLOB bpr_publish_qtgui_private LIST_DIRECTORIES true
            "${Qt6Gui_DIR}/../../QtGui.framework/Headers/[0-9]*.[0-9]*.[0-9]*")
        target_include_directories(test_review_cycle_publish_public_path SYSTEM PRIVATE
            "${bpr_publish_qtgui_private}" "${bpr_publish_qtgui_private}/QtGui")
    endif()
    bpr_add_network_isolated_test(NAME application_review_cycle_publish_public_path WINDOWS_UNCONTAINED
        COMMAND test_review_cycle_publish_public_path --git "$<TARGET_FILE:Git::Git>")
    set_tests_properties(application_review_cycle_publish_public_path PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment};BPR_CYCLE_PUBLISH_RENDER_DIR=${CMAKE_CURRENT_BINARY_DIR}/cycle-publish"
        PASS_REGULAR_EXPRESSION "REVIEW_CYCLE_PUBLISH_OK"
        TIMEOUT 300)
endif()

bpr_add_executable(test_profile_window_restart_public_path profile_window_restart_public_path_tests.cpp)
target_link_libraries(test_profile_window_restart_public_path PRIVATE BetterPullReview::Composition BetterPullReview::SQLite3)
target_compile_definitions(test_profile_window_restart_public_path PRIVATE BPR_BUILD_TESTING=1)
target_compile_features(test_profile_window_restart_public_path PRIVATE cxx_std_23)
bpr_enable_hardening(test_profile_window_restart_public_path)
bpr_enable_warnings(test_profile_window_restart_public_path)
# [[BPR-0876]]. Uncontained on Windows: contained its seeding executable exits
# 3, the contained std::filesystem::canonical abort (dos-volume-name).
bpr_add_network_isolated_test(NAME application_profile_window_restart_public_path WINDOWS_UNCONTAINED
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/profile_window_restart_contract.py"
    --application $<TARGET_FILE:test_profile_window_restart_public_path>)
set_tests_properties(application_profile_window_restart_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 260)


# BPR-0899: the production port and host drain through real private SQLite storage.
bpr_add_executable(test_profile_window_layout_drain profile_window_layout_drain_tests.cpp)
target_link_libraries(test_profile_window_layout_drain PRIVATE BetterPullReview::Application BetterPullReview::AdapterSQLite)
if(WIN32)
    # [[BPR-1126]]. scratch_root.hpp makes a Windows root with the platform
    # target's owner-private descriptor.
    target_link_libraries(test_profile_window_layout_drain PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_hardening(test_profile_window_layout_drain)
bpr_enable_warnings(test_profile_window_layout_drain)
bpr_add_network_isolated_test(NAME application_profile_window_layout_drain WINDOWS_UNCONTAINED
    COMMAND test_profile_window_layout_drain)
set_tests_properties(application_profile_window_layout_drain PROPERTIES
    TIMEOUT 30 PASS_REGULAR_EXPRESSION "profile-window-layout-drain:pass")

bpr_add_executable(test_profile_window_layout_public_path profile_window_layout_public_path_tests.cpp)
target_link_libraries(test_profile_window_layout_public_path PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_profile_window_layout_public_path PRIVATE BPR_BUILD_TESTING=1)
target_compile_features(test_profile_window_layout_public_path PRIVATE cxx_std_23)
bpr_enable_hardening(test_profile_window_layout_public_path)
bpr_enable_warnings(test_profile_window_layout_public_path)
# [[BPR-0876]]. Uncontained on Windows: contained its seeding executable exits
# 3, the contained std::filesystem::canonical abort (dos-volume-name).
bpr_add_network_isolated_test(NAME application_profile_window_layout_public_path WINDOWS_UNCONTAINED
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/profile_window_layout_contract.py"
    --application $<TARGET_FILE:test_profile_window_layout_public_path>)
set_tests_properties(application_profile_window_layout_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 500)

bpr_add_executable(test_profile_window_review_restoration_public_path profile_window_review_restoration_public_path_tests.cpp)
target_link_libraries(test_profile_window_review_restoration_public_path PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_profile_window_review_restoration_public_path PRIVATE BPR_BUILD_TESTING=1)
target_compile_features(test_profile_window_review_restoration_public_path PRIVATE cxx_std_23)
bpr_enable_hardening(test_profile_window_review_restoration_public_path)
bpr_enable_warnings(test_profile_window_review_restoration_public_path)
# [[BPR-0876]]. Uncontained on Windows: contained its authorization seed exits
# 3, the contained std::filesystem::canonical abort (dos-volume-name).
bpr_add_network_isolated_test(NAME application_profile_window_review_restoration_public_path WINDOWS_UNCONTAINED
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/profile_window_review_restoration_contract.py"
    --application $<TARGET_FILE:test_profile_window_review_restoration_public_path>)
set_tests_properties(application_profile_window_review_restoration_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment};BPR_REVIEW_RESTORATION_ARTIFACT_ROOT=${CMAKE_BINARY_DIR}/bpr0710-restoration-artifacts" TIMEOUT 1800)

# [[BPR-1132]]. Same composition and native Windows limitation as the reading course.
bpr_add_network_isolated_test(NAME application_document_tabs_restoration_public_path WINDOWS_UNCONTAINED
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/document_tabs_restoration_contract.py"
    --application $<TARGET_FILE:test_profile_window_review_restoration_public_path>)
set_tests_properties(application_document_tabs_restoration_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment};BPR_REVIEW_RESTORATION_ARTIFACT_ROOT=${CMAKE_BINARY_DIR}/bpr1132-tabs-artifacts"
    TIMEOUT 600 PASS_REGULAR_EXPRESSION "DOCUMENT_TABS_RESTORATION_OK")

# One three-commit ordinary update is observed again after physical catalogue loss.
bpr_add_network_isolated_test(
    NAME application_epoch_ordinary_update_recreated
    WINDOWS_UNCONTAINED
    COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/epoch_ordinary_update_public_path.py"
        --application $<TARGET_FILE:app_better_pull_review>
        --budget-ms ${bpr_fixture_run_budget_ms}
        --logs "${PROJECT_BINARY_DIR}/ordinary-update-logs"
)
set_tests_properties(application_epoch_ordinary_update_recreated PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}"
    PASS_REGULAR_EXPRESSION "EPOCH_ORDINARY_RECREATED_OK:1"
)
bpr_bound_automated_run(application_epoch_ordinary_update_recreated
    TIMEOUT 400 BUDGET_MS ${bpr_fixture_run_budget_ms} RUNS 6 RESERVE_MS 10000
)

bpr_add_executable(test_rate_limit_public_path rate_limit_public_path_tests.cpp)
target_link_libraries(test_rate_limit_public_path PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_rate_limit_public_path PRIVATE BPR_BUILD_TESTING=1)
target_compile_features(test_rate_limit_public_path PRIVATE cxx_std_23)
bpr_enable_hardening(test_rate_limit_public_path)
bpr_enable_warnings(test_rate_limit_public_path)
bpr_add_network_isolated_test(NAME application_rate_limit_render_recovery WINDOWS_UNCONTAINED COMMAND test_rate_limit_public_path)
set_tests_properties(application_rate_limit_render_recovery PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" RUN_SERIAL TRUE TIMEOUT 120
    PASS_REGULAR_EXPRESSION "rate-limit-public-path:full-recovery-pass")

# BPR-0969 inventories present behavior without claiming full offline acceptance.
bpr_add_executable(test_offline_inventory_public_path offline_inventory_public_path_tests.cpp)
target_link_libraries(test_offline_inventory_public_path PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_offline_inventory_public_path PRIVATE BPR_BUILD_TESTING=1)
target_compile_features(test_offline_inventory_public_path PRIVATE cxx_std_23)
bpr_enable_hardening(test_offline_inventory_public_path)
bpr_enable_warnings(test_offline_inventory_public_path)
bpr_add_network_isolated_test(NAME application_offline_inventory WINDOWS_UNCONTAINED
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/offline_inventory_contract.py"
    --application $<TARGET_FILE:test_offline_inventory_public_path>)
set_tests_properties(application_offline_inventory PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" RUN_SERIAL TRUE TIMEOUT 320
    PASS_REGULAR_EXPRESSION "OFFLINE_INVENTORY_OK")

bpr_add_network_isolated_test(NAME application_recent_account_isolation WINDOWS_UNCONTAINED
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/offline_inventory_contract.py"
    --application $<TARGET_FILE:test_offline_inventory_public_path> --identity)
set_tests_properties(application_recent_account_isolation PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" RUN_SERIAL TRUE TIMEOUT 320
    PASS_REGULAR_EXPRESSION "OFFLINE_INVENTORY_OK")

if(UNIX)
    bpr_add_executable(test_terminal_diagnostics terminal_diagnostic_tests.cpp)
    target_link_libraries(test_terminal_diagnostics PRIVATE BetterPullReview::Composition)
    bpr_enable_hardening(test_terminal_diagnostics)
    bpr_enable_warnings(test_terminal_diagnostics)
    bpr_add_network_isolated_test(NAME application_terminal_diagnostics
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/terminal_diagnostic_contract.py"
            "$<TARGET_FILE:test_terminal_diagnostics>" "$<TARGET_FILE:app_better_pull_review>" "$<TARGET_FILE:test_application_logging>")
    set_tests_properties(application_terminal_diagnostics PROPERTIES TIMEOUT 60)
endif()
if(BPR_BUILD_AGENT_INTEGRATIONS) # [[BPR-1179]]
bpr_add_network_isolated_test(NAME application_diagnostic_subsystems
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/diagnostic_subsystem_contract.py"
        "$<TARGET_FILE:test_better_pull_review_application_diff_host_tests>"
        "$<TARGET_FILE:test_better_pull_review_syntax_classification_contract_tests>"
        "$<TARGET_FILE:test_better_pull_review_review_queue_enrollment_tests>"
        "$<TARGET_FILE:test_better_pull_review_agent_protocol_loop_tests>")
set_tests_properties(application_diagnostic_subsystems PROPERTIES TIMEOUT 180)
endif()

# BPR-0972 reports structural counterexamples; the marker is not blanket AC2 acceptance.
bpr_add_executable(test_application_scaling application_scaling_tests.cpp)
target_link_libraries(test_application_scaling PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_application_scaling PRIVATE BPR_BUILD_TESTING=1)
target_compile_features(test_application_scaling PRIVATE cxx_std_23)
bpr_enable_hardening(test_application_scaling)
bpr_enable_warnings(test_application_scaling)
bpr_add_network_isolated_test(NAME application_active_changed_scaling WINDOWS_UNCONTAINED COMMAND test_application_scaling $<TARGET_FILE:Git::Git>)
set_tests_properties(application_active_changed_scaling PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" RUN_SERIAL TRUE TIMEOUT 180
    PASS_REGULAR_EXPRESSION "application-scaling:observed")

# RFC-0027 diagnostics exercise the signed-out production Settings composition.
bpr_add_executable(test_diagnostic_controls_public_path diagnostic_controls_public_path_tests.cpp)
set_target_properties(test_diagnostic_controls_public_path PROPERTIES AUTOMOC ON)
target_link_libraries(test_diagnostic_controls_public_path PRIVATE BetterPullReview::Composition BetterPullReview::SQLite3)
target_compile_definitions(test_diagnostic_controls_public_path PRIVATE BPR_BUILD_TESTING=1)
bpr_enable_hardening(test_diagnostic_controls_public_path)
bpr_enable_warnings(test_diagnostic_controls_public_path)
bpr_add_network_isolated_test(NAME application_diagnostic_controls_public_path WINDOWS_UNCONTAINED COMMAND test_diagnostic_controls_public_path)

bpr_add_network_isolated_test(NAME application_review_tour_cold_import_public_path
    WINDOWS_UNCONTAINED
    COMMAND test_review_tour_composition_public_path --cold)
set_tests_properties(application_review_tour_cold_import_public_path PROPERTIES
    TIMEOUT 90 ENVIRONMENT "${bpr_public_path_fixture_environment}"
    PASS_REGULAR_EXPRESSION "review-tour-cold-import:pass")

bpr_add_executable(test_review_tour_publication_public_path review_tour_publication_public_path.cpp)
target_link_libraries(test_review_tour_publication_public_path PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_review_tour_publication_public_path PRIVATE BPR_BUILD_TESTING=1)
bpr_enable_hardening(test_review_tour_publication_public_path)
bpr_enable_warnings(test_review_tour_publication_public_path)
foreach(course native compat stop description conflict unsupported maintenance reconciliation manual)
    bpr_add_network_isolated_test(NAME application_review_tour_publication_${course} WINDOWS_UNCONTAINED COMMAND test_review_tour_publication_public_path ${course} "${CMAKE_CURRENT_BINARY_DIR}/review-tour-publication-${course}.png")
    set_tests_properties(application_review_tour_publication_${course} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
endforeach()


if(BPR_BUILD_AGENT_INTEGRATIONS) # [[BPR-1179]]
bpr_add_executable(test_scoped_writing_agent scoped_writing_agent.cpp)
target_compile_features(test_scoped_writing_agent PRIVATE cxx_std_23)
bpr_enable_hardening(test_scoped_writing_agent)
bpr_enable_warnings(test_scoped_writing_agent)
bpr_add_network_isolated_test(NAME application_review_tour_writing
    WINDOWS_UNCONTAINED
    COMMAND test_review_tour_composition_public_path --writing "$<TARGET_FILE:test_scoped_writing_agent>" "${CMAKE_CURRENT_BINARY_DIR}/review-tour-outline.png")
set_tests_properties(application_review_tour_writing PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 90 PASS_REGULAR_EXPRESSION "review-tour-writing:pass")
# [[BPR-1301]]. The shipped bare claude, then the same writing request
# through a Claude process wrapper saved on Settings > Agents & context.
bpr_add_network_isolated_test(NAME application_review_tour_writing_wrapper
    WINDOWS_UNCONTAINED
    COMMAND test_review_tour_composition_public_path --writing-wrapper "$<TARGET_FILE:test_scoped_writing_agent>")
set_tests_properties(application_review_tour_writing_wrapper PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 90 PASS_REGULAR_EXPRESSION "review-tour-writing-wrapper:pass")

# RFC-BPR-0023 / BPR-0587: three fresh processes use the actual workbench,
# private profile, existing process launcher and designated synthetic wire.
foreach(representation IN ITEMS native file)
    bpr_add_network_isolated_test(NAME application_private_review_${representation}
        WINDOWS_UNCONTAINED
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/private_review_public_path.py"
            --application $<TARGET_FILE:app_better_pull_review>
            --agent $<TARGET_FILE:test_better_pull_review_agent_finding_canary>
            --representation ${representation}
            --render "${CMAKE_CURRENT_BINARY_DIR}/private-review-${representation}.png")
    set_tests_properties(application_private_review_${representation} PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "PRIVATE_REVIEW_PUBLIC_PATH_OK ${representation}"
        RESOURCE_LOCK bpr_private_review_course)
    bpr_bound_automated_run(application_private_review_${representation}
        TIMEOUT 400 BUDGET_MS 90000 RUNS 3 RESERVE_MS 20000)
endforeach()

bpr_add_network_isolated_test(NAME application_private_review_retry
    WINDOWS_UNCONTAINED
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/private_review_public_path.py"
        --application $<TARGET_FILE:app_better_pull_review>
        --agent $<TARGET_FILE:test_better_pull_review_agent_finding_canary>
        --representation native --lose-first-result)
set_tests_properties(application_private_review_retry PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}"
    PASS_REGULAR_EXPRESSION "PRIVATE_REVIEW_PUBLIC_PATH_OK native"
    RESOURCE_LOCK bpr_private_review_course)
bpr_bound_automated_run(application_private_review_retry
    TIMEOUT 400 BUDGET_MS 90000 RUNS 3 RESERVE_MS 20000)
endif()

# [[BPR-1157]]. The real workbench window with the real local-cycle
# composition: no-account Open Project to a first note and a suggestion,
# Home's Review Cycles section, and a switch to a pull request and back.
bpr_add_executable(test_review_cycle_desktop_public_path review_cycle_desktop_public_path.cpp)
target_link_libraries(test_review_cycle_desktop_public_path PRIVATE BetterPullReview::Composition BetterPullReview::SQLite3)
target_include_directories(test_review_cycle_desktop_public_path PRIVATE "${PROJECT_SOURCE_DIR}/src/app/src")
target_compile_features(test_review_cycle_desktop_public_path PRIVATE cxx_std_23)
bpr_enable_hardening(test_review_cycle_desktop_public_path)
if(WIN32)
    target_link_libraries(test_review_cycle_desktop_public_path PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_desktop_public_path)
bpr_add_network_isolated_test(NAME application_review_cycle_desktop_public_path WINDOWS_UNCONTAINED
    COMMAND test_review_cycle_desktop_public_path)
set_tests_properties(application_review_cycle_desktop_public_path PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;BPR_CYCLE_DESKTOP_RENDER_DIR=${CMAKE_CURRENT_BINARY_DIR}/cycle-desktop"
    PASS_REGULAR_EXPRESSION "review-cycle-desktop-public-path:pass"
    TIMEOUT 120)

# [[BPR-1180]]. Send to Agent through the real window, composition and agent
# adapter, starting a stand-in harness as a real process. In a graph without
# the agent integrations the same course shows the route absent.
bpr_add_executable(test_review_cycle_send_public_path review_cycle_send_public_path.cpp)
target_link_libraries(test_review_cycle_send_public_path PRIVATE BetterPullReview::Composition)
target_include_directories(test_review_cycle_send_public_path PRIVATE "${PROJECT_SOURCE_DIR}/src/app/src")
target_compile_features(test_review_cycle_send_public_path PRIVATE cxx_std_23)
bpr_enable_hardening(test_review_cycle_send_public_path)
if(WIN32)
    target_link_libraries(test_review_cycle_send_public_path PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_send_public_path)
set(bpr_cycle_send_arguments "")
if(BPR_BUILD_AGENT_INTEGRATIONS)
    bpr_add_executable(test_better_pull_review_cycle_send_agent "${PROJECT_SOURCE_DIR}/tests/fixtures/agent/cycle_send_agent_main.cpp")
    target_compile_features(test_better_pull_review_cycle_send_agent PRIVATE cxx_std_23)
    bpr_enable_hardening(test_better_pull_review_cycle_send_agent)
    bpr_enable_warnings(test_better_pull_review_cycle_send_agent)
    add_dependencies(test_review_cycle_send_public_path test_better_pull_review_cycle_send_agent)
    set(bpr_cycle_send_arguments "$<TARGET_FILE:test_better_pull_review_cycle_send_agent>")
endif()
bpr_add_network_isolated_test(NAME application_review_cycle_send_public_path WINDOWS_UNCONTAINED
    COMMAND test_review_cycle_send_public_path ${bpr_cycle_send_arguments})
set_tests_properties(application_review_cycle_send_public_path PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    PASS_REGULAR_EXPRESSION "review-cycle-send-public-path:pass"
    TIMEOUT 120)
