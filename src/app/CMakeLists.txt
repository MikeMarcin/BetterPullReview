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
            "${PROJECT_SOURCE_DIR}/src/application/src"
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

	set(
		bpr_attended_review_source_surface
		src/app/CMakeLists.txt
		src/app/main.cpp
		src/app/include/bpr/app/github_application.hpp
		src/app/src/github_application.cpp
		src/application/CMakeLists.txt
		src/application/include/bpr/application/review_mutation_application_host.hpp
		src/application/include/bpr/application/review_mutation_recovery_port.hpp
		src/application/src/designated_review_authority_bootstrap.cpp
		src/application/src/review_mutation_execution_authority.hpp
		src/application/src/review_mutation_application_host.cpp
		src/application/src/review_mutation_coordinator.cpp
		src/application/src/review_mutation_contract.cpp
		src/adapters/github/CMakeLists.txt
		src/adapters/github/src/github_mutation_contract.hpp
		src/adapters/github/src/github_mutation_contract.cpp
		src/adapters/github/src/designated_review_public_observer.hpp
		src/adapters/github/src/designated_live_public_observer.hpp
		src/adapters/github/src/designated_live_public_observer.cpp
		src/adapters/github/src/live_review_mutation_provider.hpp
		src/adapters/github/src/live_review_mutation_provider.cpp
		src/adapters/github/src/native_https_transport.hpp
		src/adapters/github/src/native_https_transport.cpp
		src/adapters/sqlite/CMakeLists.txt
		src/adapters/sqlite/include/bpr/adapters/sqlite/review_mutation_recovery_store.hpp
		src/adapters/sqlite/src/review_mutation_recovery_store.cpp
		src/app/src/designated_live_macos_supervisor.cpp
		scripts/designated_live_macos_capsule.py
	)
	set(bpr_attended_review_source_manifest "")
	set(bpr_attended_review_source_surface_absolute "")
	foreach(relative_path IN LISTS bpr_attended_review_source_surface)
		set(source_path "${PROJECT_SOURCE_DIR}/${relative_path}")
		list(APPEND bpr_attended_review_source_surface_absolute "${source_path}")
		file(SHA256 "${source_path}" source_sha256)
		string(
			APPEND
			bpr_attended_review_source_manifest
			"${relative_path}=${source_sha256}\n"
		)
	endforeach()
	set_property(
		DIRECTORY
		APPEND
		PROPERTY CMAKE_CONFIGURE_DEPENDS
			${bpr_attended_review_source_surface_absolute}
	)
	string(
		SHA256
		BPR_ATTENDED_REVIEW_SOURCE_SHA256
		"${bpr_attended_review_source_manifest}"
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
			"BPR_ATTENDED_REVIEW_SOURCE_SHA256=\"${BPR_ATTENDED_REVIEW_SOURCE_SHA256}\""
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

    if(BPR_BUILD_DESIGNATED_LIVE_MACOS_CAPSULE)
        if(NOT APPLE OR BUILD_TESTING)
            message(
                FATAL_ERROR
                "The designated live macOS capsule is a tests-disabled macOS target"
            )
        endif()
        if(NOT TARGET Python3::Interpreter)
            message(
                FATAL_ERROR
                "The designated live macOS capsule requires the configured Python interpreter"
            )
        endif()
        find_library(BPR_DESIGNATED_LIVE_SECURITY_FRAMEWORK Security REQUIRED)
        find_library(
            BPR_DESIGNATED_LIVE_COREFOUNDATION_FRAMEWORK
            CoreFoundation
            REQUIRED
        )
        find_program(
            BPR_DESIGNATED_LIVE_MACDEPLOYQT
            NAMES macdeployqt
            HINTS "${Qt6_DIR}/../../../bin"
            NO_DEFAULT_PATH
            REQUIRED
        )
        bpr_add_executable(
            tool_better_pull_review_designated_live_supervisor
            src/designated_live_macos_supervisor.cpp
        )
        target_link_libraries(
            tool_better_pull_review_designated_live_supervisor
            PRIVATE
                BetterPullReview::DesignatedLivePublicObserver
                "${BPR_DESIGNATED_LIVE_COREFOUNDATION_FRAMEWORK}"
                "${BPR_DESIGNATED_LIVE_SECURITY_FRAMEWORK}"
        )
        target_compile_definitions(
            tool_better_pull_review_designated_live_supervisor
            PRIVATE
                "BPR_ATTENDED_REVIEW_SOURCE_SHA256=\"${BPR_ATTENDED_REVIEW_SOURCE_SHA256}\""
        )
        target_compile_features(
            tool_better_pull_review_designated_live_supervisor
            PRIVATE
                cxx_std_23
        )
        bpr_enable_hardening(tool_better_pull_review_designated_live_supervisor)
        bpr_enable_warnings(tool_better_pull_review_designated_live_supervisor)

        set(
            bpr_designated_live_capsule_parent
            "${CMAKE_BINARY_DIR}/designated-live/${BPR_ATTENDED_REVIEW_SOURCE_SHA256}"
        )
        set(
            bpr_designated_live_capsule_root
            "${bpr_designated_live_capsule_parent}/capsule"
        )
        set(
            bpr_designated_live_capsule_inventory
            "${bpr_designated_live_capsule_parent}/capsule.inventory"
        )
        set(
            bpr_designated_live_capsule_stamp
            "${bpr_designated_live_capsule_parent}/capsule.stamp"
        )
        add_custom_command(
            OUTPUT
                "${bpr_designated_live_capsule_stamp}"
                "${bpr_designated_live_capsule_inventory}"
            COMMAND
                "${CMAKE_COMMAND}" -E remove_directory
                "${bpr_designated_live_capsule_root}"
            COMMAND
                Python3::Interpreter
                "${PROJECT_SOURCE_DIR}/scripts/designated_live_macos_capsule.py"
                build
                --application "$<TARGET_BUNDLE_DIR:app_better_pull_review>"
                --supervisor
                "$<TARGET_FILE:tool_better_pull_review_designated_live_supervisor>"
                --verifier
                "${PROJECT_SOURCE_DIR}/scripts/designated_live_macos_capsule.py"
                --output "${bpr_designated_live_capsule_root}"
                --inventory "${bpr_designated_live_capsule_inventory}"
                --source-sha256 "${BPR_ATTENDED_REVIEW_SOURCE_SHA256}"
                --macdeployqt "${BPR_DESIGNATED_LIVE_MACDEPLOYQT}"
            COMMAND
                "${CMAKE_COMMAND}" -E touch
                "${bpr_designated_live_capsule_stamp}"
            DEPENDS
                app_better_pull_review
                tool_better_pull_review_designated_live_supervisor
                "${PROJECT_SOURCE_DIR}/scripts/designated_live_macos_capsule.py"
            VERBATIM
        )
        add_custom_target(
            designated_live_macos_capsule
            DEPENDS
                "${bpr_designated_live_capsule_stamp}"
                "${bpr_designated_live_capsule_inventory}"
        )
        set_property(
            TARGET designated_live_macos_capsule
            PROPERTY BPR_DESIGNATED_LIVE_CAPSULE_ROOT
                "${bpr_designated_live_capsule_root}"
        )
        set_property(
            TARGET designated_live_macos_capsule
            PROPERTY BPR_DESIGNATED_LIVE_CAPSULE_INVENTORY
                "${bpr_designated_live_capsule_inventory}"
        )
    endif()
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
