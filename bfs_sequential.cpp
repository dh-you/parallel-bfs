#include <iostream>
#include <vector>
#include <queue>
#include <cstdint>
#include <string>
#include <sstream>
#include <utility>
#include <unordered_map>
#include <chrono>

using namespace std;

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    vector<pair<int,int>> edges;

    string line;

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

    while (getline(cin, line)) {
        if (line.empty() || line[0] == '#') continue;

        stringstream ss(line);
        int u, v;
        if (!(ss >> u >> v)) continue;

        edges.emplace_back(u, v);
    }

    for (auto &e : edges) {
        get_idx(e.first);
        get_idx(e.second);
    }

    int n = (int)id2idx.size();
    int start = 0;

    // Construct adjacency list
    vector<vector<int>> adj(n);
    for (auto &e : edges) {
        int u_idx = id2idx[e.first];
        int v_idx = id2idx[e.second];

        adj[u_idx].push_back(v_idx);
    }

    // Standard BFS
    vector<int> dist(n, -1);
    vector<uint8_t> visited(n, 0);
    queue<int> q;

    dist[start] = 0;
    visited[start] = 1;
    q.push(start);

    auto t0 = chrono::high_resolution_clock::now();
    while (!q.empty()) {
        int node = q.front();
        q.pop();
        for (int neighbor : adj[node]) {
            if (visited[neighbor]) continue;
            visited[neighbor] = 1;
            dist[neighbor] = dist[node] + 1;
            q.push(neighbor);
        }
    }

    auto t1 = chrono::high_resolution_clock::now();
    chrono::duration<double> elapsed = t1 - t0;
    cout << "Sequential BFS time: " << elapsed.count() << " seconds\n";

    return 0;
}
