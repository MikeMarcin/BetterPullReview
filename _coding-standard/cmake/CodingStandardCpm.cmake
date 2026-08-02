include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/CodingStandardCpmCache.cmake")
bpr_configure_cpm_source_cache()

set(BPR_CPM_VERSION "0.43.1")
set(BPR_CPM_SHA256 "1c40fc102ce9625d7de7eb14f541cab30cc3138dca627f0b0ec40293ce6c2934")
set(BPR_CPM_FILE "${CMAKE_BINARY_DIR}/cmake/CPM-${BPR_CPM_VERSION}.cmake")

set(bpr_download_cpm ON)
if(EXISTS "${BPR_CPM_FILE}")
  file(SHA256 "${BPR_CPM_FILE}" bpr_cpm_file_sha256)
  if(bpr_cpm_file_sha256 STREQUAL BPR_CPM_SHA256)
    set(bpr_download_cpm OFF)
  else()
    file(REMOVE "${BPR_CPM_FILE}")
  endif()
endif()

if(bpr_download_cpm)
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/cmake")
  file(
    DOWNLOAD
      "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${BPR_CPM_VERSION}/CPM.cmake"
      "${BPR_CPM_FILE}"
    EXPECTED_HASH "SHA256=${BPR_CPM_SHA256}"
    TLS_VERIFY ON
  )
endif()

include("${BPR_CPM_FILE}")
if(NOT COMMAND CPMAddPackage)
  message(FATAL_ERROR "CPM bootstrap failed: CPMAddPackage is unavailable from ${BPR_CPM_FILE}")
endif()

unset(bpr_download_cpm)
unset(bpr_cpm_file_sha256)
