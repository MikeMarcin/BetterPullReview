cmake_minimum_required(VERSION 3.21)
project(BprXdiffRecipient LANGUAGES C)

# [[BPR-1656]]. This entry point is shipped in the corresponding-source asset.
# Its only inputs are the recipient's extracted (and optionally modified) Git
# source, the two adjacent shims, and the asset's unchanged hardening helper.
# It never acquires dependencies or includes the application/provider project.
set(BPR_XDIFF_SOURCE_DIR "" CACHE PATH "Absolute path to the extracted Git 2.56.0 source")
if(NOT IS_ABSOLUTE "${BPR_XDIFF_SOURCE_DIR}" OR NOT IS_DIRECTORY "${BPR_XDIFF_SOURCE_DIR}")
    message(FATAL_ERROR "Set BPR_XDIFF_SOURCE_DIR to the absolute extracted Git 2.56.0 directory")
endif()
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration" FORCE)
endif()

get_filename_component(shims "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/Hardening.cmake")
foreach(header git-compat-util.h bpr-xdiff-regex.h)
    if(NOT EXISTS "${shims}/${header}")
        message(FATAL_ERROR "Corresponding-source compatibility header is missing: ${header}")
    endif()
endforeach()
if(NOT EXISTS "${BPR_XDIFF_SOURCE_DIR}/xdiff/xdiff.h")
    message(FATAL_ERROR "The extracted Git source has no xdiff/xdiff.h")
endif()

set(sources)
foreach(name xdiffi.c xemit.c xhistogram.c xmerge.c xpatience.c xprepare.c xutils.c)
    set(source "${BPR_XDIFF_SOURCE_DIR}/xdiff/${name}")
    if(NOT EXISTS "${source}")
        message(FATAL_ERROR "The extracted Git source is missing xdiff/${name}")
    endif()
    list(APPEND sources "${source}")
endforeach()
add_library(better_pull_review_xdiff SHARED ${sources})
target_include_directories(better_pull_review_xdiff PRIVATE "${shims}")
target_include_directories(better_pull_review_xdiff SYSTEM PUBLIC
    "${BPR_XDIFF_SOURCE_DIR}/xdiff" "${shims}")
if(MSVC)
    target_compile_options(better_pull_review_xdiff PRIVATE /w)
else()
    target_compile_options(better_pull_review_xdiff PRIVATE -w)
endif()
set_target_properties(better_pull_review_xdiff PROPERTIES
    OUTPUT_NAME bpr_xdiff
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET default
    WINDOWS_EXPORT_ALL_SYMBOLS ON)
if(APPLE)
    set_target_properties(better_pull_review_xdiff PROPERTIES
        INSTALL_NAME_DIR "@rpath"
        BUILD_WITH_INSTALL_NAME_DIR ON)
endif()
bpr_enable_hardening(better_pull_review_xdiff)
