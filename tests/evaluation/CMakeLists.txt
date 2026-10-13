# [[BPR-1458]] and [[BPR-1459]]. RFC-BPR-0083's evaluation-only candidates.
# tests/CMakeLists.txt adds this directory only when BPR_EVALUATE_LIBGIT2 is on;
# nothing here is part of the product, its presets or its ctest rows.
add_subdirectory(libgit2)
