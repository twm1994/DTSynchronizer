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

#ifndef OBJECTS_SITUATIONGRAPH_H_
#define OBJECTS_SITUATIONGRAPH_H_

// Standard library includes
#include <map>
#include <vector>
#include <utility>
// Boost includes
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/tuple/tuple_io.hpp>
// Dlib includes
#include <dlib/set.h>

// Project includes
#include "DirectedGraph.h"
#include "SituationNode.h"
#include "SituationRelation.h"

using namespace std;

// Forward declaration
class SituationEvolution;

class SituationGraph {
private:
    // Reachability index
    vector<vector<bool>> *ri;
public:
    map<long, SituationNode> situationMap;
    typedef pair<long, long> edge_id;
    map<edge_id, SituationRelation> relationMap;
    vector<DirectedGraph> layers;
    void buildReachabilityMatrix(set<long>& vertices, set<edge_id>& edges);

public:
    SituationGraph();
    vector<long> getAllOperationalSitutions();
    vector<long> getOperationalSitutions(long topNodeId);
    bool isReachable(long src, long dest);
    void loadModel(const std::string &filename, SituationEvolution* arranger);
    DirectedGraph getLayer(int index) const;
    int modelHeight() const;
    SituationNode getNode(long id) const;
    const SituationRelation* getRelation(long src, long dest) const;
    const std::map<long, SituationRelation>& getOutgoingRelations(long nodeId) const;
    // Create a subgraph containing only the specified nodes and edges
    SituationGraph createSubgraph(const dlib::set<long>::kernel_1a& nodeSet,
                                 const dlib::set<std::pair<long, long>>::kernel_1a& edgeSet) const;
    int numOfNodes() const;
    void print();
    virtual ~SituationGraph();
};

#endif /* OBJECTS_SITUATIONGRAPH_H_ */
