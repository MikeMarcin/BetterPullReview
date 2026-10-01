# [[BPR-0900]]. Runs the real `bpr_narrow_owner_private` on one directory, so
# owner_private_descriptor_agreement_tests.py judges the descriptor the CMake
# contracts actually write, not a copy of the command.
if(NOT BPR_NARROW_HELPER OR NOT BPR_NARROW_DIRECTORY)
    message(FATAL_ERROR "BPR_NARROW_HELPER and BPR_NARROW_DIRECTORY are required")
endif()
include("${BPR_NARROW_HELPER}")
bpr_narrow_owner_private("${BPR_NARROW_DIRECTORY}")
