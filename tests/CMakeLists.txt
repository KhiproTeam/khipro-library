add_executable(khipro_conformance main.cpp)
target_link_libraries(khipro_conformance PRIVATE khipro_static)
target_compile_definitions(khipro_conformance PRIVATE CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME khipro_conformance COMMAND khipro_conformance)
