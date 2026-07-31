if(NOT DEFINED BPR_ROOT)
    message(FATAL_ERROR "BPR_ROOT is required")
endif()

set(github_adapter_cmake "${BPR_ROOT}/src/adapters/github/CMakeLists.txt")
set(git_adapter_cmake "${BPR_ROOT}/src/adapters/git/CMakeLists.txt")
set(adapters_cmake "${BPR_ROOT}/src/adapters/CMakeLists.txt")
set(
    github_process_runner_relative
    "src/adapters/github/src/github_cli_process_runner.cpp"
)
set(
    github_process_interface_relative
    "src/adapters/github/src/github_cli_process.hpp"
)
set(
    github_process_runner_interface_relative
    "src/adapters/github/src/github_cli_process_runner.hpp"
)
set(
    github_native_https_relative
    "src/adapters/github/src/native_https_transport.cpp"
)
set(
    github_native_https_interface_relative
    "src/adapters/github/src/native_https_transport.hpp"
)
set(
    github_wire_validation_relative
    "src/adapters/github/src/github_wire_request_validation.cpp"
)
set(
    github_wire_validation_interface_relative
    "src/adapters/github/src/github_wire_request_validation.hpp"
)
set(
    github_cli_transport_relative
    "src/adapters/github/src/gh_cli_transport.cpp"
)
set(
    macos_keychain_relative
    "src/adapters/github/src/macos_keychain.cpp"
)
set(
    windows_credential_manager_relative
    "src/adapters/github/src/windows_credential_manager.cpp"
)
set(
    linux_secret_service_relative
    "src/adapters/github/src/linux_secret_service.cpp"
)
set(
    git_process_runner_relative
    "src/adapters/git/src/installed_git_process.cpp"
)
set(
    git_process_port_relative
    "src/application/include/bpr/application/git_process_port.hpp"
)
set(
    git_process_factory_relative
    "src/adapters/git/include/bpr/adapters/git/installed_git_process.hpp"
)

if(NOT EXISTS "${github_adapter_cmake}")
    message(FATAL_ERROR "GitHub adapter must own a dedicated CMake target")
endif()
if(NOT EXISTS "${git_adapter_cmake}")
    message(FATAL_ERROR "Git adapter must own a dedicated CMake target")
endif()

file(READ "${adapters_cmake}" adapters_cmake_contents)
if(
    NOT adapters_cmake_contents
        MATCHES "add_subdirectory\\([ \t\r\n]*github[ \t\r\n]*\\)"
)
    message(FATAL_ERROR "Adapter root must delegate to the GitHub subdirectory")
endif()
if(
    NOT adapters_cmake_contents
        MATCHES "add_subdirectory\\([ \t\r\n]*git[ \t\r\n]*\\)"
)
    message(FATAL_ERROR "Adapter root must delegate to the Git subdirectory")
endif()

file(READ "${git_adapter_cmake}" git_cmake_contents)
set(
    git_target_patterns
    "add_library\\([ \t\r\n]*better_pull_review_adapter_git([ \t\r\n]|\\))"
    "add_library\\([ \t\r\n]*BetterPullReview::AdapterGit[ \t\r\n]+ALIAS[ \t\r\n]+better_pull_review_adapter_git[ \t\r\n]*\\)"
    "src/installed_git_process\\.cpp"
    "BetterPullReview::Application"
    "Qt6::Core"
    "Threads::Threads"
)
foreach(pattern IN LISTS git_target_patterns)
    if(NOT git_cmake_contents MATCHES "${pattern}")
        message(
            FATAL_ERROR
            "Installed-Git adapter target contract is incomplete: ${pattern}"
        )
    endif()
endforeach()
if(git_cmake_contents MATCHES "Qt6::Network")
    message(FATAL_ERROR "Installed-Git adapter must not link Qt Network")
endif()
if(
    git_cmake_contents MATCHES "BPR_[A-Z0-9_]*TEST"
    OR git_cmake_contents MATCHES "BPR_BUILD_TESTS"
)
    message(
        FATAL_ERROR
        "Installed-Git adapter must not compile a test-only production seam"
    )
endif()
foreach(
    required_git_file_relative
    IN ITEMS
        "${git_process_runner_relative}"
        "${git_process_port_relative}"
        "${git_process_factory_relative}"
)
    if(NOT EXISTS "${BPR_ROOT}/${required_git_file_relative}")
        message(
            FATAL_ERROR
            "Required installed-Git boundary file is missing: "
            "${required_git_file_relative}"
        )
    endif()
endforeach()

file(READ "${github_adapter_cmake}" github_cmake_contents)
string(
    REGEX REPLACE
    "[ \t\r\n]+"
    " "
    github_cmake_compact
    "${github_cmake_contents}"
)
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

if(
    NOT github_cmake_contents
        MATCHES
            "find_package[ \t\r\n]*\\([ \t\r\n]*Threads[ \t\r\n]+REQUIRED[ \t\r\n]*\\)"
)
    message(
        FATAL_ERROR
        "GitHub adapter must resolve the required CMake Threads package"
    )
endif()
if(
    NOT github_cmake_contents
        MATCHES
            "target_link_libraries[ \t\r\n]*\\([ \t\r\n]*better_pull_review_adapter_github[^\\)]*PRIVATE[^\\)]*Threads::Threads[^\\)]*\\)"
)
    message(
        FATAL_ERROR
        "GitHub adapter must link Threads::Threads explicitly and privately"
    )
endif()

foreach(
    required_github_host_file_relative
    IN ITEMS
        "${github_process_interface_relative}"
        "${github_process_runner_interface_relative}"
        "${github_process_runner_relative}"
        "${github_cli_transport_relative}"
        "${github_native_https_interface_relative}"
        "${github_native_https_relative}"
        "${github_wire_validation_interface_relative}"
        "${github_wire_validation_relative}"
)
    get_filename_component(
        required_github_host_file
        "${BPR_ROOT}/${required_github_host_file_relative}"
        ABSOLUTE
    )
    if(NOT EXISTS "${required_github_host_file}")
        message(
            FATAL_ERROR
            "The designated GitHub host adapter file is required: "
            "${required_github_host_file_relative}"
        )
    endif()
endforeach()

function(require_github_target_source required_source_relative)
    if(
        NOT github_cmake_contents
            MATCHES
                "(add_library|target_sources)[ \t\r\n]*\\([ \t\r\n]*better_pull_review_adapter_github[^\\)]*${required_source_relative}"
    )
        message(
            FATAL_ERROR
            "GitHub host adapter source must be explicitly owned by "
            "better_pull_review_adapter_github: ${required_source_relative}"
        )
    endif()
endfunction()

require_github_target_source("src/github_cli_process_runner\\.cpp")
require_github_target_source("src/gh_cli_transport\\.cpp")
require_github_target_source("src/github_wire_request_validation\\.cpp")
require_github_target_source("src/native_https_transport\\.cpp")

if(
    NOT github_cmake_contents
        MATCHES
            "find_package[ \t\r\n]*\\([ \t\r\n]*Qt6[^\\)]*COMPONENTS[^\\)]*Network[^\\)]*\\)"
)
    message(
        FATAL_ERROR
        "GitHub native HTTPS requires the Qt6 Network component"
    )
endif()
if(
    NOT github_cmake_contents
        MATCHES
            "target_link_libraries[ \t\r\n]*\\([ \t\r\n]*better_pull_review_adapter_github[^\\)]*PRIVATE[^\\)]*Qt6::Network[^\\)]*\\)"
)
    message(
        FATAL_ERROR
        "GitHub native HTTPS must link Qt6::Network privately"
    )
endif()
if(
    NOT github_cmake_compact
        MATCHES
            "if\\(BPR_BUILD_TESTS\\) target_compile_definitions\\( better_pull_review_adapter_github PRIVATE BPR_NATIVE_HTTPS_TESTING=1 \\) endif\\(\\)"
)
    message(
        FATAL_ERROR
        "Native HTTPS fixture seams must compile privately and only in "
        "test-enabled builds"
    )
endif()

get_filename_component(
    macos_keychain_file
    "${BPR_ROOT}/${macos_keychain_relative}"
    ABSOLUTE
)
if(NOT EXISTS "${macos_keychain_file}")
    message(
        FATAL_ERROR
        "The designated macOS Keychain source is required: "
        "${macos_keychain_relative}"
    )
endif()

get_filename_component(
    windows_credential_manager_file
    "${BPR_ROOT}/${windows_credential_manager_relative}"
    ABSOLUTE
)
if(NOT EXISTS "${windows_credential_manager_file}")
    message(
        FATAL_ERROR
        "The designated Windows Credential Manager source is required: "
        "${windows_credential_manager_relative}"
    )
endif()

get_filename_component(
    linux_secret_service_file
    "${BPR_ROOT}/${linux_secret_service_relative}"
    ABSOLUTE
)
if(NOT EXISTS "${linux_secret_service_file}")
    message(
        FATAL_ERROR
        "The designated Linux Secret Service source is required: "
        "${linux_secret_service_relative}"
    )
endif()

file(STRINGS "${github_adapter_cmake}" github_cmake_lines)
set(github_apple_branch_contents "")
set(in_github_apple_branch FALSE)
set(github_apple_branch_depth 0)
foreach(github_cmake_line IN LISTS github_cmake_lines)
    set(started_github_apple_branch FALSE)
    if(
        NOT in_github_apple_branch
        AND github_cmake_line
            MATCHES "^[ \t]*if[ \t]*\\([ \t]*APPLE[ \t]*\\)"
    )
        set(in_github_apple_branch TRUE)
        set(started_github_apple_branch TRUE)
        set(github_apple_branch_depth 1)
    endif()

    if(NOT in_github_apple_branch)
        continue()
    endif()

    string(APPEND github_apple_branch_contents "${github_cmake_line}\n")
    if(
        NOT started_github_apple_branch
        AND github_cmake_line MATCHES "^[ \t]*if[ \t]*\\("
    )
        math(EXPR github_apple_branch_depth "${github_apple_branch_depth} + 1")
    endif()
    if(github_cmake_line MATCHES "^[ \t]*endif[ \t]*\\(")
        math(EXPR github_apple_branch_depth "${github_apple_branch_depth} - 1")
        if(github_apple_branch_depth EQUAL 0)
            set(in_github_apple_branch FALSE)
        endif()
    endif()
endforeach()

if(github_apple_branch_contents STREQUAL "")
    message(
        FATAL_ERROR
        "GitHub adapter CMake must define an APPLE branch for Keychain"
    )
endif()
if(
    NOT github_apple_branch_contents
        MATCHES
            "target_sources[ \t\r\n]*\\([^\\)]*better_pull_review_adapter_github[^\\)]*macos_keychain\\.cpp"
)
    message(
        FATAL_ERROR
        "Apple GitHub adapter must compile src/macos_keychain.cpp"
    )
endif()

string(
    REGEX MATCH
    "find_library[ \t\r\n]*\\([ \t\r\n]*([A-Za-z_][A-Za-z0-9_]*)[ \t\r\n]+Security[ \t\r\n]+REQUIRED[ \t\r\n]*\\)"
    github_security_find
    "${github_apple_branch_contents}"
)
set(github_security_framework_variable "${CMAKE_MATCH_1}")
if(github_security_find STREQUAL "")
    message(
        FATAL_ERROR
        "Apple GitHub adapter must find Security with REQUIRED"
    )
endif()

string(
    REGEX MATCH
    "find_library[ \t\r\n]*\\([ \t\r\n]*([A-Za-z_][A-Za-z0-9_]*)[ \t\r\n]+CoreFoundation[ \t\r\n]+REQUIRED[ \t\r\n]*\\)"
    github_core_foundation_find
    "${github_apple_branch_contents}"
)
set(github_core_foundation_framework_variable "${CMAKE_MATCH_1}")
if(github_core_foundation_find STREQUAL "")
    message(
        FATAL_ERROR
        "Apple GitHub adapter must find CoreFoundation with REQUIRED"
    )
endif()

string(
    REGEX MATCH
    "target_link_libraries[ \t\r\n]*\\([^\\)]*better_pull_review_adapter_github[^\\)]*\\)"
    github_apple_target_links
    "${github_apple_branch_contents}"
)
if(
    github_apple_target_links STREQUAL ""
    OR NOT github_apple_target_links
        MATCHES "${github_security_framework_variable}"
)
    message(
        FATAL_ERROR
        "Apple GitHub adapter must link the required Security framework"
    )
endif()
if(
    github_apple_target_links STREQUAL ""
    OR NOT github_apple_target_links
        MATCHES "${github_core_foundation_framework_variable}"
)
    message(
        FATAL_ERROR
        "Apple GitHub adapter must link the required CoreFoundation framework"
    )
endif()

set(github_win32_branch_contents "")
set(in_github_win32_branch FALSE)
set(github_win32_branch_depth 0)
foreach(github_cmake_line IN LISTS github_cmake_lines)
    set(started_github_win32_branch FALSE)
    if(
        NOT in_github_win32_branch
        AND github_cmake_line
            MATCHES "^[ \t]*if[ \t]*\\([ \t]*WIN32[ \t]*\\)"
    )
        set(in_github_win32_branch TRUE)
        set(started_github_win32_branch TRUE)
        set(github_win32_branch_depth 1)
    endif()

    if(NOT in_github_win32_branch)
        continue()
    endif()

    string(APPEND github_win32_branch_contents "${github_cmake_line}\n")
    if(
        NOT started_github_win32_branch
        AND github_cmake_line MATCHES "^[ \t]*if[ \t]*\\("
    )
        math(EXPR github_win32_branch_depth "${github_win32_branch_depth} + 1")
    endif()
    if(github_cmake_line MATCHES "^[ \t]*endif[ \t]*\\(")
        math(EXPR github_win32_branch_depth "${github_win32_branch_depth} - 1")
        if(github_win32_branch_depth EQUAL 0)
            set(in_github_win32_branch FALSE)
        endif()
    endif()
endforeach()

if(github_win32_branch_contents STREQUAL "")
    message(
        FATAL_ERROR
        "GitHub adapter CMake must define a WIN32 branch for "
        "Windows Credential Manager"
    )
endif()
if(
    NOT github_win32_branch_contents
        MATCHES
            "target_sources[ \t\r\n]*\\([^\\)]*better_pull_review_adapter_github[^\\)]*windows_credential_manager\\.cpp"
)
    message(
        FATAL_ERROR
        "Windows GitHub adapter must compile "
        "src/windows_credential_manager.cpp"
    )
endif()

string(
    REGEX MATCH
    "target_link_libraries[ \t\r\n]*\\([^\\)]*better_pull_review_adapter_github[^\\)]*\\)"
    github_win32_target_links
    "${github_win32_branch_contents}"
)
if(
    github_win32_target_links STREQUAL ""
    OR NOT github_win32_target_links
        MATCHES
            "(^|[ \t\r\n])Advapi32([ \t\r\n]|\\))"
)
    message(
        FATAL_ERROR
        "Windows GitHub adapter must link Advapi32"
    )
endif()

set(github_linux_branch_contents "")
set(github_non_linux_branch_contents "")
set(in_github_linux_branch FALSE)
set(github_linux_branch_depth 0)
foreach(github_cmake_line IN LISTS github_cmake_lines)
    set(started_github_linux_branch FALSE)
    if(
        NOT in_github_linux_branch
        AND github_cmake_line
            MATCHES
                "^[ \t]*if[ \t]*\\([ \t]*CMAKE_SYSTEM_NAME[ \t]+STREQUAL[ \t]+\"Linux\"[ \t]*\\)"
    )
        set(in_github_linux_branch TRUE)
        set(started_github_linux_branch TRUE)
        set(github_linux_branch_depth 1)
    endif()

    if(in_github_linux_branch)
        string(APPEND github_linux_branch_contents "${github_cmake_line}\n")
        if(
            NOT started_github_linux_branch
            AND github_cmake_line MATCHES "^[ \t]*if[ \t]*\\("
        )
            math(
                EXPR
                github_linux_branch_depth
                "${github_linux_branch_depth} + 1"
            )
        endif()
        if(github_cmake_line MATCHES "^[ \t]*endif[ \t]*\\(")
            math(
                EXPR
                github_linux_branch_depth
                "${github_linux_branch_depth} - 1"
            )
            if(github_linux_branch_depth EQUAL 0)
                set(in_github_linux_branch FALSE)
            endif()
        endif()
        continue()
    endif()

    string(APPEND github_non_linux_branch_contents "${github_cmake_line}\n")
endforeach()

if(github_linux_branch_contents STREQUAL "")
    message(
        FATAL_ERROR
        "GitHub adapter CMake must define an exact Linux branch for "
        "Secret Service"
    )
endif()
if(
    NOT github_linux_branch_contents
        MATCHES
            "target_sources[ \t\r\n]*\\([^\\)]*better_pull_review_adapter_github[^\\)]*linux_secret_service\\.cpp"
)
    message(
        FATAL_ERROR
        "Linux GitHub adapter must compile src/linux_secret_service.cpp"
    )
endif()
if(
    NOT github_linux_branch_contents
        MATCHES
            "find_package[ \t\r\n]*\\([ \t\r\n]*PkgConfig[ \t\r\n]+REQUIRED[ \t\r\n]*\\)"
)
    message(
        FATAL_ERROR
        "Linux GitHub adapter must require PkgConfig"
    )
endif()

string(
    REGEX MATCH
    "pkg_check_modules[ \t\r\n]*\\([ \t\r\n]*([A-Za-z_][A-Za-z0-9_]*)[ \t\r\n]+REQUIRED[ \t\r\n]+IMPORTED_TARGET[ \t\r\n]+libsecret-1>=0\\.20\\.0[ \t\r\n]*\\)"
    github_libsecret_pkg_check
    "${github_linux_branch_contents}"
)
set(github_libsecret_pkg_variable "${CMAKE_MATCH_1}")
if(github_libsecret_pkg_check STREQUAL "")
    message(
        FATAL_ERROR
        "Linux GitHub adapter must import required libsecret-1>=0.20.0"
    )
endif()

string(
    REGEX MATCH
    "target_link_libraries[ \t\r\n]*\\([^\\)]*better_pull_review_adapter_github[^\\)]*\\)"
    github_linux_target_links
    "${github_linux_branch_contents}"
)
if(
    github_linux_target_links STREQUAL ""
    OR NOT github_linux_target_links
        MATCHES "PkgConfig::${github_libsecret_pkg_variable}"
)
    message(
        FATAL_ERROR
        "Linux GitHub adapter must link the imported libsecret target"
    )
endif()

if(
    github_non_linux_branch_contents
        MATCHES
            "(linux_secret_service\\.cpp|libsecret-1|PkgConfig::${github_libsecret_pkg_variable})"
)
    message(
        FATAL_ERROR
        "Linux Secret Service source and dependency must remain Linux-only"
    )
endif()

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
    if(
        contents
            MATCHES
                "(linux_secret_service\\.cpp|PkgConfig::${github_libsecret_pkg_variable})"
    )
        file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${project_cmake_file}")
        message(
            FATAL_ERROR
            "Linux Secret Service build ownership must remain in "
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
    "QJson[A-Za-z0-9_]*"
    "#[ \t]*include[ \t]*[<\"](curl|nlohmann|rapidjson|boost/json)"
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
    GLOB_RECURSE standard_cpp_region_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/src/core/*.c"
    "${BPR_ROOT}/src/core/*.cc"
    "${BPR_ROOT}/src/core/*.cpp"
    "${BPR_ROOT}/src/core/*.cxx"
    "${BPR_ROOT}/src/core/*.h"
    "${BPR_ROOT}/src/core/*.hh"
    "${BPR_ROOT}/src/core/*.hpp"
    "${BPR_ROOT}/src/core/*.hxx"
    "${BPR_ROOT}/src/application/*.c"
    "${BPR_ROOT}/src/application/*.cc"
    "${BPR_ROOT}/src/application/*.cpp"
    "${BPR_ROOT}/src/application/*.cxx"
    "${BPR_ROOT}/src/application/*.h"
    "${BPR_ROOT}/src/application/*.hh"
    "${BPR_ROOT}/src/application/*.hpp"
    "${BPR_ROOT}/src/application/*.hxx"
)
set(
    standard_cpp_region_patterns
    "#[ \t]*include[ \t]*[<\"]Q"
    "#[ \t]*include[ \t]*[<\"]Qt"
    "QString"
    "QByteArray"
    "QVariant"
    "QUrl"
    "(adapters/github|github_read_contract)"
    "api\\.github\\.com"
    "/api/v3"
    "Authorization[ \t]*:"
    "Bearer[ \t]+"
    "node_id[^\r\n]*:"
)
foreach(standard_cpp_file IN LISTS standard_cpp_region_files)
    file(READ "${standard_cpp_file}" contents)
    foreach(pattern IN LISTS standard_cpp_region_patterns)
        if(contents MATCHES "${pattern}")
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${standard_cpp_file}")
            message(
                FATAL_ERROR
                "Core/application standard-C++ boundary violation in "
                "${relative_file}: ${pattern}"
            )
        endif()
    endforeach()
endforeach()

file(
    GLOB_RECURSE qt_github_projection_files
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
    qt_github_projection_forbidden_patterns
    "QNetwork[A-Za-z0-9_]*"
    "QProcess[A-Za-z0-9_]*"
    "QJson[A-Za-z0-9_]*"
    "QUrl"
    "(^|[^A-Za-z0-9_:])((bpr::)?adapters::github)([^A-Za-z0-9_]|$)"
    "#[ \t]*include[ \t]*[<\"][^>\"]*adapters/github"
    "#[ \t]*include[ \t]*[<\"][^>\"]*(gh_cli_transport|github_(capability_probe|cli_process|host_profile|paged_read|provider|read_contract|wire_request_validation)|native_https_transport|macos_keychain|windows_credential_manager|linux_secret_service)\\.hpp[>\"]"
    "(^|[^A-Za-z0-9_])(validated_github_host_profile|github_host_profile_candidate|native_https_transport|gh_cli_transport|github_cli_process[A-Za-z0-9_]*|github_wire_[A-Za-z0-9_]*)([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])github_(capability_probe|host_profile|paged_read|provider|read_contract)[A-Za-z0-9_]*([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])github_(credential_[A-Za-z0-9_]*|provider_[A-Za-z0-9_]*|account_transport[A-Za-z0-9_]*|authentication_transport|capability_(cache|port|request|success|error|cancelled|completion|fact|probe)[A-Za-z0-9_]*)([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])github_(profile|viewer|pull_request)_(request|success|error|cancelled|completion|port|submission|authentication)([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])github_([A-Za-z0-9_]+_record|pull_request_snapshot|session_read_model|read_limits|profile_authority|read_shape|bootstrap_credential)([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])(provider_(request|response|record|completion|transport|snapshot)|(credential|Credential)(Material|Bytes|Payload|Value|Vault|Token|Secret|Input|Store|Retrieve|Delete))([^A-Za-z0-9_]|$)"
    "https?://"
    "(api\\.github\\.com|/api/v3|/api/graphql)"
    "(^|[^A-Za-z0-9_])(URL|Url|URI|Uri|Endpoint|RestRoot|GraphqlRoot|RestRoute|GraphqlRoute)([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])(Authorization|BearerToken|AccessToken|AuthToken|TokenBytes)([^A-Za-z0-9_]|$)"
    "Authorization[ \t]*:"
    "Bearer[ \t]+"
    "(^|[^A-Za-z0-9_])(GH_TOKEN|GITHUB_TOKEN|GH_ENTERPRISE_TOKEN|GITHUB_ENTERPRISE_TOKEN)([^A-Za-z0-9_]|$)"
)
file(
    GLOB_RECURSE provider_free_projection_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/src/core/*.c"
    "${BPR_ROOT}/src/core/*.cc"
    "${BPR_ROOT}/src/core/*.cpp"
    "${BPR_ROOT}/src/core/*.cxx"
    "${BPR_ROOT}/src/core/*.h"
    "${BPR_ROOT}/src/core/*.hh"
    "${BPR_ROOT}/src/core/*.hpp"
    "${BPR_ROOT}/src/core/*.hxx"
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
    provider_free_projection_patterns
    "QUrl"
    "https?://"
    "Authorization[ \t]*:"
    "Bearer[ \t]+"
    "bpr/application/github_(read_model|provider_port)"
    "(adapters/github|github_read_contract)"
    "node_id[^\r\n]*:"
)
foreach(provider_free_file IN LISTS provider_free_projection_files)
    file(READ "${provider_free_file}" contents)
    foreach(pattern IN LISTS provider_free_projection_patterns)
        if(contents MATCHES "${pattern}")
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${provider_free_file}")
            message(
                FATAL_ERROR
                "Provider data crossed the core/Qt boundary in "
                "${relative_file}: ${pattern}"
            )
        endif()
    endforeach()
endforeach()

set(
    credential_free_region_patterns
    "github_credential_"
)
foreach(credential_free_file IN LISTS provider_free_projection_files)
    file(READ "${credential_free_file}" contents)
    foreach(pattern IN LISTS credential_free_region_patterns)
        if(contents MATCHES "${pattern}")
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${credential_free_file}")
            message(
                FATAL_ERROR
                "Credential-vault capability crossed the core/Qt boundary in "
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
    "bpr/application/github_(read_model|provider_port)"
    "(REST|GraphQL|HTTP)[A-Za-z_]*(Response|Request)"
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

foreach(qt_github_projection_file IN LISTS qt_github_projection_files)
    file(READ "${qt_github_projection_file}" contents)
    string(
        REGEX MATCHALL
        "#[ \t]*include[ \t]*[<\"]bpr/application/github_[A-Za-z0-9_]+\\.hpp[>\"]"
        qt_application_github_includes
        "${contents}"
    )
    foreach(qt_application_github_include IN LISTS qt_application_github_includes)
        if(
            NOT qt_application_github_include
                MATCHES
                    "bpr/application/github_projection\\.hpp[>\"]$"
        )
            file(
                RELATIVE_PATH
                relative_file
                "${BPR_ROOT}"
                "${qt_github_projection_file}"
            )
            message(
                FATAL_ERROR
                "Qt adapter GitHub projection boundary violation in "
                "${relative_file}: only "
                "bpr/application/github_projection.hpp may cross from the "
                "application GitHub surface"
            )
        endif()
    endforeach()

    foreach(pattern IN LISTS qt_github_projection_forbidden_patterns)
        if(contents MATCHES "${pattern}")
            file(
                RELATIVE_PATH
                relative_file
                "${BPR_ROOT}"
                "${qt_github_projection_file}"
            )
            message(
                FATAL_ERROR
                "Qt adapter GitHub projection boundary violation in "
                "${relative_file}: ${pattern}"
            )
        endif()
    endforeach()
endforeach()

file(READ "${BPR_ROOT}/src/application/CMakeLists.txt" application_cmake_contents)
if(
    application_cmake_contents
        MATCHES
            "BetterPullReview::(AdapterGitHub|AdapterQt)|Qt6::|Q[A-Za-z]+::"
)
    message(
        FATAL_ERROR
        "Application target must depend inward on Core only"
    )
endif()

file(READ "${BPR_ROOT}/src/adapters/qt/CMakeLists.txt" qt_cmake_contents)
if(qt_cmake_contents MATCHES "BetterPullReview::AdapterGitHub")
    message(
        FATAL_ERROR
        "Qt presentation adapter must not link the GitHub provider adapter"
    )
endif()

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
    set(allowed_relative_files "${allowed_relative_file}" ${ARGN})
    set(allowed_files "")
    foreach(current_allowed_relative IN LISTS allowed_relative_files)
        get_filename_component(
            current_allowed
            "${BPR_ROOT}/${current_allowed_relative}"
            ABSOLUTE
        )
        list(APPEND allowed_files "${current_allowed}")
    endforeach()

    foreach(source_file IN LISTS source_files)
        file(READ "${source_file}" contents)
        if(NOT contents MATCHES "${effect_pattern}")
            continue()
        endif()

        get_filename_component(normalized_source_file "${source_file}" ABSOLUTE)
        if(NOT normalized_source_file IN_LIST allowed_files)
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${source_file}")
            message(
                FATAL_ERROR
                "${effect_name} ownership violation in ${relative_file}; "
                "only ${allowed_relative_files} may use it"
            )
        endif()
    endforeach()

    foreach(current_allowed IN LISTS allowed_files)
        if(NOT EXISTS "${current_allowed}")
            continue()
        endif()
        get_filename_component(allowed_name "${current_allowed}" NAME)
        file(
            RELATIVE_PATH
            current_allowed_relative
            "${BPR_ROOT}"
            "${current_allowed}"
        )
        if(current_allowed_relative MATCHES "^src/adapters/git/")
            set(effect_owner_cmake "${git_cmake_contents}")
            set(effect_owner_target "better_pull_review_adapter_git")
        else()
            set(effect_owner_cmake "${github_cmake_contents}")
            set(effect_owner_target "better_pull_review_adapter_github")
        endif()
        if(NOT effect_owner_cmake MATCHES "${allowed_name}")
            message(
                FATAL_ERROR
                "Designated effect source ${allowed_name} is not owned by "
                "${effect_owner_target}"
            )
        endif()
    endforeach()
endfunction()

verify_effect_owner(
    "QProcess"
    "QProcess"
    "${github_process_runner_relative}"
    "${git_process_runner_relative}"
)
verify_effect_owner(
    "QProcessEnvironment"
    "QProcessEnvironment"
    "${github_process_runner_relative}"
    "${git_process_runner_relative}"
)
verify_effect_owner(
    "QNetworkAccessManager"
    "QNetworkAccessManager"
    "${github_native_https_relative}"
)
verify_effect_owner(
    "(QNetwork[A-Za-z0-9_]*|QSsl[A-Za-z0-9_]*)"
    "Qt network/TLS type"
    "${github_native_https_relative}"
)
verify_effect_owner(
    "GH_TELEMETRY=0"
    "GitHub CLI telemetry hardening"
    "${github_cli_transport_relative}"
)
verify_effect_owner(
    "(Security/(Security|SecItem|SecKeychain)\\.h|Sec(Item|Keychain)[A-Za-z0-9_]*|kSec[A-Z][A-Za-z0-9_]*)"
    "macOS Keychain API"
    "${macos_keychain_relative}"
)
verify_effect_owner(
    "(wincred\\.h|Cred[A-Z][A-Za-z0-9_]*|P?CREDENTIAL[A-Z0-9_]*|CRED_[A-Z0-9_]+)"
    "Windows Credential Manager API"
    "${windows_credential_manager_relative}"
)
verify_effect_owner(
    "(libsecret/secret\\.h|org\\.freedesktop\\.(Secret|secrets)|(^|[^A-Za-z0-9_])secret_(collection|item|password|schema|service|value)_[A-Za-z0-9_]*|(^|[^A-Za-z0-9_])Secret(Collection|Item|Schema|Service|Value)[A-Za-z0-9_]*|(^|[^A-Za-z0-9_])SECRET_(COLLECTION|ITEM|SCHEMA|SERVICE|VALUE)_[A-Z0-9_]*)"
    "Linux Secret Service API"
    "${linux_secret_service_relative}"
)

get_filename_component(
    github_process_interface_file
    "${BPR_ROOT}/${github_process_interface_relative}"
    ABSOLUTE
)
get_filename_component(
    github_process_runner_interface_file
    "${BPR_ROOT}/${github_process_runner_interface_relative}"
    ABSOLUTE
)
get_filename_component(
    github_process_runner_file
    "${BPR_ROOT}/${github_process_runner_relative}"
    ABSOLUTE
)
get_filename_component(
    github_cli_transport_file
    "${BPR_ROOT}/${github_cli_transport_relative}"
    ABSOLUTE
)
get_filename_component(
    git_process_runner_file
    "${BPR_ROOT}/${git_process_runner_relative}"
    ABSOLUTE
)
get_filename_component(
    git_process_port_file
    "${BPR_ROOT}/${git_process_port_relative}"
    ABSOLUTE
)
get_filename_component(
    git_process_factory_file
    "${BPR_ROOT}/${git_process_factory_relative}"
    ABSOLUTE
)

file(READ "${git_process_runner_file}" git_process_runner_contents)
file(READ "${git_process_port_file}" git_process_port_contents)
file(READ "${git_process_factory_file}" git_process_factory_contents)
set(
    required_git_process_patterns
    "QProcess::SeparateChannels"
    "QProcess::ManagedInputChannel"
    "QProcess::UnixProcessFlag::CloseFileDescriptors"
    "QProcess::UnixProcessFlag::DisableCoreDumps"
    "QProcess::UnixProcessFlag::ResetSignalHandlers"
    "QProcess::UnixProcessFlag::CreateNewSession"
    "JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE"
    "class[ \t]+process_worker[ \t]+final"
    "Quarantined[ \t\r\n]*=[ \t\r\n]*true"
    "finish_background_reap"
    "stop\\.stop_requested\\(\\)"
    "maximum_pending_input_bytes"
    "buffered_input_offset"
    "QFile[ \t\r\n]+input_file"
    "standard_output_limit"
    "standard_error_limit"
    "\"GIT_CONFIG_NOSYSTEM\""
    "\"GIT_CONFIG_SYSTEM\""
    "\"GIT_CONFIG_GLOBAL\""
    "\"GIT_ATTR_NOSYSTEM\""
    "\"GIT_EXEC_PATH\""
    "\"GIT_TEMPLATE_DIR\""
    "\"GIT_TERMINAL_PROMPT\""
    "\"GIT_NO_REPLACE_OBJECTS\""
    "\"GIT_NO_LAZY_FETCH\""
    "\"GIT_OPTIONAL_LOCKS\""
    "\"HOME\""
    "\"XDG_CONFIG_HOME\""
    "\"PATH\""
    "\"core\\.fsync=all\""
    "\"core\\.fsyncMethod=fsync\""
    "\"core\\.hooksPath=\""
    "\"protocol\\.allow=never\""
    "\"--git-dir=\""
    "\"index-pack\""
    "\"--stdin\""
    "\"--strict\""
    "\"--check-self-contained-and-connected\""
    "\"--max-input-size=\""
    "\"--threads=1\""
    "\"--no-rev-index\""
    "\"cat-file\""
    "\"merge-base\""
    "\"update-ref\""
    "\"--no-deref\""
    "\"show-ref\""
    "transaction_success"
    "probe_pack_sha256"
    "capability_probe[ \t\r\n]*\\("
)
foreach(required_git_process_pattern IN LISTS required_git_process_patterns)
    if(
        NOT git_process_runner_contents
            MATCHES "${required_git_process_pattern}"
    )
        message(
            FATAL_ERROR
            "Installed-Git runner contract is incomplete: "
            "${required_git_process_pattern}"
        )
    endif()
endforeach()

set(
    forbidden_git_process_patterns
    "QNetwork[A-Za-z0-9_]*"
    "QProcessEnvironment::systemEnvironment"
    "QProcessEnvironment::InheritFromParent"
    "(^|[^A-Za-z0-9_])(qgetenv|qEnvironmentVariable|getenv|secure_getenv|environ)([^A-Za-z0-9_]|$)"
    "(startCommand|splitCommand|setNativeArguments|startDetached|QProcess::execute)"
    "\"(fetch-pack|upload-pack|unpack-objects|clone|remote|submodule|checkout|merge|diff)\""
    "\"--(fix-thin|keep)\""
    "https?://"
    "BPR_[A-Z0-9_]*TEST"
)
foreach(forbidden_git_process_pattern IN LISTS forbidden_git_process_patterns)
    if(
        git_process_runner_contents
            MATCHES "${forbidden_git_process_pattern}"
    )
        message(
            FATAL_ERROR
            "Forbidden installed-Git process surface: "
            "${forbidden_git_process_pattern}"
        )
    endif()
endforeach()

set(
    forbidden_git_public_port_patterns
    "#[ \t]*include[ \t]*[<\"]Q"
    "(^|[^A-Za-z0-9_])Q[A-Z][A-Za-z0-9_]*"
    "(Arguments|Environment|WorkingDirectory|Timeout|OutputLimit)"
    "(URL|Url|URI|Credential|Authorization|Proxy|Certificate)"
    "(std::string|std::vector)"
)
foreach(forbidden_pattern IN LISTS forbidden_git_public_port_patterns)
    if(git_process_port_contents MATCHES "${forbidden_pattern}")
        message(
            FATAL_ERROR
            "Git application port exposes forbidden process/provider "
            "authority: ${forbidden_pattern}"
        )
    endif()
endforeach()
if(
    git_process_factory_contents MATCHES "#[ \t]*include[ \t]*[<\"]Q"
    OR git_process_factory_contents MATCHES "BPR_[A-Z0-9_]*TEST"
)
    message(
        FATAL_ERROR
        "Installed-Git factory header must remain standard C++ and "
        "production-only"
    )
endif()

foreach(
    standard_process_header
    IN ITEMS
        "${github_process_interface_file}"
        "${github_process_runner_interface_file}"
)
    file(READ "${standard_process_header}" standard_process_header_contents)
    set(
        forbidden_standard_process_header_patterns
        "#[ \t]*include[ \t]*[<\"]Q"
        "#[ \t]*include[ \t]*[<\"]Qt"
        "(^|[^A-Za-z0-9_])Q[A-Z][A-Za-z0-9_]*"
        "#[ \t]*include[ \t]*[<\"](windows\\.h|process\\.h|spawn\\.h|sys/wait\\.h)"
        "(^|[^A-Za-z0-9_])(pid_t|HANDLE|PROCESS_INFORMATION|STARTUPINFO[A-Z]?|FILE)([^A-Za-z0-9_]|$)"
        "std::string[ \t\r\n]+StandardError([^A-Za-z0-9_]|$)"
        "(^|[^A-Za-z0-9_])(Authorization|GH_TOKEN|GITHUB_TOKEN|GH_ENTERPRISE_TOKEN|GITHUB_ENTERPRISE_TOKEN)([^A-Za-z0-9_]|$)"
    )
    foreach(
        forbidden_standard_process_header_pattern
        IN LISTS forbidden_standard_process_header_patterns
    )
        if(
            standard_process_header_contents
                MATCHES "${forbidden_standard_process_header_pattern}"
        )
            file(
                RELATIVE_PATH
                relative_file
                "${BPR_ROOT}"
                "${standard_process_header}"
            )
            message(
                FATAL_ERROR
                "GitHub CLI process interface must remain standard C++ and "
                "effect-free in ${relative_file}: "
                "${forbidden_standard_process_header_pattern}"
            )
        endif()
    endforeach()
endforeach()

file(
    READ
    "${github_process_runner_interface_file}"
    github_process_runner_interface_contents
)
file(READ "${github_process_runner_file}" github_process_runner_contents)
string(
    REGEX REPLACE
    "[ \t\r\n]+"
    " "
    github_process_runner_compact
    "${github_process_runner_contents}"
)

string(
    FIND
    "${github_process_runner_compact}"
    "auto contains_nul("
    github_request_shape_start
)
string(
    FIND
    "${github_process_runner_compact}"
    "[[nodiscard]] auto decode_utf8("
    github_request_shape_end
)
if(
    github_request_shape_start EQUAL -1
    OR github_request_shape_end EQUAL -1
    OR NOT github_request_shape_start LESS github_request_shape_end
)
    message(
        FATAL_ERROR
        "GitHub CLI process runner must define bounded allocation-free "
        "pre-copy helper and request-shape validation"
    )
endif()
math(
    EXPR
    github_request_shape_length
    "${github_request_shape_end} - ${github_request_shape_start}"
)
string(
    SUBSTRING
    "${github_process_runner_compact}"
    "${github_request_shape_start}"
    "${github_request_shape_length}"
    github_request_shape_contents
)
set(
    required_github_request_shape_patterns
    "valid_request_shape\\( const github_cli_process_request& request\\) noexcept -> bool"
    "request\\.Executable\\.empty\\(\\)"
    "request\\.Executable\\.size\\(\\) > github_cli_process_max_path_bytes"
    "contains_nul\\(request\\.Executable\\)"
    "request\\.WorkingDirectory\\.empty\\(\\)"
    "request\\.WorkingDirectory\\.size\\(\\) > github_cli_process_max_path_bytes"
    "contains_nul\\(request\\.WorkingDirectory\\)"
    "request\\.Arguments\\.empty\\(\\)"
    "request\\.Arguments\\.size\\(\\) > github_cli_process_max_argument_count"
    "request\\.Environment\\.size\\(\\) > github_cli_process_max_environment_count"
    "request\\.StandardInputLimit == 0"
    "request\\.StandardInputLimit > github_cli_process_max_standard_input_bytes"
    "request\\.StandardInput\\.size\\(\\) > request\\.StandardInputLimit"
    "request\\.StandardOutputLimit == 0"
    "request\\.StandardOutputLimit > github_cli_process_max_standard_output_bytes"
    "request\\.StandardErrorLimit == 0"
    "request\\.StandardErrorLimit > github_cli_process_max_standard_error_bytes"
    "request\\.Timeout <= std::chrono::milliseconds::zero\\(\\)"
    "request\\.Timeout > std::chrono::duration_cast<std::chrono::milliseconds>\\( github_cli_process_max_timeout\\)"
    "request\\.ReapTimeout <= std::chrono::milliseconds::zero\\(\\)"
    "request\\.ReapTimeout > std::chrono::duration_cast<std::chrono::milliseconds>\\( github_cli_process_max_reap_timeout\\)"
    "argument\\.empty\\(\\)"
    "argument\\.size\\(\\) > github_cli_process_max_argument_bytes"
    "contains_nul\\(argument\\)"
    "add_bounded\\( argument_bytes, argument\\.size\\(\\), github_cli_process_max_arguments_bytes\\)"
    "entry\\.Name\\.size\\(\\) > github_cli_process_max_environment_name_bytes"
    "valid_environment_name\\(entry\\.Name\\)"
    "entry\\.Value\\.size\\(\\) > github_cli_process_max_environment_value_bytes"
    "contains_nul\\(entry\\.Value\\)"
    "add_bounded\\( environment_bytes, entry\\.Name\\.size\\(\\), github_cli_process_max_environment_bytes\\)"
    "add_bounded\\( environment_bytes, entry\\.Value\\.size\\(\\), github_cli_process_max_environment_bytes\\)"
    "equal_environment_names\\( entry\\.Name, request\\.Environment\\[prior\\]\\.Name\\)"
    "!has_empty_path \\|\\| !has_system_root"
)
foreach(
    required_request_shape_pattern
    IN LISTS required_github_request_shape_patterns
)
    if(
        NOT github_request_shape_contents
            MATCHES "${required_request_shape_pattern}"
    )
        message(
            FATAL_ERROR
            "GitHub CLI pre-copy request-shape validation must retain all "
            "path, argument, environment, stream, and deadline bounds: "
            "${required_request_shape_pattern}"
        )
    endif()
endforeach()
set(
    forbidden_github_request_shape_patterns
    "(QString|QByteArray|QFileInfo|QDir|decode_utf8)"
    "std::(string|vector|deque|list|map|unordered_[a-z_]+|set)([^A-Za-z0-9_]|$)"
    "std::(make_unique|make_shared|allocate_shared)[ \t]*[<(]"
    "(^|[^A-Za-z0-9_])(new|malloc|calloc|realloc|aligned_alloc)[ \t\r\n]*(\\(|[A-Za-z_])"
    "\\.(reserve|resize|assign|append|push_back|emplace|emplace_back)[ \t\r\n]*\\("
    "(^|[^A-Za-z0-9_])(try|throw)([^A-Za-z0-9_]|$)"
)
foreach(
    forbidden_request_shape_pattern
    IN LISTS forbidden_github_request_shape_patterns
)
    if(
        github_request_shape_contents
            MATCHES "${forbidden_request_shape_pattern}"
    )
        message(
            FATAL_ERROR
            "GitHub CLI pre-copy request-shape validation must remain "
            "allocation-free and non-throwing: "
            "${forbidden_request_shape_pattern}"
        )
    endif()
endforeach()
if(
    NOT github_process_runner_compact
        MATCHES
            "if\\(total > limit \\|\\| amount > limit - total\\) \\{ return false; \\}"
)
    message(
        FATAL_ERROR
        "GitHub CLI aggregate pre-copy bounds must reject overflow before "
        "subtraction"
    )
endif()
if(
    NOT github_process_runner_compact
        MATCHES
            "if\\(!valid_request_shape\\(request\\)\\) \\{ return std::unexpected\\{ github_cli_process_failure::invalid_request\\}; \\} if\\(stop\\.stop_requested\\(\\)\\) \\{ return std::unexpected\\{ github_cli_process_failure::cancelled\\}; \\} std::shared_ptr<process_work_item> item; try \\{ item = std::make_shared<process_work_item>\\(\\); item->Request = request;"
)
    message(
        FATAL_ERROR
        "GitHub CLI worker must validate request shape and cancellation "
        "before allocating or copying the work item"
    )
endif()
string(
    REGEX MATCHALL
    "item->Request = request"
    github_request_work_item_copies
    "${github_process_runner_compact}"
)
list(LENGTH github_request_work_item_copies github_request_work_item_copy_count)
if(NOT github_request_work_item_copy_count EQUAL 1)
    message(
        FATAL_ERROR
        "GitHub CLI worker must have exactly one guarded request-copy site"
    )
endif()

set(
    required_github_process_runner_patterns
    "#[ \t]*include[ \t]*[<\"]QProcess[>\"]"
    "#[ \t]*include[ \t]*[<\"]QProcessEnvironment[>\"]"
    "setProgram[ \t\r\n]*\\("
    "setArguments[ \t\r\n]*\\("
    "setProcessEnvironment[ \t\r\n]*\\("
    "setWorkingDirectory[ \t\r\n]*\\("
    "\"\\.git\""
    "equal_environment_names[ \t\r\n]*\\([ \t\r\n]*entry\\.Name[ \t\r\n]*,[ \t\r\n]*\"PATH\"[ \t\r\n]*\\)"
    "equal_environment_names[ \t\r\n]*\\([ \t\r\n]*entry\\.Name[ \t\r\n]*,[ \t\r\n]*\"SystemRoot\"[ \t\r\n]*\\)"
    "QProcess::SeparateChannels"
    "QProcess::ManagedInputChannel"
    "write[ \t\r\n]*\\("
    "closeWriteChannel[ \t\r\n]*\\("
    "setReadChannel[ \t\r\n]*\\("
    "read[ \t\r\n]*\\("
    "waitForStarted[ \t\r\n]*\\("
    "waitForReadyRead[ \t\r\n]*\\("
    "waitForFinished[ \t\r\n]*\\("
    "process\\.kill[ \t\r\n]*\\("
    "std::make_unique<QProcess>[ \t\r\n]*\\("
    "(^|[^A-Za-z0-9_])StandardInputLimit([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])StandardOutputLimit([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])StandardErrorLimit([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])Timeout([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])ReapTimeout([^A-Za-z0-9_]|$)"
)
foreach(required_runner_pattern IN LISTS required_github_process_runner_patterns)
    if(NOT github_process_runner_contents MATCHES "${required_runner_pattern}")
        message(
            FATAL_ERROR
            "GitHub CLI process runner must retain explicit program, "
            "argument, empty-environment, neutral-directory, separate-stream, "
            "bounded-I/O, timeout, kill, and reap handling: "
            "${required_runner_pattern}"
        )
    endif()
endforeach()

set(
    required_github_process_windows_environment_patterns
    "#if[ \t]+defined[ \t]*\\([ \t]*_WIN32[ \t]*\\)"
    "bool[ \t]+has_empty_path[ \t]*=[ \t]*false"
    "bool[ \t]+has_system_root[ \t]*=[ \t]*false"
    "has_empty_path[ \t]*=[ \t]*entry\\.Value\\.empty[ \t\r\n]*\\([ \t]*\\)"
    "has_system_root[ \t]*=[ \t]*![ \t]*entry\\.Value\\.empty[ \t\r\n]*\\([ \t]*\\)"
    "![ \t]*has_empty_path[ \t]*\\|\\|[ \t]*![ \t]*has_system_root"
)
foreach(
    required_windows_environment_pattern
    IN LISTS required_github_process_windows_environment_patterns
)
    if(
        NOT github_process_runner_contents
            MATCHES "${required_windows_environment_pattern}"
    )
        message(
            FATAL_ERROR
            "GitHub CLI process runner must retain explicit Windows empty "
            "PATH and non-empty SystemRoot validation: "
            "${required_windows_environment_pattern}"
        )
    endif()
endforeach()

set(
    required_github_process_unix_containment_patterns
    "setUnixProcessParameters[ \t\r\n]*\\("
    "QProcess::UnixProcessFlag::CloseFileDescriptors"
    "QProcess::UnixProcessFlag::DisableCoreDumps"
    "QProcess::UnixProcessFlag::ResetSignalHandlers"
    "QProcess::UnixProcessFlag::CreateNewSession"
    "const[ \t]+auto[ \t]+process_group[ \t\r\n]*=[ \t\r\n]*-[ \t]*static_cast<pid_t>[ \t\r\n]*\\([ \t\r\n]*process_group_id[ \t\r\n]*\\)"
    "::kill[ \t\r\n]*\\([ \t\r\n]*process_group[ \t\r\n]*,[ \t\r\n]*SIGKILL[ \t\r\n]*\\)"
    "QProcess::started"
    "process_pointer->processId[ \t\r\n]*\\("
    "kill_process_group_and_reap[ \t\r\n]*\\("
    "finish_background_reap[ \t\r\n]*\\("
)
foreach(
    required_unix_containment_pattern
    IN LISTS required_github_process_unix_containment_patterns
)
    if(
        NOT github_process_runner_contents
            MATCHES "${required_unix_containment_pattern}"
    )
        message(
            FATAL_ERROR
            "GitHub CLI process runner must retain Unix child hardening and "
            "initial process-group kill/reap containment: "
            "${required_unix_containment_pattern}"
        )
    endif()
endforeach()

set(
    required_process_group_resolution_patterns
    "resolve_process_group[ \t\r\n]*\\([ \t\r\n]*const[ \t]+QProcess&[ \t\r\n]+process[ \t\r\n]*,[ \t\r\n]*const[ \t]+qint64[ \t\r\n]+known_process_group_id[ \t\r\n]*\\)"
    "const[ \t]+auto[ \t]+process_id[ \t\r\n]*=[ \t\r\n]*process\\.processId[ \t\r\n]*\\([ \t]*\\)"
    "process_id[ \t\r\n]*>[ \t\r\n]*1[ \t\r\n]*\\?[ \t\r\n]*process_id[ \t\r\n]*:[ \t\r\n]*0"
)
foreach(
    required_group_resolution_pattern
    IN LISTS required_process_group_resolution_patterns
)
    if(
        NOT github_process_runner_contents
            MATCHES "${required_group_resolution_pattern}"
    )
        message(
            FATAL_ERROR
            "GitHub CLI process runner must resolve the saved initial process "
            "group from QProcess::processId when the signal has not supplied "
            "it: ${required_group_resolution_pattern}"
        )
    endif()
endforeach()

set(
    required_process_group_start_resolution
    "process->start\\(\\); \\*process_group_id = resolve_process_group\\( \\*process, \\*process_group_id\\); for\\(;;\\) \\{ \\*process_group_id = resolve_process_group\\( \\*process, \\*process_group_id\\); if\\(const auto failure = terminal_failure\\(\\); failure\\.has_value\\(\\)\\)"
)
if(
    NOT github_process_runner_compact
        MATCHES "${required_process_group_start_resolution}"
)
    message(
        FATAL_ERROR
        "GitHub CLI process runner must resolve the initial process group "
        "immediately after start and before every startup-loop terminal check"
    )
endif()

if(
    NOT github_process_runner_compact
        MATCHES
            "auto process_group_id = resolve_process_group\\(process, known_process_group_id\\); kill_process_group\\(process_group_id\\);"
    OR NOT github_process_runner_compact
        MATCHES
            "process\\.waitForFinished\\(wait\\)\\); if\\(process\\.state\\(\\) != QProcess::NotRunning\\) \\{ process_group_id = resolve_process_group\\(process, process_group_id\\); kill_process_group\\(process_group_id\\);"
)
    message(
        FATAL_ERROR
        "GitHub CLI bounded reap must resolve the initial process group "
        "before initial and repeated kill attempts"
    )
endif()

if(
    NOT github_process_runner_compact
        MATCHES
            "auto finish_background_reap\\( QProcess& process, const qint64 known_process_group_id\\) -> void \\{ auto process_group_id = resolve_process_group\\(process, known_process_group_id\\); while\\(process\\.state\\(\\) != QProcess::NotRunning\\) \\{ process_group_id = resolve_process_group\\(process, process_group_id\\); kill_process_group\\(process_group_id\\);"
)
    message(
        FATAL_ERROR
        "GitHub CLI background reap must retain and refresh resolved initial "
        "process-group ownership throughout cleanup"
    )
endif()

if(
    NOT github_process_runner_compact
        MATCHES
            "const auto process_group_id = resolve_process_group\\(\\*process, known_process_group_id\\); if\\( kill_process_group_and_reap\\( \\*process, process_group_id, reap_timeout\\)\\)"
)
    message(
        FATAL_ERROR
        "GitHub CLI failed execution must resolve initial process-group "
        "ownership before bounded reap or quarantine handoff"
    )
endif()

set(
    required_stable_process_group_state
    "const auto process_group_id = std::make_shared<qint64>\\(0\\);"
)
set(
    required_started_process_group_capture
    "QObject::connect\\( process\\.get\\(\\), &QProcess::started, process\\.get\\(\\), \\[ process_pointer = process\\.get\\(\\), process_group_id \\]\\(\\) noexcept \\{ \\*process_group_id = process_pointer->processId\\(\\); \\}, Qt::DirectConnection\\);"
)
set(
    required_finished_process_group_cleanup
    "QObject::connect\\( process\\.get\\(\\), &QProcess::finished, process\\.get\\(\\), \\[process_group_id\\]\\( const int, const QProcess::ExitStatus\\) noexcept \\{ kill_process_group\\(\\*process_group_id\\); \\}, Qt::DirectConnection\\);"
)
if(
    NOT github_process_runner_compact
        MATCHES "${required_stable_process_group_state}"
    OR NOT github_process_runner_compact
        MATCHES "${required_started_process_group_capture}"
    OR NOT github_process_runner_compact
        MATCHES "${required_finished_process_group_cleanup}"
)
    message(
        FATAL_ERROR
        "GitHub CLI process signals must capture stable process-group state "
        "by value and synchronously kill that group on finish"
    )
endif()
string(
    FIND
    "${github_process_runner_compact}"
    "&QProcess::finished"
    github_process_finished_connection_position
)
string(
    FIND
    "${github_process_runner_compact}"
    "process->start();"
    github_process_start_position
)
if(
    github_process_finished_connection_position EQUAL -1
    OR github_process_start_position EQUAL -1
    OR NOT github_process_finished_connection_position
        LESS github_process_start_position
)
    message(
        FATAL_ERROR
        "GitHub CLI finished process-group cleanup must be connected before "
        "process start"
    )
endif()
if(
    NOT github_process_runner_compact
        MATCHES
            "kill_process_group\\(\\*process_group_id\\); return \\{ \\.Result = github_cli_process_success"
)
    message(
        FATAL_ERROR
        "GitHub CLI process runner must repeat initial process-group cleanup "
        "immediately before publishing success"
    )
endif()

set(
    required_github_process_worker_patterns
    "class[ \t]+process_worker[ \t]+final"
    "~process_worker[ \t\r\n]*\\("
    "ShutdownRequested\\.store[ \t\r\n]*\\("
    "Worker\\.join[ \t\r\n]*\\("
    "while[ \t\r\n]*\\([ \t\r\n]*Busy[ \t\r\n]*\\)"
    "Busy[ \t\r\n]*=[ \t\r\n]*true"
    "Pending[ \t\r\n]*=[ \t\r\n]*item"
    "WorkAvailable\\.notify_one[ \t\r\n]*\\("
    "item->Deadline[ \t\r\n]*=[ \t\r\n]*clock::now[ \t\r\n]*\\([ \t]*\\)[ \t\r\n]*\\+[ \t\r\n]*request\\.Timeout"
    "std::unique_ptr<process_worker>[ \t\r\n]+Worker"
    "Worker[ \t\r\n]*=[ \t\r\n]*std::make_unique<process_worker>[ \t\r\n]*\\("
    "std::mutex[ \t\r\n]+WorkerMutex"
)
foreach(required_worker_pattern IN LISTS required_github_process_worker_patterns)
    if(NOT github_process_runner_contents MATCHES "${required_worker_pattern}")
        message(
            FATAL_ERROR
            "GitHub CLI process runner must retain its runner-owned, "
            "serialized, joined worker lifecycle: "
            "${required_worker_pattern}"
        )
    endif()
endforeach()
string(
    REGEX MATCHALL
    "std::unique_ptr<process_worker>"
    github_process_worker_owners
    "${github_process_runner_contents}"
)
list(LENGTH github_process_worker_owners github_process_worker_owner_count)
string(
    REGEX MATCHALL
    "std::make_unique<process_worker>[ \t\r\n]*\\("
    github_process_worker_constructions
    "${github_process_runner_contents}"
)
list(
    LENGTH
    github_process_worker_constructions
    github_process_worker_construction_count
)
string(
    REGEX MATCHALL
    "std::thread[ \t\r\n]+[A-Za-z_][A-Za-z0-9_]*"
    github_process_worker_threads
    "${github_process_runner_contents}"
)
list(LENGTH github_process_worker_threads github_process_worker_thread_count)
if(
    github_process_runner_contents
        MATCHES
            "github_cli_process_runner_test_seam[ \t\r\n]*\\*"
    OR github_process_runner_interface_contents
        MATCHES
            "github_cli_process_runner_test_seam[ \t\r\n]*\\*"
    OR NOT github_process_runner_interface_contents
        MATCHES
            "make_github_cli_process_runner[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*->[ \t\r\n]*std::unique_ptr<github_cli_process_port>"
    OR NOT github_process_runner_interface_contents
        MATCHES
            "make_github_cli_process_runner_for_test[ \t\r\n]*\\([ \t\r\n]*std::shared_ptr<github_cli_process_runner_test_seam>[ \t\r\n]+test_seam[ \t\r\n]*\\)"
    OR NOT github_process_runner_compact
        MATCHES
            "auto make_github_cli_process_runner\\(\\) -> std::unique_ptr<github_cli_process_port> \\{ return make_qprocess_github_cli_process_runner\\(nullptr\\); \\}"
    OR NOT github_process_runner_compact
        MATCHES
            "auto make_github_cli_process_runner_for_test\\( std::shared_ptr<github_cli_process_runner_test_seam> test_seam\\) -> std::unique_ptr<github_cli_process_port> \\{ return make_qprocess_github_cli_process_runner\\( std::move\\(test_seam\\)\\); \\}"
)
    message(
        FATAL_ERROR
        "GitHub CLI production factory must remain nullary while the explicit "
        "test factory and asynchronous worker retain shared seam ownership"
    )
endif()

if(
    NOT github_process_worker_owner_count EQUAL 1
    OR NOT github_process_worker_construction_count EQUAL 1
    OR NOT github_process_worker_thread_count EQUAL 1
    OR NOT github_process_runner_compact
        MATCHES
            "private: std::shared_ptr<github_cli_process_runner_test_seam> TestSeam; std::mutex WorkerMutex; std::unique_ptr<process_worker> Worker; \\};"
)
    message(
        FATAL_ERROR
        "GitHub CLI process worker must have exactly one owner, the concrete "
        "runner instance"
    )
endif()

set(
    required_pre_start_guard
    "if\\( shutdown_requested\\.load\\([^;]*\\) \\|\\| stop\\.stop_requested\\(\\)\\) \\{ return failed\\( github_cli_process_failure::cancelled\\); \\} if\\(clock::now\\(\\) >= deadline\\) \\{ return failed\\(github_cli_process_failure::timeout\\); \\} process->start\\(\\);"
)
if(NOT github_process_runner_compact MATCHES "${required_pre_start_guard}")
    message(
        FATAL_ERROR
        "GitHub CLI process runner must check shutdown, cancellation, and "
        "the absolute deadline immediately before process start"
    )
endif()
string(
    REGEX MATCHALL
    "process->start[ \t\r\n]*\\("
    github_process_start_calls
    "${github_process_runner_contents}"
)
list(LENGTH github_process_start_calls github_process_start_call_count)
if(NOT github_process_start_call_count EQUAL 1)
    message(
        FATAL_ERROR
        "GitHub CLI process runner must have exactly one guarded process "
        "start site"
    )
endif()

set(
    required_github_process_reap_ownership_patterns
    "std::unique_ptr<QProcess>[ \t\r\n]+AwaitingReap"
    "\\.AwaitingReap[ \t\r\n]*=[ \t\r\n]*std::move[ \t\r\n]*\\([ \t\r\n]*process[ \t\r\n]*\\)"
    "Quarantined[ \t\r\n]*=[ \t\r\n]*true"
    "publish[ \t\r\n]*\\([ \t\r\n]*item"
    "finish_background_reap[ \t\r\n]*\\([ \t\r\n]*\\*execution->AwaitingReap"
    "execution->AwaitingReap\\.reset[ \t\r\n]*\\("
    "Quarantined[ \t\r\n]*=[ \t\r\n]*false"
    "Busy[ \t\r\n]*=[ \t\r\n]*false"
)
foreach(
    required_reap_ownership_pattern
    IN LISTS required_github_process_reap_ownership_patterns
)
    if(
        NOT github_process_runner_contents
            MATCHES "${required_reap_ownership_pattern}"
    )
        message(
            FATAL_ERROR
            "GitHub CLI process runner must retain quarantine ownership until "
            "a failed bounded reap completes: "
            "${required_reap_ownership_pattern}"
        )
    endif()
endforeach()
if(
    NOT github_process_runner_compact
        MATCHES
            "if\\(execution->AwaitingReap != nullptr\\) \\{ \\{ std::lock_guard lock\\{Mutex\\}; Quarantined = true; \\} SlotAvailable\\.notify_all\\(\\); \\} publish\\(item,"
    OR NOT github_process_runner_compact
        MATCHES
            "finish_background_reap\\( \\*execution->AwaitingReap, execution->ProcessGroupId\\); execution->AwaitingReap\\.reset\\(\\); \\} \\{ std::lock_guard lock\\{Mutex\\}; Quarantined = false; Busy = false;"
)
    message(
        FATAL_ERROR
        "GitHub CLI process runner must publish reap_failure only after "
        "quarantine owns the process, then release the slot only after reap"
    )
endif()

if(
    NOT github_process_runner_compact
        MATCHES
            "if\\( !item->Completed\\.wait_until\\( result_lock, completion_deadline, \\[&\\] \\{ return item->Result\\.has_value\\(\\); \\}\\)\\) \\{ \\{ std::lock_guard lock\\{Mutex\\}; if\\(Busy\\) \\{ Quarantined = true; \\} \\} SlotAvailable\\.notify_all\\(\\); return std::unexpected\\{ github_cli_process_failure::reap_failure\\}; \\}"
)
    message(
        FATAL_ERROR
        "GitHub CLI completion watchdog must predicate-check publication and "
        "quarantine the busy worker before returning reap_failure"
    )
endif()

if(
    NOT github_process_runner_compact
        MATCHES
            "catch\\(\\.\\.\\.\\) \\{ publish\\( item, std::unexpected\\{ github_cli_process_failure::start_failure\\}\\); \\{ std::lock_guard lock\\{Mutex\\}; Quarantined = false; Busy = false; \\} SlotAvailable\\.notify_all\\(\\); (continue|return); \\}"
)
    message(
        FATAL_ERROR
        "GitHub CLI worker exception recovery must clear any watchdog "
        "quarantine before reopening the serialized slot"
    )
endif()

string(
    REGEX MATCHALL
    "github_cli_process_failure::reap_failure"
    github_process_reap_failure_sites
    "${github_process_runner_contents}"
)
list(
    LENGTH
    github_process_reap_failure_sites
    github_process_reap_failure_site_count
)
if(NOT github_process_reap_failure_site_count EQUAL 3)
    message(
        FATAL_ERROR
        "GitHub CLI process runner must retain exactly the bounded-reap, "
        "already-quarantined, and watchdog-quarantine reap_failure sites"
    )
endif()

set(
    required_github_process_bounded_read_patterns
    "std::array<char,[ \t\r\n]*16U[ \t]*\\*[ \t]*1024U>[ \t\r\n]+buffer"
    "const[ \t]+auto[ \t]+remaining[ \t\r\n]*=[ \t\r\n]*limit[ \t\r\n]*-[ \t\r\n]*consumed"
    "std::min<std::size_t>[ \t\r\n]*\\([ \t\r\n]*buffer\\.size[ \t\r\n]*\\([ \t]*\\)[ \t\r\n]*,[ \t\r\n]*remaining[ \t\r\n]*\\+[ \t\r\n]*1U[ \t\r\n]*\\)"
    "process\\.read[ \t\r\n]*\\([ \t\r\n]*buffer\\.data[ \t\r\n]*\\([ \t]*\\)[ \t\r\n]*,[ \t\r\n]*static_cast<qint64>[ \t\r\n]*\\([ \t\r\n]*requested[ \t\r\n]*\\)[ \t\r\n]*\\)"
    "byte_count[ \t\r\n]*>[ \t\r\n]*remaining"
    "consumed[ \t\r\n]*\\+=[ \t\r\n]*byte_count"
)
foreach(
    required_bounded_read_pattern
    IN LISTS required_github_process_bounded_read_patterns
)
    if(
        NOT github_process_runner_contents
            MATCHES "${required_bounded_read_pattern}"
    )
        message(
            FATAL_ERROR
            "GitHub CLI process runner must retain bounded fixed-buffer "
            "remaining-plus-one channel reads: "
            "${required_bounded_read_pattern}"
        )
    endif()
endforeach()

function(reject_github_process_runner_surface forbidden_pattern surface_name)
    if(github_process_runner_contents MATCHES "${forbidden_pattern}")
        message(
            FATAL_ERROR
            "Forbidden ${surface_name} surface in "
            "${github_process_runner_relative}: ${forbidden_pattern}"
        )
    endif()
endfunction()

reject_github_process_runner_surface(
    "(startCommand|splitCommand|setNativeArguments|startDetached|QProcess::execute|setCreateProcessArgumentsModifier)"
    "shell/raw-command"
)
reject_github_process_runner_surface(
    "(QProcess::(ForwardedChannels|ForwardedOutputChannel|ForwardedErrorChannel|MergedChannels)|setProcessChannelMode[ \t\r\n]*\\([ \t\r\n]*QProcess::(Forwarded|Merged)|setStandardOutputProcess|setStandard(Input|Output|Error)File)"
    "forwarded, merged, or file-redirected process channel"
)

set(
    github_process_runner_without_selected_channels
    "${github_process_runner_contents}"
)
string(
    REGEX REPLACE
    "process(\\.|->)[ \t\r\n]*setReadChannel[ \t\r\n]*\\([ \t\r\n]*channel[ \t\r\n]*\\)"
    ""
    github_process_runner_without_selected_channels
    "${github_process_runner_without_selected_channels}"
)
string(
    REGEX REPLACE
    "process(\\.|->)[ \t\r\n]*setReadChannel[ \t\r\n]*\\([ \t\r\n]*QProcess::StandardOutput[ \t\r\n]*\\)"
    ""
    github_process_runner_without_selected_channels
    "${github_process_runner_without_selected_channels}"
)
if(
    github_process_runner_without_selected_channels
        MATCHES "setReadChannel[ \t\r\n]*\\("
)
    message(
        FATAL_ERROR
        "Forbidden unselected process channel selection surface in "
        "${github_process_runner_relative}"
    )
endif()

set(
    github_process_runner_without_bounded_read
    "${github_process_runner_contents}"
)
string(
    REGEX REPLACE
    "process\\.read[ \t\r\n]*\\([ \t\r\n]*buffer\\.data[ \t\r\n]*\\([ \t]*\\)[ \t\r\n]*,[ \t\r\n]*static_cast<qint64>[ \t\r\n]*\\([ \t\r\n]*requested[ \t\r\n]*\\)[ \t\r\n]*\\)"
    ""
    github_process_runner_without_bounded_read
    "${github_process_runner_without_bounded_read}"
)
if(
    github_process_runner_without_bounded_read
        MATCHES "\\.[ \t\r\n]*read(All|Line)?[ \t\r\n]*\\("
)
    message(
        FATAL_ERROR
        "Forbidden unselected raw process channel read surface in "
        "${github_process_runner_relative}"
    )
endif()
reject_github_process_runner_surface(
    "(\\.detach[ \t\r\n]*\\(|Worker\\.release[ \t\r\n]*\\(|new[ \t\r\n]+process_worker|static[ \t\r\n]+([^;\r\n]*[ \t\r\n]+)?process_worker|(^|[\r\n])[ \t]*process_worker[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*[;={])"
    "global, detached, or leaked process worker"
)
reject_github_process_runner_surface(
    "(process\\.reset[ \t\r\n]*\\(|delete[ \t\r\n]+process|AwaitingReap\\.release[ \t\r\n]*\\(|new[ \t\r\n]+QProcess|(^|[;{}\r\n])[ \t]*QProcess[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*[;={])"
    "premature process ownership release"
)

set(
    github_process_runner_without_local_worker_view
    "${github_process_runner_contents}"
)
string(
    REGEX REPLACE
    "process_worker[ \t]*\\*[ \t]*worker[ \t]*=[ \t]*nullptr"
    ""
    github_process_runner_without_local_worker_view
    "${github_process_runner_without_local_worker_view}"
)
if(
    github_process_runner_without_local_worker_view
        MATCHES
            "(std::shared_ptr<process_worker>|process_worker[ \t]*\\*)"
)
    message(
        FATAL_ERROR
        "Forbidden unowned or shared process worker lifetime in "
        "${github_process_runner_relative}"
    )
endif()
reject_github_process_runner_surface(
    "(QProcessEnvironment::systemEnvironment|QProcessEnvironment::InheritFromParent|processEnvironment[ \t\r\n]*\\(|qgetenv[ \t\r\n]*\\(|qEnvironmentVariable[A-Za-z0-9_]*[ \t\r\n]*\\(|(^|[^A-Za-z0-9_])(getenv|secure_getenv|_wgetenv|environ)([^A-Za-z0-9_]|$))"
    "inherited or ambient environment"
)
reject_github_process_runner_surface(
    "errorString[ \t\r\n]*\\("
    "raw process error-string exposure"
)
reject_github_process_runner_surface(
    "(\"--(auth-token|with-token|token|include|cache|verbose|debug|jq|template|paginate|slurp|preview|field|raw-field|silent)\"|(^|[^A-Za-z0-9_])(Authorization|GH_TOKEN|GITHUB_TOKEN|GH_ENTERPRISE_TOKEN|GITHUB_ENTERPRISE_TOKEN|GH_DEBUG)([^A-Za-z0-9_]|$))"
    "credential, status-output, cache, or debug"
)
reject_github_process_runner_surface(
    "(std::(cout|cerr|clog)|(^|[^A-Za-z0-9_])(printf|fprintf|puts|fputs|perror|syslog|os_log|NSLog|qDebug|qInfo|qWarning|qCritical|qFatal)[ \t\r\n]*\\(|spdlog)"
    "logging/printing"
)

file(READ "${github_cli_transport_file}" github_cli_transport_contents)
set(
    required_github_cli_transport_patterns
    "\"api\""
    "\"--hostname\""
    "\"--method\""
    "\"--input\""
    "\"\\.git\""
    "GH_PROMPT_DISABLED=1"
    "GH_NO_UPDATE_NOTIFIER=1"
    "GH_NO_EXTENSION_UPDATE_NOTIFIER=1"
    "GH_SPINNER_DISABLED=1"
    "NO_COLOR=1"
    "CLICOLOR=0"
    "\"HOME\""
    "\"XDG_RUNTIME_DIR\""
    "\"DBUS_SESSION_BUS_ADDRESS\""
    "\"USERPROFILE\""
    "\"APPDATA\""
    "\"LOCALAPPDATA\""
    "\"SystemRoot\""
    "\"PATH\""
    "\"HTTPS_PROXY\""
    "\"NO_PROXY\""
    "\"SSL_CERT_FILE\""
    "\"SSL_CERT_DIR\""
)
foreach(
    required_transport_pattern
    IN LISTS required_github_cli_transport_patterns
)
    if(NOT github_cli_transport_contents MATCHES "${required_transport_pattern}")
        message(
            FATAL_ERROR
            "GitHub CLI transport must retain explicit allowlisted "
            "invocation and minimal noninteractive environment construction: "
            "${required_transport_pattern}"
        )
    endif()
endforeach()

function(reject_github_cli_transport_surface forbidden_pattern surface_name)
    if(github_cli_transport_contents MATCHES "${forbidden_pattern}")
        message(
            FATAL_ERROR
            "Forbidden ${surface_name} surface in "
            "${github_cli_transport_relative}: ${forbidden_pattern}"
        )
    endif()
endfunction()

reject_github_cli_transport_surface(
    "(#[ \t]*include[ \t]*[<\"]Q|(^|[^A-Za-z0-9_])Q(Process|ProcessEnvironment|String|ByteArray|Variant|Url)([^A-Za-z0-9_]|$))"
    "Qt/process implementation"
)
reject_github_cli_transport_surface(
    "\"--(auth-token|with-token|token|include|cache|verbose|debug|jq|template|paginate|slurp|preview|field|raw-field|silent)\""
    "credential, status-output, cache, debug, or response-transform flag"
)
reject_github_cli_transport_surface(
    "(^|[^A-Za-z0-9_])(Authorization|GH_TOKEN|GITHUB_TOKEN|GH_ENTERPRISE_TOKEN|GITHUB_ENTERPRISE_TOKEN|GH_DEBUG)([^A-Za-z0-9_]|$)"
    "credential or debug literal"
)
reject_github_cli_transport_surface(
    "(std::(cout|cerr|clog)|(^|[^A-Za-z0-9_])(printf|fprintf|puts|fputs|perror|syslog|os_log|NSLog|qDebug|qInfo|qWarning|qCritical|qFatal)[ \t\r\n]*\\(|spdlog)"
    "logging/printing"
)

get_filename_component(
    github_native_https_interface_file
    "${BPR_ROOT}/${github_native_https_interface_relative}"
    ABSOLUTE
)
get_filename_component(
    github_native_https_file
    "${BPR_ROOT}/${github_native_https_relative}"
    ABSOLUTE
)
get_filename_component(
    github_wire_validation_interface_file
    "${BPR_ROOT}/${github_wire_validation_interface_relative}"
    ABSOLUTE
)
get_filename_component(
    github_wire_validation_file
    "${BPR_ROOT}/${github_wire_validation_relative}"
    ABSOLUTE
)

foreach(
    standard_native_header
    IN ITEMS
        "${github_native_https_interface_file}"
        "${github_wire_validation_interface_file}"
)
    file(READ "${standard_native_header}" standard_native_header_contents)
    set(
        forbidden_standard_native_header_patterns
        "#[ \t]*include[ \t]*[<\"]Q"
        "#[ \t]*include[ \t]*[<\"]Qt"
        "(^|[^A-Za-z0-9_])Q[A-Z][A-Za-z0-9_]*"
        "#[ \t]*include[ \t]*[<\"](windows\\.h|winhttp\\.h|Security/|SystemConfiguration/|sys/socket\\.h|curl/)"
        "(^|[^A-Za-z0-9_])(HANDLE|SOCKET|CF[A-Z][A-Za-z0-9_]*|CURL[A-Za-z0-9_]*)([^A-Za-z0-9_]|$)"
    )
    foreach(
        forbidden_standard_native_header_pattern
        IN LISTS forbidden_standard_native_header_patterns
    )
        if(
            standard_native_header_contents
                MATCHES "${forbidden_standard_native_header_pattern}"
        )
            file(
                RELATIVE_PATH
                relative_file
                "${BPR_ROOT}"
                "${standard_native_header}"
            )
            message(
                FATAL_ERROR
                "GitHub native HTTPS interfaces must remain standard C++ "
                "and effect-free in ${relative_file}: "
                "${forbidden_standard_native_header_pattern}"
            )
        endif()
    endforeach()
endforeach()

file(READ "${github_native_https_file}" github_native_https_contents)
file(READ "${github_wire_validation_file}" github_wire_validation_contents)
file(READ "${github_cli_transport_file}" github_cli_transport_contents)

set(
    required_native_https_patterns
    "#[ \t]*include[ \t]*[<\"]QNetworkAccessManager[>\"]"
    "#[ \t]*include[ \t]*[<\"]QSslConfiguration[>\"]"
    "QNetworkProxyFactory::systemProxyForQuery[ \t\r\n]*\\("
    "maximum_system_proxy_candidates"
    "proxy_candidate_retryable[ \t\r\n]*\\("
    "QNetworkRequest::ManualRedirectPolicy"
    "QSslSocket::VerifyPeer"
    "QSsl::TlsV1_2OrLater"
    "QSslConfiguration::systemCaCertificates[ \t\r\n]*\\("
    "setLocalCertificateChain[ \t\r\n]*\\([ \t\r\n]*\\{\\}[ \t\r\n]*\\)"
    "setBackendConfiguration[ \t\r\n]*\\([ \t\r\n]*\\{\\}[ \t\r\n]*\\)"
    "QNetworkAccessManager::sslErrors"
    "QNetworkReply::encrypted"
    "QNetworkRequest::Http2AllowedAttribute"
    "\"Accept-Encoding\""
    "\"identity\""
    "\"Accept-Language\""
    "std::stop_callback"
    "active_hop_guard"
    "remaining_before_deadline[ \t\r\n]*\\("
    "Done\\.notify_one[ \t\r\n]*\\("
    "setTransferTimeout[ \t\r\n]*\\("
    "setReadBufferSize[ \t\r\n]*\\("
    "validate_github_redirect_target[ \t\r\n]*\\("
    "validate_github_wire_request[ \t\r\n]*\\("
    "BPR_NATIVE_HTTPS_TESTING"
    "QSslSocket::activeBackend[ \t\r\n]*\\("
)
foreach(required_native_https_pattern IN LISTS required_native_https_patterns)
    if(
        NOT github_native_https_contents
            MATCHES "${required_native_https_pattern}"
    )
        message(
            FATAL_ERROR
            "GitHub native HTTPS must retain explicit system proxy/trust, "
            "manual redirect, cancellation, protocol, and resource policy: "
            "${required_native_https_pattern}"
        )
    endif()
endforeach()

foreach(
    validation_consumer_contents
    IN ITEMS
        "${github_native_https_contents}"
        "${github_cli_transport_contents}"
)
    if(
        NOT validation_consumer_contents
            MATCHES
                "#[ \t]*include[ \t]*[<\"]github_wire_request_validation\\.hpp[>\"]"
    )
        message(
            FATAL_ERROR
            "GitHub transports must use the shared correlation and outbound "
            "request validator"
        )
    endif()
endforeach()
if(
    NOT github_wire_validation_contents
        MATCHES "validate_outbound_read_request[ \t\r\n]*\\("
)
    message(
        FATAL_ERROR
        "Shared GitHub wire validation must recompile and validate the "
        "canonical read-only request"
    )
endif()

function(reject_native_https_surface forbidden_pattern surface_name)
    if(github_native_https_contents MATCHES "${forbidden_pattern}")
        message(
            FATAL_ERROR
            "Forbidden native HTTPS ${surface_name} surface in "
            "${github_native_https_relative}: ${forbidden_pattern}"
        )
    endif()
endfunction()

reject_native_https_surface(
    "(ignoreSslErrors|QSslSocket::VerifyNone|setPeerVerifyName[ \t\r\n]*\\()"
    "certificate bypass"
)
reject_native_https_surface(
    "(QSslConfiguration::setDefaultConfiguration|QSslSocket::setActiveBackend)"
    "process-global TLS mutation"
)
reject_native_https_surface(
    "(QNetworkProxy::setApplicationProxy|QNetworkProxyFactory::setUseSystemConfiguration|QNetworkProxyFactory::setApplicationProxyFactory)"
    "global proxy mutation"
)
reject_native_https_surface(
    "(QNetworkRequest::(NoLessSafeRedirectPolicy|SameOriginRedirectPolicy|UserVerifiedRedirectPolicy)|FollowRedirectsAttribute)"
    "automatic redirect"
)
reject_native_https_surface(
    "\\.[ \t\r\n]*read(All|Line)[ \t\r\n]*\\("
    "unbounded response read"
)
reject_native_https_surface(
    "(errorString|reasonPhrase|std::(cout|cerr|clog)|(^|[^A-Za-z0-9_])(printf|fprintf|puts|fputs|perror|syslog|os_log|NSLog|qDebug|qInfo|qWarning|qCritical|qFatal)[ \t\r\n]*\\(|spdlog)"
    "raw diagnostic or logging"
)
reject_native_https_surface(
    "(QNetworkDiskCache|setCookieJar[ \t\r\n]*\\(|QSettings|QFile|QProcess|QTcp(Server|Socket)|QUdpSocket|QLocalServer)"
    "persistence, process, listener, or raw socket"
)
reject_native_https_surface(
    "(qgetenv[ \t\r\n]*\\(|qEnvironmentVariable[A-Za-z0-9_]*[ \t\r\n]*\\(|(^|[^A-Za-z0-9_])(getenv|secure_getenv|_wgetenv|environ)([^A-Za-z0-9_]|$))"
    "ambient environment"
)

file(READ "${macos_keychain_file}" macos_keychain_contents)
if(
    NOT macos_keychain_contents
        MATCHES
            "(^|[^A-Za-z0-9_])application::effect_executor([^A-Za-z0-9_]|$)"
    )
    message(
        FATAL_ERROR
        "macOS Keychain adapter requires the host effect executor"
    )
endif()

set(
    required_macos_keychain_patterns
    "#[ \t]*include[ \t]*[<\"]Security/(Security|SecItem|SecKeychain)\\.h[>\"]"
    "SecKeychainCopyDefault[ \t\r\n]*\\("
    "SecItemAdd[ \t\r\n]*\\("
    "SecItemCopyMatching[ \t\r\n]*\\("
    "SecItemUpdate[ \t\r\n]*\\("
    "SecItemDelete[ \t\r\n]*\\("
)
foreach(required_pattern IN LISTS required_macos_keychain_patterns)
    if(NOT macos_keychain_contents MATCHES "${required_pattern}")
        message(
            FATAL_ERROR
            "macOS Keychain adapter must use the selected Keychain lifecycle: "
            "${required_pattern}"
        )
    endif()
endforeach()

set(
    required_macos_keychain_query_constants
    kSecClassGenericPassword
    kSecAttrService
    kSecAttrAccount
    kSecUseKeychain
    kSecMatchSearchList
    kSecUseAuthenticationUI
    kSecUseAuthenticationUIFail
    kSecReturnData
    kSecMatchLimit
    kSecMatchLimitOne
)
foreach(
    required_query_constant
    IN LISTS required_macos_keychain_query_constants
)
    if(
        NOT macos_keychain_contents
            MATCHES
                "(^|[^A-Za-z0-9_])${required_query_constant}([^A-Za-z0-9_]|$)"
    )
        message(
            FATAL_ERROR
            "macOS Keychain adapter must retain the exact-keychain, "
            "noninteractive, data-only query property "
            "${required_query_constant}"
        )
    endif()
endforeach()

set(macos_keychain_effects "${macos_keychain_contents}")
foreach(
    selected_effect
    IN ITEMS
        SecKeychainCopyDefault
        SecKeychainRef
        SecItemAdd
        SecItemCopyMatching
        SecItemUpdate
        SecItemDelete
)
    string(REPLACE "${selected_effect}" "" macos_keychain_effects "${macos_keychain_effects}")
endforeach()
if(
    macos_keychain_effects
        MATCHES
            "(^|[^A-Za-z0-9_])Sec(Item|Keychain)[A-Za-z0-9_]+"
)
    message(
        FATAL_ERROR
        "macOS Keychain adapter uses a Keychain effect outside the selected "
        "SecKeychainCopyDefault and SecItem lifecycle"
    )
endif()

function(reject_macos_keychain_surface forbidden_pattern surface_name)
    if(macos_keychain_contents MATCHES "${forbidden_pattern}")
        message(
            FATAL_ERROR
            "Forbidden ${surface_name} surface in ${macos_keychain_relative}: "
            "${forbidden_pattern}"
        )
    endif()
endfunction()

reject_macos_keychain_surface(
    "kSec(UseDataProtectionKeychain|AttrSynchronizable|AttrAccessGroup|AttrAccessible[A-Za-z0-9_]*)"
    "Keychain backend-switch"
)
reject_macos_keychain_surface(
    "(application::decision_executor|DecisionLane|decision_lane)"
    "serialized decision-executor"
)
reject_macos_keychain_surface(
    "kSec(Class(InternetPassword|Certificate|Key|Identity)|Return(Attributes|Ref|PersistentRef)|MatchLimitAll|UseAuthenticationUI(Allow|Skip))"
    "Keychain query-policy"
)
reject_macos_keychain_surface(
    "(SecCopyErrorMessageString|#[ \t]*include[ \t]*<format>|std::(format|vformat|to_string)|([io]stringstream|snprintf|sprintf)[ \t\r\n]*\\(|QString::number)"
    "raw operating-system error formatting"
)
reject_macos_keychain_surface(
    "(#[ \t]*include[ \t]*[<\"](Q|Qt|AppKit/|UIKit/)|(^|[^A-Za-z0-9_])(Q[A-Z][A-Za-z0-9_]*|NS(Application|Window|View|Alert)[A-Za-z0-9_]*|UI(Application|Window|View|Alert)[A-Za-z0-9_]*))"
    "Qt/UI"
)
reject_macos_keychain_surface(
    "(QNetwork[A-Za-z0-9_]*|QtNetwork|CFNetwork|NSURLSession|NWConnection|#[ \t]*include[ \t]*[<\"]curl/|CURL[A-Za-z0-9_]*)"
    "network"
)
reject_macos_keychain_surface(
    "([Ss][Qq][Ll][Ii][Tt][Ee]3?|QSettings|NSUserDefaults|CFPreferences)"
    "settings/database"
)
reject_macos_keychain_surface(
    "(#[ \t]*include[ \t]*<(filesystem|fstream)>|std::filesystem|std::([io]?fstream|[io]stream)|(^|[^A-Za-z0-9_.:])(fopen|freopen|fread|fwrite|open|openat|creat|read|write|pread|pwrite)[ \t\r\n]*\\()"
    "filesystem"
)
reject_macos_keychain_surface(
    "((^|[^A-Za-z0-9_])(getenv|setenv|unsetenv|putenv)[ \t\r\n]*\\(|(^|[^A-Za-z0-9_])environ([^A-Za-z0-9_]|$)|QProcessEnvironment|NSProcessInfo)"
    "environment"
)
reject_macos_keychain_surface(
    "(QProcess|NSTask|posix_spawn|(^|[^A-Za-z0-9_])(system|popen|fork|exec(l|le|lp|v|ve|vp|vpe))[ \t\r\n]*\\()"
    "command/process"
)
reject_macos_keychain_surface(
    "(std::(cout|cerr|clog)|(^|[^A-Za-z0-9_])(printf|fprintf|puts|fputs|perror|syslog|os_log|NSLog|qDebug|qInfo|qWarning|qCritical|qFatal)[ \t\r\n]*\\(|spdlog)"
    "logging/printing"
)

file(
    READ
    "${windows_credential_manager_file}"
    windows_credential_manager_contents
)
if(
    NOT windows_credential_manager_contents
        MATCHES
            "(^|[^A-Za-z0-9_])application::effect_executor([^A-Za-z0-9_]|$)"
)
    message(
        FATAL_ERROR
        "Windows Credential Manager adapter requires the host effect executor"
    )
endif()

set(
    required_windows_credential_manager_patterns
    "#[ \t]*include[ \t]*[<\"]wincred\\.h[>\"]"
    "CredWriteW[ \t\r\n]*\\("
    "CredReadW[ \t\r\n]*\\("
    "CredDeleteW[ \t\r\n]*\\("
    "CredFree[ \t\r\n]*\\("
    "GetLastError[ \t\r\n]*\\("
    "SecureZeroMemory[ \t\r\n]*\\("
)
foreach(
    required_pattern
    IN LISTS required_windows_credential_manager_patterns
)
    if(NOT windows_credential_manager_contents MATCHES "${required_pattern}")
        message(
            FATAL_ERROR
            "Windows Credential Manager adapter must use the selected "
            "wide-character lifecycle: ${required_pattern}"
        )
    endif()
endforeach()

set(
    required_windows_credential_manager_constants
    CRED_TYPE_GENERIC
    CRED_PERSIST_LOCAL_MACHINE
    CRED_MAX_GENERIC_TARGET_NAME_LENGTH
    windows_credential_manager_maximum_blob_bytes
)
foreach(
    required_constant
    IN LISTS required_windows_credential_manager_constants
)
    if(
        NOT windows_credential_manager_contents
            MATCHES
                "(^|[^A-Za-z0-9_])${required_constant}([^A-Za-z0-9_]|$)"
    )
        message(
            FATAL_ERROR
            "Windows Credential Manager adapter must retain the selected "
            "generic, local-machine, bounded credential property "
            "${required_constant}"
        )
    endif()
endforeach()

set(
    required_windows_credential_manager_bounds
    "<=[ \t\r\n]*CRED_MAX_GENERIC_TARGET_NAME_LENGTH"
    "windows_credential_manager_maximum_blob_bytes[ \t\r\n]*=[ \t\r\n]*5U[ \t\r\n]*\\*[ \t\r\n]*512U"
    "application::maximum_github_credential_bytes[ \t\r\n]*==[ \t\r\n]*windows_credential_manager_maximum_blob_bytes"
    "CredentialBlobSize[ \t\r\n]*<=[ \t\r\n]*windows_credential_manager_maximum_blob_bytes"
)
foreach(required_bound IN LISTS required_windows_credential_manager_bounds)
    if(NOT windows_credential_manager_contents MATCHES "${required_bound}")
        message(
            FATAL_ERROR
            "Windows credential bound missing: ${required_bound}"
        )
    endif()
endforeach()

function(
    reject_windows_credential_manager_surface
    forbidden_pattern
    surface_name
)
    if(windows_credential_manager_contents MATCHES "${forbidden_pattern}")
        message(
            FATAL_ERROR
            "Forbidden ${surface_name} surface in "
            "${windows_credential_manager_relative}: ${forbidden_pattern}"
        )
    endif()
endfunction()

reject_windows_credential_manager_surface(
    "(application::decision_executor|DecisionLane|decision_lane)"
    "serialized decision-executor"
)
reject_windows_credential_manager_surface(
    "CRED_MAX_CREDENTIAL_BLOB_SIZE"
    "toolchain-varying legacy credential-blob bound"
)
reject_windows_credential_manager_surface(
    "(^|[^A-Za-z0-9_])(Cred(Write|Read|Delete)A?|P?CREDENTIALA?)([^A-Za-z0-9_]|$)"
    "ANSI/encoding-neutral Credential Manager"
)
reject_windows_credential_manager_surface(
    "(^|[^A-Za-z0-9_])Cred(UI|Enumerate|FindBestCredential|GetTargetInfo|ReadDomainCredentials|WriteDomainCredentials|IsProtected|Protect|Unprotect|Rename|MarshalCredential|UnmarshalCredential)[A-Za-z0-9_]*"
    "unselected Credential Manager"
)

set(
    windows_credential_manager_effects
    "${windows_credential_manager_contents}"
)
foreach(
    selected_effect
    IN ITEMS
        CredWriteW
        CredReadW
        CredDeleteW
        CredFree
)
    string(
        REPLACE
        "${selected_effect}"
        ""
        windows_credential_manager_effects
        "${windows_credential_manager_effects}"
    )
endforeach()
if(
    windows_credential_manager_effects
        MATCHES
            "(^|[^A-Za-z0-9_])Cred[A-Z][A-Za-z0-9_]*"
)
    message(
        FATAL_ERROR
        "Windows Credential Manager adapter uses a credential effect outside "
        "the selected CredWriteW, CredReadW, CredDeleteW, and CredFree "
        "lifecycle"
    )
endif()

reject_windows_credential_manager_surface(
    "(#[ \t]*include[ \t]*[<\"](dpapi|wincrypt|ncryptprotect)\\.h[>\"]|(^|[^A-Za-z0-9_])(CryptProtectData|CryptUnprotectData|CryptProtectMemory|CryptUnprotectMemory|NCryptProtectSecret|NCryptUnprotectSecret|DATA_BLOB|NCRYPT_DESCRIPTOR_HANDLE)([^A-Za-z0-9_]|$)|Crypt32)"
    "DPAPI"
)
reject_windows_credential_manager_surface(
    "(Windows[.:]+Security[.:]+Credentials|PasswordVault|PasswordCredential|#[ \t]*include[ \t]*[<\"]winrt/Windows\\.Security\\.Credentials\\.h[>\"])"
    "Credential Locker"
)
reject_windows_credential_manager_surface(
    "(^|[^A-Za-z0-9_])CRED_PERSIST_(SESSION|ENTERPRISE)([^A-Za-z0-9_]|$)"
    "non-local Credential Manager persistence"
)
reject_windows_credential_manager_surface(
    "(^|[^A-Za-z0-9_])CRED_PRESERVE_CREDENTIAL_BLOB([^A-Za-z0-9_]|$)"
    "credential-blob preservation"
)
reject_windows_credential_manager_surface(
    "(^|[^A-Za-z0-9_])CRED_TYPE_(DOMAIN_PASSWORD|DOMAIN_CERTIFICATE|DOMAIN_VISIBLE_PASSWORD|GENERIC_CERTIFICATE|DOMAIN_EXTENDED|MAXIMUM|MAXIMUM_EX)([^A-Za-z0-9_]|$)"
    "non-generic credential type"
)
reject_windows_credential_manager_surface(
    "(#[ \t]*include[ \t]*[<\"]winreg\\.h[>\"]|(^|[^A-Za-z0-9_])(Reg(Open|Create|Set|Get|Query|Delete|Close|Load|Save|Restore|Replace|Copy|Enum|Flush|Connect)[A-Za-z0-9_]*|HKEY_[A-Z0-9_]+)([^A-Za-z0-9_]|$))"
    "registry"
)
reject_windows_credential_manager_surface(
    "(#[ \t]*include[ \t]*<(filesystem|fstream)>|std::filesystem|std::([io]?fstream|[io]stream)|(^|[^A-Za-z0-9_.:])(fopen|freopen|fread|fwrite|open|openat|creat|read|write|pread|pwrite|((Create|Open|Read|Write|Delete|Move|Copy|Replace)File)[AW]?)[ \t\r\n]*\\()"
    "filesystem"
)
reject_windows_credential_manager_surface(
    "([Ss][Qq][Ll][Ii][Tt][Ee]3?|QSettings|ApplicationData[.:]+Current|LocalSettings|RoamingSettings)"
    "settings/database"
)
reject_windows_credential_manager_surface(
    "((^|[^A-Za-z0-9_])(_?w?getenv|_?w?putenv|setenv|unsetenv|GetEnvironment(Strings|Variable)[AW]?|FreeEnvironmentStrings[AW]?|SetEnvironmentVariable[AW]?)[ \t\r\n]*\\(|(^|[^A-Za-z0-9_])environ([^A-Za-z0-9_]|$)|QProcessEnvironment)"
    "environment"
)
reject_windows_credential_manager_surface(
    "(QProcess|CreateProcess(AsUser|WithLogon|WithToken)?[AW]?|ShellExecute(Ex)?[AW]?|WinExec|(^|[^A-Za-z0-9_])(system|popen|_popen|fork|exec(l|le|lp|v|ve|vp|vpe))[ \t\r\n]*\\()"
    "command/process"
)
reject_windows_credential_manager_surface(
    "(QNetwork[A-Za-z0-9_]*|QtNetwork|WinHttp[A-Za-z0-9_]*|Internet(Open|Connect|ReadFile|WriteFile|CloseHandle)[AW]?|URLDownloadToFile[AW]?|Windows[.:]+Web[.:]+Http|HttpClient|IXMLHTTPRequest|#[ \t]*include[ \t]*[<\"]curl/|CURL[A-Za-z0-9_]*|(^|[^A-Za-z0-9_])(socket|connect|send|recv|WSA[A-Za-z0-9_]*)[ \t\r\n]*\\()"
    "network"
)
reject_windows_credential_manager_surface(
    "(#[ \t]*include[ \t]*[<\"](Q|Qt|Windows\\.UI)|(^|[^A-Za-z0-9_])(Q[A-Z][A-Za-z0-9_]*|MessageBox[AW]?|CreateWindow(Ex)?[AW]?|DialogBox[A-Za-z0-9_]*|TaskDialog[A-Za-z0-9_]*)([^A-Za-z0-9_]|$))"
    "Qt/UI"
)
reject_windows_credential_manager_surface(
    "(FormatMessage[AW]?|#[ \t]*include[ \t]*<format>|std::(format|vformat|to_string|to_wstring)|([io]stringstream|snprintf|sprintf|swprintf)[ \t\r\n]*\\(|QString::number|std::system_category|std::error_code)"
    "raw operating-system error formatting"
)
reject_windows_credential_manager_surface(
    "(std::(cout|cerr|clog|wcout|wcerr|wclog)|TraceLogging[A-Za-z0-9_]*|EventWrite[A-Za-z0-9_]*|(^|[^A-Za-z0-9_])(printf|fprintf|wprintf|fwprintf|puts|fputs|perror|syslog|OutputDebugString[AW]?|ReportEvent[AW]?|qDebug|qInfo|qWarning|qCritical|qFatal)[ \t\r\n]*\\(|spdlog)"
    "logging/printing"
)

file(
    READ
    "${linux_secret_service_file}"
    linux_secret_service_contents
)
string(
    REGEX REPLACE
    "\"[ \t\r\n]*\""
    ""
    linux_secret_service_normalized_contents
    "${linux_secret_service_contents}"
)

if(
    NOT linux_secret_service_contents
        MATCHES
            "(^|[^A-Za-z0-9_])application::effect_executor([^A-Za-z0-9_]|$)"
)
    message(
        FATAL_ERROR
        "Linux Secret Service adapter requires the host effect executor"
    )
endif()

set(
    required_linux_secret_service_patterns
    "#[ \t]*include[ \t]*[<\"]libsecret/secret\\.h[>\"]"
    "canonical_service_bus_name[ \t\r\n]*=[ \t\r\n]*\"org\\.freedesktop\\.secrets\""
    "G_DEFINE_TYPE[ \t\r\n]*\\([^\\)]*non_prompting_secret_service[^\\)]*SECRET_TYPE_SERVICE[ \t\r\n]*\\)"
    "secret_service_open_sync[ \t\r\n]*\\([ \t\r\n]*non_prompting_secret_service_get_type[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*,[ \t\r\n]*canonical_service_bus_name\\.data[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*,[ \t\r\n]*SECRET_SERVICE_NONE"
    "if[ \t\r\n]*\\([ \t\r\n]*Service[ \t\r\n]*==[ \t\r\n]*nullptr[ \t\r\n]*\\)[ \t\r\n]*\\{[ \t\r\n]*const[ \t\r\n]+auto[ \t\r\n]+failure[ \t\r\n]*=[ \t\r\n]*mapped_error[ \t\r\n]*\\([ \t\r\n]*error\\.get[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*\\)[ \t\r\n]*;"
    "failure[ \t\r\n]*==[ \t\r\n]*application::github_credential_vault_failure::[ \t\r\n]*operation_failed[ \t\r\n]*\\?[ \t\r\n]*application::github_credential_vault_failure::[ \t\r\n]*unavailable[ \t\r\n]*:[ \t\r\n]*failure"
    "g_dbus_proxy_get_name[ \t\r\n]*\\([ \t\r\n]*G_DBUS_PROXY[ \t\r\n]*\\([ \t\r\n]*Service[ \t\r\n]*\\)"
    "if[ \t\r\n]*\\([ \t\r\n]*actual_name[ \t\r\n]*==[ \t\r\n]*nullptr[ \t\r\n]*[|][|][ \t\r\n]*std::strcmp[ \t\r\n]*\\([ \t\r\n]*actual_name[ \t\r\n]*,[ \t\r\n]*canonical_service_bus_name\\.data[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*\\)[ \t\r\n]*!=[ \t\r\n]*0[ \t\r\n]*\\)"
    "g_object_unref[ \t\r\n]*\\([ \t\r\n]*Service[ \t\r\n]*\\)[ \t\r\n]*;[ \t\r\n]*Service[ \t\r\n]*=[ \t\r\n]*nullptr[ \t\r\n]*;[ \t\r\n]*return[ \t\r\n]+std::unexpected[ \t\r\n]*\\{[ \t\r\n]*application::github_credential_vault_failure::unavailable"
    "secret_service_ensure_session_sync[ \t\r\n]*\\([ \t\r\n]*Service[ \t\r\n]*,[ \t\r\n]*nullptr[ \t\r\n]*,[ \t\r\n]*session_error\\.out[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*\\)"
    "mapped_error[ \t\r\n]*\\([ \t\r\n]*session_error\\.get[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*\\)"
    "secret_collection_for_alias_sync[ \t\r\n]*\\([^\\)]*SECRET_COLLECTION_DEFAULT[ \t\r\n]*,[ \t\r\n]*SECRET_COLLECTION_NONE"
    "production_schema_identifier[ \t\r\n]*=[ \t\r\n]*\"org\\.betterpullreview\\.github-native-credential\\.secret-service\\.v2\""
    "integration_test_schema_identifier[ \t\r\n]*=[ \t\r\n]*\"org\\.betterpullreview\\.github-native-credential\\.secret-service\\.integration-test\\.v2\""
    "authority_attribute[ \t\r\n]*=[ \t\r\n]*\"authority\""
    "schema_attribute[ \t\r\n]*=[ \t\r\n]*\"xdg:schema\""
    "SECRET_SCHEMA_NONE"
    "SECRET_SCHEMA_ATTRIBUTE_STRING"
    "g_hash_table_size[^\r\n]*!=[ \t]*2U"
    "static_assert[ \t\r\n]*\\([ \t\r\n]*linux_secret_service_authority_account_bytes[ \t\r\n]*==[ \t\r\n]*574U[ \t\r\n]*\\)"
    "nibble[ \t\r\n]*<[ \t\r\n]*16U"
    "authority\\.Profile\\.value[ \t\r\n]*\\("
    "authority\\.ProfileRevision\\.value[ \t\r\n]*\\("
    "authority\\.Host\\.value[ \t\r\n]*\\("
    "authority\\.Account\\.value[ \t\r\n]*\\("
    "append_hex_u8[ \t\r\n]*\\([^\\)]*server_contract_discriminator[ \t\r\n]*\\([ \t\r\n]*authority\\.ServerContract"
    "authority\\.Hostname\\.value[ \t\r\n]*\\("
    "static_cast<std::uint8_t>[ \t\r\n]*\\([ \t\r\n]*hostname\\.size[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*\\)"
    "index[ \t\r\n]*<[ \t\r\n]*application::maximum_github_hostname_bytes"
    "static_cast<unsigned[ \t\r\n]+char>[ \t\r\n]*\\([ \t\r\n]*hostname\\[index\\][ \t\r\n]*\\)"
    "append_hex_u8[ \t\r\n]*\\([ \t\r\n]*encoded[ \t\r\n]*,[ \t\r\n]*offset[ \t\r\n]*,[ \t\r\n]*value[ \t\r\n]*\\)"
    "binary_content_type[ \t\r\n]*=[ \t\r\n]*\"application/octet-stream\""
    "gnome_keyring_content_type[ \t\r\n]*=[ \t\r\n]*\"text/plain\""
    "secret_value_new[ \t\r\n]*\\("
    "secret_value_get_content_type[ \t\r\n]*\\("
    "recognized_content_type[ \t\r\n]*=[ \t\r\n]*content_type[ \t\r\n]*!=[ \t\r\n]*nullptr[ \t\r\n]*&&[ \t\r\n]*\\([ \t\r\n]*std::strcmp[ \t\r\n]*\\([ \t\r\n]*content_type[ \t\r\n]*,[ \t\r\n]*binary_content_type\\.data[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*\\)[ \t\r\n]*==[ \t\r\n]*0[ \t\r\n]*[|][|][ \t\r\n]*std::strcmp[ \t\r\n]*\\([ \t\r\n]*content_type[ \t\r\n]*,[ \t\r\n]*gnome_keyring_content_type\\.data[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*\\)[ \t\r\n]*==[ \t\r\n]*0[ \t\r\n]*\\)[ \t\r\n]*;"
    "if[ \t\r\n]*\\([ \t\r\n]*value[ \t\r\n]*==[ \t\r\n]*nullptr[ \t\r\n]*[|][|][ \t\r\n]*content_type[ \t\r\n]*==[ \t\r\n]*nullptr[ \t\r\n]*[|][|][ \t\r\n]*![ \t\r\n]*recognized_content_type[ \t\r\n]*\\)"
    "secret_value_get[ \t\r\n]*\\("
    "secret_value_unref[ \t\r\n]*\\("
    "application::maximum_github_credential_bytes"
    "secret_collection_search_sync[ \t\r\n]*\\("
    "SECRET_SEARCH_ALL"
    "secret_item_create_sync[ \t\r\n]*\\("
    "SECRET_ITEM_CREATE_REPLACE"
    "secret_item_load_secret_sync[ \t\r\n]*\\("
    "secret_item_get_secret[ \t\r\n]*\\("
    "secret_item_delete_sync[ \t\r\n]*\\("
    "secret_collection_get_locked[ \t\r\n]*\\("
    "secret_item_get_locked[ \t\r\n]*\\("
    "prompt_sync[ \t\r\n]*=[ \t\r\n]*deny_prompt_sync"
    "prompt_async[ \t\r\n]*=[ \t\r\n]*deny_prompt_async"
    "prompt_finish[ \t\r\n]*=[ \t\r\n]*deny_prompt_finish"
    "deny_prompt_sync[^{]*\\{[^}]*dismiss_prompt[ \t\r\n]*\\([ \t\r\n]*prompt[ \t\r\n]*\\)"
    "deny_prompt_async[^{]*\\{[^}]*dismiss_prompt[ \t\r\n]*\\([ \t\r\n]*prompt[ \t\r\n]*\\)"
    "prompt_dismiss_timeout_milliseconds[ \t\r\n]*=[ \t\r\n]*[1-9][0-9']*"
    "g_dbus_proxy_call_sync[ \t\r\n]*\\([ \t\r\n]*G_DBUS_PROXY[ \t\r\n]*\\([ \t\r\n]*prompt[ \t\r\n]*\\)[ \t\r\n]*,[ \t\r\n]*\"Dismiss\"[ \t\r\n]*,[ \t\r\n]*g_variant_new[ \t\r\n]*\\([ \t\r\n]*\"\\(\\)\"[ \t\r\n]*\\)[ \t\r\n]*,[ \t\r\n]*G_DBUS_CALL_FLAGS_NO_AUTO_START[ \t\r\n]*,[ \t\r\n]*prompt_dismiss_timeout_milliseconds"
    "SECRET_ERROR_IS_LOCKED"
)
foreach(required_pattern IN LISTS required_linux_secret_service_patterns)
    if(
        NOT linux_secret_service_normalized_contents
            MATCHES "${required_pattern}"
    )
        message(
            FATAL_ERROR
            "Linux Secret Service adapter must retain the selected exact, "
            "binary, noninteractive lifecycle: ${required_pattern}"
        )
    endif()
endforeach()

string(
    REGEX REPLACE
    "[ \t\r\n]+"
    " "
    linux_secret_service_compact_contents
    "${linux_secret_service_contents}"
)

string(
    FIND
    "${linux_secret_service_compact_contents}"
    "secret_service_open_sync"
    linux_secret_service_open_offset
)
string(
    FIND
    "${linux_secret_service_compact_contents}"
    "canonical_service_bus_name.data()) != 0"
    linux_secret_service_name_validation_offset
)
string(
    FIND
    "${linux_secret_service_compact_contents}"
    "secret_service_ensure_session_sync"
    linux_secret_service_session_offset
)
string(
    FIND
    "${linux_secret_service_compact_contents}"
    "secret_collection_for_alias_sync"
    linux_secret_service_collection_offset
)
if(
    linux_secret_service_open_offset LESS 0
    OR linux_secret_service_name_validation_offset LESS 0
    OR linux_secret_service_session_offset LESS 0
    OR linux_secret_service_collection_offset LESS 0
    OR NOT linux_secret_service_open_offset
        LESS linux_secret_service_name_validation_offset
    OR NOT linux_secret_service_name_validation_offset
        LESS linux_secret_service_session_offset
    OR NOT linux_secret_service_session_offset
        LESS linux_secret_service_collection_offset
)
    message(
        FATAL_ERROR
        "Linux Secret Service session ordering invalid: open without a "
        "session, validate the canonical name, then ensure the session"
    )
endif()

math(
    EXPR
    linux_secret_service_proxy_lifecycle_length
    "${linux_secret_service_collection_offset} - ${linux_secret_service_open_offset}"
)
string(
    SUBSTRING
    "${linux_secret_service_compact_contents}"
    "${linux_secret_service_open_offset}"
    "${linux_secret_service_proxy_lifecycle_length}"
    linux_secret_service_proxy_lifecycle
)
string(
    REGEX MATCHALL
    "failure[ ]*==[ ]*application::github_credential_vault_failure::[ ]*operation_failed[ ]*\\?[ ]*application::github_credential_vault_failure::[ ]*unavailable[ ]*:[ ]*failure"
    linux_secret_service_normalized_failures
    "${linux_secret_service_proxy_lifecycle}"
)
list(
    LENGTH
    linux_secret_service_normalized_failures
    linux_secret_service_normalized_failure_count
)
if(NOT linux_secret_service_normalized_failure_count EQUAL 2)
    message(
        FATAL_ERROR
        "Linux Secret Service failure normalization invalid: both open and "
        "ensure-session operation failures must become unavailable"
    )
endif()

string(
    REGEX MATCHALL
    "g_object_unref[ ]*\\([ ]*Service[ ]*\\)"
    linux_secret_service_proxy_cleanups
    "${linux_secret_service_proxy_lifecycle}"
)
list(
    LENGTH
    linux_secret_service_proxy_cleanups
    linux_secret_service_proxy_cleanup_count
)
string(
    FIND
    "${linux_secret_service_proxy_lifecycle}"
    "secret_service_ensure_session_sync"
    linux_secret_service_lifecycle_session_offset
)
string(
    FIND
    "${linux_secret_service_proxy_lifecycle}"
    "mapped_error(session_error.get())"
    linux_secret_service_session_error_offset
)
string(
    FIND
    "${linux_secret_service_proxy_lifecycle}"
    "g_object_unref(Service)"
    linux_secret_service_session_cleanup_offset
    REVERSE
)
if(
    NOT linux_secret_service_proxy_cleanup_count EQUAL 2
    OR linux_secret_service_session_error_offset LESS 0
    OR linux_secret_service_session_cleanup_offset LESS 0
    OR NOT linux_secret_service_lifecycle_session_offset
        LESS linux_secret_service_session_error_offset
    OR NOT linux_secret_service_session_error_offset
        LESS linux_secret_service_session_cleanup_offset
)
    message(
        FATAL_ERROR
        "Linux Secret Service ensure-session failure cleanup invalid"
    )
endif()

if(
    NOT linux_secret_service_compact_contents
        MATCHES
            "secret_value_new[ ]*\\([ ]*reinterpret_cast<const char\\*>[ ]*\\([ ]*bytes\\.data[ ]*\\([ ]*\\)[ ]*\\)[ ]*,[ ]*static_cast<gssize>[ ]*\\([ ]*bytes\\.size[ ]*\\([ ]*\\)[ ]*\\)[ ]*,[ ]*binary_content_type\\.data[ ]*\\([ ]*\\)[ ]*\\)"
)
    message(
        FATAL_ERROR
        "Linux Secret Service adapter must store only application/octet-stream "
        "SecretValue bytes"
    )
endif()

foreach(
    content_type_identifier_and_count
    IN ITEMS
        "binary_content_type;3"
        "gnome_keyring_content_type;2"
)
    list(GET content_type_identifier_and_count 0 content_type_identifier)
    list(GET content_type_identifier_and_count 1 expected_content_type_count)
    string(
        REGEX MATCHALL
        "${content_type_identifier}"
        linux_content_type_uses
        "${linux_secret_service_contents}"
    )
    list(LENGTH linux_content_type_uses actual_content_type_count)
    if(NOT actual_content_type_count EQUAL expected_content_type_count)
        message(
            FATAL_ERROR
            "Linux Secret Service adapter must recognize exactly the selected "
            "two content types without broadening storage or retrieval"
        )
    endif()
endforeach()

string(
    REGEX MATCHALL
    "secret_service_open_sync[ \t\r\n]*\\("
    linux_secret_service_open_calls
    "${linux_secret_service_contents}"
)
list(LENGTH linux_secret_service_open_calls linux_secret_service_open_count)
if(NOT linux_secret_service_open_count EQUAL 1)
    message(
        FATAL_ERROR
        "Linux Secret Service adapter must open exactly one independently "
        "pinned service proxy"
    )
endif()

string(
    REGEX MATCHALL
    "secret_service_ensure_session_sync[ \t\r\n]*\\("
    linux_secret_service_session_calls
    "${linux_secret_service_contents}"
)
list(
    LENGTH
    linux_secret_service_session_calls
    linux_secret_service_session_count
)
if(NOT linux_secret_service_session_count EQUAL 1)
    message(
        FATAL_ERROR
        "Linux Secret Service adapter must ensure exactly one session after "
        "canonical service-name validation"
    )
endif()

string(
    REGEX MATCHALL
    "const[ \t]+auto[ \t]+service[ \t]*=[ \t]*service_proxy"
    linux_verified_service_gates
    "${linux_secret_service_contents}"
)
list(LENGTH linux_verified_service_gates linux_verified_service_gate_count)
if(NOT linux_verified_service_gate_count EQUAL 3)
    message(
        FATAL_ERROR
        "Linux Secret Service adapter must verify the selected service before "
        "every store, retrieve, and delete item lifecycle"
    )
endif()

function(reject_linux_secret_service_surface forbidden_pattern surface_name)
    if(linux_secret_service_contents MATCHES "${forbidden_pattern}")
        message(
            FATAL_ERROR
            "Forbidden ${surface_name} surface in "
            "${linux_secret_service_relative}: ${forbidden_pattern}"
        )
    endif()
endfunction()

reject_linux_secret_service_surface(
    "(application::decision_executor|DecisionLane|decision_lane)"
    "serialized decision-executor"
)
reject_linux_secret_service_surface(
    "(SECRET_SERVICE_BUS_NAME|DBUS_SESSION_BUS_ADDRESS|XDG_RUNTIME_DIR|(^|[^A-Za-z0-9_])(g_)?(getenv|setenv|unsetenv|putenv|listenv)[ \t\r\n]*\\(|(^|[^A-Za-z0-9_])environ([^A-Za-z0-9_]|$)|QProcessEnvironment)"
    "environment/service-name redirection"
)
reject_linux_secret_service_surface(
    "SECRET_SERVICE_OPEN_SESSION"
    "session establishment during service open"
)
reject_linux_secret_service_surface(
    "((^|[^A-Za-z0-9_])secret_service_(get|get_sync|store|store_sync|lookup|lookup_sync|clear|clear_sync|search|search_sync|unlock|unlock_sync|create_collection|create_collection_sync)([^A-Za-z0-9_]|$)|(^|[^A-Za-z0-9_])secret_password_[A-Za-z0-9_]*|SECRET_SEARCH_(UNLOCK|LOAD_SECRETS)|SECRET_COLLECTION_SESSION)"
    "global/convenience Secret Service"
)
reject_linux_secret_service_surface(
    "((^|[^A-Za-z0-9_])secret_(collection|service)_(create|delete|unlock)[A-Za-z0-9_]*|\"CreateCollection\"|\"DeleteCollection\"|\"Unlock\")"
    "collection creation/deletion/unlock"
)
reject_linux_secret_service_surface(
    "((^|[^A-Za-z0-9_])secret_(service_prompt|prompt_perform|prompt_run)[A-Za-z0-9_]*|\"Prompt\"|org\\.freedesktop\\.Secret\\.Prompt\\.Prompt)"
    "interactive prompt"
)
reject_linux_secret_service_surface(
    "(org\\.freedesktop\\.Secret\\.Service|(^|[^A-Za-z0-9_])g_bus_get(_sync)?[ \t\r\n]*\\(|(^|[^A-Za-z0-9_])g_dbus_proxy_new(_sync|_for_bus|_for_bus_sync)?[ \t\r\n]*\\(|(^|[^A-Za-z0-9_])GDBusConnection([^A-Za-z0-9_]|$))"
    "alternate service proxy"
)
reject_linux_secret_service_surface(
    "(GObjectConstructParam|g_param_spec_get_name|g_value_set_string|\"g-name\"|(^|[^A-Za-z0-9_])g_object_set(_property)?[ \t\r\n]*\\()"
    "ineffective service-name property override"
)
reject_linux_secret_service_surface(
    "(g_ascii_strcasecmp|g_str_has_(prefix|suffix)|(^|[^A-Za-z0-9_])(strcasecmp|strncasecmp|strncmp|strstr)[ \t\r\n]*\\(|\\.starts_with[ \t\r\n]*\\(|\\.ends_with[ \t\r\n]*\\(|\\.contains[ \t\r\n]*\\(|\"(text|application)/\\*\"|\"\\*/\\*\")"
    "broad SecretValue content-type bypass"
)
reject_linux_secret_service_surface(
    "(QtKeychain|QKeychain|KWallet|kwallet|(^|[^A-Za-z0-9_])gnome_keyring_[A-Za-z0-9_]*[ \t\r\n]*\\(|libgnome-keyring|secret-tool|PasswordVault|PasswordCredential)"
    "alternate credential vault"
)
reject_linux_secret_service_surface(
    "(#[ \t]*include[ \t]*<(filesystem|fstream)>|std::filesystem|std::([io]?fstream|[io]stream)|(^|[^A-Za-z0-9_.:])(fopen|freopen|fread|fwrite|open|openat|creat|read|write|pread|pwrite|g_file_(get|set)_contents|g_(open|fopen|mkdir|mkdir_with_parents|remove|rename|rmdir))[ \t\r\n]*\\(|(^|[^A-Za-z0-9_])(GFile|GIOChannel)([^A-Za-z0-9_]|$))"
    "filesystem"
)
reject_linux_secret_service_surface(
    "([Ss][Qq][Ll][Ii][Tt][Ee]3?|QSettings|GSettings|g_settings_|GKeyFile|g_key_file_|dconf)"
    "settings/database"
)
reject_linux_secret_service_surface(
    "(QProcess|GSubprocess|g_subprocess_|(^|[^A-Za-z0-9_])(g_spawn[A-Za-z0-9_]*|posix_spawn|system|popen|fork|exec(l|le|lp|v|ve|vp|vpe))[ \t\r\n]*\\()"
    "command/process"
)
reject_linux_secret_service_surface(
    "(QNetwork[A-Za-z0-9_]*|QtNetwork|G(Socket|Resolver|NetworkAddress|InetAddress|Tls)[A-Za-z0-9_]*|g_(socket|resolver|network_address|inet_address|tls)_[A-Za-z0-9_]*|SoupSession|libcurl|#[ \t]*include[ \t]*[<\"]curl/|CURL[A-Za-z0-9_]*|(^|[^A-Za-z0-9_])(socket|connect|send|recv)[ \t\r\n]*\\(|https?://)"
    "network"
)
reject_linux_secret_service_surface(
    "(#[ \t]*include[ \t]*[<\"](Q|Qt|gtk/)|(^|[^A-Za-z0-9_])(Q[A-Z][A-Za-z0-9_]*|GtkWidget|GtkWindow|GtkDialog|gtk_[A-Za-z0-9_]*)([^A-Za-z0-9_]|$))"
    "Qt/UI"
)
reject_linux_secret_service_surface(
    "(std::(cout|cerr|clog)|(^|[^A-Za-z0-9_])(printf|fprintf|puts|fputs|perror|syslog|g_log|g_debug|g_info|g_message|g_warning|g_critical|g_print|g_printerr|qDebug|qCDebug|qInfo|qCInfo|qWarning|qCWarning|qCritical|qCCritical|qFatal)[ \t\r\n]*\\(|spdlog)"
    "logging/printing"
)
reject_linux_secret_service_surface(
    "(g_dbus_error_get_remote_error|g_dbus_error_strip_remote_error|g_quark_to_string|(^|[^A-Za-z0-9_])(GError[ \t]*\\*[^;\r\n]*->[ \t]*message|error->[ \t]*message)|#[ \t]*include[ \t]*<format>|std::(format|vformat|to_string)|([io]stringstream|snprintf|sprintf)[ \t\r\n]*\\()"
    "raw operating-system error formatting"
)
reject_linux_secret_service_surface(
    "(^|[^A-Za-z0-9_])[Ff]allback([^A-Za-z0-9_]|$)"
    "credential fallback"
)

foreach(
    exact_singleton
    IN ITEMS
        "g_dbus_proxy_call_sync"
)
    string(
        REGEX MATCHALL
        "${exact_singleton}"
        linux_secret_service_singletons
        "${linux_secret_service_contents}"
    )
    list(
        LENGTH
        linux_secret_service_singletons
        linux_secret_service_singleton_count
    )
    if(NOT linux_secret_service_singleton_count EQUAL 1)
        message(
            FATAL_ERROR
            "Linux Secret Service adapter must retain exactly one bounded "
            "Dismiss call"
        )
    endif()
endforeach()

set(
    linux_secret_service_selected_effects
    "${linux_secret_service_contents}"
)
foreach(
    selected_effect
    IN ITEMS
        secret_collection_for_alias_sync
        secret_collection_get_locked
        secret_collection_search_sync
        secret_item_create_sync
        secret_item_delete_sync
        secret_item_get_attributes
        secret_item_get_locked
        secret_item_get_secret
        secret_item_load_secret_sync
        secret_service_ensure_session_sync
        secret_service_open_sync
        secret_value_get_content_type
        secret_value_get
        secret_value_new
        secret_value_owner
        secret_value_unref
)
    string(
        REPLACE
        "${selected_effect}"
        ""
        linux_secret_service_selected_effects
        "${linux_secret_service_selected_effects}"
    )
endforeach()
if(
    linux_secret_service_selected_effects
        MATCHES
            "(^|[^A-Za-z0-9_])secret_(collection|item|password|schema|service|value)_[A-Za-z0-9_]*[ \t\r\n]*\\("
)
    message(
        FATAL_ERROR
        "Linux Secret Service adapter uses a libsecret effect outside the "
        "selected open, default-collection, search, binary item lifecycle"
    )
endif()

set(
    alternate_process_patterns
    "(NSTask|GSubprocess|g_subprocess_|CreateProcess(AsUser|WithLogon|WithToken)?[AW]?|ShellExecute(Ex)?[AW]?|WinExec|posix_spawnp?|(^|[^A-Za-z0-9_])(_?w?system|_?w?popen|fork|vfork|clone3?|exec(l|le|lp|v|ve|vp|vpe)|_?w?spawn(l|le|lp|lpe|v|ve|vp|vpe))[ \t\r\n]*\\()"
    "(#[ \t]*include[ \t]*[<\"]boost/process|boost::process|Poco::Process|wx(Process|Execute)|uv_spawn|g_spawn[A-Za-z0-9_]*)"
    "(/bin/(ba|z|k|c)?sh|\"(/usr/bin/)?(sh|bash|zsh|ksh|csh|fish|cmd(\\.exe)?|powershell(\\.exe)?|pwsh(\\.exe)?)\"|(^|[^A-Za-z0-9_])(sh|bash|zsh|cmd)[ \t]+(-c|/c)([^A-Za-z0-9_]|$))"
)
foreach(source_file IN LISTS source_files)
    file(READ "${source_file}" contents)
    foreach(alternate_process_pattern IN LISTS alternate_process_patterns)
        if(contents MATCHES "${alternate_process_pattern}")
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${source_file}")
            message(
                FATAL_ERROR
                "Alternate process or shell API is forbidden in "
                "${relative_file}: ${alternate_process_pattern}"
            )
        endif()
    endforeach()
endforeach()

set(
    github_cli_invocation_and_environment_patterns
    "\"api\""
    "\"--hostname\""
    "\"--method\""
    "\"--input\""
    "GH_PROMPT_DISABLED=1"
    "GH_NO_UPDATE_NOTIFIER=1"
    "GH_NO_EXTENSION_UPDATE_NOTIFIER=1"
    "GH_SPINNER_DISABLED=1"
    "NO_COLOR=1"
    "CLICOLOR=0"
    "\"HOME\""
    "\"XDG_RUNTIME_DIR\""
    "\"DBUS_SESSION_BUS_ADDRESS\""
    "\"USERPROFILE\""
    "\"APPDATA\""
    "\"LOCALAPPDATA\""
    "\"HTTPS_PROXY\""
    "\"NO_PROXY\""
    "\"SSL_CERT_FILE\""
    "\"SSL_CERT_DIR\""
)
foreach(
    cli_construction_pattern
    IN LISTS github_cli_invocation_and_environment_patterns
)
    if(
        cli_construction_pattern STREQUAL "\"HOME\""
        OR cli_construction_pattern STREQUAL "\"PATH\""
        OR cli_construction_pattern STREQUAL "\"SystemRoot\""
    )
        verify_effect_owner(
            "${cli_construction_pattern}"
            "process environment construction"
            "${github_cli_transport_relative}"
            "${git_process_runner_relative}"
        )
    else()
        verify_effect_owner(
            "${cli_construction_pattern}"
            "GitHub CLI invocation/environment construction"
            "${github_cli_transport_relative}"
        )
    endif()
endforeach()

set(
    github_cli_runner_environment_validation_patterns
    "equal_environment_names[ \t\r\n]*\\([ \t\r\n]*entry\\.Name[ \t\r\n]*,[ \t\r\n]*\"PATH\"[ \t\r\n]*\\)"
    "equal_environment_names[ \t\r\n]*\\([ \t\r\n]*entry\\.Name[ \t\r\n]*,[ \t\r\n]*\"SystemRoot\"[ \t\r\n]*\\)"
)
set(
    github_process_runner_without_environment_validation
    "${github_process_runner_contents}"
)
foreach(
    runner_environment_validation_pattern
    IN LISTS github_cli_runner_environment_validation_patterns
)
    string(
        REGEX REPLACE
        "${runner_environment_validation_pattern}"
        ""
        github_process_runner_without_environment_validation
        "${github_process_runner_without_environment_validation}"
    )
endforeach()
if(
    github_process_runner_without_environment_validation
        MATCHES "\"(PATH|SystemRoot)\""
)
    message(
        FATAL_ERROR
        "GitHub CLI runner may name PATH and SystemRoot only while validating "
        "the transport-constructed Windows environment"
    )
endif()

foreach(source_file IN LISTS source_files)
    get_filename_component(normalized_source_file "${source_file}" ABSOLUTE)
    file(READ "${source_file}" contents)
    if(
        contents MATCHES "\"(PATH|SystemRoot)\""
        AND NOT normalized_source_file STREQUAL github_cli_transport_file
        AND NOT normalized_source_file STREQUAL github_process_runner_file
        AND NOT normalized_source_file STREQUAL git_process_runner_file
    )
        file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${source_file}")
        message(
            FATAL_ERROR
            "GitHub CLI invocation/environment construction ownership "
            "violation in ${relative_file}; PATH and SystemRoot are owned by "
            "${github_cli_transport_relative}, with validation only in "
            "${github_process_runner_relative}, or by the isolated installed-"
            "Git runner ${git_process_runner_relative}"
        )
    endif()

    file(
        RELATIVE_PATH
        relative_source_file
        "${BPR_ROOT}"
        "${source_file}"
    )
    if(
        relative_source_file MATCHES "^src/adapters/github/"
        AND contents MATCHES "\"\\.git\""
        AND NOT normalized_source_file STREQUAL github_cli_transport_file
        AND NOT normalized_source_file STREQUAL github_process_runner_file
    )
        message(
            FATAL_ERROR
            "GitHub CLI neutral working-directory validation ownership "
            "violation in ${relative_source_file}; only "
            "${github_cli_transport_relative} and "
            "${github_process_runner_relative} may validate it"
        )
    endif()
endforeach()

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
