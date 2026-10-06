# [[BPR-0825]]. Header-only platform definitions shared by adapters that would
# otherwise each keep their own copy. It owns the Windows owner-private object:
# the protected descriptor a private file or directory is created with, and the
# exact check that an object carries it. [[BPR-1115]] adds the Windows launch
# restriction that gives a QProcess child its three standard handles and nothing
# else, and refuses a launch whose list cannot be built. It holds no adapter
# state and no Qt, provider, file-system policy or decision about what a process
# runs, and depends on nothing first-party, so an adapter may include it without
# an adapter-to-adapter edge. [[BPR-1252]] adds the bound a frequent Unix
# launch puts on its child's descriptor sweep. [[BPR-1239]] adds the bounded wait
# for a file another handle briefly holds, which the Git adapter and Save to file
# share.
add_library(better_pull_review_platform INTERFACE)
add_library(BetterPullReview::Platform ALIAS better_pull_review_platform)
target_sources(
    better_pull_review_platform
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
        FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/include/bpr/platform/windows_private_object.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/bpr/platform/windows_standard_handles.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/bpr/platform/unix_descriptor_sweep.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/bpr/platform/windows_sharing_retry.hpp"
)
target_compile_features(better_pull_review_platform INTERFACE cxx_std_23)
if(WIN32)
    target_link_libraries(better_pull_review_platform INTERFACE Advapi32)
endif()
