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
#include "../objects/SituationNode.h"
#include "../objects/SituationRelation.h"
#include "EventSource.h"

// Short alias for this namespace
namespace pt = boost::property_tree;

Define_Module(EventSource);

void EventSource::initialize() {
    // Create a root
    pt::ptree root;
    // Load the json file in this ptree
    pt::read_json("../files/SG.json", root);

    std::vector<SituationNode> situations;
    std::vector<SituationRelation> relations;
    for (pt::ptree::value_type &node : root.get_child("nodes")) {
        SituationNode situation;
        situation.id = node.second.get<int>("ID");
        situations.push_back(situation);

        if (!node.second.get_child("Predecessor").empty()) {
            for (pt::ptree::value_type &pre : node.second.get_child(
                    "Predecessor")) {
                SituationRelation relation;
                relation.src = situation.id;
                relation.dest = pre.second.get<long>("ID");
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
                relations.push_back(relation);
            }
        }

        if (!node.second.get_child("Children").empty()) {
            for (pt::ptree::value_type &chd : node.second.get_child("Children")) {
                SituationRelation relation;
                relation.dest = situation.id;
                relation.src = chd.second.get<long>("ID");
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
                relations.push_back(relation);
            }
        }
    }

//    cout << "print situations" << endl;
//    for (auto s : situations) {
//        cout << s << endl;
//    }
//    cout << "print situation relations" << endl;
//    cout << "relation size: " << relations.size() << endl;
//    for (auto r : relations) {
//        cout << r << endl;
//    }
}

void EventSource::handleMessage(cMessage *msg) {
    // TODO - Generated method body
}
