include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/AitoCpm.cmake")

# RFC-BPR-0093: allocator ownership belongs to executable composition, never
# to a core/host/adapter interface. Instrumented graphs retain their allocator.
function(bpr_configure_mimalloc)
    option(BPR_USE_MIMALLOC "Use process-wide mimalloc in product executables (OFF is a development baseline)" ON)
    set(instrumented "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS} ${CMAKE_EXE_LINKER_FLAGS}")
    foreach(configuration IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        string(APPEND instrumented " ${CMAKE_C_FLAGS_${configuration}} ${CMAKE_CXX_FLAGS_${configuration}} ${CMAKE_EXE_LINKER_FLAGS_${configuration}}")
    endforeach()
    if(BPR_BUILD_FUZZERS OR instrumented MATCHES "fsanitize=|/fsanitize=")
        set(BPR_MIMALLOC_ACTIVE OFF PARENT_SCOPE)
        if(BPR_DISTRIBUTION_BUILD)
            message(FATAL_ERROR "Distribution builds require mimalloc and cannot use sanitizer/fuzzer instrumentation")
        endif()
        message(STATUS "Product allocator: toolchain instrumentation (mimalloc excluded)")
        return()
    endif()
    if(BPR_DISTRIBUTION_BUILD AND NOT BPR_USE_MIMALLOC)
        message(FATAL_ERROR "Distribution builds require BPR_USE_MIMALLOC=ON")
    endif()
    set(BPR_MIMALLOC_ACTIVE OFF PARENT_SCOPE)
    if(NOT BPR_USE_MIMALLOC OR (NOT BPR_BUILD_QT AND NOT BPR_BUILD_TOUR_CLI))
        return()
    endif()
    if(WIN32)
        if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
            message(FATAL_ERROR "Windows mimalloc requires native MSVC x64 and the dynamic CRT")
        endif()
        if(DEFINED CMAKE_MSVC_RUNTIME_LIBRARY AND NOT CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
           AND NOT CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreadedDLL"
           AND NOT CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreadedDebugDLL")
            message(FATAL_ERROR "Windows mimalloc requires the dynamic CRT (/MD or /MDd)")
        endif()
        if(instrumented MATCHES "(^|[ ;])/MTd?([ ;]|$)")
            message(FATAL_ERROR "Windows mimalloc refuses static CRT compiler flags")
        endif()
    endif()
    foreach(name IN ITEMS BetterPullReview::Mimalloc mimalloc mimalloc-static mimalloc-obj)
        if(TARGET "${name}")
            message(FATAL_ERROR "mimalloc target '${name}' must be owned by the pinned provider")
        endif()
    endforeach()
    foreach(name IN ITEMS FETCHCONTENT_SOURCE_DIR_MIMALLOC FETCHCONTENT_SOURCE_DIR_BPRMIMALLOC CPM_mimalloc_SOURCE CPM_BprMimalloc_SOURCE)
        if(DEFINED ${name} AND NOT "${${name}}" STREQUAL "")
            message(FATAL_ERROR "mimalloc source substitution is forbidden: ${name}")
        endif()
    endforeach()
    bpr_load_cpm()
    set(archive "${CPM_SOURCE_CACHE}/mimalloc-3.5.3.tar.gz")
    set(digest "3b4a15153a59905995f7070296ed604bb5ccc00cabb8b93446931aff77224d47")
    file(LOCK "${archive}.lock" GUARD FUNCTION TIMEOUT 120)
    if(NOT EXISTS "${archive}")
        if(BPR_DEPENDENCY_OFFLINE)
            message(FATAL_ERROR "Offline mode requires cached mimalloc 3.5.3 archive")
        endif()
        file(DOWNLOAD "https://github.com/microsoft/mimalloc/archive/refs/tags/v3.5.3.tar.gz"
             "${archive}" EXPECTED_HASH "SHA256=${digest}" TLS_VERIFY ON)
    endif()
    file(SHA256 "${archive}" actual)
    file(SIZE "${archive}" bytes)
    if(NOT actual STREQUAL digest OR NOT bytes EQUAL 1455942)
        message(FATAL_ERROR "mimalloc archive does not match pinned 3.5.3")
    endif()
    set(stage "${CMAKE_BINARY_DIR}/_bpr/mimalloc-source")
    file(REMOVE_RECURSE "${stage}")
    file(MAKE_DIRECTORY "${stage}")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${stage}")
    set(source "${stage}/mimalloc-3.5.3")
    file(SHA256 "${source}/LICENSE" license_digest)
    if(NOT license_digest STREQUAL "82ade5d7d9b029044b5fbbc326207fc47c2d44ee4dd71e8a559c36d728217de9")
        message(FATAL_ERROR "mimalloc licence does not match the pinned notice")
    endif()
    # One shared allocator per process. No upstream tests, archives or object
    # variants; C compilation is upstream's recommendation for macOS interpose.
    set(MI_BUILD_SHARED ON)
    set(MI_BUILD_STATIC OFF)
    set(MI_BUILD_OBJECT OFF)
    set(MI_BUILD_TESTS OFF)
    set(MI_OVERRIDE ON)
    set(MI_OSX_INTERPOSE ON)
    set(MI_OSX_ZONE ON)
    set(MI_USE_CXX OFF)
    set(MI_XMALLOC OFF)
    set(MI_OPT_ARCH OFF)
    set(MI_OPT_SIMD OFF)
    set(MI_TRACK OFF)
    set(MI_DEBUG_TSAN OFF)
    set(MI_DEBUG_UBSAN OFF)
    set(MI_WIN_REDIRECT ON)
    add_subdirectory("${source}" "${CMAKE_BINARY_DIR}/_bpr/mimalloc" EXCLUDE_FROM_ALL)
    if(APPLE)
        # One real, versioned runtime file gives the closed package inventory a
        # stable load name and avoids shipping producer symlinks.
        set_target_properties(mimalloc PROPERTIES VERSION 3 SOVERSION 3)
    endif()
    if(MSVC)
        # The installed PE audit binds this source-built DLL to full producer
        # link evidence. The upstream redirect DLL is separately byte-pinned.
        target_compile_options(mimalloc PRIVATE "$<$<CONFIG:Release>:/Zi>")
        target_link_options(mimalloc PRIVATE /MAP
            "$<$<CONFIG:Release>:/DEBUG>" "$<$<CONFIG:Release>:/PDBALTPATH:%_PDB%>"
            "$<$<CONFIG:Release>:/OPT:REF>" "$<$<CONFIG:Release>:/OPT:ICF>"
            "$<$<CONFIG:Release>:/INCREMENTAL:NO>")
    endif()
    add_library(BetterPullReview::Mimalloc ALIAS mimalloc)
    set(BPR_MIMALLOC_ACTIVE ON PARENT_SCOPE)
    set(BPR_MIMALLOC_SOURCE_DIR "${source}" PARENT_SCOPE)
endfunction()

function(bpr_attach_product_allocator target)
    # All shipped programs, including the Qt-free MCP helper and supervisor.
    # Tests/benchmarks/tools outside this list keep toolchain allocation.
    if(NOT target MATCHES "^(app_better_pull_review|app_bpr|tool_better_pull_review_mcp_context_helper|tool_better_pull_review_agent_supervisor|tool_bpr_update_helper)$")
        return()
    endif()
    set_property(GLOBAL APPEND PROPERTY BPR_PRODUCT_EXECUTABLES "${target}")
    if(NOT BPR_MIMALLOC_ACTIVE)
        return()
    endif()
    target_link_libraries("${target}" PRIVATE BetterPullReview::Mimalloc)
    # Keep the dependency even when the program has no explicit mi_* call.
    # macOS uses the dylib's upstream interpose table plus malloc zone;
    # Windows' import retains the upstream CRT redirect dependency.
    if(APPLE)
        target_link_options("${target}" PRIVATE "LINKER:-u,_mi_version")
    elseif(MSVC)
        target_link_options("${target}" PRIVATE "/INCLUDE:mi_version")
        # Keep both pinned DLLs beside every development product image. This
        # dependency is executable-local and never changes a child's environment.
        # An unchanged import library can leave the EXE up to date after its
        # DLL changes. Run deployment as a dependency of every product build.
        add_custom_target("bpr_allocator_runtime_${target}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:${target}>"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:mimalloc>" "$<TARGET_FILE_DIR:${target}>"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE_DIR:mimalloc>/mimalloc-redirect.dll" "$<TARGET_FILE_DIR:${target}>"
            DEPENDS mimalloc
            VERBATIM)
        add_dependencies("${target}" "bpr_allocator_runtime_${target}")
    else()
        target_link_options("${target}" PRIVATE "LINKER:--no-as-needed")
    endif()
    set_property(TARGET "${target}" PROPERTY BPR_MIMALLOC_ENABLED TRUE)
endfunction()
