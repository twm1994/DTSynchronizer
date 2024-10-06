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
#include "DirectedGraph.h"

DirectedGraph::DirectedGraph() {
    // TODO Auto-generated constructor stub

}

// Function to add an edge from vertices u to v of the graph
void DirectedGraph::add_edge(long src, long dest) {
    // Add edge from u to v
    adjList[src].push_back(dest);
}

// Function to print the adjacency list representation of the graph
void DirectedGraph::print() {
    cout << "Adjacency list for the Graph: " << endl;
    // Iterate over each vertex
    for (auto i : adjList) {
        // Print the vertex
        cout << i.first << " -> ";
        // Iterate over the connected vertices
        for (auto j : i.second) {
            // Print the connected vertex
            cout << j << " ";
        }
        cout << endl;
    }
}

void DirectedGraph::DFS_topological(unordered_map<long, bool> &visited, stack<long> &st, long node) {
    visited[node] = true;

    for (auto neighbor : adjList[node]) {
        if (!visited[neighbor]) {
            DFS_topological(visited, st, neighbor);
        }
    }
    //Pushing a node after all its connected elements are traversed.
    st.push(node);
    return;
}

vector<long> DirectedGraph::topo_sort() {
    unordered_map<long, bool> visited;
    stack<long> st;

    int V = adjList.size();
    for(auto adj : adjList){
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

    return ans;
}

DirectedGraph::~DirectedGraph() {
    // TODO Auto-generated destructor stub
}

