//
// Created by paul on 4/4/26.
//

#ifndef INDEPENDENT_TREES_CONJECTURE_GRAPHGENERATOR_H
#define INDEPENDENT_TREES_CONJECTURE_GRAPHGENERATOR_H

#include <memory>
#include <random>

class Graph;

class GraphGenerator {
    public:
        explicit GraphGenerator(std::mt19937::result_type seed);

        std::unique_ptr<Graph> GenerateRandomGraph(int n, int m);
        std::unique_ptr<Graph> RandomPinchingGraph(int n, int connectivity);
        std::unique_ptr<Graph> RandomPinchingOddGraph(int n, int connectivity);

    private:
        std::mt19937 rng_;
};

#endif //INDEPENDENT_TREES_CONJECTURE_GRAPHGENERATOR_H
