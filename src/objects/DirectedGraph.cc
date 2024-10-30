//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include <iostream>
#include <algorithm>
#include "DirectedGraph.h"

DirectedGraph::DirectedGraph() {
    // TODO Auto-generated constructor stub
}

void DirectedGraph::add_vertex(long vertex) {
    verList.insert(vertex);
}

// Function to add an edge from vertices u to v of the graph
void DirectedGraph::add_edge(long src, long dest) {
    // Add edge from u to v
    adjList[src].push_back(dest);
}

void DirectedGraph::DFS_topological(unordered_map<long, bool> &visited, stack<long> &st,
        long node) {
    visited[node] = true;

    for (auto neighbor : adjList[node]) {
        if (!visited[neighbor]) {
            DFS_topological(visited, st, neighbor);
        }
    }
    //Pushing a node after all its connected elements are traversed.
    if (node != -1) {
        st.push(node);
    }
    return;
}

vector<long> DirectedGraph::topo_sort() {
    unordered_map<long, bool> visited;
    stack<long> st;

    int V = adjList.size();
    for (auto adj : adjList) {
        long key = adj.first;
        if (!visited[key]) {
            DFS_topological(visited, st, key);
        }
    }

    vector<long> ans;
    while (!st.empty()) {
        ans.push_back(st.top());
        st.pop();
    }

    // add orphan vertices at the beginning of the vector to return
    int i = 0;
    for (auto vertex : verList) {
        if (find(ans.begin(), ans.end(), vertex) == ans.end()) {
            ans.insert(ans.begin() + i, vertex);
            i++;
        }
    }

    return ans;
}

// Function to print the adjacency list representation of the graph
void DirectedGraph::print() {
    set<long> printed;
    cout << "Adjacency list for the Graph: " << endl;
    // Iterate over each vertex
    for (auto i : adjList) {
        // Print the vertex
        cout << i.first << " -> ";
        printed.insert((long) i.first);
        // Iterate over the connected vertices
        for (auto j : i.second) {
            // Print the connected vertex
            cout << j << " ";
            printed.insert((long) j);
        }
        cout << endl;
    }

    // print orphan vertices
    for (auto vertex : verList) {
        auto it = printed.find(vertex);
        if (it == printed.end()) {
            cout << vertex << endl;
        }
    }
}

DirectedGraph::~DirectedGraph() {
    // TODO Auto-generated destructor stub
}

