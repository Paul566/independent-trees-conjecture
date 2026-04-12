#include "Graph.h"
#include "GraphGenerator.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <memory>
#include <optional>
#include <random>
#include <thread>
#include <vector>

namespace {

unsigned int SearchThreadCount() {
    const char *thread_count_text = std::getenv("SEARCH_NUM_THREADS");
    if (thread_count_text == nullptr) {
        return std::max(1u, std::thread::hardware_concurrency());
    }

    try {
        const int parsed = std::stoi(thread_count_text);
        if (parsed > 0) {
            return static_cast<unsigned int>(parsed);
        }
    } catch (const std::exception &) {
    }
    return 1u;
}

}  // namespace

void FindEvenNonDecomposable(int conn,
                             int num_iter,
                             int min_n,
                             int max_n,
                             int seed = 239,
                             int print_every_num_iter = 100,
                             bool verbose = false) {
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
        // std::unique_ptr<Graph> graph = generator.KargerPinchingEvenGraph(num_vertices, conn, verbose);
        std::unique_ptr<Graph> graph = generator.SingletonPinchingEvenGraph(num_vertices, conn);

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

void SearchForExamples(int conn, int num_iter, int min_n, int max_n, int seed) {
    const std::filesystem::path output_directory = "../graphs/" + std::to_string(conn) + "_non_decomposable";
    std::filesystem::create_directories(output_directory);
    constexpr int kRoot = 0;
    const unsigned int num_threads = SearchThreadCount();
    std::cout << "num_threads: " << num_threads << std::endl;
    std::atomic<int> next_attempt = 0;
    std::mutex output_mutex;

    std::vector<std::thread> workers;
    workers.reserve(num_threads);
    for (unsigned int thread_index = 0; thread_index < num_threads; ++thread_index) {
        workers.emplace_back([&, thread_index]() {
            GraphGenerator generator(seed + static_cast<int>(2 * thread_index));
            std::mt19937 rng(seed + static_cast<int>(2 * thread_index + 1));
            std::uniform_int_distribution<int> vertex_distribution(min_n, max_n);

            while (true) {
                const int attempt = next_attempt.fetch_add(1);
                if (attempt >= num_iter) {
                    return;
                }

                const int num_vertices = vertex_distribution(rng);
                std::unique_ptr<Graph> graph =
                    generator.SingletonPinchingEvenGraph(num_vertices, conn);

                bool decomposable = false;
                for (int factor_conn = 2; factor_conn <= conn / 2; ++factor_conn) {
                    if (graph->DecomposeConnectivity(
                            factor_conn, conn - factor_conn).has_value()) {
                        decomposable = true;
                        break;
                    }
                }
                if (decomposable) {
                    continue;
                }

                const std::optional<std::vector<RootedSpanningTree> > trees =
                    graph->EdgeIndependentTrees(conn, kRoot);
                const bool is_counterexample = !trees.has_value();

                {
                    std::lock_guard<std::mutex> lock(output_mutex);
                    std::cout << "iter\t" << attempt << " " << graph->NumVertices();
                    if (is_counterexample) {
                        std::cout << " counterexample";
                    }
                    std::cout << std::endl;
                    if (is_counterexample) {
                        const std::string base_name = "graph_" + std::to_string(attempt);
                        graph->ExportGraph(
                            (output_directory / (base_name + ".txt")).string());
                    }
                }
            }
        });
    }

    for (std::thread &worker : workers) {
        worker.join();
    }
}

int main() {
    // ExportTrees(8);

    // FindEvenNonDecomposable(8, 1000'000, 15, 40, 1, 1000, false);

    SearchForExamples(8, 1000'000, 15, 40, 2);

    return 0;
}
