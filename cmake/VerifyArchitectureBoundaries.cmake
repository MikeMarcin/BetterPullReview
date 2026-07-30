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
    "production_schema_identifier[ \t\r\n]*=[ \t\r\n]*\"org\\.betterpullreview\\.github-native-credential\\.secret-service\\.v1\""
    "integration_test_schema_identifier[ \t\r\n]*=[ \t\r\n]*\"org\\.betterpullreview\\.github-native-credential\\.secret-service\\.integration-test\\.v1\""
    "authority_attribute[ \t\r\n]*=[ \t\r\n]*\"authority\""
    "schema_attribute[ \t\r\n]*=[ \t\r\n]*\"xdg:schema\""
    "SECRET_SCHEMA_NONE"
    "SECRET_SCHEMA_ATTRIBUTE_STRING"
    "g_hash_table_size[^\r\n]*!=[ \t]*2U"
    "static_assert[ \t\r\n]*\\([ \t\r\n]*linux_secret_service_authority_account_bytes[ \t\r\n]*==[ \t\r\n]*66U[ \t\r\n]*\\)"
    "nibble[ \t\r\n]*<[ \t\r\n]*16U"
    "authority\\.Profile\\.value[ \t\r\n]*\\("
    "authority\\.ProfileRevision\\.value[ \t\r\n]*\\("
    "authority\\.Host\\.value[ \t\r\n]*\\("
    "authority\\.Account\\.value[ \t\r\n]*\\("
    "append_hex_u8[ \t\r\n]*\\([^\\)]*server_contract_discriminator[ \t\r\n]*\\([ \t\r\n]*authority\\.ServerContract"
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
