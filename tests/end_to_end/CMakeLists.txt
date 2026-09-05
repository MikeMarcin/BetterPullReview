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
            "PUBLIC_PATH_OK[\r\n]+NATIVE_PLATFORM_VAULT_SELECTED:${bpr_native_platform_vault_self_test_result}[\r\n]+CLI_UNAVAILABLE_OK[\r\n]+CLI_UNAVAILABLE_COUNT:1"
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
            "PUBLIC_PATH_OK[\r\n]+NATIVE_PLATFORM_VAULT_SELECTED:${bpr_native_platform_vault_self_test_result}[\r\n]+CLI_UNAVAILABLE_OK[\r\n]+CLI_UNAVAILABLE_COUNT:2"
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
            "PUBLIC_PATH_OK[\r\n]+NATIVE_PLATFORM_VAULT_SELECTED:${bpr_native_platform_vault_self_test_result}[\r\n]+CLI_UNAVAILABLE_OK[\r\n]+CLI_UNAVAILABLE_COUNT:1[\r\n]+DEVICE_FLOW_HOST_COMPOSED:true[\r\n]+DEVICE_FLOW_PRESENTED:true"
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
            "NATIVE_PLATFORM_VAULT_SELECTED:${bpr_native_platform_vault_self_test_result}[\r\n]+CLI_UNAVAILABLE_OK[\r\n]+CLI_UNAVAILABLE_COUNT:1[\r\n]+DEVICE_FLOW_HOST_COMPOSED:true[\r\n]+DEVICE_FLOW_PRESENTED:true[\r\n]+${bpr_device_selection_denied_self_test_expression}"
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
            "PUBLIC_PATH_OK[\r\n]+NATIVE_PLATFORM_VAULT_SELECTED:${bpr_native_platform_vault_self_test_result}[\r\n]+CLI_UNAVAILABLE_OK[\r\n]+CLI_UNAVAILABLE_COUNT:2[\r\n]+${bpr_public_preview_enterprise_self_test_expression}"
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
            "NATIVE_PLATFORM_VAULT_SELECTED:${bpr_native_platform_vault_self_test_result}[\r\n]+CLI_UNAVAILABLE_OK[\r\n]+CLI_UNAVAILABLE_COUNT:1[\r\n]+${bpr_public_preview_selection_self_test_expression}"
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
            "NATIVE_PLATFORM_VAULT_SELECTED:${bpr_native_platform_vault_self_test_result}[\r\n]+CLI_UNAVAILABLE_OK[\r\n]+CLI_UNAVAILABLE_COUNT:1[\r\n]+PUBLIC_PREVIEW_COMPOSED:true[\r\n]+PUBLIC_PREVIEW_STAGE:enabled[\r\n]+PUBLIC_PREVIEW_REQUESTS:0[\r\n]+PUBLIC_PREVIEW_HEADER_EFFECTS:0[\r\n]+PUBLIC_PREVIEW_VAULT_EFFECTS:0[\r\n]+PUBLIC_PREVIEW_SIGN_IN_REQUESTS:0[\r\n]+PUBLIC_PREVIEW_ENABLED_OK"
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

set(bpr_public_path_fixture_environment "QT_QPA_PLATFORM=offscreen")
if(UNIX AND NOT APPLE)
    list(APPEND
        bpr_public_path_fixture_environment
        "QT_STYLE_OVERRIDE=Windows"
    )
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

if(UNIX)
    bpr_add_network_isolated_test(
        NAME application_agent_crash_recovery
        COMMAND Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/agent_composition_kill_recovery_tests.py"
            --executable "$<TARGET_FILE:test_better_pull_review_agent_invocation_composition_public_path>"
            --canary "$<TARGET_FILE:test_better_pull_review_agent_canary>"
            --git "$<TARGET_FILE:Git::Git>"
    )
    set_tests_properties(application_agent_crash_recovery PROPERTIES
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
bpr_bound_automated_run(
    application_queue_composition_contract
    TIMEOUT 75
    BUDGET_MS 25000
    RUNS 2
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
bpr_bound_automated_run(
    application_native_notification_attended_contract
    TIMEOUT 120
    BUDGET_MS 25000
    RUNS 3
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
# fixture runs use. Nothing is shown and no fixture is selected: the subject is
# the destructor, not a course.
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
            "PUBLIC_PATH_OK[\r\n]+GITHUB_FIXTURE_OK:github.com[\r\n]+DIFF_WORKBENCH_OK[\r\n]+DIFF_INTERACTION_EVIDENCE_OK[\r\n]+CONVERSATION_ANCHOR_EVIDENCE_OK[\r\n]+SYNTAX_THEME_REPAINT_OK[\r\n]+RENDER_OK[\r\n]+REVIEW_LEDGER_PRESENT_ON_ENTRY:0[\r\n]+REVIEW_LEDGER_OK[\r\n]+WORKBENCH_HOST_COMPOSED_OK[\r\n]+APPEARANCE_HOST_COMPOSED_OK[\r\n]+SYNTAX_CLASSIFIER_COMPOSED_OK[\r\n]+SYNTAX_CLASSIFICATION:[1-9][0-9]*:[1-9][0-9]*[\r\n]+SYNTAX_DELIVERY:[0-9]+:[0-9]+:[0-9]+:[0-9]+:[0-9]+[\r\n]+SYNTAX_DISPLAYED:[01]:[01][\r\n]+SYNTAX_DISPLAYED_SPANS:[0-9]+:[0-9]+:[0-9]+:[01][\r\n]+APPEARANCE_SETTINGS:[a-z_]+:[0-9]+[\r\n]+NOTIFICATION_DELIVERY_COMPOSED_OK[\r\n]+NOTIFICATION_PERMISSION:[a-z-]+[\r\n]+NOTIFICATION_BUNDLE_IDENTITY:[A-Za-z0-9.-]+[\r\n]+APPEARANCE_DIVERGENCE:0[\r\n]+APPEARANCE_OBSERVED:[a-z]+:[a-z-]+:[0-9]+[\r\n]+APPEARANCE_APPLIED:#[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]:#[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]:from-profile"
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
bpr_bound_automated_run(
    application_workbench_composition_contract
    TIMEOUT 60
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

bpr_add_network_isolated_test(
    NAME application_review_ledger_composition_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/review-ledger-composition-contract"
        "-DBPR_RUN_BUDGET_MS=${bpr_fixture_run_budget_ms}"
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
    TIMEOUT 180
    BUDGET_MS ${bpr_fixture_run_budget_ms}
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
        "-DBPR_ISOLATION_SCRIPT=${PROJECT_SOURCE_DIR}/scripts/verify_live_mutation_isolation.py"
        "-DBPR_MANIFEST=${CMAKE_BINARY_DIR}/_bpr/$<CONFIG>/automated-executables.txt"
        "-DBPR_OBJECT_MANIFEST=${CMAKE_BINARY_DIR}/_bpr/$<CONFIG>/automated-executable-objects.txt"
        "-DBPR_PYTHON=$<TARGET_FILE:Python3::Interpreter>"
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
        "-DBPR_RUN_BUDGET_MS=${bpr_fixture_run_budget_ms}"
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
bpr_bound_automated_run(
    application_defect_composition_contract
    TIMEOUT 120
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 2
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
        $<TARGET_FILE:app_better_pull_review>
        --github-fixture
        github-com
        --automated-stall-probe
        --automated-run-budget-ms
        ${bpr_stall_budget_ms}
)
set_tests_properties(
    application_automated_run_budget
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        # `PUBLIC_PATH_STAGE:2` is `open`, the third stage. It is asserted by
        # number because a number is what the evidence prints; if a stage is
        # ever inserted ahead of `open` this assertion is meant to be revisited
        # rather than to keep passing against a different stage.
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_STAGE:2[\r\n]+PUBLIC_PATH_VERDICT:[^\r\n]+[\r\n]+PUBLIC_PATH_GROUPS:[^\r\n]+[\r\n]+PUBLIC_PATH_BUDGET:expired=1 budget_ms=${bpr_stall_budget_ms} elapsed_ms=[0-9]+ stage_ticks=[0-9]+"
)
bpr_bound_automated_run(
    application_automated_run_budget
    TIMEOUT 30
    BUDGET_MS ${bpr_stall_budget_ms}
    RUNS 1
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
# replays a second script here -- the nine credential-free operations in the
# order the anonymous authority issues them -- so what this row proves is not
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
        ${bpr_fixture_run_budget_ms}
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
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PREVIEW_COMPOSED:true[\r\n]+PUBLIC_PREVIEW_STAGE:enabled[\r\n]+PUBLIC_PREVIEW_REQUESTS:1[\r\n]+PUBLIC_PREVIEW_HEADER_EFFECTS:0[\r\n]+PUBLIC_PREVIEW_VAULT_EFFECTS:0[\r\n]+PUBLIC_PREVIEW_SIGN_IN_REQUESTS:0[\r\n]+PUBLIC_PREVIEW_REVIEW_COMMENT_GROUPS:1[\r\n]+PUBLIC_PREVIEW_WRITE_REFUSALS:2[\r\n]+PUBLIC_PREVIEW_ENABLED_OK"
)
bpr_bound_automated_run(
    application_anonymous_public_preview_public_path
    TIMEOUT 60
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 1
    RESERVE_MS 2000
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
        $<TARGET_FILE:app_better_pull_review>
)
set_tests_properties(
    runtime_process_surface
    PROPERTIES
        PASS_REGULAR_EXPRESSION "RUNTIME_SURFACE_OK"
        TIMEOUT 15
)

bpr_add_network_isolated_test(
    NAME composed_application_hosts
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_composed_hosts.py"
        --profile
        development-fixture
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

if(UNIX)
bpr_add_executable(test_application_logging application_logging_tests.cpp)
target_link_libraries(test_application_logging PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_application_logging PRIVATE BPR_BUILD_TESTING=1)
bpr_enable_hardening(test_application_logging)
bpr_enable_warnings(test_application_logging)
bpr_add_network_isolated_test(NAME application_logging WINDOWS_UNCONTAINED COMMAND test_application_logging)
set_tests_properties(application_logging PROPERTIES ENVIRONMENT "${bpr_public_path_fixture_environment}" TIMEOUT 30)
endif()

# Real normal controls, typed fake server, real SQLite; no live effects.
bpr_add_executable(test_better_pull_review_review_submission_composition_public_path review_submission_composition_public_path.cpp)
target_link_libraries(test_better_pull_review_review_submission_composition_public_path PRIVATE BetterPullReview::Composition)
target_compile_definitions(test_better_pull_review_review_submission_composition_public_path PRIVATE BPR_BUILD_TESTING=1)
bpr_enable_hardening(test_better_pull_review_review_submission_composition_public_path)
bpr_enable_warnings(test_better_pull_review_review_submission_composition_public_path)
bpr_add_network_isolated_test(NAME application_review_submission_composition_public_path
    WINDOWS_UNCONTAINED
    COMMAND test_better_pull_review_review_submission_composition_public_path "${CMAKE_CURRENT_BINARY_DIR}/review-submission-composition.png")
set_tests_properties(application_review_submission_composition_public_path PROPERTIES
    ENVIRONMENT "${bpr_public_path_fixture_environment}"
    TIMEOUT 90 PASS_REGULAR_EXPRESSION "review-submission-composition:pass")
