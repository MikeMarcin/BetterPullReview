# [[BPR-0974]]. Where the application's Qt resources live.
#
# A tests-disabled Windows image keeps its compressed Qt resources -- the
# application icon, the Dif sprite strips and the KSyntaxHighlighting syntax
# definitions, 3.5 MB of near-random bytes that were 28% of the executable --
# in one binary resource file beside the executable instead of inside it.
# BPR-1110 found that block among the static signals a cloud classifier weighs,
# and the owner chose to move it out (decision 5c30287a, 2026-09-25). The image
# registers the file before it reads any resource and refuses to start unless
# the file is exactly the one it was built with, so a missing or damaged file
# fails at startup rather than leaving an image without its icon, Dif or syntax
# highlighting.
#
# Every other graph embeds the same resources as before. Tests-enabled Windows
# graphs embed them too, and also build the same bundle, so a Windows contract
# can prove its contents and its registration without a tests-disabled image.
# macOS and Linux build no bundle.
#
# Three steps, in configure order:
#   bpr_add_qt_resources()           a first-party resource collection
#   bpr_add_qt_resource_collection() an upstream .qrc the bundle includes whole
#   bpr_add_qt_resource_bundle()     the bundle, once every collection is known

if(WIN32 AND BPR_BUILD_QT)
    set(BPR_QT_RESOURCE_BUNDLE ON)
else()
    set(BPR_QT_RESOURCE_BUNDLE OFF)
endif()
if(BPR_QT_RESOURCE_BUNDLE AND NOT BUILD_TESTING)
    set(BPR_EXTERNAL_QT_RESOURCES ON)
else()
    set(BPR_EXTERNAL_QT_RESOURCES OFF)
endif()

set(BPR_QT_RESOURCE_BUNDLE_NAME "better_pull_review.rcc")

function(bpr_require_open_qt_resource_bundle)
    get_property(sealed GLOBAL PROPERTY BPR_QT_RESOURCE_BUNDLE_SEALED)
    if(sealed)
        message(
            FATAL_ERROR
            "A Qt resource collection was added after the resource bundle was "
            "built; add it before src/app so the bundle carries it"
        )
    endif()
endfunction()

# bpr_add_qt_resources(<target> <name> PREFIX <prefix> BASE <directory>
#                      FILES <file>... [NO_COMPRESS | ALWAYS_COMPRESS])
#
# Embeds the files in <target> through qt6_add_resources, exactly as a direct
# call would, unless this graph ships its resources outside the image. Where a
# bundle is built, the same files, paths and compression also go into it.
#
# ALWAYS_COMPRESS sets rcc's threshold to zero. rcc otherwise stores a file
# whole unless compression saves 70% of it, and a font saves about 45%
# ([[BPR-1270]]: 9.97 MB stored, 5.46 MB compressed for the code font's faces).
function(bpr_add_qt_resources target name)
    cmake_parse_arguments(PARSE_ARGV 2 resource "NO_COMPRESS;ALWAYS_COMPRESS" "PREFIX;BASE" "FILES")
    if(resource_NO_COMPRESS AND resource_ALWAYS_COMPRESS)
        message(FATAL_ERROR "bpr_add_qt_resources(${target} ${name}) takes NO_COMPRESS or ALWAYS_COMPRESS, not both")
    endif()
    if(resource_UNPARSED_ARGUMENTS OR NOT resource_PREFIX OR NOT resource_BASE OR NOT resource_FILES)
        message(
            FATAL_ERROR
            "bpr_add_qt_resources(${target} ${name}) requires PREFIX, BASE and "
            "FILES and nothing else"
        )
    endif()
    if(NOT BPR_EXTERNAL_QT_RESOURCES)
        set(options "")
        if(resource_NO_COMPRESS)
            set(options OPTIONS --no-compress)
        elseif(resource_ALWAYS_COMPRESS)
            set(options OPTIONS --threshold 0)
        endif()
        qt6_add_resources(
            "${target}"
            "${name}"
            PREFIX "${resource_PREFIX}"
            BASE "${resource_BASE}"
            ${options}
            FILES ${resource_FILES}
        )
    endif()
    if(NOT BPR_QT_RESOURCE_BUNDLE)
        return()
    endif()
    bpr_require_open_qt_resource_bundle()
    get_filename_component(base "${resource_BASE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set(attributes "")
    if(resource_NO_COMPRESS)
        set(attributes " compression-algorithm=\"none\"")
    elseif(resource_ALWAYS_COMPRESS)
        set(attributes " threshold=\"0\"")
    endif()
    set(block "<qresource prefix=\"${resource_PREFIX}\">\n")
    set(depends "")
    foreach(file IN LISTS resource_FILES)
        get_filename_component(path "${file}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        file(RELATIVE_PATH alias "${base}" "${path}")
        if(alias MATCHES "^\\.\\./" OR IS_ABSOLUTE "${alias}")
            message(FATAL_ERROR "Qt resource ${path} is outside its BASE ${base}")
        endif()
        string(APPEND block "<file alias=\"${alias}\"${attributes}>${path}</file>\n")
        list(APPEND depends "${path}")
    endforeach()
    string(APPEND block "</qresource>\n")
    set_property(GLOBAL APPEND_STRING PROPERTY BPR_QT_RESOURCE_BUNDLE_BLOCKS "${block}")
    set_property(GLOBAL APPEND PROPERTY BPR_QT_RESOURCE_BUNDLE_DEPENDS ${depends})
endfunction()

# bpr_add_qt_resource_collection(QRC <file> DEPENDS <file>...)
#
# An upstream collection the bundle includes as it stands, with the files it
# names; for the syntax definitions, KSyntaxHighlighting's own syntax-data.qrc.
# The graph that embeds its resources gets them from upstream, not from here.
function(bpr_add_qt_resource_collection)
    cmake_parse_arguments(PARSE_ARGV 0 collection "" "QRC" "DEPENDS")
    if(collection_UNPARSED_ARGUMENTS OR NOT collection_QRC)
        message(FATAL_ERROR "bpr_add_qt_resource_collection requires QRC")
    endif()
    if(NOT BPR_QT_RESOURCE_BUNDLE)
        return()
    endif()
    bpr_require_open_qt_resource_bundle()
    if(NOT EXISTS "${collection_QRC}")
        message(FATAL_ERROR "Qt resource collection ${collection_QRC} does not exist")
    endif()
    set_property(GLOBAL APPEND PROPERTY BPR_QT_RESOURCE_BUNDLE_COLLECTIONS "${collection_QRC}")
    set_property(GLOBAL APPEND PROPERTY BPR_QT_RESOURCE_BUNDLE_DEPENDS "${collection_QRC}" ${collection_DEPENDS})
endfunction()

# bpr_add_qt_resource_bundle(<target>)
#
# Builds the bundle and a header naming its SHA-256 in
# ${CMAKE_CURRENT_BINARY_DIR}/_bpr_resources, and closes the record. Where the
# image ships its resources outside itself, <target> compiles the header in and
# gets the bundle copied beside it after every link. The target
# better_pull_review_resource_bundle builds both and names them in its
# BPR_BUNDLE_FILE and BPR_BUNDLE_HEADER_DIRECTORY properties.
function(bpr_add_qt_resource_bundle target)
    if(NOT BPR_QT_RESOURCE_BUNDLE)
        return()
    endif()
    bpr_require_open_qt_resource_bundle()
    set_property(GLOBAL PROPERTY BPR_QT_RESOURCE_BUNDLE_SEALED TRUE)
    get_property(blocks GLOBAL PROPERTY BPR_QT_RESOURCE_BUNDLE_BLOCKS)
    get_property(collections GLOBAL PROPERTY BPR_QT_RESOURCE_BUNDLE_COLLECTIONS)
    get_property(depends GLOBAL PROPERTY BPR_QT_RESOURCE_BUNDLE_DEPENDS)
    if(blocks STREQUAL "" OR NOT collections)
        message(FATAL_ERROR "The Qt resource bundle has no first-party or upstream collection")
    endif()

    set(directory "${CMAKE_CURRENT_BINARY_DIR}/_bpr_resources")
    set(collection "${directory}/better_pull_review_resources.qrc")
    set(bundle "${directory}/${BPR_QT_RESOURCE_BUNDLE_NAME}")
    set(header "${directory}/better_pull_review_resources_sha256.hpp")
    set(digest_script "${PROJECT_SOURCE_DIR}/cmake/BprQtResourceBundleDigest.cmake")
    file(
        CONFIGURE
        OUTPUT "${collection}"
        CONTENT "<!DOCTYPE RCC>\n<RCC version=\"1.0\">\n${blocks}</RCC>\n"
        @ONLY
    )
    add_custom_command(
        OUTPUT "${bundle}" "${header}"
        COMMAND Qt6::rcc --binary --output "${bundle}" "${collection}" ${collections}
        COMMAND
            "${CMAKE_COMMAND}"
            "-DBPR_BUNDLE=${bundle}"
            "-DBPR_HEADER=${header}"
            -P "${digest_script}"
        DEPENDS "${collection}" ${depends} "${digest_script}"
        COMMENT "Building the external Qt resource bundle ${BPR_QT_RESOURCE_BUNDLE_NAME}"
        VERBATIM
    )
    add_custom_target(better_pull_review_resource_bundle DEPENDS "${bundle}" "${header}")
    set_target_properties(
        better_pull_review_resource_bundle
        PROPERTIES
            BPR_BUNDLE_FILE "${bundle}"
            BPR_BUNDLE_HEADER_DIRECTORY "${directory}"
    )

    if(BPR_EXTERNAL_QT_RESOURCES)
        target_sources("${target}" PRIVATE "${header}")
        target_include_directories("${target}" PRIVATE "${directory}")
        target_compile_definitions("${target}" PRIVATE BPR_EXTERNAL_QT_RESOURCES=1)
        add_dependencies("${target}" better_pull_review_resource_bundle)
        add_custom_command(TARGET "${target}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${bundle}"
                "$<TARGET_FILE_DIR:${target}>/${BPR_QT_RESOURCE_BUNDLE_NAME}"
            VERBATIM)
    endif()
endfunction()
