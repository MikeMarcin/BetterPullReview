include_guard(GLOBAL)

include("${PROJECT_SOURCE_DIR}/cmake/BprDesignatedLiveStageZero.cmake")

if(
    BPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR
    OR BPR_DESIGNATED_LIVE_MUTATION_INVENTORY
)
    message(
        FATAL_ERROR
        "Designated live plain supervisor/inventory selection is removed; use the exact image topology"
    )
endif()

set(
    BPR_MACOS_TEST_SANDBOX_PROFILE
    "${PROJECT_SOURCE_DIR}/cmake/sandbox/macos-network-deny.sb"
)
set(
    BPR_MACOS_TEST_SANDBOX_RUNNER
    "${PROJECT_SOURCE_DIR}/scripts/run_network_isolated_test.py"
)
option(
    BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST
    "Register the single pre-launch-validated designated live-mutation CTest lane"
    OFF
)
set(
    BPR_DESIGNATED_LIVE_MUTATION_APPROVAL
    ""
    CACHE FILEPATH
    "Absolute path to the one-shot designated live-mutation approval artifact"
)
set(
    BPR_DESIGNATED_LIVE_MUTATION_IMAGE
    ""
    CACHE FILEPATH
    "Absolute path to the exact designated live capsule image"
)
set(
    BPR_DESIGNATED_LIVE_MUTATION_IMAGE_SHA256
    ""
    CACHE STRING
    "Literal SHA-256 of the exact designated live capsule image"
)
set(
    BPR_DESIGNATED_LIVE_MUTATION_MOUNT_PARENT
    ""
    CACHE PATH
    "Absolute owner-private parent for the fresh designated live mount root"
)
set(
    BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR
    ""
    CACHE PATH
    "Absolute build directory that produced the designated shipped application"
)
set(BPR_DESIGNATED_LIVE_MUTATION_PROFILE "" CACHE STRING "Optional existing profile ID; approval must bind its exact replay namespace")
if(BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST)
    if(NOT APPLE OR NOT BPR_BUILD_QT)
        message(
            FATAL_ERROR
            "BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST requires macOS and BPR_BUILD_QT=ON"
        )
    endif()
    if(
        NOT BPR_DESIGNATED_LIVE_MUTATION_APPROVAL
        OR NOT IS_ABSOLUTE "${BPR_DESIGNATED_LIVE_MUTATION_APPROVAL}"
        OR NOT EXISTS "${BPR_DESIGNATED_LIVE_MUTATION_APPROVAL}"
        OR IS_DIRECTORY "${BPR_DESIGNATED_LIVE_MUTATION_APPROVAL}"
        OR IS_SYMLINK "${BPR_DESIGNATED_LIVE_MUTATION_APPROVAL}"
    )
        message(
            FATAL_ERROR
            "BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST requires an existing absolute non-symlink BPR_DESIGNATED_LIVE_MUTATION_APPROVAL file"
        )
    endif()
    if(
        NOT BPR_DESIGNATED_LIVE_MUTATION_IMAGE
        OR NOT IS_ABSOLUTE "${BPR_DESIGNATED_LIVE_MUTATION_IMAGE}"
        OR NOT EXISTS "${BPR_DESIGNATED_LIVE_MUTATION_IMAGE}"
        OR IS_DIRECTORY "${BPR_DESIGNATED_LIVE_MUTATION_IMAGE}"
        OR IS_SYMLINK "${BPR_DESIGNATED_LIVE_MUTATION_IMAGE}"
    )
        message(
            FATAL_ERROR
            "BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST requires an existing absolute non-symlink BPR_DESIGNATED_LIVE_MUTATION_IMAGE file"
        )
    endif()
    if(
        NOT BPR_DESIGNATED_LIVE_MUTATION_IMAGE_SHA256
        OR NOT BPR_DESIGNATED_LIVE_MUTATION_IMAGE_SHA256 MATCHES "^[0-9a-f]+$"
    )
        message(
            FATAL_ERROR
            "BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST requires a literal canonical BPR_DESIGNATED_LIVE_MUTATION_IMAGE_SHA256"
        )
    endif()
    string(LENGTH "${BPR_DESIGNATED_LIVE_MUTATION_IMAGE_SHA256}" bpr_live_image_digest_length)
    if(NOT bpr_live_image_digest_length EQUAL 64)
        message(
            FATAL_ERROR
            "BPR_DESIGNATED_LIVE_MUTATION_IMAGE_SHA256 must contain exactly 64 lowercase hexadecimal characters"
        )
    endif()
    if(
        NOT BPR_DESIGNATED_LIVE_MUTATION_MOUNT_PARENT
        OR NOT IS_ABSOLUTE "${BPR_DESIGNATED_LIVE_MUTATION_MOUNT_PARENT}"
        OR NOT IS_DIRECTORY "${BPR_DESIGNATED_LIVE_MUTATION_MOUNT_PARENT}"
        OR IS_SYMLINK "${BPR_DESIGNATED_LIVE_MUTATION_MOUNT_PARENT}"
    )
        message(
            FATAL_ERROR
            "BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST requires an existing absolute non-symlink BPR_DESIGNATED_LIVE_MUTATION_MOUNT_PARENT directory"
        )
    endif()
    if(
        NOT BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR
        OR NOT IS_ABSOLUTE "${BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR}"
        OR NOT EXISTS "${BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR}/CMakeCache.txt"
        OR IS_SYMLINK "${BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR}"
    )
        message(
            FATAL_ERROR
            "BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST requires an absolute BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR with CMakeCache.txt"
        )
    endif()

    file(
        REAL_PATH
        "${BPR_DESIGNATED_LIVE_MUTATION_IMAGE}"
        bpr_designated_live_image_real
    )
    file(
        REAL_PATH
        "${BPR_DESIGNATED_LIVE_MUTATION_MOUNT_PARENT}"
        bpr_designated_live_mount_parent_real
    )
    file(
        REAL_PATH
        "${BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR}"
        bpr_designated_live_build_real
    )
    file(
        REAL_PATH
        "${BPR_DESIGNATED_LIVE_MUTATION_APPROVAL}"
        bpr_designated_live_approval_real
    )
    if(
        NOT BPR_DESIGNATED_LIVE_MUTATION_IMAGE
            STREQUAL bpr_designated_live_image_real
        OR NOT BPR_DESIGNATED_LIVE_MUTATION_MOUNT_PARENT
            STREQUAL bpr_designated_live_mount_parent_real
        OR NOT BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR
            STREQUAL bpr_designated_live_build_real
        OR NOT BPR_DESIGNATED_LIVE_MUTATION_APPROVAL
            STREQUAL bpr_designated_live_approval_real
    )
        message(
            FATAL_ERROR
            "Designated live image, mount parent, producer build, and approval paths must already be resolved absolute paths"
        )
    endif()
    cmake_path(
        IS_PREFIX
        bpr_designated_live_build_real
        "${bpr_designated_live_image_real}"
        NORMALIZE
        bpr_designated_live_image_in_build
    )
    if(NOT bpr_designated_live_image_in_build)
        message(
            FATAL_ERROR
            "Designated live image must be inside its exact producer build directory"
        )
    endif()
    cmake_path(
        IS_PREFIX
        bpr_designated_live_build_real
        "${bpr_designated_live_approval_real}"
        NORMALIZE
        bpr_designated_live_approval_in_build
    )
    cmake_path(
        IS_PREFIX
        bpr_designated_live_mount_parent_real
        "${bpr_designated_live_approval_real}"
        NORMALIZE
        bpr_designated_live_approval_in_mount_parent
    )
    if(
        bpr_designated_live_approval_in_build
        OR bpr_designated_live_approval_in_mount_parent
    )
        message(
            FATAL_ERROR
            "Designated live approval must remain external to the producer and mount parent"
        )
    endif()

    file(SHA256 "${bpr_designated_live_image_real}" bpr_designated_live_image_sha256)
    if(
        NOT bpr_designated_live_image_sha256
            STREQUAL BPR_DESIGNATED_LIVE_MUTATION_IMAGE_SHA256
    )
        message(
            FATAL_ERROR
            "Designated live image digest does not match its literal expected SHA-256"
        )
    endif()

    get_filename_component(
        bpr_designated_live_image_parent
        "${bpr_designated_live_image_real}"
        DIRECTORY
    )
    get_filename_component(
        bpr_designated_live_source_identity
        "${bpr_designated_live_image_parent}"
        NAME
    )
    set(
        bpr_designated_live_source_manifest
        "${bpr_designated_live_image_parent}/source-manifest.txt"
    )
    if(
        NOT EXISTS "${bpr_designated_live_source_manifest}"
        OR IS_DIRECTORY "${bpr_designated_live_source_manifest}"
        OR IS_SYMLINK "${bpr_designated_live_source_manifest}"
    )
        message(FATAL_ERROR "Designated live producer source-manifest.txt is unavailable")
    endif()
    file(
        READ
        "${bpr_designated_live_source_manifest}"
        bpr_designated_live_source_manifest_bytes
        LIMIT 1048577
    )
    string(
        LENGTH
        "${bpr_designated_live_source_manifest_bytes}"
        bpr_designated_live_source_manifest_length
    )
    if(
        bpr_designated_live_source_manifest_length EQUAL 0
        OR bpr_designated_live_source_manifest_length GREATER 1048576
    )
        message(
            FATAL_ERROR
            "Designated live producer source manifest is outside its bounded size"
        )
    endif()
    string(
        SHA256
        bpr_designated_live_source_manifest_sha256
        "${bpr_designated_live_source_manifest_bytes}"
    )
    if(
        NOT bpr_designated_live_source_manifest_sha256
            STREQUAL bpr_designated_live_source_identity
    )
        message(
            FATAL_ERROR
            "Designated live source manifest digest does not match the producer source identity"
        )
    endif()

    math(
        EXPR
        bpr_designated_live_source_manifest_last
        "${bpr_designated_live_source_manifest_length} - 1"
    )
    string(
        SUBSTRING
        "${bpr_designated_live_source_manifest_bytes}"
        "${bpr_designated_live_source_manifest_last}"
        1
        bpr_designated_live_source_manifest_terminator
    )
    if(
        NOT bpr_designated_live_source_manifest_terminator STREQUAL "\n"
        OR bpr_designated_live_source_manifest_bytes MATCHES "\r"
        OR bpr_designated_live_source_manifest_bytes MATCHES ";"
        OR bpr_designated_live_source_manifest_bytes MATCHES "(^|\n)\n"
    )
        message(
            FATAL_ERROR
            "Designated live producer source manifest framing is not canonical ASCII/LF"
        )
    endif()
    string(
        SUBSTRING
        "${bpr_designated_live_source_manifest_bytes}"
        0
        "${bpr_designated_live_source_manifest_last}"
        bpr_designated_live_source_manifest_body
    )
    string(
        REPLACE
        "\n"
        ";"
        bpr_designated_live_source_entries
        "${bpr_designated_live_source_manifest_body}"
    )
    set(
        bpr_designated_live_required_source_entries
        src/app/CMakeLists.txt
        cmake/BprDesignatedLiveStageZero.cmake
        cmake/BprTestIsolation.cmake
        scripts/designated_live_macos_capsule.py
    )
    set(bpr_designated_live_seen_source_entries "")
    unset(bpr_designated_live_stage_zero_sha256)
    foreach(bpr_designated_live_source_entry IN LISTS bpr_designated_live_source_entries)
        if(
            NOT bpr_designated_live_source_entry
                MATCHES "^([A-Za-z0-9_./+-]+)=([0-9a-f]+)$"
        )
            message(FATAL_ERROR "Designated live producer source manifest is malformed")
        endif()
        set(bpr_designated_live_source_relative "${CMAKE_MATCH_1}")
        set(bpr_designated_live_source_sha256 "${CMAKE_MATCH_2}")
        string(
            LENGTH
            "${bpr_designated_live_source_sha256}"
            bpr_designated_live_source_sha256_length
        )
        if(
            NOT bpr_designated_live_source_sha256_length EQUAL 64
            OR bpr_designated_live_source_relative MATCHES "(^|/)\\.\\.(/|$)"
            OR bpr_designated_live_source_relative MATCHES "(^|/)[.](/|$)"
            OR bpr_designated_live_source_relative MATCHES "//|/$"
            OR IS_ABSOLUTE "${bpr_designated_live_source_relative}"
            OR bpr_designated_live_source_relative IN_LIST bpr_designated_live_seen_source_entries
        )
            message(FATAL_ERROR "Designated live producer source manifest entry is unsafe")
        endif()
        list(
            APPEND
            bpr_designated_live_seen_source_entries
            "${bpr_designated_live_source_relative}"
        )
        set(
            bpr_designated_live_current_source
            "${PROJECT_SOURCE_DIR}/${bpr_designated_live_source_relative}"
        )
        if(
            NOT EXISTS "${bpr_designated_live_current_source}"
            OR IS_DIRECTORY "${bpr_designated_live_current_source}"
            OR IS_SYMLINK "${bpr_designated_live_current_source}"
        )
            message(FATAL_ERROR "Designated live current source entry is unavailable")
        endif()
        file(
            SHA256
            "${bpr_designated_live_current_source}"
            bpr_designated_live_current_sha256
        )
        if(
            NOT bpr_designated_live_current_sha256
                STREQUAL bpr_designated_live_source_sha256
        )
            message(
                FATAL_ERROR
                "Designated live current source differs from producer entry: ${bpr_designated_live_source_relative}"
            )
        endif()
        if(bpr_designated_live_source_relative STREQUAL "scripts/designated_live_macos_capsule.py")
            set(bpr_designated_live_stage_zero_sha256 "${bpr_designated_live_source_sha256}")
        endif()
    endforeach()
    foreach(required IN LISTS bpr_designated_live_required_source_entries)
        if(NOT required IN_LIST bpr_designated_live_seen_source_entries)
            message(FATAL_ERROR "Designated live producer source manifest lacks ${required}")
        endif()
    endforeach()

    file(
        REAL_PATH
        "${PROJECT_SOURCE_DIR}"
        bpr_designated_live_source_real
    )
    file(
        STRINGS
        "${bpr_designated_live_build_real}/CMakeCache.txt"
        bpr_designated_live_producer_cache
    )
    foreach(
        bpr_designated_live_required_cache
        IN ITEMS
            "BUILD_TESTING:BOOL=OFF"
            "BPR_BUILD_QT:BOOL=ON"
            "BPR_BUILD_DESIGNATED_LIVE_MACOS_CAPSULE:BOOL=ON"
            "CMAKE_BUILD_TYPE:STRING=Release"
            "CMAKE_HOME_DIRECTORY:INTERNAL=${bpr_designated_live_source_real}"
    )
        list(FIND
            bpr_designated_live_producer_cache
            "${bpr_designated_live_required_cache}"
            bpr_designated_live_required_cache_index
        )
        if(bpr_designated_live_required_cache_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live producer cache must contain exactly ${bpr_designated_live_required_cache}"
            )
        endif()
    endforeach()

    file(
        GLOB_RECURSE
        bpr_designated_live_product_inputs
        LIST_DIRECTORIES false
        "${PROJECT_SOURCE_DIR}/src/*"
        "${PROJECT_SOURCE_DIR}/cmake/*"
    )
    list(
        APPEND
        bpr_designated_live_product_inputs
        "${PROJECT_SOURCE_DIR}/CMakeLists.txt"
        "${PROJECT_SOURCE_DIR}/CMakePresets.json"
    )
    file(
        TIMESTAMP
        "${bpr_designated_live_image_real}"
        bpr_designated_live_image_timestamp
        "%Y%m%d%H%M%S"
        UTC
    )
    foreach(bpr_designated_live_product_input IN LISTS bpr_designated_live_product_inputs)
        file(
            TIMESTAMP
            "${bpr_designated_live_product_input}"
            bpr_designated_live_product_input_timestamp
            "%Y%m%d%H%M%S"
            UTC
        )
        if(
            bpr_designated_live_product_input_timestamp
                STRGREATER bpr_designated_live_image_timestamp
        )
            message(
                FATAL_ERROR
                "Designated live image predates product source or build configuration: ${bpr_designated_live_product_input}"
            )
        endif()
    endforeach()
endif()

if(APPLE)
    if(NOT EXISTS "/usr/bin/sandbox-exec")
        message(
            FATAL_ERROR
            "Automated tests require /usr/bin/sandbox-exec on macOS; refusing an unsandboxed test graph"
        )
    endif()
    if(NOT EXISTS "${BPR_MACOS_TEST_SANDBOX_PROFILE}")
        message(
            FATAL_ERROR
            "Automated-test sandbox profile is missing: ${BPR_MACOS_TEST_SANDBOX_PROFILE}"
        )
    endif()
    if(NOT EXISTS "${BPR_MACOS_TEST_SANDBOX_RUNNER}")
        message(
            FATAL_ERROR
            "Automated-test sandbox runner is missing: ${BPR_MACOS_TEST_SANDBOX_RUNNER}"
        )
    endif()
    set(BPR_AUTOMATED_TEST_ISOLATION_BACKEND "macos-sandbox-exec")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    bpr_add_executable(
        tool_better_pull_review_network_sandbox
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/linux_network_sandbox.cpp"
    )
    target_compile_features(
        tool_better_pull_review_network_sandbox
        PRIVATE
            cxx_std_23
    )
    bpr_enable_hardening(tool_better_pull_review_network_sandbox)
    bpr_enable_warnings(tool_better_pull_review_network_sandbox)
    set(
        BPR_AUTOMATED_TEST_ISOLATION_PREFIX
        "$<TARGET_FILE:tool_better_pull_review_network_sandbox>"
    )
    set(BPR_AUTOMATED_TEST_ISOLATION_BACKEND "linux-network-namespace")
elseif(WIN32)
    include("${PROJECT_SOURCE_DIR}/cmake/WindowsTestIsolationSetup.cmake")
    set(
        bpr_windows_network_sandbox_source
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/windows_network_sandbox.cpp"
    )
    bpr_configure_windows_test_isolation(
        "${bpr_windows_network_sandbox_source}"
    )
    bpr_add_executable(
        tool_better_pull_review_network_sandbox
        "${bpr_windows_network_sandbox_source}"
    )
    target_compile_features(
        tool_better_pull_review_network_sandbox
        PRIVATE
            cxx_std_23
    )
    target_link_libraries(
        tool_better_pull_review_network_sandbox
        PRIVATE
            userenv
            advapi32
            ole32
    )
    bpr_enable_hardening(tool_better_pull_review_network_sandbox)
    bpr_enable_warnings(tool_better_pull_review_network_sandbox)
    set(
        BPR_AUTOMATED_TEST_ISOLATION_PREFIX
        "$<TARGET_FILE:tool_better_pull_review_network_sandbox>"
    )
    set(BPR_AUTOMATED_TEST_ISOLATION_BACKEND "windows-appcontainer")
else()
    message(
        FATAL_ERROR
        "No default-deny automated-test network boundary exists for ${CMAKE_SYSTEM_NAME}"
    )
endif()

# BPR-0802. The part of the seam's environment that does not depend on how the
# row is launched: the automated-test boundary marker and the pre-boundary
# scrubs, appended to one registered test in that order. Every row
# `bpr_add_network_isolated_test` registers gets it from there. The only other
# caller is a row `scripts/verify_test_network_isolation.py` names as a macOS
# exception because it cannot run under the sandbox runner, so that row carries
# the same entries without a second copy of the list to drift from this one.
function(bpr_apply_automated_test_environment bpr_test_name)
    if(NOT ARGC EQUAL 1)
        message(
            FATAL_ERROR
            "bpr_apply_automated_test_environment takes exactly one test name"
        )
    endif()
    # BPR-0797. The one thing a composed process cannot work out for itself:
    # whether it is running inside the automated test boundary. `src/app`
    # reads this to decide whether it may bind the real system browser opener
    # and the real GitHub CLI transport, or must install the recording
    # stand-ins instead. That decision used to be made at build time by
    # `BPR_BUILD_TESTING`, so the application a person actually ran carried
    # the stand-ins and reported "Opened in your browser" without opening one.
    #
    # Applied by `bpr_add_network_isolated_test`, the single registration seam
    # for every isolated test on all three platforms: a row added later
    # inherits the marker and the answer cannot drift, and no test author has
    # to remember to answer it. The asymmetry is the one the boundary itself is
    # built on - a fixture transport reached by mistake writes a line to a log,
    # and a browser opened by mistake opens a browser on whoever is running the
    # suite. A row that escaped this seam is a row allowed to do the second.
    #
    # ENVIRONMENT_MODIFICATION rather than ENVIRONMENT, and that is not a
    # style choice. `set_tests_properties(<row> PROPERTIES ENVIRONMENT ...)`
    # runs after the seam returns and replaces the property outright, so
    # an APPEND to ENVIRONMENT here is erased by each of the hundred-odd rows
    # in tests/adapters_qt and tests/end_to_end that go on to set their own
    # `QT_QPA_PLATFORM=offscreen` - among them every
    # `application_device_authorization_*` course, which are exactly the rows
    # that press "Open browser". Measured on CMake 4.4.3: with the marker in
    # ENVIRONMENT the child saw nothing at all once its row wrote an
    # ENVIRONMENT of its own; with it here the child saw the marker and the
    # row's own variable together, because CTest applies
    # ENVIRONMENT_MODIFICATION after ENVIRONMENT and nothing a row writes can
    # reach it. `scripts/verify_test_network_isolation.py` reads this exact
    # entry back out of CTest's own manifest for every configured row, so a
    # row that loses it fails the manifest gate instead of opening a browser.
    set_property(
        TEST "${bpr_test_name}"
        APPEND
        PROPERTY ENVIRONMENT_MODIFICATION "BPR_AUTOMATED_TEST_BOUNDARY=set:1"
    )

    set_property(
        TEST "${bpr_test_name}"
        APPEND
        PROPERTY ENVIRONMENT_MODIFICATION
            "PYTHONPATH=unset:"
            "PYTHONHOME=unset:"
            "PYTHONUSERBASE=unset:"
            "PYTHONINSPECT=unset:"
            "PYTHONSTARTUP=unset:"
            "DYLD_INSERT_LIBRARIES=unset:"
            "DYLD_LIBRARY_PATH=unset:"
            "DYLD_FRAMEWORK_PATH=unset:"
            "DYLD_VERSIONED_LIBRARY_PATH=unset:"
            "DYLD_VERSIONED_FRAMEWORK_PATH=unset:"
            "DYLD_FALLBACK_LIBRARY_PATH=unset:"
            "DYLD_FALLBACK_FRAMEWORK_PATH=unset:"
            "DYLD_IMAGE_SUFFIX=unset:"
            "DYLD_ROOT_PATH=unset:"
            "DYLD_SHARED_REGION=unset:"
            "LD_PRELOAD=unset:"
            "LD_LIBRARY_PATH=unset:"
            "LD_AUDIT=unset:"
            "LD_DEBUG=unset:"
            "LD_PROFILE=unset:"
    )
endfunction()

# WINDOWS_UNCONTAINED opts a test out of the Windows AppContainer, and only
# there. A lowbox token cannot translate a volume device into a drive letter,
# cannot run a batch file through cmd.exe, and cannot give ninja its named
# pipe, because the objects involved - the `\GLOBAL??\C:` link, the
# mount-point manager, the volume device, the pipe namespace - carry no
# application-package grant and no directory ACL can reach them (BPR-0362).
# A test that needs one of those runs without the launcher on Windows,
# labelled `bpr_windows_uncontained` instead of `bpr_network_isolated`, and
# `scripts/verify_test_network_isolation.py` pins the exact set with each
# member's measured cause, so the exemption can neither grow quietly nor
# outlive its reason. On macOS and Linux the keyword changes nothing: those
# boundaries leave all three intact and the test stays contained.
function(bpr_add_network_isolated_test)
    cmake_parse_arguments(
        PARSE_ARGV 0
        bpr_test
        "COMMAND_EXPAND_LISTS;UNRELATED_ENDPOINT_PROBE;WINDOWS_UNCONTAINED;EXPECTS_FAILED_RUN;EXPECTS_NONZERO_EXIT;EXPECTS_WORKSPACE_RECOVERY_REFUSAL"
        "NAME;WORKING_DIRECTORY;LOOPBACK_PORT_COUNT"
        "COMMAND;CONFIGURATIONS"
    )
    if(bpr_test_UNPARSED_ARGUMENTS)
        message(
            FATAL_ERROR
            "bpr_add_network_isolated_test received unsupported arguments: ${bpr_test_UNPARSED_ARGUMENTS}"
        )
    endif()
    if(NOT bpr_test_NAME OR NOT bpr_test_COMMAND)
        message(
            FATAL_ERROR
            "bpr_add_network_isolated_test requires NAME and COMMAND"
        )
    endif()
    if(NOT bpr_test_LOOPBACK_PORT_COUNT)
        set(bpr_test_LOOPBACK_PORT_COUNT 0)
    endif()
    if(NOT bpr_test_LOOPBACK_PORT_COUNT MATCHES "^[0-8]$")
        message(
            FATAL_ERROR
            "bpr_add_network_isolated_test LOOPBACK_PORT_COUNT must be between 0 and 8"
        )
    endif()
    if(bpr_test_EXPECTS_FAILED_RUN AND bpr_test_EXPECTS_NONZERO_EXIT)
        message(
            FATAL_ERROR
            "bpr_add_network_isolated_test: EXPECTS_FAILED_RUN already exempts the "
            "row from both failure expressions; EXPECTS_NONZERO_EXIT is the "
            "narrower one and the two cannot both be true of one row"
        )
    endif()
    if(bpr_test_UNRELATED_ENDPOINT_PROBE AND NOT APPLE)
        message(
            FATAL_ERROR
            "UNRELATED_ENDPOINT_PROBE currently has target-native evidence only on macOS"
        )
    endif()

    list(POP_FRONT bpr_test_COMMAND bpr_test_executable)
    if(TARGET "${bpr_test_executable}")
        set(bpr_test_executable "$<TARGET_FILE:${bpr_test_executable}>")
    endif()

    set(bpr_registration_options "")
    if(bpr_test_CONFIGURATIONS)
        list(
            APPEND
            bpr_registration_options
            CONFIGURATIONS
            ${bpr_test_CONFIGURATIONS}
        )
    endif()
    if(bpr_test_WORKING_DIRECTORY)
        list(
            APPEND
            bpr_registration_options
            WORKING_DIRECTORY
            "${bpr_test_WORKING_DIRECTORY}"
        )
    endif()
    if(bpr_test_COMMAND_EXPAND_LISTS)
        list(APPEND bpr_registration_options COMMAND_EXPAND_LISTS)
    endif()

    if(APPLE)
        set(
            bpr_isolation_prefix
            "$<TARGET_FILE:Python3::Interpreter>"
            -I
            -S
            "${BPR_MACOS_TEST_SANDBOX_RUNNER}"
            --sandbox-exec
            /usr/bin/sandbox-exec
            --profile-template
            "${BPR_MACOS_TEST_SANDBOX_PROFILE}"
            --loopback-port-count
            "${bpr_test_LOOPBACK_PORT_COUNT}"
        )
        if(bpr_test_UNRELATED_ENDPOINT_PROBE)
            list(APPEND bpr_isolation_prefix --unrelated-endpoint-probe)
        endif()
        list(APPEND bpr_isolation_prefix --)
    else()
        set(bpr_isolation_prefix ${BPR_AUTOMATED_TEST_ISOLATION_PREFIX})
    endif()
    set(bpr_isolation_label "bpr_network_isolated")
    if(
        bpr_test_WINDOWS_UNCONTAINED
        AND BPR_AUTOMATED_TEST_ISOLATION_BACKEND STREQUAL "windows-appcontainer"
    )
        set(bpr_isolation_prefix "")
        set(bpr_isolation_label "bpr_windows_uncontained")
    endif()

    add_test(
        NAME "${bpr_test_NAME}"
        COMMAND
            ${bpr_isolation_prefix}
            "${bpr_test_executable}"
            ${bpr_test_COMMAND}
        ${bpr_registration_options}
    )
    set_property(
        TEST "${bpr_test_NAME}"
        APPEND
        PROPERTY LABELS "${bpr_isolation_label}"
    )

    bpr_apply_automated_test_environment("${bpr_test_NAME}")

    # BPR-0552. Every row registered here fails on the run's own verdict,
    # whatever else it pins.
    #
    # `report_verdict` in `src/app/main.cpp` prints `PUBLIC_PATH_OK` or
    # `PUBLIC_PATH_FAILED` unconditionally, from the same `passed` conjunction
    # that decides the process exit code. `PUBLIC_PATH_FAILED` is therefore the
    # negation of every field `PUBLIC_PATH_VERDICT` reports - presentation,
    # shutdown, cleanup, epoch projection delivery and epoch handoff delivery.
    # That verdict block itself prints only inside `if (!passed)`, so no row
    # could pin it even in principle, and only ten of the fifty rows carrying a
    # `PASS_REGULAR_EXPRESSION` pinned `PUBLIC_PATH_OK`. For the other forty a
    # regression in any of those five fields left the row green.
    #
    # Of the three ways to close that, this is the only one CTest can express
    # once, for every row, in one place:
    #
    # - Adding `PUBLIC_PATH_OK` to a row's pass list does nothing. CTest passes
    #   a test when *any* required expression matches, so the row's own weaker
    #   expression still carries it.
    # - Failing on the child's exit code is not reachable while a pass
    #   expression exists. `cmCTestRunTest` computes success as
    #   `!forceFail && (retVal == 0 || !RequiredRegularExpressions.empty())`, so
    #   a non-zero exit is ignored outright on any row that has a
    #   `PASS_REGULAR_EXPRESSION`; measured on CMake 4.4.3, a test that prints
    #   its marker and exits 1 passes. Reaching the exit code would mean
    #   deleting the marker pinning from forty rows, which is the evidence they
    #   exist for. `scripts/run_network_isolated_test.py` already returns the
    #   child's code unchanged; nothing downstream was looking at it.
    # - An error expression is evaluated independently of the pass list and
    #   forces the failure regardless of it, which is the behaviour needed.
    #
    # Set here rather than row by row because this function is the single
    # registration seam for every isolated test on all three platforms, so a
    # row added later inherits it and the guarantee cannot drift. A row whose
    # driver script captures the child's output - the `cmake -P` contracts -
    # never shows the marker to CTest and is decided instead by that script's
    # own check of the child's result code, which those scripts already make.
    # BPR-0890. The same argument reaches one field further. `PUBLIC_PATH_OK` is
    # printed by `report_verdict` *before* `QApplication::exec()` returns and
    # before the desktop, the QApplication and the static destructors run, so
    # an abort anywhere in that shutdown tail left the marker already on stdout
    # and the row green - the tail BPR-0215, BPR-0217, BPR-0827 and BPR-0875's
    # thirteen-row Windows cluster all live in. The verdict text cannot answer
    # that; only the process's own exit status can, and an error expression is
    # the one verdict CTest evaluates independently of the pass list.
    #
    # So the launcher states the exit status on the row's own output and this
    # matches it: `scripts/run_network_isolated_test.py` prints
    # `TEST_RUNNER_CHILD_EXIT:<n>` for every run as the evidence, and
    # `TEST_RUNNER_CHILD_EXIT_FAILED:<n>` only when that code is non-zero,
    # which is what the expression below catches. A literal rather than a
    # negated digit class because CTest's regular expressions have no
    # lookahead. The Linux `execvp` launcher replaces itself with the child and
    # so has no exit status to state, and the Windows launcher does not print
    # it yet; on those two the entry is inert rather than wrong, and pinning it
    # here means each platform's launcher is the only thing left to change.
    #
    # APPEND, on both entries. `set_property(... PROPERTY ...)` without it
    # replaces the whole property, so the second entry written that way would
    # have silently deleted the first. The remaining way to lose them - a
    # row-level `set_tests_properties(... FAIL_REGULAR_EXPRESSION ...)` running
    # after this function returns - APPEND cannot defend against from in here,
    # because the row is registered in this call and nothing precedes it. That
    # is asserted instead, against CTest's own manifest, by
    # `scripts/verify_test_network_isolation.py`, so the comment above about a
    # guarantee that cannot drift is now enforced rather than claimed.
    set(bpr_failure_expressions "")
    if(bpr_test_EXPECTS_FAILED_RUN)
        # One row asks what a *failed* run reports, so the marker it must not
        # be failed by is the marker it exists to observe, and the non-zero
        # exit that run ends with is the verdict it is asserting rather than a
        # fault. Stating that here keeps the default fail-closed: an exemption
        # is written down at the row, not inferred from the absence of a
        # property.
        set_property(
            TEST "${bpr_test_NAME}"
            APPEND
            PROPERTY LABELS "bpr_expects_failed_run"
        )
    elseif(bpr_test_EXPECTS_NONZERO_EXIT)
        # A narrower exemption, and a different fact. This row's child exits
        # non-zero because that is the answer it is asserting - the RFC-0017
        # negative control runs the shipped-artifact scan against the
        # tests-enabled binary, where the scan must fail and name the test-seam
        # factory, so the row pins that text and the scanner exits 1. Nothing
        # about that says the run's own verdict may be ignored, so
        # `PUBLIC_PATH_FAILED` stays. Measured under this card: before the
        # child-exit entry existed the row was green with a child exit of 1,
        # which is the same green a real shutdown abort would have produced.
        set_property(
            TEST "${bpr_test_NAME}"
            APPEND
            PROPERTY LABELS "bpr_expects_nonzero_exit"
        )
        list(APPEND bpr_failure_expressions "PUBLIC_PATH_FAILED")
    else()
        list(
            APPEND
            bpr_failure_expressions
            "PUBLIC_PATH_FAILED"
            "TEST_RUNNER_CHILD_EXIT_FAILED"
        )
    endif()
    # BPR-1050. A course that roots its application data directly in a bare
    # temporary directory puts the agent workspace neighbourhood's parent in the
    # system temporary directory. On Linux that is a shared, root-owned `1777`
    # `/tmp`, and on Windows `%TEMP%` carries inherited entries, so startup
    # recovery refuses it and prints this line once. The course still passed:
    # before BPR-1044, twelve rows printed it on every run. The product's own
    # line is the evidence, so it fails every row here, whatever the course is
    # written in, whenever the line reaches CTest. A row whose subject is the
    # refusal says so with EXPECTS_WORKSPACE_RECOVERY_REFUSAL, which is written
    # down at the row and pinned in `scripts/verify_test_network_isolation.py`.
    #
    # What this cannot see: a driver that captures its child's standard error
    # and does not relay it, and any macOS run under CTest, where the runner's
    # `TMPDIR` is a private directory that recovery accepts. There a bare root
    # is not refused, so there is nothing to fail on.
    if(bpr_test_EXPECTS_WORKSPACE_RECOVERY_REFUSAL)
        set_property(
            TEST "${bpr_test_NAME}"
            APPEND
            PROPERTY LABELS "bpr_expects_workspace_recovery_refusal"
        )
    else()
        list(APPEND bpr_failure_expressions "AGENT_WORKSPACE_RECOVERY_REFUSED")
    endif()
    if(bpr_failure_expressions)
        set_property(
            TEST "${bpr_test_NAME}"
            APPEND
            PROPERTY FAIL_REGULAR_EXPRESSION ${bpr_failure_expressions}
        )
    endif()
    # A Windows loader finds a shared Qt only on PATH: nothing deploys the
    # DLLs beside the executables, and unlike a Mach-O or ELF link the import
    # libraries carry no runtime path. The provider resolves Qt outside the
    # build tree, so the directory of Qt6::Core's DLL is prepended for every
    # test, contained or not; the launcher hands its environment to the child
    # unchanged, and the isolation setup already grants that prefix to the
    # container. Without this every Qt-linked test exits before main with
    # STATUS_DLL_NOT_FOUND (BPR-0430).
    if(WIN32 AND TARGET Qt6::Core)
        # Qt ships no fonts, and the offscreen platform every contract runs
        # under has no fontconfig here: without a font directory QFontDatabase
        # writes "Cannot find font directory …/lib/fonts" to stderr of every
        # Qt GUI process, which the contracts that read stderr take for a
        # failure of their own. The Windows font directory is readable from
        # inside the container as well (BPR-0412).
        file(TO_CMAKE_PATH "$ENV{WINDIR}" bpr_windows_directory)
        set_property(
            TEST "${bpr_test_NAME}"
            APPEND
            PROPERTY ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
                "QT_QPA_FONTDIR=set:${bpr_windows_directory}/Fonts"
        )
    endif()
    if(bpr_test_LOOPBACK_PORT_COUNT GREATER 0)
        set_property(
            TEST "${bpr_test_NAME}"
            APPEND
            PROPERTY LABELS "bpr_loopback_fixture"
        )
    endif()
    if(bpr_test_UNRELATED_ENDPOINT_PROBE)
        set_property(
            TEST "${bpr_test_NAME}"
            APPEND
            PROPERTY LABELS "bpr_unrelated_endpoint_probe"
        )
    endif()
endfunction()

function(bpr_add_designated_live_mutation_test)
    if(ARGC GREATER 0)
        message(
            FATAL_ERROR
            "bpr_add_designated_live_mutation_test accepts no caller-selected name, target, or command"
        )
    endif()
    if(NOT BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST)
        return()
    endif()
    bpr_designated_live_stage_zero_command(
        bpr_designated_live_attended_command
        ATTENDED
        SCRIPT "${PROJECT_SOURCE_DIR}/scripts/designated_live_macos_capsule.py"
        SCRIPT_SHA256 "${bpr_designated_live_stage_zero_sha256}"
        IMAGE "${BPR_DESIGNATED_LIVE_MUTATION_IMAGE}"
        EXPECTED_IMAGE_SHA256 "${BPR_DESIGNATED_LIVE_MUTATION_IMAGE_SHA256}"
        MOUNT_PARENT "${BPR_DESIGNATED_LIVE_MUTATION_MOUNT_PARENT}"
        SOURCE_SHA256 "${bpr_designated_live_source_identity}"
        APPROVAL "${BPR_DESIGNATED_LIVE_MUTATION_APPROVAL}"
        PROFILE "${BPR_DESIGNATED_LIVE_MUTATION_PROFILE}"
    )
    add_test(
        NAME live_review_mutation_designated_target
        COMMAND ${bpr_designated_live_attended_command}
    )
    set_property(
        TEST live_review_mutation_designated_target
        PROPERTY LABELS "bpr_live_mutation"
    )
    set_property(
        TEST live_review_mutation_designated_target
        PROPERTY TIMEOUT 1800
    )
    set_property(
        TEST live_review_mutation_designated_target
        APPEND
        PROPERTY ENVIRONMENT_MODIFICATION
            "PYTHONPATH=unset:"
            "PYTHONHOME=unset:"
            "PYTHONUSERBASE=unset:"
            "PYTHONINSPECT=unset:"
            "PYTHONSTARTUP=unset:"
            "DYLD_INSERT_LIBRARIES=unset:"
            "DYLD_LIBRARY_PATH=unset:"
            "DYLD_FRAMEWORK_PATH=unset:"
            "DYLD_VERSIONED_LIBRARY_PATH=unset:"
            "DYLD_VERSIONED_FRAMEWORK_PATH=unset:"
            "DYLD_FALLBACK_LIBRARY_PATH=unset:"
            "DYLD_FALLBACK_FRAMEWORK_PATH=unset:"
            "DYLD_IMAGE_SUFFIX=unset:"
            "DYLD_ROOT_PATH=unset:"
            "DYLD_SHARED_REGION=unset:"
            "LD_PRELOAD=unset:"
            "LD_LIBRARY_PATH=unset:"
            "LD_AUDIT=unset:"
            "LD_DEBUG=unset:"
            "LD_PROFILE=unset:"
            "GH_TOKEN=unset:"
            "GITHUB_TOKEN=unset:"
            "GH_ENTERPRISE_TOKEN=unset:"
            "GITHUB_ENTERPRISE_TOKEN=unset:"
    )
endfunction()
