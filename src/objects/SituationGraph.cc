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

#include <omnetpp.h>
#include <stack>
#include <boost/json.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "boost/tuple/tuple.hpp"
#include "boost/tuple/tuple_comparison.hpp"
#include "boost/tuple/tuple_io.hpp"
#include "SituationEvolution.h"
#include "SituationGraph.h"

using namespace omnetpp;
namespace pt = boost::property_tree;

SituationGraph::SituationGraph() {
    // TODO Auto-generated constructor stub
}

vector<long> SituationGraph::getAllOperationalSitutions() {
    vector<long> operational_situations;
    DirectedGraph& bottom = layers[layers.size() - 1];
    vector<long> bottom_nodes = bottom.topo_sort();
    for (auto node : bottom_nodes) {
        SituationNode& operational_situation = situationMap[node];
        operational_situations.push_back(operational_situation.id);
    }
    return operational_situations;
}

vector<long> SituationGraph::getOperationalSitutions(long topNodeId) {
    vector<long> operational_situations;

    SituationNode& topNode = situationMap[topNodeId];
    stack<SituationNode> toChecks;
    toChecks.push(topNode);
    while (!toChecks.empty()) {
        SituationNode toCheck = toChecks.top();
        toChecks.pop();
        if (!toCheck.evidences.empty()) {
            for (auto evidenceId : toCheck.evidences) {
                SituationNode evidence = situationMap[evidenceId];
                toChecks.push(evidence);
            }
        } else {
            operational_situations.push_back(toCheck.id);
        }
    }

    return operational_situations;
}

bool SituationGraph::isReachable(long src, long dest){
    int i = situationMap[src].index;
    int j = situationMap[dest].index;
    return ri->at(i)[j];
}

/*
 * For square matrix only
 */
vector<vector<bool>> boolMatrixPower(vector<vector<bool>> &mat, int n) {
    vector<vector<bool>> mat_n = mat;

    for (int pow = 0; pow < n - 1; pow++) {
        vector<vector<bool>> temp = mat_n;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                bool value = false;
                for (int m = 0; m < n; m++) {
                    value = value || (temp[i][m] && mat[m][j]);
                    if (value)
                        // early out
                        break;
                }
                mat_n[i][j] = value;
            }
        }
    }

    return mat_n;
}

/*
 * For square matrix only
 */
void boolMatrixAdd(vector<vector<bool>>* result, vector<vector<bool>> &mat1,
        vector<vector<bool>> &mat2) {
    int n = mat1.size();

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            result->at(i)[j] = mat1[i][j] || mat2[i][j];
        }
    }
}

void SituationGraph::buildReachabilityMatrix(set<long>& vertices, set<edge_id>& edges) {
    /*
     * initialize reachability matrix
     */
    int size = vertices.size();
    ri = new vector<vector<bool>>(size, vector<bool>(size, false));

    /*
     * build adjacency matrix
     */
    vector<vector<bool>> adjMatrix(size, vector<bool>(size, false));
    for (auto src : vertices) {
        for (auto dest : vertices) {
            if (src != dest) {
                edge_id eid(src, dest);
                if (edges.count(eid)) {
                    int i = situationMap[src].index;
                    int j = situationMap[dest].index;
                    adjMatrix[i][j] = true;
                }
            }
        }
    }

    /*
     * build reachability matrix
     */
    for (int i = 1; i <= size; i++) {
        vector<vector<bool>> adjMatrixPow = boolMatrixPower(adjMatrix, i);
        vector<vector<bool>> temp = *ri;
        boolMatrixAdd(ri, temp, adjMatrixPow);

    }
}

void SituationGraph::loadModel(const std::string &filename, SituationEvolution* se) {
    // Create a root
    pt::ptree root;
    // Load the json file in this ptree
    pt::read_json(filename, root);
    int index = 0;

    /*
     * for building reachability index
     */
    set<long> vertices;
    set<edge_id> edges;

    /*
     * Create situation nodes
     */
    for (pt::ptree::value_type & layer : root.get_child("layers")) {

        std::map<long, SituationNode> layerMap;

        for (pt::ptree::value_type & node : layer.second) {

            SituationNode situation;
            long id = node.second.get<long>("ID");
            situation.id = id;
            vertices.insert(id);
            situation.index = index;
            index++;

            double duration = node.second.get<double>("Duration") / 1000.0;
            if(node.second.get<string>("Cycle") != "null"){
                // cycle is in millisecond
                double cycle = node.second.get<double>("Cycle") / 1000.0;
                se->addInstance(id, SimTime(duration), SimTime(cycle));
            }else{
                se->addInstance(id, SimTime(duration));
            }

            if (!node.second.get_child("Predecessors").empty()) {
                for (pt::ptree::value_type &pre : node.second.get_child(
                        "Predecessors")) {
                    SituationRelation relation;
                    long src = pre.second.get<long>("ID");
                    relation.src = src;
                    relation.dest = situation.id;
                    situation.causes.push_back(src);
                    relation.type = SituationRelation::H;
                    short relationValue = pre.second.get<short>("Relation");
                    switch (relationValue) {
                    case 1:
                        relation.relation = SituationRelation::AND;
                        break;
                    case 2:
                        relation.relation = SituationRelation::OR;
                        break;
                    default:
                        relation.relation = SituationRelation::SOLE;
                    }
                    relation.weight = pre.second.get<double>("Weight-x");
                    edge_id eid(src, relation.dest);
                    relationMap[eid] = relation;
                    edges.insert(eid);
                }
            }

            if (!node.second.get_child("Children").empty()) {
                for (pt::ptree::value_type & chd : node.second.get_child(
                        "Children")) {
                    SituationRelation relation;
                    long src = chd.second.get<long>("ID");
                    relation.src = src;
                    relation.dest = situation.id;
                    situation.evidences.push_back(chd.second.get<long>("ID"));
                    relation.type = SituationRelation::V;
                    short relationValue = chd.second.get<short>("Relation");
                    switch (relationValue) {
                    case 1:
                        relation.relation = SituationRelation::AND;
                        break;
                    case 2:
                        relation.relation = SituationRelation::OR;
                        break;
                    default:
                        relation.relation = SituationRelation::SOLE;
                    }
                    relation.weight = chd.second.get<double>("Weight-y");
                    edge_id eid(src, relation.dest);
                    relationMap[eid] = relation;
                    edge_id reid(relation.dest, src);
                    edges.insert(eid);
                    edges.insert(reid);
                }
            }

            layerMap[situation.id] = situation;
        }

        /**
         * Create situation graph layers
         */
        DirectedGraph graph;
        for (auto m : layerMap) {
            graph.add_vertex(m.first);
            SituationNode& node = m.second;
            for (auto p : node.causes) {
                graph.add_edge(p, node.id);
            }
        }
//        graph.print();
        layers.push_back(graph);

        // Create mapping relations
        situationMap.insert(layerMap.begin(), layerMap.end());
    }

    /*
     * Create reachability index
     */
    buildReachabilityMatrix(vertices, edges);
//    cout << "print reachability matrix" << endl;
//    for(auto row : *ri){
//        for(auto col : row){
//            cout << col << "  ";
//        }
//        cout << endl;
//    }
}

DirectedGraph SituationGraph::getLayer (int index){
    return layers[index];
}

SituationNode SituationGraph::getNode(long id){
    return situationMap[id];
}

int SituationGraph::modelHeight(){
    return layers.size();
}

void SituationGraph::print() {
    for (auto m : situationMap) {
        cout << m.second;
    }
}

SituationGraph::~SituationGraph() {
    // TODO why cannot release pointer here?
//    delete ri;
}

