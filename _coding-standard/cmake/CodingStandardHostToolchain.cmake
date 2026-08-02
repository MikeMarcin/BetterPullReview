# Normalize and create the output directories supplied by the optional
# standalone output preset. Compiler and SDK selection remains with the caller.
foreach(_bpr_output_variable IN ITEMS
    BPR_OUTPUT_ROOT
    BPR_PROJECT_OUTPUT_ROOT
    BPR_PACKAGE_ROOT
    BPR_PACKAGE_SMOKE_ROOT
    BPR_CACHE_ROOT
    BPR_RUN_ROOT
    BPR_TEMP_ROOT)
  if(DEFINED ENV{${_bpr_output_variable}} AND NOT "$ENV{${_bpr_output_variable}}" STREQUAL "")
    cmake_path(SET _bpr_output_path NORMALIZE "$ENV{${_bpr_output_variable}}")
    set(ENV{${_bpr_output_variable}} "${_bpr_output_path}")
    file(MAKE_DIRECTORY "${_bpr_output_path}")
  endif()
endforeach()
unset(_bpr_output_variable)
unset(_bpr_output_path)
