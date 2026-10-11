add_executable(khipro_conformance main.cpp)
target_link_libraries(khipro_conformance PRIVATE khipro_static)
target_compile_definitions(khipro_conformance PRIVATE CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME khipro_conformance COMMAND khipro_conformance)

add_executable(khipro_unit unit.cpp)
target_link_libraries(khipro_unit PRIVATE khipro_static)
target_include_directories(khipro_unit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../core/src)
add_test(NAME khipro_unit COMMAND khipro_unit)
