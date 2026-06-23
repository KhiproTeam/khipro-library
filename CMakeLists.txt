cmake_minimum_required(VERSION 3.16)

# Read metadata before project() so VERSION is derived from khipro.meta
file(STRINGS "${CMAKE_SOURCE_DIR}/khipro.meta" _meta_lines)
foreach(line ${_meta_lines})
  if(line MATCHES "^([A-Za-z_]+)=(.*)$")
    set("_meta_${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
  endif()
endforeach()

project(${_meta_name} VERSION ${_meta_layout_version} LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(KHIPRO_NAME            ${_meta_name})
set(KHIPRO_DESCRIPTION     ${_meta_description})
set(KHIPRO_HOMEPAGE        ${_meta_homepage})
set(KHIPRO_LICENSE         ${_meta_license})
set(KHIPRO_AUTHOR          ${_meta_author})
set(KHIPRO_LAYOUT_VERSION  ${_meta_layout_version})
set(KHIPRO_LIBRARY_PATCH   ${_meta_library_patch})
set(KHIPRO_LIBRARY_VERSION "${KHIPRO_LAYOUT_VERSION}-${KHIPRO_LIBRARY_PATCH}")

message(STATUS "Khipro layout version: ${KHIPRO_LAYOUT_VERSION}")
message(STATUS "Khipro library version: ${KHIPRO_LIBRARY_VERSION}")

option(KHIPRO_BUILD_SHARED "Build shared library" ON)
option(KHIPRO_BUILD_STATIC "Build static library" ON)
option(KHIPRO_BUILD_TESTS "Build tests" ON)
option(KHIPRO_BUILD_WEB "Build WebAssembly bindings" OFF)
option(KHIPRO_BUILD_ANDROID "Build Android bindings" OFF)

if(ANDROID)
  set(KHIPRO_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  set(KHIPRO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  add_subdirectory(core)
  add_subdirectory(bindings/android)
  return()
endif()

add_subdirectory(core)

if(KHIPRO_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(KHIPRO_BUILD_WEB)
  add_subdirectory(bindings/web)
endif()

if(KHIPRO_BUILD_ANDROID)
  add_subdirectory(bindings/android)
endif()
