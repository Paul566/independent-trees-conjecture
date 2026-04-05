#!/usr/bin/env python3

import argparse
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch
import networkx as nx


def read_graph(path: Path) -> tuple[nx.MultiGraph, list[tuple[int, int]]]:
    with path.open("r", encoding="utf-8") as input_file:
        lines = [line.strip() for line in input_file if line.strip()]

    if not lines:
        raise ValueError("Graph file is empty")

    num_vertices = int(lines[0])
    graph = nx.MultiGraph()
    graph.add_nodes_from(range(num_vertices))
    ordered_edges: list[tuple[int, int]] = []

    for line in lines[1:]:
        head_text, tail_text = line.split()
        edge = (int(head_text), int(tail_text))
        ordered_edges.append(edge)
        graph.add_edge(*edge)

    return graph, ordered_edges


def read_decomposition(path: Path, num_edges: int) -> list[int]:
    with path.open("r", encoding="utf-8") as input_file:
        lines = [line.strip() for line in input_file if line.strip()]

    if len(lines) != num_edges:
        raise ValueError("Decomposition file must contain one label per edge")

    labels = [int(line) for line in lines]
    if any(label not in (0, 1) for label in labels):
        raise ValueError("Decomposition labels must be 0 or 1")
    return labels


def collect_draw_edges(
    ordered_edges: list[tuple[int, int]], decomposition: list[int] | None = None
) -> list[tuple[int, int, float, str, str]]:
    edge_colors = {0: "#2a9d8f", 1: "#e76f51"}
    multiplicities = defaultdict(list)
    for edge_index, (head, tail) in enumerate(ordered_edges):
        edge_key = tuple(sorted((head, tail)))
        canonical_head, canonical_tail = edge_key
        color = edge_colors[decomposition[edge_index]] if decomposition else "#2a9d8f"
        multiplicities[edge_key].append((canonical_head, canonical_tail, color, "solid"))

    draw_edges = []
    for parallel_edges in multiplicities.values():
        count = len(parallel_edges)
        for index, (head, tail, color, linestyle) in enumerate(parallel_edges):
            radius = 0.0 if count == 1 else 0.4 * (index - (count - 1) / 2.0)
            draw_edges.append((head, tail, radius, color, linestyle))
    return draw_edges


def draw_graph(
    graph: nx.MultiGraph,
    ordered_edges: list[tuple[int, int]],
    output_path: Path,
    decomposition: list[int] | None = None,
) -> None:
    positions = nx.spring_layout(graph, seed=0)

    _, axis = plt.subplots(figsize=(8, 6))
    nx.draw_networkx_nodes(
        graph,
        positions,
        node_color="#f4d35e",
        edgecolors="#1b1b1e",
        node_size=700,
        ax=axis,
    )
    nx.draw_networkx_labels(
        graph, positions, font_size=12, font_weight="bold", ax=axis
    )

    for head, tail, radius, color, linestyle in collect_draw_edges(
        ordered_edges, decomposition
    ):
        edge_patch = FancyArrowPatch(
            positions[head],
            positions[tail],
            connectionstyle=f"arc3,rad={radius}",
            arrowstyle="-",
            linewidth=2.4,
            linestyle=linestyle,
            color=color,
            mutation_scale=1.0,
            zorder=1,
        )
        axis.add_patch(edge_patch)

    axis.set_axis_off()
    if decomposition is not None:
        axis.text(
            0.02,
            0.02,
            "teal: first subgraph, orange: second subgraph",
            transform=axis.transAxes,
            fontsize=10,
            bbox={"facecolor": "white", "edgecolor": "#1b1b1e", "alpha": 0.85},
        )
    plt.tight_layout()
    plt.savefig(output_path, dpi=200, bbox_inches="tight")
    plt.close()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Read an exported graph and draw it to an image."
    )
    parser.add_argument("input", type=Path, help="Path to the exported graph file")
    parser.add_argument("output", type=Path, help="Path to the output image")
    parser.add_argument(
        "--decomposition",
        type=Path,
        help="Optional path to a sidecar file with one 0/1 label per edge",
    )
    args = parser.parse_args()

    graph, ordered_edges = read_graph(args.input)
    decomposition = None
    if args.decomposition is not None:
        decomposition = read_decomposition(args.decomposition, len(ordered_edges))
    draw_graph(graph, ordered_edges, args.output, decomposition)


if __name__ == "__main__":
    main()
