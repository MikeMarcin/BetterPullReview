# Fixture data for the syntax adapter's definition selection. This file is never
# added to the build graph; it exists so a CMake file name selects the CMake
# definition by name rather than by extension.
set(BPR_FIXTURE_GREETING "hello")
message(STATUS "${BPR_FIXTURE_GREETING}")
