#!/usr/bin/env python3

import tempfile
from pathlib import Path

from draw_graph import collect_draw_edges
from draw_graph import read_decomposition
from draw_graph import read_graph


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


def main() -> None:
    test_read_graph_preserves_edge_order()
    test_decomposition_labels_follow_exported_edge_order()
    test_parallel_edges_get_distinct_offsets()
    test_read_decomposition_rejects_wrong_length()


if __name__ == "__main__":
    main()
