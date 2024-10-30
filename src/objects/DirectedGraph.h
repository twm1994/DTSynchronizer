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

#ifndef OBJECTS_DIRECTEDGRAPH_H_
#define OBJECTS_DIRECTEDGRAPH_H_

#include <list>
#include <vector>
#include <map>
#include <unordered_map>
#include <stack>
#include <set>

using namespace std;

class DirectedGraph {
private:
    map<long, list<long> > adjList; // Adjacency list to store the graph
    set<long> verList;

    void DFS_topological(unordered_map<long, bool> &visited, stack<long> &st, long node);

public:
    DirectedGraph();
    void add_vertex(long vertex);
    // Function to add an edge between vertices u and v of the graph
    void add_edge(long src, long dist);
    // Function to print the adjacency list representation of the graph
    void print();
    //Function to return list containing vertices in Topological order.
    vector<long> topo_sort();
    virtual ~DirectedGraph();
};

#endif /* OBJECTS_DIRECTEDGRAPH_H_ */
