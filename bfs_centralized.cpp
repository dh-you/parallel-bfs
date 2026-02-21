#include <mpi.h>
#include <iostream>
#include <vector> 
#include <algorithm>
#include <cstdint>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // No workers
    if (size <= 1) MPI_Abort(MPI_COMM_WORLD, 1);

    int n = 12;
    int start = 1;

    std::vector<std::vector<int>> adj(n+1);
    
    std::vector<std::pair<int, int>> edges = {
        {1, 2}, {1, 3}, {2, 4}, {2, 5}, {3, 5}, {3, 6}, 
        {4, 7}, {4, 8}, {6, 9}, {6, 10}, {8, 11}, {8, 12}
    };

    // Construct adjacency list
    for (auto e : edges) {
        adj[e.first].push_back(e.second);
        adj[e.second].push_back(e.first);
    }

    int frontier_size;
    std::vector<int> frontier = {start};
    std::vector<uint8_t> visited(n + 1, 0);
    visited[start] = 1;

    int new_frontier_sum;
    int new_frontier_size = 0; 
    std::vector<int> slice;
    std::vector<int> new_frontier;
    std::vector<int> temp;

    std::vector<int> recvcounts(size, 0);
    std::vector<int> displs(size, 0); 

    while (1) {
        if (rank == 0) {
            // Output frontier
            for (int node : frontier) std::cout << node << " ";
            std::cout << std::endl;
            
            // Manager broadcast frontier sizes
            frontier_size = frontier.size();
            MPI_Bcast(&frontier_size, 1, MPI_INT, 0, MPI_COMM_WORLD);

            // Break once finished
            if (frontier_size == 0) break;

            // Manager broadcast frontier
            MPI_Bcast(frontier.data(), frontier_size, MPI_INT, 0, MPI_COMM_WORLD);

            // Gather local frontier sizes
            int zero = 0;
            MPI_Gather(&zero, 1, MPI_INT, recvcounts.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

            // Construct displacements
            int total = 0;
            for (int r = 0; r < size; r++) {
                displs[r] = total;
                total += recvcounts[r];
            }
            temp.resize(total);

            // Gather new frontier
            MPI_Gatherv(nullptr, 0, MPI_INT, temp.data(), recvcounts.data(), displs.data(), MPI_INT, 0, MPI_COMM_WORLD);

            // Get rid of visited nodes
            frontier.clear();
            for (int node : temp) {
                if (!visited[node]) {
                    visited[node] = 1; 
                    frontier.push_back(node);  
                }
            }            
        } else {
            slice.clear();
            new_frontier.clear();

            // Worker receive frontier size and resize
            MPI_Bcast(&frontier_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
            frontier.resize(frontier_size);

            // Break once finished
            if (frontier_size == 0) break;

            // Worker receive frontier
            MPI_Bcast(frontier.data(), frontier_size, MPI_INT, 0, MPI_COMM_WORLD);

            // Take slice of frontier and construct partial new frontier 
            for (int i = rank-1; i < frontier.size(); i += size-1) slice.push_back(frontier[i]);
            for (int node : slice) {
                for (int neighbor : adj[node]) {
                    new_frontier.push_back(neighbor);
                }
            }

            // Remove duplicates from local frontier
            std::sort(new_frontier.begin(), new_frontier.end());
            new_frontier.erase(std::unique(new_frontier.begin(), new_frontier.end()), new_frontier.end());
            
            // Send frontier length to manager
            new_frontier_size = new_frontier.size();
            MPI_Gather(&new_frontier_size, 1, MPI_INT, nullptr, 0, MPI_INT, 0, MPI_COMM_WORLD);

            // Gather new frontier
            MPI_Gatherv(new_frontier.data(), new_frontier_size, MPI_INT, nullptr, nullptr, nullptr, MPI_INT, 0, MPI_COMM_WORLD);
        }
    }

    MPI_Finalize();
    return 0;
}
