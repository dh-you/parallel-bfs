#include <mpi.h>
#include <iostream>
#include <vector> 
#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <sstream>

using namespace std;

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // No workers
    if (size <= 1) MPI_Abort(MPI_COMM_WORLD, 1);

    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    vector<pair<int,int>> edges;
    int m = 0;

    if (rank == 0) {
        string line;
        while (getline(cin, line)) {
            if (line.empty() || line[0] == '#') continue;

            stringstream ss(line);
            int u, v;
            if (!(ss >> u >> v)) continue;

            edges.emplace_back(u, v);
        }
        m = (int)edges.size();
    }

    MPI_Bcast(&m, 1, MPI_INT, 0, MPI_COMM_WORLD);

    vector<int> buf(2 * m);
    if (rank == 0) {
        for (int i = 0; i < m; i++) {
            buf[2*i]   = edges[i].first;
            buf[2*i+1] = edges[i].second;
        }
    }

    MPI_Bcast(buf.data(), 2 * m, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank != 0) {
        edges.resize(m);
        for (int i = 0; i < m; i++) {
            edges[i].first  = buf[2*i];
            edges[i].second = buf[2*i+1];
        }
    }

    unordered_map<int,int> id2idx;
    vector<int> idx2id;

    auto get_idx = [&](int id) -> int {
        auto it = id2idx.find(id);
        if (it != id2idx.end()) return it->second;
        int idx = (int)id2idx.size();
        id2idx[id] = idx;
        idx2id.push_back(id);
        return idx;
    };

    for (auto &e : edges) {
        get_idx(e.first);
        get_idx(e.second);
    }

    int n = (int)id2idx.size();
    int start = 0;

    unordered_map<int,int> g2l;
    vector<int> local2global;   

    for (int g = 0; g < n; g++) {
        if (g % size == rank) {   
            int local_idx = (int)local2global.size();
            g2l[g] = local_idx;
            local2global.push_back(g);
        }
    }

    int local_n = (int)local2global.size();
    vector<int> deg_local(local_n, 0);

    for (auto &e : edges) {
        int u_idx = id2idx[e.first];
        int v_idx = id2idx[e.second];

        if (u_idx % size == rank) {
            int lu = g2l[u_idx];
            deg_local[lu]++;
        }
    }

    vector<int> row_ptr_local(local_n + 1, 0);
    for (int i = 0; i < local_n; i++) {
        row_ptr_local[i + 1] = row_ptr_local[i] + deg_local[i];
    }

    int local_edges = row_ptr_local[local_n];
    vector<int> col_idx_local(local_edges);

    vector<int> next_pos = row_ptr_local;
    for (auto &e : edges) {
        int u_idx = id2idx[e.first];
        int v_idx = id2idx[e.second];

        if (u_idx % size == rank) {
            int lu = g2l[u_idx];
            int pos = next_pos[lu]++;
            col_idx_local[pos] = v_idx;
        }
    }

    edges.clear();
    edges.shrink_to_fit();
    buf.clear();
    buf.shrink_to_fit();
    deg_local.clear();
    deg_local.shrink_to_fit();
    next_pos.clear();
    next_pos.shrink_to_fit();

    vector<int> frontier, next_frontier;
    vector<uint8_t> visited(n + 1, 0);
    if (rank == start % size) { 
        visited[start] = 1;
        frontier.push_back(start);
    }

    vector<vector<int>> buckets(size);
    for (int r = 0; r < size; r++) buckets[r].clear();

    vector<int> sendbuf;
    vector<int> sendcounts(size, 0);
    vector<int> sdispls(size, 0); 

    vector<int> recvbuf;
    vector<int> recvcounts(size, 0);
    vector<int> rdispls(size, 0);

    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();
    double t_neighbors = 0, t_pack = 0, t_alltoall = 0, t_alltoallv = 0, t_filter = 0;

    while (1) {
        double t0 = MPI_Wtime();

        for (int r = 0; r < size; r++) buckets[r].clear();
        sendbuf.clear();
        fill(sendcounts.begin(), sendcounts.end(), 0);

        // Traverse through neighbors and mark owners
        for (int node : frontier) {
            auto it = g2l.find(node);
            if (it == g2l.end()) continue;

            int lu = it->second;
            int row_start = row_ptr_local[lu];
            int row_end = row_ptr_local[lu + 1];
            
            for (int ei = row_start; ei < row_end; ei++) {
                int neighbor = col_idx_local[ei];
                int owner = neighbor % size;
                buckets[owner].push_back(neighbor);
            }
        }
        t_neighbors += MPI_Wtime() - t0;

        t0 = MPI_Wtime();
        int total_send = 0;
        for (int r = 0; r < size; r++) {
            sendcounts[r] = (int)buckets[r].size();
            sdispls[r] = total_send;
            total_send += sendcounts[r];
        }

        // Finish if no more unexplored
        int local_nodes = total_send, global_nodes = 0;
        MPI_Allreduce(&local_nodes, &global_nodes, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
        if (global_nodes == 0) break;

        sendbuf.resize(total_send);
        for (int r = 0; r < size; r++) {
            int off = sdispls[r];
            if (!buckets[r].empty()) {
                copy(buckets[r].begin(), buckets[r].end(), sendbuf.begin() + off);
            }
        }
        t_pack += MPI_Wtime() - t0;

        // Broadcast and receive sizing info for each rank
        t0 = MPI_Wtime();
        MPI_Alltoall(sendcounts.data(), 1, MPI_INT, 
                     recvcounts.data(), 1, MPI_INT, 
                     MPI_COMM_WORLD);
        t_alltoall += MPI_Wtime() - t0;

        // Reconstruct displacements for new nodes
        int total = 0;
        for (int r = 0; r < size; r++) {
            rdispls[r] = total;
            total += recvcounts[r];
        }
        recvbuf.resize(total);

        // Broadcast and receive new nodes
        t0 = MPI_Wtime();
        MPI_Alltoallv(sendbuf.data(), sendcounts.data(), sdispls.data(), MPI_INT, 
                      recvbuf.data(), recvcounts.data(), rdispls.data(), MPI_INT,
                      MPI_COMM_WORLD);
        t_alltoallv += MPI_Wtime() - t0;

        t0 = MPI_Wtime();
        next_frontier.clear();
        next_frontier.reserve(recvbuf.size());

        for (int node : recvbuf) {
            if (!visited[node]) {
                visited[node] = 1;
                next_frontier.push_back(node);
            }
        }

        frontier.swap(next_frontier);
        t_filter += MPI_Wtime() - t0;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double end_time = MPI_Wtime();
    double elapsed = end_time - start_time;

    double max_elapsed = 0.0;
    MPI_Reduce(&elapsed, &max_elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    // After loop: reduce + print (MAX across ranks)
    auto reduce_max = [&](double local) {
        double g = 0;
        MPI_Reduce(&local, &g, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        return g; // only valid on rank 0, but ok for printing guarded by rank==0
    };

    double g_neighbors = reduce_max(t_neighbors);
    double g_pack= reduce_max(t_pack);
    double g_alltoall = reduce_max(t_alltoall);
    double g_alltoallv = reduce_max(t_alltoallv);
    double g_filter = reduce_max(t_filter);

    if (rank == 0) {
        cout << "BFS runtime: " << max_elapsed << " seconds\n";
        cout << "phase max times (s):\n";
        cout << "  neighbors  : " << g_neighbors << "\n";
        cout << "  pack(send) : " << g_pack << "\n";
        cout << "  alltoall   : " << g_alltoall << "\n";
        cout << "  alltoallv  : " << g_alltoallv << "\n";
        cout << "  filter     : " << g_filter << "\n";
    }

    MPI_Finalize();
    return 0;
}
