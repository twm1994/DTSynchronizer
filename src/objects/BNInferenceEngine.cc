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

#include "BNInferenceEngine.h"

BNInferenceEngine::BNInferenceEngine() {
    // TODO Auto-generated constructor stub

}

BNInferenceEngine::~BNInferenceEngine() {
    // TODO Auto-generated destructor stub
}

void BNInferenceEngine::loadModel(SituationGraph sg) {
    /*
     * Initialize Bayesian network
     */
    // the informed graph size must be greater than any node index
    long numOfNodes = (sg.modelHeight() + 1) * 100;
    cout << "number_of_nodes: " << numOfNodes << endl;
    BNet.set_number_of_nodes(numOfNodes);
    for (auto relation : sg.relationMap) {
        long src = relation.first.first;
        long dest = relation.first.second;
        BNet.add_edge(src, dest);
    }
    /*
     * Inform all the nodes in the network that they are binary.
     * That is, they only have two possible values.
     */
    for (auto node : sg.situationMap) {
        set_node_num_values(BNet, node.first, 2);
    }

    /*
     * Construct the CPT of the SG
     */
    assignment parent_state;
    DirectedGraph g = sg.getLayer(0);
    std::vector<long> sortedNodes = g.topo_sort();
    for (auto node : sortedNodes) {

//        cout << "set probability of node " << node << endl;

        std::vector<long> &causes = sg.situationMap[node].causes;
        if (causes.empty()) {

//            cout << "set priori probability of node " << node << endl;

            set_node_probability(BNet, node, 1, parent_state, 0.5);
            set_node_probability(BNet, node, 0, parent_state, 0.5);
        } else {
            const short size = causes.size();
            // conditional probabilities of a single cause: p(B|A) = weight, p(B|-A) = 0
            pair<double, double> p[size];
            for (int i = 0; i < size; i++) {
                SituationGraph::edge_id eid;
                eid.first = causes[i];
                eid.second = node;
                SituationRelation sr = sg.relationMap[eid];
                p[i].first = 0;
                p[i].second = sr.weight;
            }

            // 2^n binary combinations
            const short totalCombinations = 1 << size;
            for (int i = 0; i < totalCombinations; i++) {
                // Be careful! It means the number of cause of a situation cannot exceed 32
                bitset<32> binary(i);
                // conditional probability of multiple causes
                double p_cond = 1;
                for (int i = 0; i < size; i++) {

//                    cout << "causes[i]: " << causes[i] << endl;

                    if (binary[i]) {
                        p_cond *= p[i].second;
                        if(!parent_state.has_index(causes[i])){
                            parent_state.add(causes[i], 1);
                        }else{
                            parent_state[causes[i]] = 1;
                        }
                    } else {
                        p_cond *= p[i].first;
                        if(!parent_state.has_index(causes[i])){
                            parent_state.add(causes[i], 0);
                        }else{
                            parent_state[causes[i]] = 0;
                        }
                    }
                }
                set_node_probability(BNet, node, 1, parent_state, p_cond);
                set_node_probability(BNet, node, 0, parent_state, 1 - p_cond);

//                cout << "print CPT of node " << node << endl;
//                cout << "condition: ";
//                for(int i= 0; i < size; i++){
//                  cout << binary[i] << ", ";
//                }
//                cout << endl;
//                cout << "conditional probability of p(triggered): " << p_cond << endl;
//                cout << "conditional probability of p(untriggered): " << 1- p_cond << endl;
            }
            // Clear out parent state so that it doesn't have any of the previous assignment
            parent_state.clear();
        }
    }
}

void BNInferenceEngine::reason(SituationGraph sg,
        std::map<int, SituationInstance> &instanceMap, simtime_t current) {
    typedef dlib::set<unsigned long>::compare_1b_c set_type;
    typedef graph<set_type, set_type>::kernel_1a_c join_tree_type;
    join_tree_type join_tree;
    create_moral_graph(BNet, join_tree);
    create_join_tree(join_tree, join_tree);
    bool hasEvidence = false;
    for (auto instance : instanceMap) {
        long sid = instance.first;
        SituationInstance si = instance.second;
        if (si.state != SituationInstance::UNDETERMINED) {
            set_node_value(BNet, sid, si.state);
            set_node_as_evidence(BNet, sid);
            hasEvidence = true;

//            cout << "set evidence of node " << sid << ": " << si.state << endl;
        }
    }

    if(hasEvidence){
        bayesian_network_join_tree solution_with_evidence(BNet, join_tree);
        for (auto& instance : instanceMap) {
            long sid = instance.first;
            SituationInstance &si = instance.second;
            // probability of triggering
            double p_tr = solution_with_evidence.probability(sid)(1);
            if(si.state == SituationInstance::UNDETERMINED){
                if (p_tr >= sg.situationMap[sid].threshold) {
                    si.state = SituationInstance::TRIGGERED;
                    si.counter++;
                    si.next_start = current;
                } else {
                    si.state = SituationInstance::UNTRIGGERED;
                }

//                cout << "probability of triggering node " << sid << ": " << p_tr << endl;
//                cout << "state of undetermined node " << sid << ": " << si.state << endl;
//                cout << "counter of node " << sid << ": " << si.counter << endl;
            }
        }
    }
}
