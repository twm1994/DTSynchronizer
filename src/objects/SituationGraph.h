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

#include <vector>
#include <map>
#include "boost/tuple/tuple.hpp"
#include "boost/tuple/tuple_comparison.hpp"
#include "boost/tuple/tuple_io.hpp"
#include "SituationNode.h"
#include "SituationRelation.h"
#include "DirectedGraph.h"

using namespace std;
using namespace boost::tuples;

// forward declaration
class SituationEvolution;

class SituationGraph {
private:
    map<long, SituationNode> situationMap;
    typedef boost::tuple<long, long> edge_id;
    map<edge_id, SituationRelation> relationMap;
    vector<DirectedGraph> layers;
    // reachability index
    vector<vector<bool>> *ri;

    void buildReachabilityMatrix(set<long>& vertices, set<edge_id>& edges);
public:
    SituationGraph();
    vector<long> getAllOperationalSitutions();
    vector<long> getOperationalSitutions(long topNodeId);
    bool isReachable(long src, long dest);
    void loadModel(const std::string &filename, SituationEvolution* arrangeer);
    DirectedGraph getLayer (int index);
    int modelHeight();
    SituationNode getNode(long id);
    void print();
    virtual ~SituationGraph();
};

#endif /* OBJECTS_SITUATIONGRAPH_H_ */
