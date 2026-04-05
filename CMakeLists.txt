cmake_minimum_required(VERSION 4.2)
project(independent_trees_conjecture)

set(CMAKE_CXX_STANDARD 20)

enable_testing()

add_executable(independent_trees_conjecture main.cpp
        Graph.cpp
        Graph.h
        GraphGenerator.cpp
        GraphGenerator.h)

add_executable(graph_test
        graph_test.cpp
        Graph.cpp
        Graph.h
        GraphGenerator.cpp
        GraphGenerator.h)

add_test(NAME graph_test COMMAND graph_test)
