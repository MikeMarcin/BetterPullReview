# [[BPR-1908]]. The composition root is src/app/src/github_application.cpp and
# the responsibility files split out of it under github_application/
# ([[BPR-1905]] onward). A contract that asks what the composition composes
# reads all of them, in a fixed order. A split file's include lines are its
# own dependencies, not composition, so they are left out.
function(bpr_read_composition_source root_file out_variable)
    file(READ "${root_file}" bpr_composition_text)
    get_filename_component(bpr_composition_directory "${root_file}" DIRECTORY)
    file(GLOB bpr_composition_parts LIST_DIRECTORIES false
        "${bpr_composition_directory}/github_application/*.cpp"
        "${bpr_composition_directory}/github_application/*.hpp"
    )
    list(SORT bpr_composition_parts)
    foreach(bpr_composition_part IN LISTS bpr_composition_parts)
        file(READ "${bpr_composition_part}" bpr_composition_part_text)
        string(REGEX REPLACE "(^|\n)[ \t]*#[ \t]*include[^\n]*" "\\1" bpr_composition_part_text "${bpr_composition_part_text}")
        string(APPEND bpr_composition_text "\n${bpr_composition_part_text}")
    endforeach()
    set(${out_variable} "${bpr_composition_text}" PARENT_SCOPE)
endfunction()
