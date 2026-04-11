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
        std::unique_ptr<Graph> RandomPinchingEvenGraph(int n, int connectivity);
        std::unique_ptr<Graph> KargerPinchingEvenGraph(
            int n,
            int connectivity,
            bool verbose = false,
            bool prefer_non_parallel = false,
            bool prefer_non_single_vertex_cut = false,
            int connectivity_luft = 0,
            int max_cut_sampling_attempts = 10);
        std::unique_ptr<Graph> RandomPinchingOddGraph(int n, int connectivity);
        std::unique_ptr<Graph> AdversarialPinchingEvenGraph(
            int n_max,
            int a,
            int b,
            bool pinch_from_first_factor);

    private:
        std::mt19937 rng_;
};

#endif //INDEPENDENT_TREES_CONJECTURE_GRAPHGENERATOR_H
