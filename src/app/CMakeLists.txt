if(BPR_BUILD_QT)
    add_library(
        better_pull_review_composition
        STATIC
            src/foundation_application.cpp
            src/github_application.cpp
            src/profile_desktop.cpp
            src/review_submission_workflow.cpp
            src/system_browser_opener.cpp
            src/worker_executor.cpp
    )
    if(BUILD_TESTING)
        target_link_libraries(better_pull_review_composition PRIVATE glaze::glaze)
        target_sources(
            better_pull_review_composition
            PRIVATE
                src/github_public_path_fixture.cpp
                src/review_submission_fixture.cpp
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
            BetterPullReview::AdapterLogging
            BetterPullReview::AdapterSyntax
            BetterPullReview::AdapterAI
			BetterPullReview::AdapterAgent
            BetterPullReview::AdapterMcp
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
    if(MSVC)
        # github_application.cpp exceeds the COFF object's section limit in the
        # windows-dev Debug graph (C1128).
        target_compile_options(better_pull_review_composition PRIVATE /bigobj)
    endif()
    if(WIN32)
        # [[BPR-0653]]. The strip is the Windows title row. Removing the system
        # title bar goes through Qt's Windows native interface, which Qt reaches
        # through private headers: pinned to the Qt the build provides and
        # rechecked on every Qt upgrade. Only the headers are taken, the one
        # versioned directory each module installs, as the macOS native menu
        # course takes QtGui's.
        target_sources(better_pull_review_composition PRIVATE src/windows_title_row_host.cpp)
        set(bpr_qt_private_include_dirs "")
        foreach(bpr_qt_private_module IN ITEMS QtCore QtGui)
            file(GLOB bpr_qt_private_headers LIST_DIRECTORIES true
                "${Qt6Gui_DIR}/../../../include/${bpr_qt_private_module}/[0-9]*.[0-9]*.[0-9]*")
            list(LENGTH bpr_qt_private_headers bpr_qt_private_header_count)
            if(NOT bpr_qt_private_header_count EQUAL 1)
                message(FATAL_ERROR
                    "Expected one versioned ${bpr_qt_private_module} private header directory, found: "
                    "${bpr_qt_private_headers}")
            endif()
            list(APPEND bpr_qt_private_include_dirs
                "${bpr_qt_private_headers}" "${bpr_qt_private_headers}/${bpr_qt_private_module}")
        endforeach()
        target_include_directories(better_pull_review_composition SYSTEM PRIVATE ${bpr_qt_private_include_dirs})
    endif()
    if(BUILD_TESTING)
        target_compile_definitions(
            better_pull_review_composition
            PRIVATE
                BPR_BUILD_TESTING=1
                # BPR-0515. Makes the GitHub adapter's fixture transport
                # factories visible to the composition, so a tests-enabled run
                # can compose its device-authorization wire against a closed
                # loopback port and deny its own egress locally. The seam is
                # the adapter's existing one and stays private and
                # BUILD_TESTING-only in both targets; a tests-disabled build
                # cannot see the declarations and does not link the symbols.
                BPR_NATIVE_HTTPS_TESTING=1
                "BPR_GITHUB_CONTRACT_FIXTURE_ROOT=\"${PROJECT_SOURCE_DIR}/tests/fixtures/github/contracts/v1\""
                "BPR_GITHUB_CAPABILITY_FIXTURE_ROOT=\"${PROJECT_SOURCE_DIR}/tests/fixtures/github/capabilities/v1\""
                # RFC-BPR-0017. The promoted signed-out bodies. The recorded
                # public-path fixture serves the inline review conversation
                # from the same captures the contract tests read, so the one
                # operation only a signed-out read issues is replayed from a
                # promoted live body rather than from a second invention.
                "BPR_RFC0017_ANONYMOUS_FIXTURE_ROOT=\"${PROJECT_SOURCE_DIR}/tests/fixtures/github/rfc0017-anonymous-v001\""
                "BPR_GITHUB_PULL_REQUEST_FIXTURE_ROOT=\"${PROJECT_SOURCE_DIR}/tests/fixtures/github/pull-requests\""
                "BPR_GITHUB_DEVICE_AUTHORIZATION_FIXTURE_ROOT=\"${PROJECT_SOURCE_DIR}/tests/fixtures/github/device-authorization/v1\""
                # RFC-BPR-0016 renewal. Documented rather than recorded, and a
                # separate root so the refresh course cannot read a documented
                # projection as a captured one.
                "BPR_GITHUB_DEVICE_DOCUMENTED_FIXTURE_ROOT=\"${PROJECT_SOURCE_DIR}/tests/fixtures/github/device-authorization/documented-v1\""
                # RFC-BPR-0012. The tests-enabled composition supervises the
                # shipped helper out of the build tree; the shipped one resolves
                # it beside its own executable. The construction is the same in
                # both graphs, and only the path differs.
                "BPR_MCP_HELPER_EXECUTABLE=\"$<TARGET_FILE:tool_better_pull_review_mcp_context_helper>\""
        )
        add_dependencies(
            better_pull_review_composition
            tool_better_pull_review_mcp_context_helper
        )
    endif()

    bpr_add_executable(
        app_better_pull_review
        main.cpp
    )
    # The bundled agent supervisor sits beside the application image on every
    # platform that has one ([[BPR-0622]] added Windows).
    if(UNIX OR WIN32)
        add_dependencies(app_better_pull_review tool_better_pull_review_agent_supervisor)
        set_property(TARGET app_better_pull_review APPEND PROPERTY LINK_DEPENDS
            "$<TARGET_FILE:tool_better_pull_review_agent_supervisor>")
        add_custom_command(TARGET app_better_pull_review POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:tool_better_pull_review_agent_supervisor>"
                "$<TARGET_FILE_DIR:app_better_pull_review>/$<TARGET_FILE_NAME:tool_better_pull_review_agent_supervisor>"
            VERBATIM)
    endif()

    if(APPLE)
        # BPR-0351. The application authenticates its stage-one supervisor
        # before QApplication exists, so the channel cannot live behind the
        # composition library the way the rest of the application does. It is
        # compiled into both graphs and both executables: the supervisor is the
        # other half of the same handshake, and a contract that links it proves
        # the halves against each other rather than against a copy.
        find_library(BPR_PROVENANCE_SECURITY_FRAMEWORK Security REQUIRED)
        find_library(
            BPR_PROVENANCE_COREFOUNDATION_FRAMEWORK
            CoreFoundation
            REQUIRED
        )
        target_sources(
            app_better_pull_review
            PRIVATE
                src/designated_live_macos_provenance.cpp
                src/designated_live_macos_capsule_identity.cpp
        )
        target_include_directories(
            app_better_pull_review
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src"
        )
        target_link_libraries(
            app_better_pull_review
            PRIVATE
                "${BPR_PROVENANCE_COREFOUNDATION_FRAMEWORK}"
                "${BPR_PROVENANCE_SECURITY_FRAMEWORK}"
                bsm
        )
    endif()

	set(
		bpr_attended_review_source_surface
		src/app/CMakeLists.txt
		src/app/main.cpp
		src/app/include/bpr/app/github_application.hpp
		src/app/src/github_application.cpp
		src/app/src/review_submission_workflow.hpp
		src/app/src/review_submission_workflow.cpp
		src/application/include/bpr/application/review_submission_application_host.hpp
		src/application/include/bpr/application/review_submission_contract.hpp
		src/application/include/bpr/application/review_submission_port.hpp
		src/application/include/bpr/application/review_submission_recovery_port.hpp
		src/application/src/review_submission_execution_authority.hpp
		src/application/src/review_submission_application_host.cpp
		src/application/src/review_submission_contract.cpp
		src/application/src/review_submission_recovery_contract.cpp
		src/adapters/github/src/live_review_submission_provider.hpp
		src/adapters/github/src/live_review_submission_provider.cpp
		src/adapters/github/src/review_submission_session.hpp
		src/adapters/github/src/review_submission_session.cpp
		src/adapters/sqlite/include/bpr/adapters/sqlite/review_submission_recovery_store.hpp
		src/core/include/bpr/core/review_submission.hpp
		src/core/src/review_submission.cpp
		src/adapters/qt/include/bpr/qt/review_submission_editor.hpp
		src/adapters/qt/src/review_submission_editor.cpp
		src/adapters/qt/include/bpr/qt/workbench_window.hpp
		src/adapters/qt/src/workbench_window.cpp
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
		src/app/src/designated_live_macos_provenance.hpp
		src/app/src/designated_live_macos_provenance.cpp
		src/app/src/designated_live_macos_capsule_identity.hpp
		src/app/src/designated_live_macos_capsule_identity.cpp
		src/app/src/designated_live_macos_supervisor_identity.hpp
		src/app/src/designated_live_macos_supervisor_identity.cpp
		src/app/src/designated_live_macos_supervisor_launch.hpp
		src/app/src/designated_live_macos_supervisor_launch.cpp
		cmake/BprDesignatedLiveStageZero.cmake
		cmake/BprTestIsolation.cmake
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
    elseif(APPLE)
        # Both macOS graphs are application bundles. macOS Notification Center
        # registers applications by bundle identity, so an unbundled process
        # cannot request authorization, never gains a System Settings entry,
        # and its delivery requests go nowhere: the attended notification
        # course binds the tests-enabled executable, and a bare Mach-O could
        # not have produced a banner for anyone to click (BPR-0392).
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
        if(BUILD_TESTING)
            # A deliberately distinct identifier and display name. Notification
            # Center keys authorization by bundle identity, so sharing the
            # shipped identifier would let a tests-enabled build grant or
            # consume the shipped application's permission. `OUTPUT_NAME` stays
            # at `bpr_add_executable`'s default, so the executable inside the
            # bundle is still `better_pull_review`: documented paths and the
            # selected-graph boundary verifier keep finding it by name, and
            # every registered test resolves it through `TARGET_FILE`.
            set_target_properties(
                app_better_pull_review
                PROPERTIES
                    MACOSX_BUNDLE TRUE
                    MACOSX_BUNDLE_BUNDLE_NAME "BetterPullReview (tests)"
                    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
                    MACOSX_BUNDLE_GUI_IDENTIFIER
                        "io.github.MikeMarcin.BetterPullReview.tests"
                    MACOSX_BUNDLE_ICON_FILE "better_pull_review.icns"
                    MACOSX_BUNDLE_INFO_PLIST
                        "${PROJECT_SOURCE_DIR}/packaging/macos/Info.plist.in"
                    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            )
        else()
            set(BPR_MACOS_CODESIGN_IDENTITY "-" CACHE STRING
                "Local Mac application signing identity; - uses ad-hoc signing")
            set_target_properties(
                app_better_pull_review
                PROPERTIES
                    OUTPUT_NAME "BetterPullReview"
                    MACOSX_BUNDLE TRUE
                    MACOSX_BUNDLE_BUNDLE_NAME "BetterPullReview"
                    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
                    MACOSX_BUNDLE_GUI_IDENTIFIER
                        "io.github.MikeMarcin.BetterPullReview"
                    MACOSX_BUNDLE_ICON_FILE "better_pull_review.icns"
                    MACOSX_BUNDLE_INFO_PLIST
                        "${PROJECT_SOURCE_DIR}/packaging/macos/Info.plist.in"
                    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            )
            # Seal the completed bundle, not only the linker's Mach-O image.
            # A stable locally configured identity also survives rebuilds for
            # Keychain trust. No timestamp service or notarization is contacted.
            add_custom_command(TARGET app_better_pull_review POST_BUILD
                COMMAND /usr/bin/codesign --force --deep
                    --sign "${BPR_MACOS_CODESIGN_IDENTITY}" --timestamp=none
                    "$<TARGET_BUNDLE_DIR:app_better_pull_review>"
                VERBATIM)
        endif()
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
    # An MSVC-linked image carries no symbol table, so the composed-host
    # gate reads the linker's map of the image instead: every public and
    # static symbol the linker kept, beside the executable as <name>.map
    # (BPR-0364). Nothing else consumes it.
    if(MSVC)
        target_link_options(app_better_pull_review PRIVATE /MAP)
    endif()

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
        include("${PROJECT_SOURCE_DIR}/cmake/BprDesignatedLiveStageZero.cmake")
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
        file(
            SHA256
            "${PROJECT_SOURCE_DIR}/scripts/designated_live_macos_capsule.py"
            BPR_DESIGNATED_LIVE_VERIFIER_SHA256
        )
        bpr_add_executable(
            tool_better_pull_review_designated_live_supervisor
            src/designated_live_macos_supervisor.cpp
            src/designated_live_macos_provenance.cpp
            src/designated_live_macos_capsule_identity.cpp
            src/designated_live_macos_supervisor_identity.cpp
            src/designated_live_macos_supervisor_launch.cpp
        )
        target_link_libraries(
            tool_better_pull_review_designated_live_supervisor
            PRIVATE
                BetterPullReview::DesignatedLivePublicObserver
                "${BPR_DESIGNATED_LIVE_COREFOUNDATION_FRAMEWORK}"
                "${BPR_DESIGNATED_LIVE_SECURITY_FRAMEWORK}"
                bsm
        )
        target_compile_definitions(
            tool_better_pull_review_designated_live_supervisor
            PRIVATE
                "BPR_ATTENDED_REVIEW_SOURCE_SHA256=\"${BPR_ATTENDED_REVIEW_SOURCE_SHA256}\""
                "BPR_DESIGNATED_LIVE_VERIFIER_SHA256=\"${BPR_DESIGNATED_LIVE_VERIFIER_SHA256}\""
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
        set(
            bpr_designated_live_capsule_image
            "${bpr_designated_live_capsule_parent}/capsule.dmg"
        )
        set(
            bpr_designated_live_capsule_image_digest
            "${bpr_designated_live_capsule_parent}/capsule.dmg.sha256"
        )
        set(
            bpr_designated_live_source_manifest
            "${bpr_designated_live_capsule_parent}/source-manifest.txt"
        )
        set(
            bpr_designated_live_probe_mount_parent
            "${bpr_designated_live_capsule_parent}/probe-mounts"
        )
        file(
            GENERATE
            OUTPUT "${bpr_designated_live_source_manifest}"
            CONTENT "${bpr_attended_review_source_manifest}"
        )
        set(
            bpr_designated_live_producer_loader_command
            "$<TARGET_FILE:Python3::Interpreter>"
            -I
            -S
            -c
            "${BPR_DESIGNATED_LIVE_STAGE_ZERO_PRELOADER}"
            "${PROJECT_SOURCE_DIR}/scripts/designated_live_macos_capsule.py"
            "${BPR_DESIGNATED_LIVE_VERIFIER_SHA256}"
        )
        bpr_designated_live_stage_zero_command(
            bpr_designated_live_probe_command
            PROBE
            SCRIPT "${PROJECT_SOURCE_DIR}/scripts/designated_live_macos_capsule.py"
            SCRIPT_SHA256 "${BPR_DESIGNATED_LIVE_VERIFIER_SHA256}"
            IMAGE "${bpr_designated_live_capsule_image}"
            EXPECTED_IMAGE_SHA256_FILE "${bpr_designated_live_capsule_image_digest}"
            MOUNT_PARENT "${bpr_designated_live_probe_mount_parent}"
            SOURCE_SHA256 "${BPR_ATTENDED_REVIEW_SOURCE_SHA256}"
        )
        add_custom_command(
            OUTPUT
                "${bpr_designated_live_capsule_stamp}"
                "${bpr_designated_live_capsule_inventory}"
                "${bpr_designated_live_capsule_image}"
                "${bpr_designated_live_capsule_image_digest}"
            COMMAND
                "${CMAKE_COMMAND}" -E remove_directory
                "${bpr_designated_live_capsule_root}"
            COMMAND
                "${CMAKE_COMMAND}" -E rm -f
                "${bpr_designated_live_capsule_image}"
                "${bpr_designated_live_capsule_image_digest}"
            COMMAND
                ${bpr_designated_live_producer_loader_command}
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
                ${bpr_designated_live_producer_loader_command}
                image
                --capsule "${bpr_designated_live_capsule_root}"
                --inventory "${bpr_designated_live_capsule_inventory}"
                --output "${bpr_designated_live_capsule_image}"
                --image-digest "${bpr_designated_live_capsule_image_digest}"
                --source-sha256 "${BPR_ATTENDED_REVIEW_SOURCE_SHA256}"
            COMMAND
                ${bpr_designated_live_probe_command}
            COMMAND
                "${CMAKE_COMMAND}" -E touch
                "${bpr_designated_live_capsule_stamp}"
            DEPENDS
                app_better_pull_review
                tool_better_pull_review_designated_live_supervisor
                "${PROJECT_SOURCE_DIR}/scripts/designated_live_macos_capsule.py"
                "${PROJECT_SOURCE_DIR}/cmake/BprDesignatedLiveStageZero.cmake"
                "${PROJECT_SOURCE_DIR}/cmake/BprTestIsolation.cmake"
                "${bpr_designated_live_source_manifest}"
            VERBATIM
        )
        add_custom_target(
            designated_live_macos_capsule
            DEPENDS
                "${bpr_designated_live_capsule_stamp}"
                "${bpr_designated_live_capsule_inventory}"
                "${bpr_designated_live_capsule_image}"
                "${bpr_designated_live_capsule_image_digest}"
                "${bpr_designated_live_source_manifest}"
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
        set_property(
            TARGET designated_live_macos_capsule
            PROPERTY BPR_DESIGNATED_LIVE_CAPSULE_IMAGE
                "${bpr_designated_live_capsule_image}"
        )
        set_property(
            TARGET designated_live_macos_capsule
            PROPERTY BPR_DESIGNATED_LIVE_CAPSULE_IMAGE_DIGEST
                "${bpr_designated_live_capsule_image_digest}"
        )
        set_property(
            TARGET designated_live_macos_capsule
            PROPERTY BPR_DESIGNATED_LIVE_SOURCE_MANIFEST
                "${bpr_designated_live_source_manifest}"
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
