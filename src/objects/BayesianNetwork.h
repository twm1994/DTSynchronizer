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

#ifndef OBJECTS_BAYESIANNETWORK_H_
#define OBJECTS_BAYESIANNETWORK_H_

#include <set>
#include <map>
#include <tuple>
#include <dlib/bayes_utils.h>
#include <dlib/graph_utils.h>
#include <dlib/graph.h>
#include <dlib/directed_graph.h>

using namespace std;
using namespace dlib;
using namespace bayes_node_utils;

class BayesianNetwork {
private:
    bayesian_network_join_tree* solution_with_evidence;
    directed_graph<bayes_node>::kernel_1a_c BNet;
public:
    /*
     * 1. edges contains a <src, dest> pairs of nodes.
     * 2. They key of each entry in CPT is a tuple<child, c_state, set<parent, p_state>>,
     * an empty set for a leaf node; The value of the entry is the conditional probability.
     */
    void BuildNetwork(long node_count, std::set<std::pair<long, long>> edges,
            std::map<std::tuple<long, long, std::set<std::pair<long, long>>>,
                    double> CPT);
    /*
     * An evidence is in the format of <node ID, state>, where state is either 0 or 1.
     */
    void buildSolution(std::map<long, long> evidences);
    double getProbability(long node, long state);
    void clearSolution();
    BayesianNetwork();
    virtual ~BayesianNetwork();
};

#endif /* OBJECTS_BAYESIANNETWORK_H_ */
