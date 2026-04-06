#!/usr/bin/env python3

import argparse
from collections import defaultdict
import math
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


def read_trees(path: Path) -> tuple[int, list[list[int]]]:
    with path.open("r", encoding="utf-8") as input_file:
        lines = [line.strip() for line in input_file if line.strip()]

    if not lines:
        raise ValueError("Tree file is empty")

    header_parts = lines[0].split()
    if len(header_parts) != 2:
        raise ValueError("Tree file header must contain number of trees and root")
    num_trees = int(header_parts[0])
    root = int(header_parts[1])

    if len(lines) != num_trees + 1:
        raise ValueError("Tree file must contain one line per tree")

    trees: list[list[int]] = []
    expected_size: int | None = None
    for line in lines[1:]:
        tree = [int(value) for value in line.split()]
        if expected_size is None:
            expected_size = len(tree)
        elif len(tree) != expected_size:
            raise ValueError("All trees must have the same number of vertices")
        trees.append(tree)
    return root, trees


def collect_draw_edges(
    ordered_edges: list[tuple[int, int]], decomposition: list[int] | None = None
) -> list[tuple[int, int, float, str, str]]:
    return [
        (head, tail, radius, color, linestyle)
        for _, head, tail, radius, color, linestyle in collect_indexed_draw_edges(
            ordered_edges, decomposition
        )
    ]


def collect_indexed_draw_edges(
    ordered_edges: list[tuple[int, int]], decomposition: list[int] | None = None
) -> list[tuple[int, int, int, float, str, str]]:
    edge_colors = {0: "#2a9d8f", 1: "#e76f51"}
    multiplicities = defaultdict(list)
    for edge_index, (head, tail) in enumerate(ordered_edges):
        edge_key = tuple(sorted((head, tail)))
        canonical_head, canonical_tail = edge_key
        color = edge_colors[decomposition[edge_index]] if decomposition else "#2a9d8f"
        multiplicities[edge_key].append(
            (edge_index, canonical_head, canonical_tail, color, "solid")
        )

    draw_edges = []
    for parallel_edges in multiplicities.values():
        count = len(parallel_edges)
        for index, (edge_index, head, tail, color, linestyle) in enumerate(parallel_edges):
            radius = 0.0 if count == 1 else 0.4 * (index - (count - 1) / 2.0)
            draw_edges.append((edge_index, head, tail, radius, color, linestyle))
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


def draw_tree_overlay(
    axis: plt.Axes,
    positions: dict[int, tuple[float, float]],
    graph: nx.MultiGraph,
    ordered_edges: list[tuple[int, int]],
    tree: list[int],
    root: int,
    color: str,
    title: str,
) -> None:
    nx.draw_networkx_nodes(
        graph,
        positions,
        node_color="#f4d35e",
        edgecolors="#1b1b1e",
        node_size=520,
        ax=axis,
    )
    nx.draw_networkx_labels(graph, positions, font_size=10, font_weight="bold", ax=axis)

    base_edges = collect_indexed_draw_edges(ordered_edges)
    highlighted_edges = set(edge_index for edge_index in tree if edge_index >= 0)
    for edge_index, head, tail, radius, _, _ in base_edges:
        is_highlighted = edge_index in highlighted_edges
        edge_patch = FancyArrowPatch(
            positions[head],
            positions[tail],
            connectionstyle=f"arc3,rad={radius}",
            arrowstyle="-",
            linewidth=3.0 if is_highlighted else 1.1,
            linestyle="solid",
            color=color if is_highlighted else "#c7c7c7",
            alpha=1.0 if is_highlighted else 0.75,
            mutation_scale=1.0,
            zorder=2 if is_highlighted else 1,
        )
        axis.add_patch(edge_patch)

    axis.set_title(f"{title}, root={root}", fontsize=11)
    axis.set_axis_off()


def draw_trees(
    graph: nx.MultiGraph,
    ordered_edges: list[tuple[int, int]],
    output_path: Path,
    root: int,
    trees: list[list[int]],
) -> None:
    positions = nx.spring_layout(graph, seed=0)
    num_trees = len(trees)
    num_columns = 2 if num_trees > 1 else 1
    num_rows = math.ceil(num_trees / num_columns)
    figure, axes = plt.subplots(
        num_rows, num_columns, figsize=(7 * num_columns, 5 * num_rows)
    )
    if isinstance(axes, plt.Axes):
        axes_list = [axes]
    else:
        axes_list = list(axes.flat)

    palette = ["#1d4ed8", "#b91c1c", "#0f766e", "#7c3aed", "#ca8a04", "#db2777"]
    for tree_index, tree in enumerate(trees):
        draw_tree_overlay(
            axes_list[tree_index],
            positions,
            graph,
            ordered_edges,
            tree,
            root,
            palette[tree_index % len(palette)],
            f"Tree {tree_index}",
        )

    for axis in axes_list[num_trees:]:
        axis.set_axis_off()

    plt.tight_layout()
    plt.savefig(output_path, dpi=200, bbox_inches="tight")
    plt.close(figure)


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
    parser.add_argument(
        "--trees",
        type=Path,
        help="Optional path to a sidecar file with rooted spanning trees",
    )
    args = parser.parse_args()

    graph, ordered_edges = read_graph(args.input)
    if args.trees is not None:
        root, trees = read_trees(args.trees)
        draw_trees(graph, ordered_edges, args.output, root, trees)
        return

    decomposition = None
    if args.decomposition is not None:
        decomposition = read_decomposition(args.decomposition, len(ordered_edges))
    draw_graph(graph, ordered_edges, args.output, decomposition)


if __name__ == "__main__":
    main()
