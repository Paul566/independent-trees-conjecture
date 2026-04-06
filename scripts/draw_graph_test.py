#!/usr/bin/env python3

import tempfile
from pathlib import Path

from draw_graph import collect_draw_edges
from draw_graph import collect_indexed_draw_edges
from draw_graph import read_decomposition
from draw_graph import read_graph
from draw_graph import read_trees


def test_read_graph_preserves_edge_order() -> None:
    with tempfile.TemporaryDirectory() as temp_dir:
        graph_path = Path(temp_dir) / "graph.txt"
        graph_path.write_text("4\n0 1\n1 2\n0 1\n2 3\n", encoding="utf-8")

        _, ordered_edges = read_graph(graph_path)
        assert ordered_edges == [(0, 1), (1, 2), (0, 1), (2, 3)]


def test_decomposition_labels_follow_exported_edge_order() -> None:
    ordered_edges = [(0, 1), (1, 2), (0, 1), (2, 3)]
    decomposition = [0, 1, 1, 0]

    draw_edges = collect_draw_edges(ordered_edges, decomposition)
    first_parallel_colors = [
        color for head, tail, _, color, _ in draw_edges
        if tuple(sorted((head, tail))) == (0, 1)
    ]
    assert sorted(first_parallel_colors) == ["#2a9d8f", "#e76f51"]


def test_parallel_edges_get_distinct_offsets() -> None:
    ordered_edges = [(2, 4), (4, 2)]

    draw_edges = collect_draw_edges(ordered_edges, [0, 1])
    radii = [radius for _, _, radius, _, _ in draw_edges]
    endpoints = [(head, tail) for head, tail, _, _, _ in draw_edges]
    assert endpoints == [(2, 4), (2, 4)]
    assert len(set(radii)) == 2
    assert sorted(radii) == [-0.2, 0.2]


def test_read_decomposition_rejects_wrong_length() -> None:
    with tempfile.TemporaryDirectory() as temp_dir:
        decomposition_path = Path(temp_dir) / "decomposition.txt"
        decomposition_path.write_text("0\n1\n", encoding="utf-8")

        try:
            read_decomposition(decomposition_path, 3)
        except ValueError:
            return
        raise AssertionError("Expected read_decomposition() to reject wrong length")


def test_read_trees_parses_header_and_trees() -> None:
    with tempfile.TemporaryDirectory() as temp_dir:
        trees_path = Path(temp_dir) / "trees.txt"
        trees_path.write_text("2 0\n-1 0 1\n-1 2 3\n", encoding="utf-8")

        root, trees = read_trees(trees_path)
        assert root == 0
        assert trees == [[-1, 0, 1], [-1, 2, 3]]


def test_collect_indexed_draw_edges_preserves_edge_indices() -> None:
    ordered_edges = [(2, 4), (4, 2), (1, 3)]

    indexed_draw_edges = collect_indexed_draw_edges(ordered_edges)
    edge_to_radius = {
        edge_index: radius
        for edge_index, head, tail, radius, _, _ in indexed_draw_edges
        if (head, tail) == (2, 4)
    }
    assert set(edge_to_radius.keys()) == {0, 1}
    assert sorted(edge_to_radius.values()) == [-0.2, 0.2]


def main() -> None:
    test_read_graph_preserves_edge_order()
    test_decomposition_labels_follow_exported_edge_order()
    test_parallel_edges_get_distinct_offsets()
    test_read_decomposition_rejects_wrong_length()
    test_read_trees_parses_header_and_trees()
    test_collect_indexed_draw_edges_preserves_edge_indices()


if __name__ == "__main__":
    main()
