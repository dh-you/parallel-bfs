# parallel-bfs
MPI-based implementations of BFS for SNAP graphs

| Snap Graph | Sequential | 2 threads | 4 threads | 6 threads |
|:-----------|:----------:|:---------:|:---------:|:---------:|
| Arxiv General Relativity (5k nodes, 0.03M edges) | 0.965 | 4.390 | 3.424 | 1.856 |
| Arxiv Astro Physics (19k nodes, 0.4M edges) | 5.669 | 48.081 | 37.392 | 8.775 |
| Arxiv Condensed Matter (23k nodes, 0.2M edges) | 5.381 | 36.747 | 14.496 | 4.053 |
| Epinions Social Network (76k nodes, 0.5M edges) | 12.239 | 25.538 | 15.103 | 10.862 |
| EU Email Communication Network (265k nodes, 0.4M edges) | 15.917 | 14.301 | 9.626 | 7.331 |

Time measured in milliseconds
