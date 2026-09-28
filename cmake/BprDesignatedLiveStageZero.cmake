include_guard(GLOBAL)

# The generated CTest manifest and the tests-disabled producer graph are the
# trusted execution plans for stage zero. They execute only bytes captured from
# the source launcher after this preloader has matched them to the producer's
# literal digest. A later same-inode write cannot change the captured program.
set(BPR_DESIGNATED_LIVE_STAGE_ZERO_PRELOADER_SOURCE [=[
import hashlib
import os
import stat
import sys

path = sys.argv[1]
expected = sys.argv[2]
descriptor = os.open(path, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW)
try:
    before = os.fstat(descriptor)
    if not stat.S_ISREG(before.st_mode) or before.st_size <= 0 or before.st_size > 1048576:
        raise SystemExit("designated-live-stage-zero: launcher is not a bounded regular file")
    source = bytearray()
    while len(source) <= 1048576:
        block = os.read(descriptor, min(65536, 1048577 - len(source)))
        if not block:
            break
        source.extend(block)
    after = os.fstat(descriptor)
finally:
    os.close(descriptor)

identity = lambda value: (
    value.st_dev,
    value.st_ino,
    value.st_mode,
    value.st_uid,
    value.st_gid,
    value.st_size,
    value.st_mtime_ns,
    value.st_ctime_ns,
)
captured = bytes(source)
if identity(before) != identity(after) or len(captured) != before.st_size:
    raise SystemExit("designated-live-stage-zero: launcher changed while captured")
if hashlib.sha256(captured).hexdigest() != expected:
    raise SystemExit("designated-live-stage-zero: launcher digest mismatch")
sys.argv = [path, *sys.argv[3:]]
scope = {"__name__": "__main__", "__file__": path}
exec(compile(captured, path, "exec"), scope, scope)
]=])
set(
    bpr_designated_live_stage_zero_preloader_encoded
    "${BPR_DESIGNATED_LIVE_STAGE_ZERO_PRELOADER_SOURCE}"
)
string(
    REPLACE
    "\\"
    "\\\\"
    bpr_designated_live_stage_zero_preloader_encoded
    "${bpr_designated_live_stage_zero_preloader_encoded}"
)
string(
    REPLACE
    "'"
    "\\'"
    bpr_designated_live_stage_zero_preloader_encoded
    "${bpr_designated_live_stage_zero_preloader_encoded}"
)
string(
    REPLACE
    "\n"
    "\\n"
    bpr_designated_live_stage_zero_preloader_encoded
    "${bpr_designated_live_stage_zero_preloader_encoded}"
)
set(
    BPR_DESIGNATED_LIVE_STAGE_ZERO_PRELOADER
    "exec('${bpr_designated_live_stage_zero_preloader_encoded}')"
)
unset(bpr_designated_live_stage_zero_preloader_encoded)

function(bpr_designated_live_stage_zero_command output)
    cmake_parse_arguments(
        PARSE_ARGV 1
        stage_zero
        "PROBE;ATTENDED"
        "SCRIPT;SCRIPT_SHA256;IMAGE;EXPECTED_IMAGE_SHA256;EXPECTED_IMAGE_SHA256_FILE;MOUNT_PARENT;SOURCE_SHA256;APPROVAL;PROFILE"
        ""
    )
    if(stage_zero_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unexpected designated live stage-zero arguments")
    endif()
    if(
        (stage_zero_PROBE AND stage_zero_ATTENDED)
        OR (NOT stage_zero_PROBE AND NOT stage_zero_ATTENDED)
    )
        message(FATAL_ERROR "Designated live stage zero requires exactly one fixed mode")
    endif()
    foreach(required IN ITEMS SCRIPT SCRIPT_SHA256 IMAGE MOUNT_PARENT SOURCE_SHA256)
        if(NOT stage_zero_${required})
            message(FATAL_ERROR "Designated live stage zero requires ${required}")
        endif()
    endforeach()
    string(LENGTH "${stage_zero_SCRIPT_SHA256}" stage_zero_script_sha256_length)
    if(
        NOT stage_zero_SCRIPT_SHA256 MATCHES "^[0-9a-f]+$"
        OR NOT stage_zero_script_sha256_length EQUAL 64
    )
        message(FATAL_ERROR "Designated live stage zero requires a canonical script SHA-256")
    endif()
    if(
        (stage_zero_EXPECTED_IMAGE_SHA256 AND stage_zero_EXPECTED_IMAGE_SHA256_FILE)
        OR (NOT stage_zero_EXPECTED_IMAGE_SHA256 AND NOT stage_zero_EXPECTED_IMAGE_SHA256_FILE)
    )
        message(FATAL_ERROR "Designated live stage zero requires one image digest source")
    endif()
    if(stage_zero_ATTENDED AND NOT stage_zero_APPROVAL)
        message(FATAL_ERROR "Attended designated live stage zero requires approval")
    endif()
    string(LENGTH "${stage_zero_PROFILE}" stage_zero_profile_length)
    if(NOT "${stage_zero_PROFILE}" STREQUAL "" AND (NOT stage_zero_ATTENDED OR NOT stage_zero_PROFILE MATCHES "^[1-9][0-9]*$" OR stage_zero_profile_length GREATER 20 OR (stage_zero_profile_length EQUAL 20 AND stage_zero_PROFILE STRGREATER "18446744073709551615")))
        message(FATAL_ERROR "Profile selection requires attended mode and a positive canonical identifier")
    endif()
    if(stage_zero_PROBE AND stage_zero_APPROVAL)
        message(FATAL_ERROR "Probe designated live stage zero cannot receive approval")
    endif()
    if(stage_zero_ATTENDED AND stage_zero_EXPECTED_IMAGE_SHA256_FILE)
        message(FATAL_ERROR "Attended designated live stage zero requires a literal image SHA-256")
    endif()

    set(command
        "/usr/bin/python3"
        -I
        -S
        -c
        "${BPR_DESIGNATED_LIVE_STAGE_ZERO_PRELOADER}"
        "${stage_zero_SCRIPT}"
        "${stage_zero_SCRIPT_SHA256}"
        stage-zero
        --image
        "${stage_zero_IMAGE}"
    )
    if(stage_zero_EXPECTED_IMAGE_SHA256)
        list(APPEND command --expected-image-sha256 "${stage_zero_EXPECTED_IMAGE_SHA256}")
    else()
        list(APPEND command --expected-image-sha256-file "${stage_zero_EXPECTED_IMAGE_SHA256_FILE}")
    endif()
    list(
        APPEND
        command
        --mount-parent
        "${stage_zero_MOUNT_PARENT}"
        --source-sha256
        "${stage_zero_SOURCE_SHA256}"
    )
    if(stage_zero_PROBE)
        list(APPEND command --probe-launch)
    else()
        list(APPEND command --attended-launch --approval "${stage_zero_APPROVAL}")
        if(NOT "${stage_zero_PROFILE}" STREQUAL "")
            list(APPEND command --bpr-profile "${stage_zero_PROFILE}")
        endif()
    endif()
    set("${output}" "${command}" PARENT_SCOPE)
endfunction()
