include_guard(GLOBAL)

function(bpr_configure_cpm_source_cache)
  if(DEFINED ENV{BPR_CACHE_ROOT} AND NOT "$ENV{BPR_CACHE_ROOT}" STREQUAL "")
    set(bpr_cpm_source_cache "$ENV{BPR_CACHE_ROOT}/cpm")
  else()
    get_filename_component(bpr_code_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    set(bpr_cpm_source_cache "${bpr_code_root}/.build/cache/cpm")
  endif()
  if(DEFINED CPM_SOURCE_CACHE AND NOT CPM_SOURCE_CACHE STREQUAL bpr_cpm_source_cache)
    message(STATUS "Using the repository CPM cache at ${bpr_cpm_source_cache} instead of ${CPM_SOURCE_CACHE}")
  endif()
  set(
    CPM_SOURCE_CACHE
    "${bpr_cpm_source_cache}"
    CACHE PATH
    "BPR repo-local CPM source cache"
    FORCE
  )
endfunction()
