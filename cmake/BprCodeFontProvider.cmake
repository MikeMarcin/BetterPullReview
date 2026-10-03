include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/AitoCpm.cmake")

# [[BPR-1270]]. The bundled code font: four faces of JetBrains Mono NL from the
# official JetBrains Mono v2.304 release asset. The archive is pinned by size
# and SHA-256, every extracted face by its own SHA-256, and the archive's
# licence and authors texts must equal the copies the installed notices carry
# from third_party/jetbrains-mono. Nothing upstream executes.
set(BPR_CODE_FONT_VERSION "2.304")
set(BPR_CODE_FONT_FAMILY "JetBrains Mono NL")
set(
    BPR_CODE_FONT_FACES
    "JetBrainsMonoNL-Regular.ttf|fb3b2575d7b0657359707993288f12a7360344d39387bb26050e276d61f6bd2a"
    "JetBrainsMonoNL-Bold.ttf|0198e841824025f8876e5c297f0b9b497ee8d6eb9969710a3328e1303f996ec3"
    "JetBrainsMonoNL-Italic.ttf|c7392a134293e1af1d36fbf04940dd844f632afbf97f8325d1591d3af5096cb8"
    "JetBrainsMonoNL-BoldItalic.ttf|6c2716f2e85101f4109a8c16916061eb38176b3b10342834858d6221ec2fda1b"
)

# bpr_acquire_code_font()
#
# Defines BetterPullReview::CodeFont, an interface target whose
# BPR_CODE_FONT_DIRECTORY property names the directory holding the verified
# faces and whose BPR_CODE_FONT_FILES property lists them by name.
function(bpr_acquire_code_font)
    if(TARGET BetterPullReview::CodeFont OR TARGET better_pull_review_code_font)
        message(FATAL_ERROR "The code font target must be owned by the pinned provider")
    endif()
    bpr_load_cpm()
    set(archive "${CPM_SOURCE_CACHE}/JetBrainsMono-${BPR_CODE_FONT_VERSION}.zip")
    set(digest "6f6376c6ed2960ea8a963cd7387ec9d76e3f629125bc33d1fdcd7eb7012f7bbf")
    file(LOCK "${archive}.lock" GUARD FUNCTION TIMEOUT 120)
    if(NOT EXISTS "${archive}")
        if(BPR_DEPENDENCY_OFFLINE)
            message(
                FATAL_ERROR
                "Offline mode requires the cached JetBrains Mono v${BPR_CODE_FONT_VERSION} "
                "release archive at ${archive}"
            )
        endif()
        file(
            DOWNLOAD
            "https://github.com/JetBrains/JetBrainsMono/releases/download/v${BPR_CODE_FONT_VERSION}/JetBrainsMono-${BPR_CODE_FONT_VERSION}.zip"
            "${archive}"
            EXPECTED_HASH "SHA256=${digest}"
            TLS_VERIFY ON
        )
    endif()
    file(SHA256 "${archive}" actual)
    file(SIZE "${archive}" bytes)
    if(NOT actual STREQUAL digest OR NOT bytes EQUAL 5622857)
        message(FATAL_ERROR "JetBrains Mono archive does not match pinned v${BPR_CODE_FONT_VERSION}")
    endif()

    # Extract the verified archive into a scratch directory, never trusting an
    # editable cached tree, then copy only changed bytes into the stage so a
    # reconfigure does not rebuild the resources that embed the faces. A file
    # in the stage that is no longer a pinned face is removed.
    set(scratch "${CMAKE_BINARY_DIR}/_bpr/code-font-extract")
    set(stage "${CMAKE_BINARY_DIR}/_bpr/code-font")
    file(REMOVE_RECURSE "${scratch}")
    file(MAKE_DIRECTORY "${scratch}" "${stage}")
    set(names)
    set(members)
    foreach(face IN LISTS BPR_CODE_FONT_FACES)
        string(REPLACE "|" ";" face "${face}")
        list(GET face 0 name)
        list(APPEND names "${name}")
        list(APPEND members "fonts/ttf/${name}")
    endforeach()
    file(GLOB staged LIST_DIRECTORIES true RELATIVE "${stage}" "${stage}/*")
    foreach(entry IN LISTS staged)
        if(NOT entry IN_LIST names)
            file(REMOVE_RECURSE "${stage}/${entry}")
        endif()
    endforeach()
    file(
        ARCHIVE_EXTRACT
        INPUT "${archive}"
        DESTINATION "${scratch}"
        PATTERNS ${members} OFL.txt AUTHORS.txt
    )

    set(notices "${PROJECT_SOURCE_DIR}/third_party/jetbrains-mono")
    foreach(text IN ITEMS OFL.txt AUTHORS.txt)
        file(SHA256 "${scratch}/${text}" member_digest)
        file(SHA256 "${notices}/${text}" retained_digest)
        if(NOT member_digest STREQUAL retained_digest)
            message(
                FATAL_ERROR
                "The code font archive's ${text} differs from the retained "
                "third_party/jetbrains-mono/${text}"
            )
        endif()
    endforeach()

    foreach(face IN LISTS BPR_CODE_FONT_FACES)
        string(REPLACE "|" ";" face "${face}")
        list(GET face 0 name)
        list(GET face 1 face_digest)
        file(SHA256 "${scratch}/fonts/ttf/${name}" actual_face)
        if(NOT actual_face STREQUAL face_digest)
            message(FATAL_ERROR "Code font face ${name} does not match its pinned digest")
        endif()
        file(COPY_FILE "${scratch}/fonts/ttf/${name}" "${stage}/${name}" ONLY_IF_DIFFERENT)
    endforeach()
    file(REMOVE_RECURSE "${scratch}")

    add_library(better_pull_review_code_font INTERFACE)
    add_library(BetterPullReview::CodeFont ALIAS better_pull_review_code_font)
    set_target_properties(
        better_pull_review_code_font
        PROPERTIES
            BPR_CODE_FONT_DIRECTORY "${stage}"
            BPR_CODE_FONT_FILES "${names}"
            BPR_CODE_FONT_FAMILY "${BPR_CODE_FONT_FAMILY}"
    )
endfunction()
