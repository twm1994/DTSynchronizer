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

BNInferenceEngine::BNInferenceEngine() : _joinTree(std::make_unique<join_tree_type>()) {}

BNInferenceEngine::~BNInferenceEngine() = default;

void BNInferenceEngine::loadModel(SituationGraph sg) {
    // Clear existing network
    _bn.clear();
    _nodeMap.clear();
    _evidence.clear();
    _joinTree = std::make_unique<join_tree_type>();
    
    // Step 1: Add nodes for each situation
    for (int layer = 0; layer < sg.modelHeight(); layer++) {
        DirectedGraph currentLayer = sg.getLayer(layer);
        for (const auto& nodeId : currentLayer.getVertices()) {
            const SituationNode& node = sg.getNode(nodeId);
            addNode(std::to_string(node.id));
        }
    }
    
    // Step 2: Add edges based on relations
    for (int layer = 0; layer < sg.modelHeight(); layer++) {
        DirectedGraph currentLayer = sg.getLayer(layer);
        for (const auto& nodeId : currentLayer.getVertices()) {
            const SituationNode& node = sg.getNode(nodeId);
            // Add edges for both vertical and horizontal relations
            for (const auto& [childId, relation] : sg.getOutgoingRelations(node.id)) {
                addEdge(std::to_string(node.id), std::to_string(childId));
            }
        }
    }

    // Build join tree for inference
    buildJoinTree();
}

void BNInferenceEngine::reason(SituationGraph sg,
        std::map<long, SituationInstance> &instanceMap, simtime_t current) {
    
    // Clear previous evidence
    _evidence.clear();
    
    // Step 1: Set evidence from known states
    for (const auto& [id, instance] : instanceMap) {
        if (instance.state != SituationInstance::UNDETERMINED) {
            setEvidence(std::to_string(id), 
                       instance.state == SituationInstance::TRIGGERED ? 1 : 0);
        }
    }
    
    // Step 2: Perform inference
    performInference();
    
    // Step 3: Update beliefs based on inference results
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
    
    // Step 4: Prune weak edges to refine the model
    pruneWeakEdges(0.1);
}

void BNInferenceEngine::buildJoinTree() {
    try {
        // Create a new join tree
        _joinTree = std::make_unique<join_tree_type>();
        
        // Create a join tree from the Bayesian network using dlib's create_join_tree
        dlib::create_join_tree(_bn, *_joinTree);
        
        // Initialize the joint probability tables
        dlib::bayesian_network_join_tree(_bn, *_joinTree);
    } catch (const std::exception& e) {
        throw std::runtime_error("Error building join tree: " + std::string(e.what()));
    }
}

void BNInferenceEngine::addNode(const std::string& name) {
    unsigned long idx = _bn.add_node();
    _nodeMap[name] = idx;
    
    // Initialize node with binary states
    auto& node = _bn.node(idx);
    node.data.table().set_size(2, 1);  // Binary node (True/False)
    node.data.table()(0,0) = 0.5;      // Initial probability for False
    node.data.table()(1,0) = 0.5;      // Initial probability for True
}

void BNInferenceEngine::addEdge(const std::string& parent, const std::string& child) {
    if (_nodeMap.find(parent) == _nodeMap.end() || _nodeMap.find(child) == _nodeMap.end()) {
        throw std::runtime_error("Node not found when adding edge");
    }
    
    unsigned long parent_idx = _nodeMap[parent];
    unsigned long child_idx = _nodeMap[child];
    
    _bn.add_edge(parent_idx, child_idx);
    
    // Update child's CPT size based on new parent
    auto parents = getParents(child_idx);
    size_t parent_count = parents.size();
    auto& node = _bn.node(child_idx);
    node.data.table().set_size(2, std::pow(2, parent_count));  // 2 states, 2^parents columns
}

void BNInferenceEngine::setEvidence(const std::string& nodeName, size_t value) {
    if (_nodeMap.find(nodeName) == _nodeMap.end()) {
        throw std::runtime_error("Node " + nodeName + " not found in network");
    }
    if (value > 1) { // Binary nodes only accept 0 or 1
        throw std::runtime_error("Invalid evidence value. Must be 0 or 1");
    }
    _evidence[nodeName] = value;
}

void BNInferenceEngine::performInference() {
    try {
        if (!_joinTree) {
            buildJoinTree();
        }

        // Set evidence in the network
        for (const auto& [nodeName, value] : _evidence) {
            if (_nodeMap.find(nodeName) == _nodeMap.end()) {
                throw std::runtime_error("Node " + nodeName + " not found in network");
            }
            unsigned long node_idx = _nodeMap[nodeName];
            auto& node = _bn.node(node_idx);
            
            // Clear current probability table
            node.data.table().set_size(2, 1);
            // Set evidence (0 for false state, 1 for true state)
            node.data.table()(value, 0) = 1.0;
            node.data.table()(!value, 0) = 0.0;
        }

        // Perform belief propagation using dlib's join tree algorithm
        dlib::bayesian_network_join_tree(_bn, *_joinTree);
    } catch (const std::exception& e) {
        throw std::runtime_error("Error performing inference: " + std::string(e.what()));
    }
}

std::vector<double> BNInferenceEngine::getPosterior(const std::string& nodeName) {
    if (_nodeMap.find(nodeName) == _nodeMap.end()) {
        throw std::runtime_error("Node " + nodeName + " not found in network");
    }
    
    // Get node's probability table
    unsigned long node_idx = _nodeMap[nodeName];
    const auto& node = _bn.node(node_idx);
    const auto& table = node.data.table();
    
    // Convert to vector format
    std::vector<double> posterior;
    posterior.reserve(2);  // Binary nodes
    posterior.push_back(table(0,0));  // P(False)
    posterior.push_back(table(1,0));  // P(True)
    
    return posterior;
}

std::vector<unsigned long> BNInferenceEngine::getParents(unsigned long node_idx) const {
    std::vector<unsigned long> parents;
    for (auto i = 0; i < _bn.node(node_idx).number_of_parents(); ++i) {
        parents.push_back(_bn.node(node_idx).parent(i).index());
    }
    return parents;
}

std::vector<unsigned long> BNInferenceEngine::getChildren(unsigned long node_idx) const {
    std::vector<unsigned long> children;
    for (auto i = 0; i < _bn.node(node_idx).number_of_children(); ++i) {
        children.push_back(_bn.node(node_idx).child(i).index());
    }
    return children;
}

double BNInferenceEngine::calculateMutualInformation(unsigned long node1, unsigned long node2,
                                                   const std::map<long, SituationInstance>& instanceMap) {
    // Implement mutual information calculation for structure learning
    // This is a placeholder - actual implementation would use empirical probabilities from instanceMap
    return 0.0;
}

void BNInferenceEngine::pruneWeakEdges(double threshold) {
    // Implement edge pruning based on mutual information or correlation
    // This is a placeholder - actual implementation would remove edges with weak dependencies
}
