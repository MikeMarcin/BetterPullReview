include_guard(GLOBAL)

option(BPR_ENABLE_INSTALL_TARGETS "Enable install and package metadata targets" ON)

function(bpr_configure_build_surface)
  if(NOT BPR_ENABLE_INSTALL_TARGETS)
    set(CMAKE_SKIP_INSTALL_RULES ON CACHE BOOL "Disable generated install/package targets for focused builds" FORCE)
  endif()
endfunction()

function(bpr_package_export_set output package component default_component)
  if(component STREQUAL "${default_component}")
    set("${output}" "${package}Targets" PARENT_SCOPE)
  else()
    set("${output}" "${package}_${component}Targets" PARENT_SCOPE)
  endif()
endfunction()

function(bpr_attach_header_file_set target)
  set(include_dir "${CMAKE_CURRENT_SOURCE_DIR}/include")
  if(NOT EXISTS "${include_dir}")
    return()
  endif()

  file(
    GLOB_RECURSE headers
    CONFIGURE_DEPENDS
      "${include_dir}/*.h"
      "${include_dir}/*.hpp"
  )
  if(NOT headers)
    return()
  endif()

  get_target_property(target_type "${target}" TYPE)
  if(target_type STREQUAL "INTERFACE_LIBRARY")
    set(scope INTERFACE)
  else()
    set(scope PUBLIC)
  endif()

  target_sources(
    "${target}"
    "${scope}"
      FILE_SET HEADERS
      BASE_DIRS "${include_dir}"
      FILES ${headers}
  )
endfunction()

function(bpr_install_package_target target)
  set(one_value_args PACKAGE DEFAULT_COMPONENT)
  cmake_parse_arguments(BPR_INSTALL "" "${one_value_args}" "" ${ARGN})

  if(NOT BPR_INSTALL_PACKAGE)
    message(FATAL_ERROR "bpr_install_package_target(${target}) requires PACKAGE")
  endif()
  if(NOT BPR_INSTALL_DEFAULT_COMPONENT)
    message(FATAL_ERROR "bpr_install_package_target(${target}) requires DEFAULT_COMPONENT")
  endif()

  get_target_property(component "${target}" EXPORT_NAME)
  if(NOT component)
    set(component "${target}")
  endif()

  bpr_attach_header_file_set("${target}")
  bpr_package_export_set(
    export_set
    "${BPR_INSTALL_PACKAGE}"
    "${component}"
    "${BPR_INSTALL_DEFAULT_COMPONENT}"
  )

  if(NOT BPR_ENABLE_INSTALL_TARGETS)
    return()
  endif()

  get_target_property(header_sets "${target}" INTERFACE_HEADER_SETS)
  if(NOT header_sets)
    get_target_property(header_sets "${target}" HEADER_SETS)
  endif()

  if(header_sets)
    install(TARGETS "${target}" EXPORT "${export_set}" FILE_SET HEADERS)
  else()
    install(TARGETS "${target}" EXPORT "${export_set}")
  endif()
endfunction()

function(bpr_package_export_files output)
  set(one_value_args PACKAGE DEFAULT_COMPONENT)
  set(multi_value_args COMPONENTS)
  cmake_parse_arguments(BPR_PACKAGE "" "${one_value_args}" "${multi_value_args}" ${ARGN})

  if(NOT BPR_PACKAGE_PACKAGE)
    message(FATAL_ERROR "bpr_package_export_files requires PACKAGE")
  endif()
  if(NOT BPR_PACKAGE_DEFAULT_COMPONENT)
    message(FATAL_ERROR "bpr_package_export_files requires DEFAULT_COMPONENT")
  endif()

  set(files)
  foreach(component IN LISTS BPR_PACKAGE_COMPONENTS)
    bpr_package_export_set(export_set "${BPR_PACKAGE_PACKAGE}" "${component}" "${BPR_PACKAGE_DEFAULT_COMPONENT}")
    list(APPEND files "${export_set}.cmake")
  endforeach()
  list(REMOVE_DUPLICATES files)
  set("${output}" "${files}" PARENT_SCOPE)
endfunction()

function(bpr_install_package_metadata)
  set(one_value_args PACKAGE NAMESPACE DEFAULT_COMPONENT DESCRIPTION LICENSE HOMEPAGE_URL)
  set(multi_value_args COMPONENTS)
  cmake_parse_arguments(BPR_PACKAGE "" "${one_value_args}" "${multi_value_args}" ${ARGN})

  if(NOT BPR_PACKAGE_PACKAGE)
    message(FATAL_ERROR "bpr_install_package_metadata requires PACKAGE")
  endif()
  if(NOT BPR_PACKAGE_NAMESPACE)
    message(FATAL_ERROR "bpr_install_package_metadata requires NAMESPACE")
  endif()
  if(NOT BPR_PACKAGE_DEFAULT_COMPONENT)
    message(FATAL_ERROR "bpr_install_package_metadata requires DEFAULT_COMPONENT")
  endif()
  if(NOT BPR_PACKAGE_COMPONENTS)
    message(FATAL_ERROR "bpr_install_package_metadata requires at least one COMPONENT")
  endif()

  if(NOT BPR_ENABLE_INSTALL_TARGETS)
    return()
  endif()

  if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
    include(GNUInstallDirs)
  endif()

  if(NOT BPR_PACKAGE_LICENSE)
    set(BPR_PACKAGE_LICENSE "NOASSERTION")
  endif()
  if(NOT BPR_PACKAGE_DESCRIPTION)
    set(BPR_PACKAGE_DESCRIPTION "${BPR_PACKAGE_PACKAGE}")
  endif()

  set(package_cmake_dir "${CMAKE_INSTALL_LIBDIR}/cmake/${BPR_PACKAGE_PACKAGE}")
  set(package_cps_dir "${CMAKE_INSTALL_LIBDIR}/cps/${BPR_PACKAGE_PACKAGE}")

  bpr_package_export_set(
    root_export_set
    "${BPR_PACKAGE_PACKAGE}"
    "${BPR_PACKAGE_DEFAULT_COMPONENT}"
    "${BPR_PACKAGE_DEFAULT_COMPONENT}"
  )

  install(
    EXPORT "${root_export_set}"
    NAMESPACE "${BPR_PACKAGE_NAMESPACE}"
    FILE "${root_export_set}.cmake"
    DESTINATION "${package_cmake_dir}"
  )

  set(package_info_args
    PACKAGE_INFO "${BPR_PACKAGE_PACKAGE}"
    EXPORT "${root_export_set}"
    DESTINATION "${package_cps_dir}"
    VERSION "${PROJECT_VERSION}"
    COMPAT_VERSION "${PROJECT_VERSION}"
    VERSION_SCHEMA simple
    DEFAULT_TARGETS "${BPR_PACKAGE_DEFAULT_COMPONENT}"
    DEFAULT_LICENSE "${BPR_PACKAGE_LICENSE}"
    DESCRIPTION "${BPR_PACKAGE_DESCRIPTION}"
  )
  if(BPR_PACKAGE_HOMEPAGE_URL)
    list(APPEND package_info_args HOMEPAGE_URL "${BPR_PACKAGE_HOMEPAGE_URL}")
  endif()
  install(${package_info_args})

  foreach(component IN LISTS BPR_PACKAGE_COMPONENTS)
    if(component STREQUAL "${BPR_PACKAGE_DEFAULT_COMPONENT}")
      continue()
    endif()

    bpr_package_export_set(
      appendix_export_set
      "${BPR_PACKAGE_PACKAGE}"
      "${component}"
      "${BPR_PACKAGE_DEFAULT_COMPONENT}"
    )

    install(
      EXPORT "${appendix_export_set}"
      NAMESPACE "${BPR_PACKAGE_NAMESPACE}"
      FILE "${appendix_export_set}.cmake"
      DESTINATION "${package_cmake_dir}"
    )

    install(
      PACKAGE_INFO "${BPR_PACKAGE_PACKAGE}"
      EXPORT "${appendix_export_set}"
      APPENDIX "${component}"
      DESTINATION "${package_cps_dir}"
      DEFAULT_LICENSE "${BPR_PACKAGE_LICENSE}"
    )
  endforeach()
endfunction()
