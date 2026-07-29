if(NOT DEFINED BPR_ROOT)
    message(FATAL_ERROR "BPR_ROOT is required")
endif()

file(
    GLOB_RECURSE qt_files
    "${BPR_ROOT}/src/adapters/qt/*.cpp"
    "${BPR_ROOT}/src/adapters/qt/*.h"
    "${BPR_ROOT}/src/adapters/qt/*.hpp"
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
    GLOB_RECURSE runtime_files
    "${BPR_ROOT}/CMakeLists.txt"
    "${BPR_ROOT}/cmake/*.cmake"
    "${BPR_ROOT}/src/*.cpp"
    "${BPR_ROOT}/src/*.h"
    "${BPR_ROOT}/src/*.hpp"
    "${BPR_ROOT}/src/*/CMakeLists.txt"
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
    "QNetworkAccessManager"
    "QProcess"
    "QWeb"
    "listen[ \t]*\\("
    "telemetry"
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

message(STATUS "Application and Qt architecture boundaries verified")
