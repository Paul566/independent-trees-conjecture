#!/usr/bin/env python3

import argparse
from pathlib import Path

import numpy as np


def read_graph(path: Path) -> tuple[int, list[tuple[int, int]]]:
    with path.open("r", encoding="utf-8") as input_file:
        lines = [line.strip() for line in input_file if line.strip()]

    if not lines:
        raise ValueError("Graph file is empty")

    num_vertices = int(lines[0])
    edges: list[tuple[int, int]] = []
    for line in lines[1:]:
        head_text, tail_text = line.split()
        edges.append((int(head_text), int(tail_text)))
    return num_vertices, edges


def build_laplacian(
    num_vertices: int, edges: list[tuple[int, int]]
) -> np.ndarray:
    laplacian = np.zeros((num_vertices, num_vertices), dtype=np.float64)
    for head, tail in edges:
        laplacian[head][head] += 1
        laplacian[tail][tail] += 1
        laplacian[head][tail] -= 1
        laplacian[tail][head] -= 1
    return laplacian


def minor_matrix(matrix: np.ndarray, row: int, column: int) -> np.ndarray:
    reduced = np.delete(matrix, row, axis=0)
    return np.delete(reduced, column, axis=1)


def determinant(matrix: np.ndarray) -> int:
    if matrix.size == 0:
        return 1
    return int(round(float(np.linalg.det(matrix))))


def count_spanning_trees(num_vertices: int, edges: list[tuple[int, int]]) -> int:
    if num_vertices <= 1:
        return 1
    laplacian = build_laplacian(num_vertices, edges)
    return determinant(minor_matrix(laplacian, 0, 0))


def iter_graph_files(path: Path) -> list[Path]:
    if path.is_file():
        return [path]
    return sorted(
        graph_path
        for graph_path in path.glob("graph_*.txt")
        if not graph_path.name.endswith("_metadata.txt")
    )


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Print the number of spanning trees for exported graph files."
    )
    parser.add_argument(
        "path", type=Path, help="Path to a graph file or to a directory of graph files"
    )
    args = parser.parse_args()

    for graph_path in iter_graph_files(args.path):
        num_vertices, edges = read_graph(graph_path)
        print(f"{graph_path.name}: {count_spanning_trees(num_vertices, edges)}")


if __name__ == "__main__":
    main()
