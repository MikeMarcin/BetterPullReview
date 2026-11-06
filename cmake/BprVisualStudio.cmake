# BPR uses its pinned dependency providers, never Visual Studio's user-wide
# vcpkg integration. Set this before project() and all dependency targets.
# Preserve unrelated caller globals; MSBuild property names ignore case.
if(CMAKE_GENERATOR MATCHES "^Visual Studio ")
    list(
        FILTER CMAKE_VS_GLOBALS EXCLUDE
        REGEX "^[Vv][Cc][Pp][Kk][Gg][Ee][Nn][Aa][Bb][Ll][Ee][Dd]="
    )
    list(APPEND CMAKE_VS_GLOBALS "VcpkgEnabled=false")
endif()
