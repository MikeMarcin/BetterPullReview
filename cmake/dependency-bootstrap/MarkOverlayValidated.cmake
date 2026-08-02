cmake_minimum_required(VERSION 4.4.2)

if(
    NOT DEFINED BPR_CPS_OVERLAY
    OR NOT DEFINED BPR_CPS_GENERATOR_REVISION
)
    message(FATAL_ERROR "CPS validation-seal inputs are incomplete")
endif()

set(manifest "${BPR_CPS_OVERLAY}/artifacts.json")
if(NOT EXISTS "${manifest}")
    message(FATAL_ERROR "Cannot seal a CPS overlay without its manifest")
endif()
file(SHA256 "${manifest}" manifest_sha256)
set(validation_seal "${BPR_CPS_OVERLAY}/validated.txt")
set(temporary_seal "${validation_seal}.tmp")
file(WRITE
    "${temporary_seal}"
    "${BPR_CPS_GENERATOR_REVISION}:${manifest_sha256}\n"
)
configure_file("${temporary_seal}" "${validation_seal}" COPYONLY)
file(REMOVE "${temporary_seal}")
