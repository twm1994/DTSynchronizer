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

#include <boost/json.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "boost/tuple/tuple.hpp"
#include "boost/tuple/tuple_comparison.hpp"
#include "boost/tuple/tuple_io.hpp"
#include "../objects/SituationNode.h"
#include "../objects/SituationRelation.h"
#include "../objects/DirectedGraph.h"
#include "EventSource.h"

// Short alias for this namespace
namespace pt = boost::property_tree;

Define_Module(EventSource);

void EventSource::initialize() {
    // Create a root
    pt::ptree root;
    // Load the json file in this ptree
    pt::read_json("../files/SG.json", root);

    std::map<long, SituationNode> situationMap;
    typedef boost::tuple<long, long> edge_id;
    std::map<edge_id, SituationRelation> relationMap;
    std::vector<DirectedGraph> layers;

    for (pt::ptree::value_type &layer : root.get_child("layers")) {

        std::map<long, SituationNode> layerMap;

        for (pt::ptree::value_type &node : layer.second) {

            SituationNode situation;
            situation.id = node.second.get<int>("ID");

            if (!node.second.get_child("Predecessor").empty()) {
                for (pt::ptree::value_type &pre : node.second.get_child(
                        "Predecessor")) {
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
                }
            }

            if (!node.second.get_child("Children").empty()) {
                for (pt::ptree::value_type &chd : node.second.get_child(
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
                }
            }

            layerMap[situation.id] = situation;
        }

        DirectedGraph graph;
        for (auto m : layerMap) {
            SituationNode node = m.second;
            for (auto p : node.causes) {
                graph.add_edge(p, node.id);
            }
        }
//        graph.print();

        situationMap.insert(layerMap.begin(), layerMap.end());
        layers.push_back(graph);
    }

//    for (auto m : situationMap) {
//        cout << m.second;
//    }
//    for (auto m : relationMap) {
//        cout << m.second;
//    }
}

void EventSource::handleMessage(cMessage *msg) {
    // TODO - Generated method body
}
