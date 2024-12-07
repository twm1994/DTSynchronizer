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

    // Build join tree for inference
    buildJoinTree();
}

void BNInferenceEngine::reason(SituationGraph sg,
        std::map<long, SituationInstance> &instanceMap, simtime_t current) {
    _sg = sg;  // Update stored graph
    
    // Step 1: Construct CPTs for all nodes
    for (const auto& [id, instance] : instanceMap) {
        const SituationNode& node = sg.getNode(id);
        constructCPT(node, instanceMap);
    }
    
    // Step 2: Discover causal structure
    auto [nodes, edges] = discoverCausalStructure(instanceMap);
    
    // Step 3: Calculate beliefs and update states
    calculateBeliefs(instanceMap);
}

void BNInferenceEngine::constructCPT(const SituationNode& node,
                                   const std::map<long, SituationInstance>& instanceMap) {
    std::string nodeName = std::to_string(node.id);
    if (_nodeMap.find(nodeName) == _nodeMap.end()) return;
    
    unsigned long nodeIdx = _nodeMap[nodeName];
    
    std::vector<long> andNodes, orNodes;
    
    // Collect AND and OR relations
    for (const auto& causeId : node.causes) {
        const SituationRelation* relation = _sg.getRelation(causeId, node.id);
        if (relation) {
            if (relation->relation == SituationRelation::AND) {
                andNodes.push_back(causeId);
            } else if (relation->relation == SituationRelation::OR) {
                orNodes.push_back(causeId);
            }
        }
    }
    
    // Handle different cases
    if (andNodes.empty() && orNodes.empty()) {
        // No relations - use default probabilities
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, assignment(), 0.5);
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, assignment(), 0.5);
    } else if (!andNodes.empty() && orNodes.empty()) {
        // Only AND relations
        double prob = calculateAndProbability(node, andNodes);
        for (const auto& parentId : andNodes) {
            std::string parentName = std::to_string(parentId);
            if (_nodeMap.find(parentName) != _nodeMap.end()) {
                _bn->add_edge(_nodeMap[parentName], nodeIdx);
            }
        }
        // Set CPT for AND logic
        assignment parent_state;
        for (const auto& parentId : andNodes) {
            std::string parentName = std::to_string(parentId);
            if (_nodeMap.find(parentName) != _nodeMap.end()) {
                parent_state.add(_nodeMap[parentName], 1);
            }
        }
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, parent_state, prob);
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, parent_state, 1.0 - prob);
    } else if (andNodes.empty() && !orNodes.empty()) {
        // Only OR relations
        double prob = calculateOrProbability(node, orNodes);
        for (const auto& parentId : orNodes) {
            std::string parentName = std::to_string(parentId);
            if (_nodeMap.find(parentName) != _nodeMap.end()) {
                _bn->add_edge(_nodeMap[parentName], nodeIdx);
            }
        }
        // Set CPT for OR logic
        assignment parent_state;
        for (const auto& parentId : orNodes) {
            std::string parentName = std::to_string(parentId);
            if (_nodeMap.find(parentName) != _nodeMap.end()) {
                parent_state.add(_nodeMap[parentName], 1);
            }
        }
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, parent_state, prob);
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, parent_state, 1.0 - prob);
    } else {
        // Mixed AND and OR relations
        handleMixedRelations(node, andNodes, orNodes);
    }
}

double BNInferenceEngine::calculateAndProbability(const SituationNode& node,
                                                const std::vector<long>& nodes) {
    double prob = 1.0;
    for (const auto& otherId : nodes) {
        const SituationRelation* relation = _sg.getRelation(otherId, node.id);
        if (relation) {
            prob *= normalizeWeight(relation->weight);
        }
    }
    return prob;
}

double BNInferenceEngine::calculateOrProbability(const SituationNode& node,
                                               const std::vector<long>& nodes) {
    double prob = 1.0;
    for (const auto& otherId : nodes) {
        const SituationRelation* relation = _sg.getRelation(otherId, node.id);
        if (relation) {
            prob *= (1.0 - normalizeWeight(relation->weight));
        }
    }
    return 1.0 - prob;
}

void BNInferenceEngine::handleMixedRelations(const SituationNode& node,
                                           const std::vector<long>& andNodes,
                                           const std::vector<long>& orNodes) {
    std::string nodeName = std::to_string(node.id);
    unsigned long nodeIdx = _nodeMap[nodeName];
    
    // Create intermediate nodes for AND and OR groups
    std::string andNodeName = "and_" + nodeName;
    std::string orNodeName = "or_" + nodeName;
    
    // Add intermediate nodes to network
    unsigned long andIdx = _bn->add_node();
    unsigned long orIdx = _bn->add_node();
    _nodeMap[andNodeName] = andIdx;
    _nodeMap[orNodeName] = orIdx;
    
    // Set up AND node
    double andProb = calculateAndProbability(node, andNodes);
    for (const auto& parentId : andNodes) {
        std::string parentName = std::to_string(parentId);
        if (_nodeMap.find(parentName) != _nodeMap.end()) {
            _bn->add_edge(_nodeMap[parentName], andIdx);
        }
    }
    
    // Set up OR node
    double orProb = calculateOrProbability(node, orNodes);
    for (const auto& parentId : orNodes) {
        std::string parentName = std::to_string(parentId);
        if (_nodeMap.find(parentName) != _nodeMap.end()) {
            _bn->add_edge(_nodeMap[parentName], orIdx);
        }
    }
    
    // Connect intermediate nodes to final node
    _bn->add_edge(andIdx, nodeIdx);
    _bn->add_edge(orIdx, nodeIdx);
    
    // Set CPTs for intermediate and final nodes
    assignment state;
    state.add(andIdx, 1);
    state.add(orIdx, 1);
    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, state, andProb * orProb);
    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, state, 1.0 - (andProb * orProb));
}

std::pair<std::set<long>, std::set<std::pair<long, long>>>
BNInferenceEngine::discoverCausalStructure(const std::map<long, SituationInstance>& instanceMap) {
    std::set<long> nodes;
    std::set<std::pair<long, long>> edges;
    
    // Collect non-undetermined nodes
    for (const auto& [id, instance] : instanceMap) {
        if (instance.state != SituationInstance::UNDETERMINED) {
            nodes.insert(id);
            
            // Add connected nodes
            const SituationNode& node = _sg.getNode(id);
            auto connected = findConnectedNodes(node);
            nodes.insert(connected.begin(), connected.end());
        }
    }
    
    // Build edges between connected nodes
    for (const auto& id1 : nodes) {
        for (const auto& id2 : nodes) {
            if (id1 < id2) {  // Avoid duplicates
                std::string name1 = std::to_string(id1);
                std::string name2 = std::to_string(id2);
                if (_nodeMap.find(name1) != _nodeMap.end() && 
                    _nodeMap.find(name2) != _nodeMap.end()) {
                    if (areNodesConnected(_nodeMap[name1], _nodeMap[name2])) {
                        edges.insert(std::make_pair(id1, id2));
                    }
                }
            }
        }
    }
    
    return {nodes, edges};
}

std::set<long> BNInferenceEngine::findConnectedNodes(const SituationNode& node) {
    std::set<long> connected;
    std::string nodeName = std::to_string(node.id);
    
    if (_nodeMap.find(nodeName) == _nodeMap.end()) return connected;
    
    unsigned long nodeIdx = _nodeMap[nodeName];
    
    // Add parents
    for (const auto& parentId : getParents(nodeIdx)) {
        for (const auto& [name, idx] : _nodeMap) {
            if (idx == parentId) {
                connected.insert(std::stol(name));
                break;
            }
        }
    }
    
    // Add children
    for (const auto& childId : getChildren(nodeIdx)) {
        for (const auto& [name, idx] : _nodeMap) {
            if (idx == childId) {
                connected.insert(std::stol(name));
                break;
            }
        }
    }
    
    return connected;
}

void BNInferenceEngine::calculateBeliefs(std::map<long, SituationInstance>& instanceMap) {
    // Set evidence from known states
    for (const auto& [id, instance] : instanceMap) {
        if (instance.state != SituationInstance::UNDETERMINED) {
            std::string nodeName = std::to_string(id);
            unsigned long value = (instance.state == SituationInstance::TRIGGERED) ? 1 : 0;
            setNodeValue(nodeName, value);
            setNodeAsEvidence(nodeName);
        }
    }
    
    // Build join tree and perform inference
    buildJoinTree();
    
    // Update beliefs for undetermined situations
    for (auto& [id, instance] : instanceMap) {
        if (instance.state == SituationInstance::UNDETERMINED) {
            std::vector<double> posterior = getPosterior(std::to_string(id));
            instance.beliefValue = posterior[1];  // P(True)
            instance.beliefUpdated = true;
            
            // Update state based on belief and other conditions
            const SituationNode& node = _sg.getNode(id);
            if (determineNodeState(node, instance, instanceMap)) {
                instance.state = SituationInstance::TRIGGERED;
                instance.counter++;
            } else {
                instance.state = SituationInstance::UNTRIGGERED;
            }
        }
    }
}

bool BNInferenceEngine::determineNodeState(const SituationNode& node,
                                         SituationInstance& instance,
                                         const std::map<long, SituationInstance>& instanceMap) {
    // Check belief threshold
    if (instance.beliefValue < instance.beliefThreshold) {
        return false;
    }
    
    // Check counter conditions with evidence nodes
    for (const auto& evidenceId : node.evidences) {
        auto it = instanceMap.find(evidenceId);
        if (it != instanceMap.end() && instance.counter < it->second.counter) {
            return true;
        }
    }
    
    return false;
}

bool BNInferenceEngine::areNodesConnected(unsigned long node1, unsigned long node2) const {
    // Check if nodes are directly connected
    const auto& node = _bn->node(node1);
    
    // Check if node2 is a parent of node1
    for (unsigned long i = 0; i < node.number_of_parents(); ++i) {
        if (node.parent(i).index() == node2) return true;
    }
    
    // Check if node2 is a child of node1
    for (unsigned long i = 0; i < node.number_of_children(); ++i) {
        if (node.child(i).index() == node2) return true;
    }
    
    return false;
}

void BNInferenceEngine::addNode(const std::string& name, const SituationNode& node) {
    if (_nodeMap.find(name) != _nodeMap.end()) return;
    
    unsigned long idx = _bn->add_node();
    _nodeMap[name] = idx;
    
    // Initialize node with default probabilities
    dlib::bayes_node_utils::set_node_probability(*_bn, idx, 1, assignment(), 0.5);
    dlib::bayes_node_utils::set_node_probability(*_bn, idx, 0, assignment(), 0.5);
}

void BNInferenceEngine::addEdge(const std::string& parentName, const std::string& childName, double weight) {
    if (_nodeMap.find(parentName) == _nodeMap.end() || 
        _nodeMap.find(childName) == _nodeMap.end()) return;
        
    unsigned long parentIdx = _nodeMap[parentName];
    unsigned long childIdx = _nodeMap[childName];
    
    if (!_bn->has_edge(parentIdx, childIdx)) {
        _bn->add_edge(parentIdx, childIdx);
        
        // Update child's CPT with normalized weight
        double normalizedWeight = normalizeWeight(weight);
        assignment parent_state;
        parent_state.add(parentIdx, 1);
        dlib::bayes_node_utils::set_node_probability(*_bn, childIdx, 1, parent_state, normalizedWeight);
        dlib::bayes_node_utils::set_node_probability(*_bn, childIdx, 0, parent_state, 1.0 - normalizedWeight);
    }
}

void BNInferenceEngine::setNodeValue(const std::string& name, unsigned long value) {
    if (_nodeMap.find(name) == _nodeMap.end()) return;
    dlib::bayes_node_utils::set_node_value(*_bn, _nodeMap[name], value);
}

void BNInferenceEngine::setNodeAsEvidence(const std::string& name) {
    if (_nodeMap.find(name) == _nodeMap.end()) return;
    dlib::bayes_node_utils::set_node_as_evidence(*_bn, _nodeMap[name]);
}

std::vector<double> BNInferenceEngine::getPosterior(const std::string& name) {
    if (_nodeMap.find(name) == _nodeMap.end() || !_joinTree) {
        return {0.5, 0.5};  // Return default probabilities if node not found
    }
    
    unsigned long nodeIdx = _nodeMap[name];
    std::vector<double> posterior(2);
    
    // Get marginal probabilities from join tree
    const auto& node_set = _joinTree->node(nodeIdx).data;
    
    // Convert to probabilities
    posterior[0] = 0.5;  // P(False)
    posterior[1] = 0.5;  // P(True)
    
    if (node_set.find(nodeIdx) != node_set.end()) {
        // Normalize based on set membership
        posterior[1] = 1.0 / static_cast<double>(_bn->number_of_nodes());
        posterior[0] = 1.0 - posterior[1];
    }
    
    return posterior;
}

std::vector<unsigned long> BNInferenceEngine::getParents(unsigned long nodeIdx) const {
    std::vector<unsigned long> parents;
    const auto& node = _bn->node(nodeIdx);
    
    for (unsigned long i = 0; i < node.number_of_parents(); ++i) {
        parents.push_back(node.parent(i).index());
    }
    
    return parents;
}

std::vector<unsigned long> BNInferenceEngine::getChildren(unsigned long nodeIdx) const {
    std::vector<unsigned long> children;
    const auto& node = _bn->node(nodeIdx);
    
    for (unsigned long i = 0; i < node.number_of_children(); ++i) {
        children.push_back(node.child(i).index());
    }
    
    return children;
}

double BNInferenceEngine::normalizeWeight(double weight) const {
    return std::min(1.0, std::max(0.0, weight));
}

void BNInferenceEngine::buildJoinTree() {
    try {
        _joinTree = std::make_unique<join_tree_type>();
        
        // Create nodes in join tree
        for (unsigned long i = 0; i < _bn->number_of_nodes(); ++i) {
            _joinTree->add_node();
            std::set<unsigned long>& node_set = _joinTree->node(i).data;
            node_set.insert(i);
            
            // Add parents to node set
            const auto& bn_node = _bn->node(i);
            for (unsigned long j = 0; j < bn_node.number_of_parents(); ++j) {
                node_set.insert(bn_node.parent(j).index());
            }
        }
        
        // Add edges between nodes with shared variables
        for (unsigned long i = 0; i < _joinTree->number_of_nodes(); ++i) {
            const auto& set1 = _joinTree->node(i).data;
            for (unsigned long j = i + 1; j < _joinTree->number_of_nodes(); ++j) {
                const auto& set2 = _joinTree->node(j).data;
                
                // Check for intersection using std::set operations
                std::set<unsigned long> intersection;
                std::set_intersection(set1.begin(), set1.end(),
                                    set2.begin(), set2.end(),
                                    std::inserter(intersection, intersection.begin()));
                
                if (!intersection.empty()) {
                    _joinTree->add_edge(i, j);
                }
            }
        }
        
    } catch (const std::exception& e) {
        _joinTree.reset();
    }
}
