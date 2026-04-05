cmake_minimum_required(VERSION 4.2)
project(independent_trees_conjecture)

set(CMAKE_CXX_STANDARD 20)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(
    ORTOOLS_ROOT
    "/tmp/ortools_install/or-tools_x86_64_Ubuntu-24.04_cpp_v9.12.4544"
    CACHE PATH "Path to the OR-Tools C++ binary distribution")
set(ORTOOLS_INCLUDE_DIR "${ORTOOLS_ROOT}/include")
set(ORTOOLS_LIBRARY "${ORTOOLS_ROOT}/lib/libortools.so")
set(ORTOOLS_LIBRARY_DIR "${ORTOOLS_ROOT}/lib")
set(
        ORTOOLS_COMPILE_DEFINITIONS
        OR_PROTO_DLL=
        PROTOBUF_USE_DLLS
        HAVE_CONFIG_H
        EIGEN_MPL2_ONLY
        USE_LP_PARSER
        USE_MATH_OPT
        USE_BOP
        USE_CBC
        USE_CLP
        USE_GLOP
        USE_PDLP
        USE_SCIP)

enable_testing()

add_executable(independent_trees_conjecture main.cpp
        Graph.cpp
        Graph.h
        GraphGenerator.cpp
        GraphGenerator.h)
target_include_directories(independent_trees_conjecture PRIVATE "${ORTOOLS_INCLUDE_DIR}")
target_compile_definitions(independent_trees_conjecture PRIVATE ${ORTOOLS_COMPILE_DEFINITIONS})
target_link_libraries(independent_trees_conjecture PRIVATE "${ORTOOLS_LIBRARY}")
set_target_properties(independent_trees_conjecture PROPERTIES
        BUILD_RPATH "${ORTOOLS_LIBRARY_DIR}")

add_executable(graph_test
        graph_test.cpp
        Graph.cpp
        Graph.h
        GraphGenerator.cpp
        GraphGenerator.h)
target_include_directories(graph_test PRIVATE "${ORTOOLS_INCLUDE_DIR}")
target_compile_definitions(graph_test PRIVATE ${ORTOOLS_COMPILE_DEFINITIONS})
target_link_libraries(graph_test PRIVATE "${ORTOOLS_LIBRARY}")
set_target_properties(graph_test PROPERTIES
        BUILD_RPATH "${ORTOOLS_LIBRARY_DIR}")

add_test(NAME graph_test COMMAND graph_test)
add_test(NAME draw_graph_test COMMAND ${Python3_EXECUTABLE} scripts/draw_graph_test.py)
