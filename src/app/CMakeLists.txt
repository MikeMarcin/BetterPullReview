if(BPR_BUILD_QT)
    add_library(
        better_pull_review_composition
        STATIC
            src/foundation_application.cpp
            src/github_application.cpp
            src/worker_executor.cpp
    )
    if(BUILD_TESTING)
        target_sources(
            better_pull_review_composition
            PRIVATE
                src/github_public_path_fixture.cpp
        )
    endif()
    add_library(
        BetterPullReview::Composition
        ALIAS
        better_pull_review_composition
    )

    target_link_libraries(
        better_pull_review_composition
        PUBLIC
            BetterPullReview::Application
            BetterPullReview::AdapterGitHub
            BetterPullReview::AdapterGit
            BetterPullReview::AdapterSQLite
            BetterPullReview::AdapterSyntax
            BetterPullReview::AdapterAI
			BetterPullReview::AdapterAgent
            BetterPullReview::AdapterQt
            Threads::Threads
    )
    target_include_directories(
        better_pull_review_composition
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
    )
    target_compile_features(better_pull_review_composition PUBLIC cxx_std_23)
    bpr_enable_hardening(better_pull_review_composition)
    bpr_enable_warnings(better_pull_review_composition)
    if(BUILD_TESTING)
        target_compile_definitions(
            better_pull_review_composition
            PRIVATE
                BPR_BUILD_TESTING=1
                "BPR_GITHUB_CONTRACT_FIXTURE_ROOT=\"${PROJECT_SOURCE_DIR}/tests/fixtures/github/contracts/v1\""
                "BPR_GITHUB_CAPABILITY_FIXTURE_ROOT=\"${PROJECT_SOURCE_DIR}/tests/fixtures/github/capabilities/v1\""
                "BPR_GITHUB_PULL_REQUEST_FIXTURE_ROOT=\"${PROJECT_SOURCE_DIR}/tests/fixtures/github/pull-requests\""
        )
    endif()

    bpr_add_executable(
        app_better_pull_review
        main.cpp
    )
    target_link_libraries(
        app_better_pull_review
        PRIVATE
            BetterPullReview::Composition
    )
    target_compile_definitions(
        app_better_pull_review
        PRIVATE
            "BPR_VERSION=\"${PROJECT_VERSION}\""
            "BPR_BUILD_CONFIGURATION=\"$<CONFIG>\""
    )

    set(bpr_app_icon_root "${PROJECT_SOURCE_DIR}/packaging/icons")
    set(
        bpr_app_icon_pngs
        "${bpr_app_icon_root}/png/16x16/better_pull_review.png"
        "${bpr_app_icon_root}/png/24x24/better_pull_review.png"
        "${bpr_app_icon_root}/png/32x32/better_pull_review.png"
        "${bpr_app_icon_root}/png/40x40/better_pull_review.png"
        "${bpr_app_icon_root}/png/48x48/better_pull_review.png"
        "${bpr_app_icon_root}/png/64x64/better_pull_review.png"
        "${bpr_app_icon_root}/png/128x128/better_pull_review.png"
        "${bpr_app_icon_root}/png/256x256/better_pull_review.png"
        "${bpr_app_icon_root}/png/512x512/better_pull_review.png"
    )
    qt6_add_resources(
        app_better_pull_review
        better_pull_review_app_icon
        PREFIX
            "/icons"
        BASE
            "${bpr_app_icon_root}/png"
        FILES
            ${bpr_app_icon_pngs}
    )

    if(WIN32)
        enable_language(RC)
        set(
            bpr_app_icon_windows_resource
            "${bpr_app_icon_root}/windows/better_pull_review.rc"
        )
        target_sources(
            app_better_pull_review
            PRIVATE
                "${bpr_app_icon_windows_resource}"
        )
        set_source_files_properties(
            "${bpr_app_icon_windows_resource}"
            PROPERTIES
                OBJECT_DEPENDS
                    "${bpr_app_icon_root}/windows/better_pull_review.ico"
        )
    elseif(APPLE AND NOT BUILD_TESTING)
        # Release/package graphs are tests-disabled application bundles. The
        # tests-enabled graph keeps its established directly executable path;
        # the embedded QIcon still supplies its running application icon.
        set(
            bpr_app_icon_macos
            "${bpr_app_icon_root}/macos/better_pull_review.icns"
        )
        set_source_files_properties(
            "${bpr_app_icon_macos}"
            PROPERTIES
                MACOSX_PACKAGE_LOCATION "Resources"
        )
        target_sources(
            app_better_pull_review
            PRIVATE
                "${bpr_app_icon_macos}"
        )
        set_target_properties(
            app_better_pull_review
            PROPERTIES
                MACOSX_BUNDLE TRUE
                MACOSX_BUNDLE_BUNDLE_NAME "BetterPullReview"
                MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
                MACOSX_BUNDLE_GUI_IDENTIFIER
                    "io.github.MikeMarcin.BetterPullReview"
                MACOSX_BUNDLE_ICON_FILE "better_pull_review.icns"
                MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        )
    endif()

    if(BUILD_TESTING)
        target_include_directories(
            app_better_pull_review
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src"
        )
        target_compile_definitions(
            app_better_pull_review
            PRIVATE
                BPR_BUILD_TESTING=1
        )
        if(APPLE)
            find_library(
                BPR_MACOS_APPEARANCE_COREFOUNDATION_FRAMEWORK
                CoreFoundation
                REQUIRED
            )
            target_link_libraries(
                app_better_pull_review
                PRIVATE
                    "${BPR_MACOS_APPEARANCE_COREFOUNDATION_FRAMEWORK}"
            )
        endif()
    endif()
    bpr_enable_hardening(app_better_pull_review)
    bpr_enable_warnings(app_better_pull_review)
else()
    add_library(better_pull_review_composition INTERFACE)
    add_library(
        BetterPullReview::Composition
        ALIAS
        better_pull_review_composition
    )
    target_link_libraries(
        better_pull_review_composition
        INTERFACE
            BetterPullReview::Application
            BetterPullReview::AdapterGitHub
            BetterPullReview::AdapterGit
            BetterPullReview::AdapterSQLite
            BetterPullReview::AdapterAI
    )
endif()
