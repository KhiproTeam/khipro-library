find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(KHIPRO_CORE_SOURCES
  src/sexpr.cpp
  src/spec.cpp
  src/engine.cpp
  src/convert.cpp
  src/api.cpp
)

set(KHIPRO_EMBEDDED_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/src/embedded_specs.h")
set(KHIPRO_DEFAULT_MIM "${CMAKE_SOURCE_DIR}/resources/bn-khipro.mim")
set(KHIPRO_TOUCHSCREEN_MIM "${CMAKE_SOURCE_DIR}/resources/bn-khipro-touchscreen.mim")

add_custom_command(
  OUTPUT "${KHIPRO_EMBEDDED_HEADER}"
  COMMAND ${CMAKE_COMMAND} -E env KHIPRO_NO_SYNC=1
          ${Python3_EXECUTABLE}
          "${CMAKE_SOURCE_DIR}/scripts/resources/embed.py"
          --default "${KHIPRO_DEFAULT_MIM}"
          --touchscreen "${KHIPRO_TOUCHSCREEN_MIM}"
          --out "${KHIPRO_EMBEDDED_HEADER}"
  DEPENDS
    "${CMAKE_SOURCE_DIR}/scripts/resources/embed.py"
    "${CMAKE_SOURCE_DIR}/scripts/resources/meta.py"
    "${CMAKE_SOURCE_DIR}/khipro.meta"
    "${KHIPRO_DEFAULT_MIM}"
    "${KHIPRO_TOUCHSCREEN_MIM}"
  COMMENT "Embedding .mim layout specs"
  VERBATIM
)

add_custom_target(khipro_embed_specs DEPENDS "${KHIPRO_EMBEDDED_HEADER}")

function(khipro_apply_target_flags target_name)
  target_include_directories(${target_name}
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  target_compile_definitions(${target_name} PRIVATE KHIPRO_LAYOUT_VERSION="${KHIPRO_LAYOUT_VERSION}")
  target_compile_options(${target_name} PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fvisibility=hidden
    -fvisibility-inlines-hidden
  )
  if(NOT MSVC)
    target_link_options(${target_name} PRIVATE -Wl,--gc-sections)
  endif()
  add_dependencies(${target_name} khipro_embed_specs)
endfunction()

if(KHIPRO_BUILD_SHARED)
  add_library(khipro_shared SHARED ${KHIPRO_CORE_SOURCES})
  add_library(khipro::shared ALIAS khipro_shared)
  set_target_properties(khipro_shared PROPERTIES
    OUTPUT_NAME khipro
    POSITION_INDEPENDENT_CODE ON
  )
  target_compile_definitions(khipro_shared PRIVATE KHIPRO_BUILD_SHARED)
  if(MSVC)
    target_sources(khipro_shared PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/khipro.def")
  endif()
  khipro_apply_target_flags(khipro_shared)
endif()

if(KHIPRO_BUILD_STATIC)
  add_library(khipro_static STATIC ${KHIPRO_CORE_SOURCES})
  add_library(khipro::static ALIAS khipro_static)
  set_target_properties(khipro_static PROPERTIES OUTPUT_NAME khipro)
  khipro_apply_target_flags(khipro_static)
endif()

add_library(khipro ALIAS khipro_static)

include(GNUInstallDirs)

install(FILES include/khipro/khipro.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/khipro)

if(KHIPRO_BUILD_SHARED)
  install(TARGETS khipro_shared
    EXPORT khiproTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()

if(KHIPRO_BUILD_STATIC)
  install(TARGETS khipro_static
    EXPORT khiproTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
endif()

install(EXPORT khiproTargets
  FILE khiproTargets.cmake
  NAMESPACE khipro::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/khipro
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/khiproConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/khiproConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/khipro
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/khiproConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/khiproConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/khiproConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/khipro
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/khipro.pc.in"
  "${CMAKE_CURRENT_BINARY_DIR}/khipro.pc"
  @ONLY
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/khipro.pc" DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
