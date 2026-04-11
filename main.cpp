#include "Graph.h"
#include "GraphGenerator.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <random>

void FindEvenNonDecomposable(int conn,
                             int num_iter,
                             int min_n,
                             int max_n,
                             int seed = 239,
                             int print_every_num_iter = 100, bool verbose=false) {
    const std::filesystem::path output_directory = "../graphs/" + std::to_string(conn) + "_non_decomposable";
    std::filesystem::create_directories(output_directory);

    GraphGenerator generator(seed);
    std::mt19937 rng(seed + 1);
    std::uniform_int_distribution<int> vertex_distribution(min_n, max_n);

    int exported_graphs = 0;
    for (int attempt = 0; attempt < num_iter; ++attempt) {
        if (attempt % print_every_num_iter == 0) {
            std::cout << "attempt=" << attempt
                << " exported=" << exported_graphs << std::endl;
        }

        const int num_vertices = vertex_distribution(rng);
        std::unique_ptr<Graph> graph =
            generator.KargerPinchingEvenGraph(num_vertices, conn, verbose);

        bool decomposable = false;
        for (int factor_conn = 2; factor_conn <= conn / 2; ++factor_conn) {
            if (graph->DecomposeConnectivity(factor_conn, conn - factor_conn).has_value()) {
                decomposable = true;
                break;
            }
        }

        if (!decomposable) {
            const std::string base_name = "graph_" + std::to_string(attempt);
            graph->ExportGraph((output_directory / (base_name + ".txt")).string());
            ++exported_graphs;
        }
    }

    std::cout << "exported_total=" << exported_graphs << std::endl;
}

void ExportTrees(int connectivity) {
    constexpr int kRoot = 0;
    const std::filesystem::path input_directory = "../graphs/" + std::to_string(connectivity) + "_non_decomposable";

    for (const std::filesystem::directory_entry &entry :
         std::filesystem::directory_iterator(input_directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".txt") {
            continue;
        }
        if (entry.path().stem().string().ends_with("_metadata")) {
            continue;
        }

        std::cout << entry.path().filename().string() << std::flush;
        Graph graph(entry.path());

        const std::optional<std::vector<RootedSpanningTree> > trees =
            graph.EdgeIndependentTrees(connectivity, kRoot);

        if (!trees.has_value()) {
            std::cout << " counterexample" << std::endl;
        } else {
            std::cout << std::endl;
            std::ofstream output(entry.path().parent_path() /
                (entry.path().stem().string() + "_trees.txt"));
            output << trees->size() << " " << kRoot << "\n";
            for (const RootedSpanningTree &tree : *trees) {
                for (int vertex = 0; vertex < graph.NumVertices(); ++vertex) {
                    if (vertex > 0) {
                        output << " ";
                    }
                    output << tree[vertex];
                }
                output << "\n";
            }
        }
    }
}

int main() {
    // ExportTrees(8);

    FindEvenNonDecomposable(8, 1000'000, 15, 40, 1, 1000, false);

    // Graph graph("../graphs/8_non_decomposable/graph_17940.txt");
    // const std::optional<std::vector<RootedSpanningTree> > trees =
    //     graph.EdgeIndependentTrees(8, 0);
    //
    // std::cout << " trees_found=" << trees.has_value() << std::endl;

    return 0;
}
