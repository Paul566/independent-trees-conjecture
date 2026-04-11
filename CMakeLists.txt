cmake_minimum_required(VERSION 4.2)
project(independent_trees_conjecture)

set(CMAKE_CXX_STANDARD 20)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(
    ORTOOLS_ROOT
    "/tmp/ortools_install/or-tools_x86_64_Ubuntu-24.04_cpp_v9.12.4544"
    CACHE PATH "Path to the OR-Tools C++ binary distribution")
if(EXISTS "/usr/lib/cmake/absl/abslConfig.cmake")
    set(
        absl_DIR
        "/usr/lib/cmake/absl"
        CACHE PATH "Path to abslConfig.cmake" FORCE)
endif()
if(EXISTS "${ORTOOLS_ROOT}/lib/cmake/ortools/ortoolsConfig.cmake")
    set(
        ortools_DIR
        "${ORTOOLS_ROOT}/lib/cmake/ortools"
        CACHE PATH "Path to ortoolsConfig.cmake" FORCE)
endif()

find_package(absl CONFIG REQUIRED)
find_package(ortools CONFIG REQUIRED)

find_path(
    ORTOOLS_CP_MODEL_HEADER
    NAMES ortools/sat/cp_model.h
    HINTS "${ORTOOLS_ROOT}/include")
if(NOT ORTOOLS_CP_MODEL_HEADER)
    message(FATAL_ERROR
            "Could not find ortools/sat/cp_model.h. Install the OR-Tools C++ "
            "binary distribution or set ORTOOLS_ROOT to its extraction directory.")
endif()

enable_testing()

add_executable(independent_trees_conjecture main.cpp
        Graph.cpp
        Graph.h
        GraphGenerator.cpp
        GraphGenerator.h)
target_link_libraries(independent_trees_conjecture PRIVATE ortools::ortools)

add_executable(graph_test
        graph_test.cpp
        Graph.cpp
        Graph.h
        GraphGenerator.cpp
        GraphGenerator.h)
target_link_libraries(graph_test PRIVATE ortools::ortools)

add_test(NAME graph_test COMMAND graph_test)
add_test(
    NAME draw_graph_test
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/scripts/draw_graph_test.py")
add_test(
    NAME count_spanning_trees_test
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/count_spanning_trees_test.py")
set_tests_properties(
    draw_graph_test
    count_spanning_trees_test
    PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
