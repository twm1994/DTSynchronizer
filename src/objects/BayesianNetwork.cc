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

#include "BayesianNetwork.h"

BayesianNetwork::BayesianNetwork() {
    solution_with_evidence = NULL;
}

BayesianNetwork::~BayesianNetwork() {
    if (solution_with_evidence) {
        delete solution_with_evidence;
    }
}

void BayesianNetwork::BuildNetwork(long node_count,
        std::set<std::pair<long, long>> edges,
        std::map<std::tuple<long, long, std::set<std::pair<long, long>>>, double> CPT) {
    std::set<long> nodes;
    /*
     * Initialize Bayesian network
     */
    cout << "number_of_nodes: " << node_count << endl;
    BNet.set_number_of_nodes(node_count);
    for (auto edge : edges) {
        long src = edge.first;
        long dest = edge.second;
        nodes.insert(src);
        nodes.insert(dest);
        BNet.add_edge(src, dest);
    }
    /*
     * Inform all the nodes in the network that they are binary.
     * That is, they only have two possible values.
     */
    for (auto node : nodes) {
        set_node_num_values(BNet, node, 2);
    }

    /*
     * Construct the CPT of the SG
     */
    assignment parent_state;
    for (auto entry : CPT) {
        // child node ID
        long c_id = std::get<0>(entry.first);
        // child node state
        long c_state = std::get<1>(entry.first);
        double p = entry.second;
        std::set<std::pair<long, long>> causes = std::get<2>(entry.first);
        if (causes.size() > 0) {
            for (auto cause : causes) {
                long id = cause.first;
                long p_state = cause.second;
                parent_state.add(id, p_state);
            }
        }
        set_node_probability(BNet, c_id, c_state, parent_state, p);
        // Clear out parent state so that it doesn't have any of the previous assignment
        parent_state.clear();
    }
}

void BayesianNetwork::buildSolution(std::map<long, long> evidences) {
    typedef dlib::set<unsigned long>::compare_1b_c set_type;
    typedef graph<set_type, set_type>::kernel_1a_c join_tree_type;
    join_tree_type join_tree;
    create_moral_graph(BNet, join_tree);
    create_join_tree(join_tree, join_tree);

    for (auto evidence : evidences) {
        long id = evidence.first;
        long state = evidence.second;
        set_node_value(BNet, id, state);
        set_node_as_evidence(BNet, id);
    }

    solution_with_evidence = new bayesian_network_join_tree(BNet, join_tree);
}

double BayesianNetwork::getProbability(long node, long state) {
    return solution_with_evidence->probability(node)(state);
}

void BayesianNetwork::clearSolution() {
    if (solution_with_evidence) {
        delete solution_with_evidence;
        solution_with_evidence = NULL;
    }
    BNet.clear();
}
