# CPack 4.4 emits file-keyed components only for perMachine/NONE, but WiX 5
# supports a dual-purpose package. Keep CPack's maintained template and change
# only its package scope; reject a template change rather than silently making
# an everyone-only installer. Our Start menu component uses HKMU, too.
function(bpr_configure_windows_installer_template output)
    file(READ "${CMAKE_ROOT}/Modules/Internal/CPack/WIX.template.in" template)
    set(scope "Scope=\"$(var.CPACK_WIX_INSTALL_SCOPE)\"")
    string(FIND "${template}" "${scope}" scope_position)
    if(scope_position EQUAL -1)
        message(FATAL_ERROR "CPack WiX template changed: review dual-context package scope")
    endif()
    string(REPLACE "${scope}" "Scope=\"perUserOrMachine\"" template "${template}")
    get_filename_component(output_directory "${output}" DIRECTORY)
    file(MAKE_DIRECTORY "${output_directory}")
    file(WRITE "${output}" "${template}")
endfunction()
