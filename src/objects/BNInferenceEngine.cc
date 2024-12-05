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
#include <stdexcept>
#include <cmath>

BNInferenceEngine::BNInferenceEngine() : _joinTree(nullptr) {}

BNInferenceEngine::~BNInferenceEngine() = default;

void BNInferenceEngine::loadModel(SituationGraph sg) {
    // Clear existing network
    _bn.clear();
    _nodeMap.clear();
    _joinTree.reset();
    
    // Step 1: Add nodes for each situation
    for (int layer = 0; layer < sg.modelHeight(); layer++) {
        DirectedGraph currentLayer = sg.getLayer(layer);
        for (const auto& nodeId : currentLayer.getVertices()) {
            const SituationNode& node = sg.getNode(nodeId);
            addNode(std::to_string(node.id), node);
        }
    }
    
    // Step 2: Add edges based on relations
    for (int layer = 0; layer < sg.modelHeight(); layer++) {
        DirectedGraph currentLayer = sg.getLayer(layer);
        for (const auto& nodeId : currentLayer.getVertices()) {
            const SituationNode& node = sg.getNode(nodeId);
            // Add edges for both vertical and horizontal relations
            for (const auto& [childId, relation] : sg.getOutgoingRelations(node.id)) {
                addEdge(std::to_string(node.id), std::to_string(childId), relation.weight);
            }
        }
    }

    // Build join tree for inference
    buildJoinTree();
}

void BNInferenceEngine::reason(SituationGraph sg,
        std::map<long, SituationInstance> &instanceMap, simtime_t current) {
    
    // Set evidence from known states
    for (const auto& [id, instance] : instanceMap) {
        if (instance.state != SituationInstance::UNDETERMINED) {
            std::string nodeName = std::to_string(id);
            // Set node value (0 for UNTRIGGERED, 1 for TRIGGERED)
            unsigned long value = (instance.state == SituationInstance::TRIGGERED) ? 1 : 0;
            setNodeValue(nodeName, value);
            setNodeAsEvidence(nodeName);
        }
    }
    
    // Build join tree for this inference
    buildJoinTree();
    
    // Update beliefs based on inference results
    for (auto& [id, instance] : instanceMap) {
        if (instance.state == SituationInstance::UNDETERMINED) {
            std::vector<double> posterior = getPosterior(std::to_string(id));
            instance.beliefValue = posterior[1];  // P(True)
            instance.beliefUpdated = true;
            
            // Update state based on belief threshold
            if (instance.beliefValue >= instance.beliefThreshold) {
                instance.state = SituationInstance::TRIGGERED;
            } else {
                instance.state = SituationInstance::UNTRIGGERED;
            }
        }
    }
}

void BNInferenceEngine::buildJoinTree() {
    try {
        _joinTree = std::make_unique<join_tree_type>();
        
        // Create a temporary graph for triangulation
        using dgraph = dlib::graph<set_type>::kernel_1a;
        dgraph temp_graph;
        
        // Copy structure from Bayesian network to temporary graph
        for (unsigned long i = 0; i < _bn.number_of_nodes(); ++i) {
            temp_graph.add_node();
            auto& node_set = temp_graph.node(i).data;
            node_set.clear();
            
            // Store current node index
            unsigned long idx = i;
            node_set.add(idx);
            
            // Add parents to node set
            const auto& node = _bn.node(i);
            for (unsigned long j = 0; j < node.number_of_parents(); ++j) {
                unsigned long parent_idx = node.parent(j).index();
                node_set.add(parent_idx);
            }
        }
        
        // Create edges in temporary graph
        for (unsigned long i = 0; i < temp_graph.number_of_nodes(); ++i) {
            const auto& node_set = temp_graph.node(i).data;
            for (unsigned long j = i + 1; j < temp_graph.number_of_nodes(); ++j) {
                const auto& other_set = temp_graph.node(j).data;
                
                // Check for intersection
                bool has_intersection = false;
                for (unsigned long k = 0; k < node_set.size(); ++k) {
                    if (other_set.is_member(k)) {
                        has_intersection = true;
                        break;
                    }
                }
                
                if (has_intersection) {
                    temp_graph.add_edge(i, j);
                }
            }
        }
        
        // Create join tree from triangulated graph
        for (unsigned long i = 0; i < temp_graph.number_of_nodes(); ++i) {
            _joinTree->add_node();
            // Initialize probability matrix for each node
            _joinTree->node(i).data = dlib::matrix<double>(2, 1);  // Binary node
            _joinTree->node(i).data = 0.5;  // Initialize with uniform probabilities
        }
        
        // Copy edges from temporary graph
        for (unsigned long i = 0; i < temp_graph.number_of_nodes(); ++i) {
            const auto& node = temp_graph.node(i);
            for (unsigned long j = 0; j < node.number_of_neighbors(); ++j) {
                unsigned long neighbor = node.neighbor(j).index();
                if (i < neighbor) {  // Add edge only once
                    _joinTree->add_edge(i, neighbor);
                }
            }
        }
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Error building join tree: " + std::string(e.what()));
    }
}

void BNInferenceEngine::addNode(const std::string& name, const SituationNode& node) {
    unsigned long idx = _bn.add_node();
    _nodeMap[name] = idx;
    
    // Set number of values (binary node: true/false)
    set_node_num_values(_bn, idx, 2);
    
    // Set prior probabilities if no parents
    if (node.causes.empty()) {
        dlib::assignment parent_state;
        // Default prior probability of 0.5 for both states
        set_node_probability(_bn, idx, 1, parent_state, 0.5);
        set_node_probability(_bn, idx, 0, parent_state, 0.5);
    }
}

void BNInferenceEngine::addEdge(const std::string& parent, const std::string& child, double weight) {
    if (_nodeMap.find(parent) == _nodeMap.end() || _nodeMap.find(child) == _nodeMap.end()) {
        throw std::runtime_error("Node not found when adding edge");
    }
    
    unsigned long parent_idx = _nodeMap[parent];
    unsigned long child_idx = _nodeMap[child];
    
    _bn.add_edge(parent_idx, child_idx);
    
    // Set conditional probabilities
    dlib::assignment parent_state;
    parent_state.add(parent_idx, 1);  // Parent is true
    set_node_probability(_bn, child_idx, 1, parent_state, weight);
    set_node_probability(_bn, child_idx, 0, parent_state, 1.0 - weight);
    
    parent_state[parent_idx] = 0;  // Parent is false
    set_node_probability(_bn, child_idx, 1, parent_state, 0.0);
    set_node_probability(_bn, child_idx, 0, parent_state, 1.0);
}

std::vector<double> BNInferenceEngine::getPosterior(const std::string& nodeName) {
    if (_nodeMap.find(nodeName) == _nodeMap.end()) {
        throw std::runtime_error("Node " + nodeName + " not found in network");
    }
    
    unsigned long node_idx = _nodeMap[nodeName];
    std::vector<double> posterior(2);
    
    if (_joinTree) {
        // Get marginal probabilities from join tree
        const auto& probs = _joinTree->node(node_idx).data;
        posterior[0] = probs(0,0);  // P(False)
        posterior[1] = probs(1,0);  // P(True)
    } else {
        // Default probabilities if no inference has been performed
        posterior[0] = posterior[1] = 0.5;
    }
    
    return posterior;
}

void BNInferenceEngine::setNodeValue(const std::string& nodeName, unsigned long value) {
    if (_nodeMap.find(nodeName) == _nodeMap.end()) {
        throw std::runtime_error("Node " + nodeName + " not found in network");
    }
    if (value > 1) {
        throw std::runtime_error("Invalid value. Must be 0 or 1");
    }
    set_node_value(_bn, _nodeMap[nodeName], value);
}

void BNInferenceEngine::setNodeAsEvidence(const std::string& nodeName) {
    if (_nodeMap.find(nodeName) == _nodeMap.end()) {
        throw std::runtime_error("Node " + nodeName + " not found in network");
    }
    set_node_as_evidence(_bn, _nodeMap[nodeName]);
}

std::vector<unsigned long> BNInferenceEngine::getParents(unsigned long node_idx) const {
    std::vector<unsigned long> parents;
    const auto& node = _bn.node(node_idx);
    for (unsigned long i = 0; i < node.number_of_parents(); ++i) {
        parents.push_back(node.parent(i).index());
    }
    return parents;
}

std::vector<unsigned long> BNInferenceEngine::getChildren(unsigned long node_idx) const {
    std::vector<unsigned long> children;
    const auto& node = _bn.node(node_idx);
    for (unsigned long i = 0; i < node.number_of_children(); ++i) {
        children.push_back(node.child(i).index());
    }
    return children;
}
