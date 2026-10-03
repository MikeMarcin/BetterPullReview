include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/AitoCpm.cmake")

# [[BPR-1270]]. The bundled code font: four faces of JetBrainsMonoNL Nerd Font
# from the official Nerd Fonts v3.5.1 release asset. The archive is pinned by
# size and SHA-256, every extracted face by its own SHA-256, and the archive's
# licence and attribution texts must equal the copies the installed notices
# carry from third_party/jetbrains-mono-nerd-font. Nothing upstream executes.
set(BPR_CODE_FONT_VERSION "3.5.1")
set(BPR_CODE_FONT_FAMILY "JetBrainsMonoNL Nerd Font")
set(
    BPR_CODE_FONT_FACES
    "JetBrainsMonoNLNerdFont-Regular.ttf|f726c0fb0db90a00920e8fa9ef281ccf9f5f848785ebcaca09bade9d15d38b58"
    "JetBrainsMonoNLNerdFont-Bold.ttf|104897b8ff7f74674c8d6d81b6dbe9138e5c249b54d6c08c8b21600f75d3c5c1"
    "JetBrainsMonoNLNerdFont-Italic.ttf|dd76e7fe83fd2b1ae8e416efb3af8348d975b004648eb28e9a836e1fc4dd72a0"
    "JetBrainsMonoNLNerdFont-BoldItalic.ttf|8d5892b455cc166f8aac459182aa7a0d7df6cf2d70d61047deb8783fbd2b5d67"
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
    set(archive "${CPM_SOURCE_CACHE}/nerd-fonts-v${BPR_CODE_FONT_VERSION}-JetBrainsMono.tar.xz")
    set(digest "04d5e8f903693f9dd13e16f867e994834e681eb3c72c0d337a770dcda09010cf")
    file(LOCK "${archive}.lock" GUARD FUNCTION TIMEOUT 120)
    if(NOT EXISTS "${archive}")
        if(BPR_DEPENDENCY_OFFLINE)
            message(
                FATAL_ERROR
                "Offline mode requires the cached Nerd Fonts v${BPR_CODE_FONT_VERSION} "
                "JetBrainsMono.tar.xz archive at ${archive}"
            )
        endif()
        file(
            DOWNLOAD
            "https://github.com/ryanoasis/nerd-fonts/releases/download/v${BPR_CODE_FONT_VERSION}/JetBrainsMono.tar.xz"
            "${archive}"
            EXPECTED_HASH "SHA256=${digest}"
            TLS_VERIFY ON
        )
    endif()
    file(SHA256 "${archive}" actual)
    file(SIZE "${archive}" bytes)
    if(NOT actual STREQUAL digest OR NOT bytes EQUAL 7284244)
        message(FATAL_ERROR "Nerd Fonts JetBrainsMono archive does not match pinned v${BPR_CODE_FONT_VERSION}")
    endif()

    # Extract the verified archive into a scratch directory, never trusting an
    # editable cached tree, then copy only changed bytes into the stage so a
    # reconfigure does not rebuild the resources that embed the faces.
    set(scratch "${CMAKE_BINARY_DIR}/_bpr/code-font-extract")
    set(stage "${CMAKE_BINARY_DIR}/_bpr/code-font")
    file(REMOVE_RECURSE "${scratch}")
    file(MAKE_DIRECTORY "${scratch}" "${stage}")
    set(names)
    foreach(face IN LISTS BPR_CODE_FONT_FACES)
        string(REPLACE "|" ";" face "${face}")
        list(GET face 0 name)
        list(APPEND names "${name}")
    endforeach()
    file(
        ARCHIVE_EXTRACT
        INPUT "${archive}"
        DESTINATION "${scratch}"
        PATTERNS ${names} OFL.txt README.md
    )

    set(notices "${PROJECT_SOURCE_DIR}/third_party/jetbrains-mono-nerd-font")
    foreach(pair IN ITEMS "OFL.txt|OFL.txt" "README.md|NERD-FONTS-README.md")
        string(REPLACE "|" ";" pair "${pair}")
        list(GET pair 0 member)
        list(GET pair 1 retained)
        file(SHA256 "${scratch}/${member}" member_digest)
        file(SHA256 "${notices}/${retained}" retained_digest)
        if(NOT member_digest STREQUAL retained_digest)
            message(
                FATAL_ERROR
                "The code font archive's ${member} differs from the retained "
                "third_party/jetbrains-mono-nerd-font/${retained}"
            )
        endif()
    endforeach()

    foreach(face IN LISTS BPR_CODE_FONT_FACES)
        string(REPLACE "|" ";" face "${face}")
        list(GET face 0 name)
        list(GET face 1 face_digest)
        file(SHA256 "${scratch}/${name}" actual_face)
        if(NOT actual_face STREQUAL face_digest)
            message(FATAL_ERROR "Code font face ${name} does not match its pinned digest")
        endif()
        file(COPY_FILE "${scratch}/${name}" "${stage}/${name}" ONLY_IF_DIFFERENT)
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
