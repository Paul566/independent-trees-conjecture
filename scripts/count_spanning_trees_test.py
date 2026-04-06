#!/usr/bin/env python3

import tempfile
from pathlib import Path

from count_spanning_trees import count_spanning_trees
from count_spanning_trees import iter_graph_files
from count_spanning_trees import read_graph


def test_known_spanning_tree_counts() -> None:
    assert count_spanning_trees(1, []) == 1
    assert count_spanning_trees(4, [(0, 1), (1, 2), (2, 3)]) == 1
    assert count_spanning_trees(4, [(0, 1), (1, 2), (2, 3), (3, 0)]) == 4
    assert count_spanning_trees(2, [(0, 1), (0, 1), (0, 1)]) == 3


def test_read_graph_and_directory_iteration() -> None:
    with tempfile.TemporaryDirectory() as temp_dir_name:
        temp_dir = Path(temp_dir_name)
        (temp_dir / "graph_1.txt").write_text("3\n0 1\n1 2\n", encoding="utf-8")
        (temp_dir / "graph_1_metadata.txt").write_text("ignore\n", encoding="utf-8")
        (temp_dir / "graph_0.txt").write_text("2\n0 1\n0 1\n", encoding="utf-8")

        graph_files = iter_graph_files(temp_dir)
        assert [path.name for path in graph_files] == ["graph_0.txt", "graph_1.txt"]

        num_vertices, edges = read_graph(temp_dir / "graph_0.txt")
        assert num_vertices == 2
        assert edges == [(0, 1), (0, 1)]


def main() -> None:
    test_known_spanning_tree_counts()
    test_read_graph_and_directory_iteration()


if __name__ == "__main__":
    main()
