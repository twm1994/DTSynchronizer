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
#include <iostream> // Added for std::ostream
#include <memory> // Added for std::unique_ptr
#include <fstream>
#include <nlohmann/json.hpp>

// Project includes
#include "DirectedGraph.h"
#include "SituationNode.h"
#include "SituationRelation.h"

using namespace std;
using json = nlohmann::json;

// Forward declaration
class SituationEvolution;

class SituationGraph {
private:
    // Reachability index
    std::vector<std::vector<bool>>* ri;
public:
    std::map<long, SituationNode> situationMap;
    typedef pair<long, long> edge_id;
    std::map<edge_id, SituationRelation> relationMap;
    std::vector<DirectedGraph> layers;
private:
    void buildReachabilityMatrix(std::set<long>& vertices, std::set<edge_id>& edges);

public:
    SituationGraph();
    SituationGraph(const SituationGraph& other);
    SituationGraph& operator=(const SituationGraph& other);
    SituationGraph(SituationGraph&&) = default;
    SituationGraph& operator=(SituationGraph&&) = default;
    std::vector<long> getAllOperationalSitutions();
    std::vector<long> getOperationalSitutions(long topNodeId);
    bool isReachable(long src, long dest);
    void loadModel(const std::string &filename, SituationEvolution* se);
    DirectedGraph getLayer(int index) const;
    int modelHeight() const;
    SituationNode getNode(long id) const;
    const SituationRelation* getRelation(long src, long dest) const;
    const std::map<long, SituationRelation>& getOutgoingRelations(long nodeId) const;
    int numOfNodes() const;
    void print(std::ostream& os = std::cout);
    virtual ~SituationGraph();
};

#endif /* OBJECTS_SITUATIONGRAPH_H_ */
