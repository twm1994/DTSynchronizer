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
#include <algorithm>
#include <dlib/bayes_utils.h>

using namespace dlib;
using namespace dlib::bayes_node_utils;

BNInferenceEngine::BNInferenceEngine() : _bn(std::make_unique<bn_type>()), _joinTree(nullptr) {}

BNInferenceEngine::~BNInferenceEngine() = default;

void BNInferenceEngine::loadModel(SituationGraph sg) {
    _sg = sg;  // Store the graph
    
    // Create new network
    _bn = std::make_unique<bn_type>();  // Reset network
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
}

void BNInferenceEngine::reason(SituationGraph sg, std::map<long, SituationInstance> &instanceMap, simtime_t current) {
    _sg = sg;  // Update stored graph
    
    // Step 1: Construct CPTs for all nodes
    for (const auto& [id, instance] : instanceMap) {
        const SituationNode& node = sg.getNode(id);
        constructCPT(node, instanceMap);
    }
    
    // Step 2: Discover causal structure
    auto [nodes, edges] = findCausallyConnectedNodes(instanceMap);
    
    // Step 3: Calculate beliefs and update states
    calculateBeliefs(instanceMap, current);
}

void BNInferenceEngine::addNode(const std::string& nodeName, const SituationNode& node) {
    // Add node to Bayesian network
    unsigned long nodeIndex = _bn->add_node();
    _nodeMap[nodeName] = nodeIndex;
    
    // Set node name
    // dlib::bayes_node_utils::set_node_name(*_bn, nodeIndex, nodeName);
}

void BNInferenceEngine::addEdge(const std::string& parentName, const std::string& childName, double weight) {
    if (_nodeMap.find(parentName) != _nodeMap.end() && _nodeMap.find(childName) != _nodeMap.end()) {
        unsigned long parentIndex = _nodeMap[parentName];
        unsigned long childIndex = _nodeMap[childName];
        
        // Add edge to Bayesian network
        _bn->add_edge(parentIndex, childIndex);
        
        // Set edge weight (if applicable)
        // Note: dlib does not directly support edge weights, so this is a placeholder
        // for potential custom logic
        // dlib::bayes_node_utils::set_edge_weight(*_bn, parentIndex, childIndex, weight);
    }
}

void BNInferenceEngine::constructCPT(const SituationNode& node, const std::map<long, SituationInstance>& instanceMap) {
    std::vector<long> andNodes;
    std::vector<long> orNodes;
    
    // Step 1: Classify nodes into AND and OR relations
    // Check causes
    for (const auto& cause : node.causes) {
        auto it = instanceMap.find(cause);
        if (it != instanceMap.end()) {
            // Add to appropriate vector based on relation type
            // Note: You'll need to specify how to determine if a node is AND or OR connected
            // For now, assuming all causes are AND-connected
            andNodes.push_back(cause);
        }
    }
    
    // Check evidences
    for (const auto& evidence : node.evidences) {
        auto it = instanceMap.find(evidence);
        if (it != instanceMap.end()) {
            // Note: You'll need to specify how to determine if a node is AND or OR connected
            // For now, assuming all evidences are OR-connected
            orNodes.push_back(evidence);
        }
    }
    
    // Get node index in Bayesian network
    const std::string nodeName = std::to_string(node.id);
    if (_nodeMap.find(nodeName) == _nodeMap.end()) return;
    unsigned long nodeIdx = _nodeMap[nodeName];
    
    // Case 1: No predecessors or children
    if (andNodes.empty() && orNodes.empty()) {
        // P(A) = 0 | 1
        assignment empty_assignment;
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, empty_assignment, 0.0);  // Set to false by default
        return;
    }
    
    // Case 2: Single predecessor/child with SOLE relation
    if (andNodes.size() + orNodes.size() == 1) {
        long connectedId = andNodes.empty() ? orNodes[0] : andNodes[0];
        std::string connectedName = std::to_string(connectedId);
        
        if (_nodeMap.find(connectedName) == _nodeMap.end()) return;
        unsigned long connectedIdx = _nodeMap[connectedName];
        
        // Set conditional probability table (CPT)
        assignment single_assignment;
        single_assignment.add(connectedIdx, 0);
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, single_assignment, 0.0);  // P(A|B=0)
        single_assignment.add(connectedIdx, 1);
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, single_assignment, 1.0);  // P(A|B=1)
        return;
    }
    
    // Handle mixed relations
    handleMixedRelations(node, andNodes, orNodes);
}

void BNInferenceEngine::handleMixedRelations(const SituationNode& node, const std::vector<long>& andNodes, const std::vector<long>& orNodes) {
    // Implementation detail: Handle mixed AND/OR relations
    // Placeholder for actual logic
    return;  // No operation for demonstration
}

void BNInferenceEngine::calculateBeliefs(std::map<long, SituationInstance>& instanceMap, simtime_t current) {
    // Convert instanceMap to Bayesian Network
    loadModel(_sg);

    // Construct moral graph and join tree for inference using graph_kernel_1
    using join_tree_type = dlib::graph_kernel_1<dlib::set<unsigned long>::kernel_1a_c, dlib::set<unsigned long>::kernel_1a_c>;
    join_tree_type join_tree;
    dlib::graph_kernel_1<dlib::set<unsigned long>::kernel_1a_c, dlib::set<unsigned long>::kernel_1a_c> moral_graph;
    dlib::create_moral_graph(*_bn, moral_graph);
    dlib::create_join_tree(moral_graph, join_tree);

    // Ensure the join tree is valid
    if (!dlib::is_join_tree(moral_graph, join_tree)) {
        throw std::runtime_error("Invalid join tree for the Bayesian network");
    }

    // Create bayesian network join tree for inference
    dlib::bayesian_network_join_tree bn_join_tree(*_bn, join_tree);

    // Set evidence in the Bayesian Network
    for (const auto& [id, instance] : instanceMap) {
        const std::string nodeName = std::to_string(id);
        if (_nodeMap.find(nodeName) != _nodeMap.end()) {
            unsigned long nodeIdx = _nodeMap[nodeName];
            if (instance.state == SituationInstance::TRIGGERED || instance.state == SituationInstance::UNTRIGGERED) {
                dlib::bayes_node_utils::set_node_value(*_bn, nodeIdx, instance.state == SituationInstance::TRIGGERED ? 1 : 0);
                dlib::bayes_node_utils::set_node_as_evidence(*_bn, nodeIdx);
            }
        }
    }

    // Perform inference using join tree
    for (auto& [id, instance] : instanceMap) {
        const std::string nodeName = std::to_string(id);
        if (_nodeMap.find(nodeName) != _nodeMap.end()) {
            unsigned long nodeIdx = _nodeMap[nodeName];
            if (instance.state == SituationInstance::UNDETERMINED) {
                auto belief = bn_join_tree.probability(nodeIdx);
                double belief_value = belief(1);  // Assuming binary node, get probability of being true
                if (belief_value >= _sg.getNode(id).threshold) {
                    bool shouldTrigger = false;
                    for (const auto& evidenceId : _sg.getNode(id).evidences) {
                        if (instanceMap[evidenceId].counter < instance.counter) {
                            shouldTrigger = true;
                            break;
                        }
                    }
                    if (shouldTrigger) {
                        instance.state = SituationInstance::TRIGGERED;
                        instance.counter++;
                        instance.next_start = current;
                    } else {
                        instance.state = SituationInstance::UNTRIGGERED;
                    }
                } else {
                    instance.state = SituationInstance::UNTRIGGERED;
                }
            }
        }
    }
}

std::pair<std::unique_ptr<dlib::set<long>::kernel_1a>, std::unique_ptr<dlib::set<std::pair<long, long>>::kernel_1a>> 
BNInferenceEngine::findCausallyConnectedNodes(const std::map<long, SituationInstance>& instanceMap) {
    auto nodes = std::make_unique<dlib::set<long>::kernel_1a>();
    auto edges = std::make_unique<dlib::set<std::pair<long, long>>::kernel_1a>();

    for (const auto& [id, instance] : instanceMap) {
        const SituationNode& node = _sg.getNode(id);
        auto connectedNodes = findConnectedNodes(node);
        
        // Copy nodes using temporary non-const values
        connectedNodes->reset();
        while (connectedNodes->move_next()) {
            long nodeId = connectedNodes->element();
            nodes->add(nodeId);
        }

        // Create edges
        for (const auto& cause : node.causes) {
            std::pair<long, long> edge = std::make_pair(cause, id);
            edges->add(edge);
        }
    }

    return {std::move(nodes), std::move(edges)};
}

std::unique_ptr<dlib::set<long>::kernel_1a> BNInferenceEngine::findConnectedNodes(const SituationNode& node) {
    auto result = std::make_unique<dlib::set<long>::kernel_1a>();
    
    // Add the node's own ID
    long nodeId = node.id;
    result->add(nodeId);
    
    // Add causes
    for (const auto& cause : node.causes) {
        long causeId = cause;
        result->add(causeId);
    }
    
    // Add evidences
    for (const auto& evidence : node.evidences) {
        long evidenceId = evidence;
        result->add(evidenceId);
    }
    
    return result;
}
