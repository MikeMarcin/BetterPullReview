if(NOT DEFINED BPR_ROOT)
    message(FATAL_ERROR "BPR_ROOT is required")
endif()

set(github_adapter_cmake "${BPR_ROOT}/src/adapters/github/CMakeLists.txt")
set(adapters_cmake "${BPR_ROOT}/src/adapters/CMakeLists.txt")
set(
    github_process_runner_relative
    "src/adapters/github/src/github_cli_process_runner.cpp"
)
set(
    github_native_https_relative
    "src/adapters/github/src/native_https_transport.cpp"
)
set(
    github_cli_transport_relative
    "src/adapters/github/src/gh_cli_transport.cpp"
)

if(NOT EXISTS "${github_adapter_cmake}")
    message(FATAL_ERROR "GitHub adapter must own a dedicated CMake target")
endif()

file(READ "${adapters_cmake}" adapters_cmake_contents)
if(
    NOT adapters_cmake_contents
        MATCHES "add_subdirectory\\([ \t\r\n]*github[ \t\r\n]*\\)"
)
    message(FATAL_ERROR "Adapter root must delegate to the GitHub subdirectory")
endif()

file(READ "${github_adapter_cmake}" github_cmake_contents)
set(
    github_target_patterns
    "add_library\\([ \t\r\n]*better_pull_review_adapter_github([ \t\r\n]|\\))"
    "add_library\\([ \t\r\n]*BetterPullReview::AdapterGitHub[ \t\r\n]+ALIAS[ \t\r\n]+better_pull_review_adapter_github[ \t\r\n]*\\)"
    "target_link_libraries\\([ \t\r\n]*better_pull_review_adapter_github"
    "BetterPullReview::Application"
)
foreach(pattern IN LISTS github_target_patterns)
    if(NOT github_cmake_contents MATCHES "${pattern}")
        message(
            FATAL_ERROR
            "GitHub adapter target contract is incomplete: ${pattern}"
        )
    endif()
endforeach()

file(
    GLOB_RECURSE project_cmake_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/CMakeLists.txt"
    "${BPR_ROOT}/src/*/CMakeLists.txt"
)
foreach(project_cmake_file IN LISTS project_cmake_files)
    if(project_cmake_file STREQUAL github_adapter_cmake)
        continue()
    endif()

    file(READ "${project_cmake_file}" contents)
    if(
        contents
            MATCHES
                "add_library\\([ \t\r\n]*(better_pull_review_adapter_github|BetterPullReview::AdapterGitHub)([ \t\r\n]|\\))"
    )
        file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${project_cmake_file}")
        message(
            FATAL_ERROR
            "GitHub adapter target must be declared only in "
            "src/adapters/github/CMakeLists.txt, not ${relative_file}"
        )
    endif()
endforeach()

file(
    GLOB_RECURSE effect_free_region_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/src/core/*.c"
    "${BPR_ROOT}/src/core/*.cc"
    "${BPR_ROOT}/src/core/*.cpp"
    "${BPR_ROOT}/src/core/*.cxx"
    "${BPR_ROOT}/src/core/*.h"
    "${BPR_ROOT}/src/core/*.hh"
    "${BPR_ROOT}/src/core/*.hpp"
    "${BPR_ROOT}/src/core/*.hxx"
    "${BPR_ROOT}/src/core/*.m"
    "${BPR_ROOT}/src/core/*.mm"
    "${BPR_ROOT}/src/application/*.c"
    "${BPR_ROOT}/src/application/*.cc"
    "${BPR_ROOT}/src/application/*.cpp"
    "${BPR_ROOT}/src/application/*.cxx"
    "${BPR_ROOT}/src/application/*.h"
    "${BPR_ROOT}/src/application/*.hh"
    "${BPR_ROOT}/src/application/*.hpp"
    "${BPR_ROOT}/src/application/*.hxx"
    "${BPR_ROOT}/src/application/*.m"
    "${BPR_ROOT}/src/application/*.mm"
    "${BPR_ROOT}/src/adapters/qt/*.cc"
    "${BPR_ROOT}/src/adapters/qt/*.cpp"
    "${BPR_ROOT}/src/adapters/qt/*.cxx"
    "${BPR_ROOT}/src/adapters/qt/*.h"
    "${BPR_ROOT}/src/adapters/qt/*.hh"
    "${BPR_ROOT}/src/adapters/qt/*.hpp"
    "${BPR_ROOT}/src/adapters/qt/*.hxx"
    "${BPR_ROOT}/src/adapters/qt/*.mm"
)
set(
    effect_free_region_patterns
    "QProcess"
    "QNetwork[A-Za-z0-9_]*"
)
foreach(effect_free_file IN LISTS effect_free_region_files)
    file(READ "${effect_free_file}" contents)
    foreach(pattern IN LISTS effect_free_region_patterns)
        if(contents MATCHES "${pattern}")
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${effect_free_file}")
            message(
                FATAL_ERROR
                "Core/application/Qt effect-boundary violation in "
                "${relative_file}: ${pattern}"
            )
        endif()
    endforeach()
endforeach()

file(
    GLOB_RECURSE qt_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/src/adapters/qt/*.cc"
    "${BPR_ROOT}/src/adapters/qt/*.cpp"
    "${BPR_ROOT}/src/adapters/qt/*.cxx"
    "${BPR_ROOT}/src/adapters/qt/*.h"
    "${BPR_ROOT}/src/adapters/qt/*.hh"
    "${BPR_ROOT}/src/adapters/qt/*.hpp"
    "${BPR_ROOT}/src/adapters/qt/*.hxx"
    "${BPR_ROOT}/src/adapters/qt/*.mm"
)
set(
    qt_forbidden_patterns
    "QNetwork"
    "QProcess"
    "QTcp"
    "QUdp"
    "QLocalServer"
    "QPluginLoader"
    "QLibrary"
    "QDesktopServices"
    "QUrl"
    "sqlite3"
    "bpr/adapters/(github|git|sqlite|ai)"
)

foreach(qt_file IN LISTS qt_files)
    file(READ "${qt_file}" contents)
    foreach(pattern IN LISTS qt_forbidden_patterns)
        if(contents MATCHES "${pattern}")
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${qt_file}")
            message(
                FATAL_ERROR
                "Qt adapter effect-boundary violation in ${relative_file}: ${pattern}"
            )
        endif()
    endforeach()
endforeach()

file(
    GLOB_RECURSE source_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/src/*.c"
    "${BPR_ROOT}/src/*.cc"
    "${BPR_ROOT}/src/*.cpp"
    "${BPR_ROOT}/src/*.cxx"
    "${BPR_ROOT}/src/*.h"
    "${BPR_ROOT}/src/*.hh"
    "${BPR_ROOT}/src/*.hpp"
    "${BPR_ROOT}/src/*.hxx"
    "${BPR_ROOT}/src/*.m"
    "${BPR_ROOT}/src/*.mm"
)

function(verify_effect_owner effect_pattern effect_name allowed_relative_file)
    get_filename_component(
        allowed_file
        "${BPR_ROOT}/${allowed_relative_file}"
        ABSOLUTE
    )

    foreach(source_file IN LISTS source_files)
        file(READ "${source_file}" contents)
        if(NOT contents MATCHES "${effect_pattern}")
            continue()
        endif()

        get_filename_component(normalized_source_file "${source_file}" ABSOLUTE)
        if(NOT normalized_source_file STREQUAL allowed_file)
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${source_file}")
            message(
                FATAL_ERROR
                "${effect_name} ownership violation in ${relative_file}; "
                "only ${allowed_relative_file} may use it"
            )
        endif()
    endforeach()

    if(EXISTS "${allowed_file}")
        get_filename_component(allowed_name "${allowed_file}" NAME)
        if(NOT github_cmake_contents MATCHES "${allowed_name}")
            message(
                FATAL_ERROR
                "Designated GitHub effect source ${allowed_name} is not owned "
                "by better_pull_review_adapter_github"
            )
        endif()
    endif()
endfunction()

verify_effect_owner(
    "QProcess"
    "QProcess"
    "${github_process_runner_relative}"
)
verify_effect_owner(
    "QNetworkAccessManager"
    "QNetworkAccessManager"
    "${github_native_https_relative}"
)
verify_effect_owner(
    "GH_TELEMETRY=0"
    "GitHub CLI telemetry hardening"
    "${github_cli_transport_relative}"
)

file(
    GLOB_RECURSE runtime_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/CMakeLists.txt"
    "${BPR_ROOT}/cmake/*.cmake"
    "${BPR_ROOT}/src/*/CMakeLists.txt"
    "${BPR_ROOT}/src/*.c"
    "${BPR_ROOT}/src/*.cc"
    "${BPR_ROOT}/src/*.cpp"
    "${BPR_ROOT}/src/*.cxx"
    "${BPR_ROOT}/src/*.h"
    "${BPR_ROOT}/src/*.hh"
    "${BPR_ROOT}/src/*.hpp"
    "${BPR_ROOT}/src/*.hxx"
    "${BPR_ROOT}/src/*.m"
    "${BPR_ROOT}/src/*.mm"
)
set(
    runtime_forbidden_patterns
    "WebEngine"
    "Chromium"
    "Electron"
    "Tauri"
    "QTcpServer"
    "QUdpSocket"
    "QLocalServer"
    "QPluginLoader"
    "QWeb"
    "listen[ \t]*\\("
    "auto[_-]?update"
)

foreach(runtime_file IN LISTS runtime_files)
    if(runtime_file STREQUAL CMAKE_CURRENT_LIST_FILE)
        continue()
    endif()

    file(READ "${runtime_file}" contents)
    foreach(pattern IN LISTS runtime_forbidden_patterns)
        if(contents MATCHES "${pattern}")
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${runtime_file}")
            message(
                FATAL_ERROR
                "Forbidden runtime surface in ${relative_file}: ${pattern}"
            )
        endif()
    endforeach()
endforeach()

set(telemetry_pattern "[Tt][Ee][Ll][Ee][Mm][Ee][Tt][Rr][Yy]")
get_filename_component(
    github_cli_transport
    "${BPR_ROOT}/${github_cli_transport_relative}"
    ABSOLUTE
)
foreach(runtime_file IN LISTS runtime_files)
    if(runtime_file STREQUAL CMAKE_CURRENT_LIST_FILE)
        continue()
    endif()

    file(READ "${runtime_file}" contents)
    if(NOT contents MATCHES "${telemetry_pattern}")
        continue()
    endif()

    get_filename_component(normalized_runtime_file "${runtime_file}" ABSOLUTE)
    if(normalized_runtime_file STREQUAL github_cli_transport)
        string(
            REPLACE
            "GH_TELEMETRY=0"
            ""
            contents_without_hardening
            "${contents}"
        )
        if(NOT contents_without_hardening MATCHES "${telemetry_pattern}")
            continue()
        endif()
    endif()

    file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${runtime_file}")
    message(
        FATAL_ERROR
        "Forbidden telemetry surface in ${relative_file}; only the exact "
        "GH_TELEMETRY=0 broker hardening setting is allowed"
    )
endforeach()

if(EXISTS "${github_cli_transport}")
    file(READ "${github_cli_transport}" github_cli_transport_contents)
    if(NOT github_cli_transport_contents MATCHES "GH_TELEMETRY=0")
        message(
            FATAL_ERROR
            "The designated GitHub CLI transport must set GH_TELEMETRY=0"
        )
    endif()
endif()

message(STATUS "Application, provider, and Qt architecture boundaries verified")
