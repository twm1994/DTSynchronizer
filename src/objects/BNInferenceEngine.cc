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
#include "SituationReasoner.h"

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

void BNInferenceEngine::reason(SituationGraph sg, std::map<long, SituationInstance> &instanceMap, simtime_t current, std::shared_ptr<ReasonerLogger> logger) {
    _sg = sg;  // Update stored graph
    
    // Step 1: Construct CPTs for all nodes
    std::map<long, std::map<assignment, std::pair<double, double>>> cptMap;
    for (const auto& [id, instance] : instanceMap) {
        const SituationNode& node = sg.getNode(id);
        constructCPT(node, instanceMap);
        
        // Store CPT for this node
        const std::string nodeName = std::to_string(id);
        if (_nodeMap.find(nodeName) != _nodeMap.end()) {
            unsigned long nodeIdx = _nodeMap[nodeName];
            std::map<assignment, std::pair<double, double>> nodeCPT;
            
            // Store all probability assignments for this node
            assignment a = dlib::bayes_node_utils::node_first_parent_assignment(*_bn, nodeIdx);
            do {
                double p0 = dlib::bayes_node_utils::node_probability(*_bn, nodeIdx, 0, a);
                double p1 = dlib::bayes_node_utils::node_probability(*_bn, nodeIdx, 1, a);
                nodeCPT[a] = std::make_pair(p0, p1);
            } while(dlib::bayes_node_utils::node_next_parent_assignment(*_bn, nodeIdx, a));
            
            cptMap[id] = nodeCPT;
        }
    }
    
    // Step 2: Discover causal structure and create subgraph
    auto [nodeSet, edgeSet] = findCausallyConnectedNodes(instanceMap);
    
    // Create new graph with only causally connected nodes
    SituationGraph causalGraph;
    std::map<long, SituationInstance> causalInstanceMap;
    
    // Copy nodes to causal graph
    nodeSet->reset();
    while (nodeSet->move_next()) {
        long nodeId = nodeSet->element();
        const SituationNode& origNode = sg.getNode(nodeId);
        causalGraph.situationMap[nodeId] = origNode;
        causalInstanceMap[nodeId] = instanceMap[nodeId];
    }
    
    // Build layers for causal graph (assuming same layer structure as original)
    causalGraph.layers.clear();
    for (int i = 0; i < sg.modelHeight(); i++) {
        DirectedGraph origLayer = sg.getLayer(i);
        DirectedGraph layer;  // Declare layer here to be used in the entire function
        // Add vertices that exist in the causal subgraph
        for (const auto& nodeId : origLayer.getVertices()) {
            if (nodeSet->is_member(nodeId)) {
                layer.add_vertex(nodeId);
            }
        }
        
        // Add edges that exist in the causal subgraph
        edgeSet->reset();
        while (edgeSet->move_next()) {
            auto edge = edgeSet->element();
            const auto& vertices = origLayer.getVertices();
            if (std::find(vertices.begin(), vertices.end(), edge.first) != vertices.end() &&
                std::find(vertices.begin(), vertices.end(), edge.second) != vertices.end()) {
                layer.add_vertex(edge.first);
                layer.add_vertex(edge.second);
                layer.add_edge(edge.first, edge.second);
            }
        }
        
        causalGraph.layers.push_back(layer);
    }
    
    // Create new Bayesian network for causal subgraph
    _bn = std::make_unique<bn_type>();
    _nodeMap.clear();
    _joinTree.reset();
    
    // Add nodes and restore their CPTs
    for (const auto& [id, instance] : causalInstanceMap) {
        addNode(std::to_string(id), causalGraph.getNode(id));
        
        // Restore CPT from stored map
        const std::string nodeName = std::to_string(id);
        if (_nodeMap.find(nodeName) != _nodeMap.end() && cptMap.find(id) != cptMap.end()) {
            unsigned long nodeIdx = _nodeMap[nodeName];
            for (const auto& [a, probs] : cptMap[id]) {
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, probs.first);
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, probs.second);
            }
        }
    }
    
    // Add edges for causal subgraph
    edgeSet->reset();
    while (edgeSet->move_next()) {
        auto edge = edgeSet->element();
        const SituationRelation* relation = causalGraph.getRelation(edge.first, edge.second);
        if (relation) {
            addEdge(std::to_string(edge.first), std::to_string(edge.second), relation->weight);
        }
    }
    
    // Step 3: Calculate beliefs and update states for causal subgraph
    if (logger) {
        logger->logStep("BN Belief Calculation Start", 
                       current, 
                       -1, 
                       0.0, 
                       {}, 
                       {}, 
                       SituationInstance::UNDETERMINED);
    }

    // Create join tree for inference
    _joinTree = std::make_unique<join_tree_type>();
    
    // Create moral graph first, then create join tree
    create_moral_graph(*_bn, *_joinTree);
    create_join_tree(*_joinTree, *_joinTree);
    
    if (logger) {
        logger->logStep("BN Join Tree Created", 
                       current, 
                       -1, 
                       0.0, 
                       {}, 
                       {}, 
                       SituationInstance::UNDETERMINED);
    }
    
    // Set evidence in Bayesian network based on instance states
    for (const auto& [id, instance] : causalInstanceMap) {
        const std::string nodeName = std::to_string(id);
        if (_nodeMap.find(nodeName) == _nodeMap.end()) continue;
        
        unsigned long nodeIdx = _nodeMap[nodeName];
        if (instance.state == SituationInstance::TRIGGERED) {
            set_node_value(*_bn, nodeIdx, 1);  // Set to true for triggered
            if (logger) {
                logger->logStep("BN Set Evidence", 
                              current, 
                              id, 
                              1.0,  // Evidence value for triggered 
                              {}, 
                              {}, 
                              instance.state);
            }
        } else if (instance.state == SituationInstance::UNTRIGGERED) {
            set_node_value(*_bn, nodeIdx, 0);  // Set to false for untriggered
            if (logger) {
                logger->logStep("BN Set Evidence", 
                              current, 
                              id, 
                              0.0,  // Evidence value for untriggered
                              {}, 
                              {}, 
                              instance.state);
            }
        }
    }
    
    // Perform belief propagation using join tree algorithm
    bayesian_network_join_tree solution(*_bn, *_joinTree);
    
    // Update beliefs and states for each node
    calculateBeliefs(causalInstanceMap, current);
    
    // Step 4: Update original instanceMap with new beliefs and states
    for (const auto& [id, instance] : causalInstanceMap) {
        instanceMap[id].beliefValue = instance.beliefValue;
        instanceMap[id].counter = instance.counter;
        instanceMap[id].state = instance.state;
        instanceMap[id].next_start = instance.next_start;
    }
}

void BNInferenceEngine::addNode(const std::string& nodeName, const SituationNode& node) {
    // Add node to Bayesian network
    unsigned long nodeIndex = _bn->add_node();
    _nodeMap[nodeName] = nodeIndex;
    
    // Initialize node with binary values (0 = false, 1 = true)
    dlib::bayes_node_utils::set_node_num_values(*_bn, nodeIndex, 2);
    
    // Set initial probabilities for root nodes
    assignment empty_assignment;
    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIndex, 0, empty_assignment, 0.5);  // P(node=false)
    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIndex, 1, empty_assignment, 0.5);  // P(node=true)
}

void BNInferenceEngine::addEdge(const std::string& parentName, const std::string& childName, double weight) {
    if (_nodeMap.find(parentName) != _nodeMap.end() && _nodeMap.find(childName) != _nodeMap.end()) {
        unsigned long parentIndex = _nodeMap[parentName];
        unsigned long childIndex = _nodeMap[childName];
        
        // Add edge to Bayesian network
        _bn->add_edge(parentIndex, childIndex);
        
        // Initialize CPT for the child node
        assignment parent_assignment;
        parent_assignment.add(parentIndex, 0);  // parent = false
        dlib::bayes_node_utils::set_node_probability(*_bn, childIndex, 0, parent_assignment, 1.0 - weight);
        dlib::bayes_node_utils::set_node_probability(*_bn, childIndex, 1, parent_assignment, weight);
        
        parent_assignment[parentIndex] = 1;  // parent = true
        dlib::bayes_node_utils::set_node_probability(*_bn, childIndex, 0, parent_assignment, 1.0 - weight);
        dlib::bayes_node_utils::set_node_probability(*_bn, childIndex, 1, parent_assignment, weight);
    }
}

void BNInferenceEngine::constructCPT(const SituationNode& node, const std::map<long, SituationInstance>& instanceMap) {
    std::vector<long> andNodes;
    std::vector<long> orNodes;
    
    // Step 1: Classify nodes into AND and OR relations
    auto classifyNode = [&](long nodeId, const SituationRelation* relation) {
        if (relation) {
            if (relation->relation == SituationRelation::AND) {
                andNodes.push_back(nodeId);
            } else if (relation->relation == SituationRelation::OR) {
                orNodes.push_back(nodeId);
            }
            // SOLE relations will be handled in Case 2
        }
    };
    
    // Check causes
    for (const auto& cause : node.causes) {
        const SituationRelation* relation = _sg.getRelation(cause, node.id);
        classifyNode(cause, relation);
    }
    
    // Check evidences
    for (const auto& evidence : node.evidences) {
        const SituationRelation* relation = _sg.getRelation(node.id, evidence);
        classifyNode(evidence, relation);
    }
    
    // Get node index in Bayesian network
    const std::string nodeName = std::to_string(node.id);
    if (_nodeMap.find(nodeName) == _nodeMap.end()) return;
    unsigned long nodeIdx = _nodeMap[nodeName];
    
    // Case 1: No predecessors or children
    if (andNodes.empty() && orNodes.empty()) {
        assignment empty_assignment;
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, empty_assignment, 1.0);  // P(NOT A) = 1
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, empty_assignment, 0.0);  // P(A) = 0
        return;
    }
    
    // Case 2: Single predecessor/child with SOLE relation
    if (andNodes.empty() && orNodes.empty()) {
        for (const auto& cause : node.causes) {
            const SituationRelation* relation = _sg.getRelation(cause, node.id);
            if (relation && relation->relation == SituationRelation::SOLE) {
                std::string connectedName = std::to_string(cause);
                if (_nodeMap.find(connectedName) == _nodeMap.end()) continue;
                unsigned long connectedIdx = _nodeMap[connectedName];
                double w = relation->weight;
                
                assignment a;
                a.add(connectedIdx, 0);  // B = false
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0);    // P(NOT A|NOT B) = 1
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, 0.0);    // P(A|NOT B) = 0
                
                a[connectedIdx] = 1;     // B = true
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0-w);  // P(NOT A|B) = 1-w
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, w);      // P(A|B) = w
                return;
            }
        }
        
        for (const auto& evidence : node.evidences) {
            const SituationRelation* relation = _sg.getRelation(node.id, evidence);
            if (relation && relation->relation == SituationRelation::SOLE) {
                // Similar to cause case but with evidence
                std::string connectedName = std::to_string(evidence);
                if (_nodeMap.find(connectedName) == _nodeMap.end()) continue;
                unsigned long connectedIdx = _nodeMap[connectedName];
                double w = relation->weight;
                
                assignment a;
                a.add(connectedIdx, 0);
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0);
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, 0.0);
                
                a[connectedIdx] = 1;
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0-w);
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, w);
                return;
            }
        }
    }
    
    // Case 3: Only AND relations
    if (!andNodes.empty() && orNodes.empty()) {
        double w = 1.0;
        for (const auto& andNode : andNodes) {
            const SituationRelation* relation = _sg.getRelation(andNode, node.id);
            if (!relation) continue;
            
            auto it = instanceMap.find(andNode);
            if (it != instanceMap.end() && it->second.state == SituationInstance::TRIGGERED) {
                w *= relation->weight;
            }
        }
        
        // Set probabilities for all possible combinations
        assignment a;
        for (const auto& andNode : andNodes) {
            std::string nodeName = std::to_string(andNode);
            if (_nodeMap.find(nodeName) == _nodeMap.end()) continue;
            unsigned long idx = _nodeMap[nodeName];
            a.add(idx, 0);  // Initialize all to false
        }
        
        do {
            bool allTrue = true;
            for (const auto& andNode : andNodes) {
                std::string nodeName = std::to_string(andNode);
                if (_nodeMap.find(nodeName) == _nodeMap.end()) continue;
                unsigned long idx = _nodeMap[nodeName];
                if (a[idx] == 0) {
                    allTrue = false;
                    break;
                }
            }
            
            if (allTrue) {
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0-w);  // P(NOT A|B) = 1-w
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, w);      // P(A|B) = w
            } else {
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0);    // P(NOT A|NOT B) = 1
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, 0.0);    // P(A|NOT B) = 0
            }
        } while(dlib::bayes_node_utils::node_next_parent_assignment(*_bn, nodeIdx, a));
        
        return;
    }
    
    // Case 4: Only OR relations
    if (andNodes.empty() && !orNodes.empty()) {
        double w = 1.0;
        for (const auto& orNode : orNodes) {
            const SituationRelation* relation = _sg.getRelation(orNode, node.id);
            if (!relation) continue;
            
            auto it = instanceMap.find(orNode);
            if (it != instanceMap.end() && it->second.state == SituationInstance::TRIGGERED) {
                w *= (1.0 - relation->weight);
            }
        }
        
        // Set probabilities for all possible combinations
        assignment a;
        for (const auto& orNode : orNodes) {
            std::string nodeName = std::to_string(orNode);
            if (_nodeMap.find(nodeName) == _nodeMap.end()) continue;
            unsigned long idx = _nodeMap[nodeName];
            a.add(idx, 0);  // Initialize all to false
        }
        
        do {
            bool anyTrue = false;
            for (const auto& orNode : orNodes) {
                std::string nodeName = std::to_string(orNode);
                if (_nodeMap.find(nodeName) == _nodeMap.end()) continue;
                unsigned long idx = _nodeMap[nodeName];
                if (a[idx] == 1) {
                    anyTrue = true;
                    break;
                }
            }
            
            if (anyTrue) {
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, w);      // P(NOT A|B) = w
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, 1.0-w);  // P(A|B) = 1-w
            } else {
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0);    // P(NOT A|NOT B) = 1
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, 0.0);    // P(A|NOT B) = 0
            }
        } while(dlib::bayes_node_utils::node_next_parent_assignment(*_bn, nodeIdx, a));
        
        return;
    }
    
    // Case 5: Mixed AND and OR relations
    if (!andNodes.empty() && !orNodes.empty()) {
        // Create intermediary nodes s_u (AND) and s_v (OR)
        unsigned long suIdx = _bn->add_node();
        unsigned long svIdx = _bn->add_node();
        dlib::bayes_node_utils::set_node_num_values(*_bn, suIdx, 2);
        dlib::bayes_node_utils::set_node_num_values(*_bn, svIdx, 2);
        
        // Add edges from intermediary nodes to target node
        _bn->add_edge(suIdx, nodeIdx);
        _bn->add_edge(svIdx, nodeIdx);
        
        // Set CPT for target node A given s_u and s_v
        assignment a;
        a.add(suIdx, 0);
        a.add(svIdx, 0);
        
        // P(A|s_u=0,s_v=0) = 0, P(NOT A|s_u=0,s_v=0) = 1
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0);
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, 0.0);
        
        // P(A|s_u=0,s_v=1) = 0, P(NOT A|s_u=0,s_v=1) = 1
        a[svIdx] = 1;
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0);
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, 0.0);
        
        // P(A|s_u=1,s_v=0) = 0, P(NOT A|s_u=1,s_v=0) = 1
        a[suIdx] = 1;
        a[svIdx] = 0;
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0);
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, 0.0);
        
        // P(A|s_u=1,s_v=1) = 1, P(NOT A|s_u=1,s_v=1) = 0
        a[svIdx] = 1;
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 0.0);
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, 1.0);
        
        // Recursively construct CPTs for s_u and s_v
        // For s_u (AND node)
        double w_u = 1.0;
        for (const auto& andNode : andNodes) {
            const SituationRelation* relation = _sg.getRelation(andNode, node.id);
            if (!relation) continue;
            
            auto it = instanceMap.find(andNode);
            if (it != instanceMap.end() && it->second.state == SituationInstance::TRIGGERED) {
                w_u *= relation->weight;
            }
            
            std::string nodeName = std::to_string(andNode);
            if (_nodeMap.find(nodeName) == _nodeMap.end()) continue;
            unsigned long idx = _nodeMap[nodeName];
            _bn->add_edge(idx, suIdx);
        }
        
        // For s_v (OR node)
        double w_v = 1.0;
        for (const auto& orNode : orNodes) {
            const SituationRelation* relation = _sg.getRelation(orNode, node.id);
            if (!relation) continue;
            
            auto it = instanceMap.find(orNode);
            if (it != instanceMap.end() && it->second.state == SituationInstance::TRIGGERED) {
                w_v *= (1.0 - relation->weight);
            }
            
            std::string nodeName = std::to_string(orNode);
            if (_nodeMap.find(nodeName) == _nodeMap.end()) continue;
            unsigned long idx = _nodeMap[nodeName];
            _bn->add_edge(idx, svIdx);
        }
        
        // Set CPTs for s_u and s_v using Case 3 and Case 4 logic
        // This is a simplified version
        assignment au;
        for (const auto& andNode : andNodes) {
            std::string nodeName = std::to_string(andNode);
            if (_nodeMap.find(nodeName) == _nodeMap.end()) continue;
            unsigned long idx = _nodeMap[nodeName];
            au.add(idx, 0);
        }
        
        do {
            bool allTrue = true;
            for (const auto& andNode : andNodes) {
                std::string nodeName = std::to_string(andNode);
                if (_nodeMap.find(nodeName) == _nodeMap.end()) continue;
                unsigned long idx = _nodeMap[nodeName];
                if (au[idx] == 0) {
                    allTrue = false;
                    break;
                }
            }
            
            if (allTrue) {
                dlib::bayes_node_utils::set_node_probability(*_bn, suIdx, 0, au, 1.0-w_u);
                dlib::bayes_node_utils::set_node_probability(*_bn, suIdx, 1, au, w_u);
            } else {
                dlib::bayes_node_utils::set_node_probability(*_bn, suIdx, 0, au, 1.0);
                dlib::bayes_node_utils::set_node_probability(*_bn, suIdx, 1, au, 0.0);
            }
        } while(dlib::bayes_node_utils::node_next_parent_assignment(*_bn, suIdx, au));
        
        assignment av;
        for (const auto& orNode : orNodes) {
            std::string nodeName = std::to_string(orNode);
            if (_nodeMap.find(nodeName) == _nodeMap.end()) continue;
            unsigned long idx = _nodeMap[nodeName];
            av.add(idx, 0);
        }
        
        do {
            bool anyTrue = false;
            for (const auto& orNode : orNodes) {
                std::string nodeName = std::to_string(orNode);
                if (_nodeMap.find(nodeName) == _nodeMap.end()) continue;
                unsigned long idx = _nodeMap[nodeName];
                if (av[idx] == 1) {
                    anyTrue = true;
                    break;
                }
            }
            
            if (anyTrue) {
                dlib::bayes_node_utils::set_node_probability(*_bn, svIdx, 0, av, w_v);
                dlib::bayes_node_utils::set_node_probability(*_bn, svIdx, 1, av, 1.0-w_v);
            } else {
                dlib::bayes_node_utils::set_node_probability(*_bn, svIdx, 0, av, 1.0);
                dlib::bayes_node_utils::set_node_probability(*_bn, svIdx, 1, av, 0.0);
            }
        } while(dlib::bayes_node_utils::node_next_parent_assignment(*_bn, svIdx, av));
    }
}

bool BNInferenceEngine::isCollider(unsigned long node, const std::vector<unsigned long>& path) const {
    if (path.size() < 3) return false;
    
    // Find position of node in path
    auto it = std::find(path.begin(), path.end(), node);
    if (it == path.end()) return false;
    
    size_t pos = std::distance(path.begin(), it);
    if (pos == 0 || pos == path.size() - 1) return false;
    
    // Check if node is a collider (← node ←)
    unsigned long prev = path[pos - 1];
    unsigned long next = path[pos + 1];
    
    // Check if both prev and next are parents of node
    const auto& parents = getParents(node);
    return std::find(parents.begin(), parents.end(), prev) != parents.end() &&
           std::find(parents.begin(), parents.end(), next) != parents.end();
}

std::vector<unsigned long> BNInferenceEngine::getDescendants(unsigned long node) const {
    std::vector<unsigned long> descendants;
    std::set<unsigned long> visited;
    std::queue<unsigned long> queue;
    
    // Start with immediate children
    auto children = getChildren(node);
    for (auto child : children) {
        queue.push(child);
    }
    
    while (!queue.empty()) {
        unsigned long current = queue.front();
        queue.pop();
        
        if (visited.find(current) != visited.end()) continue;
        
        visited.insert(current);
        descendants.push_back(current);
        
        // Add children of current node
        children = getChildren(current);
        for (auto child : children) {
            if (visited.find(child) == visited.end()) {
                queue.push(child);
            }
        }
    }
    
    return descendants;
}

bool BNInferenceEngine::isActive(unsigned long node, 
                                const std::set<unsigned long>& conditioningSet,
                                const std::vector<unsigned long>& path,
                                std::set<unsigned long>& visited) const {
    bool nodeInCondSet = conditioningSet.find(node) != conditioningSet.end();
    
    if (isCollider(node, path)) {
        // For colliders, node or its descendants must be in conditioning set
        if (nodeInCondSet) return true;
        
        auto descendants = getDescendants(node);
        for (auto desc : descendants) {
            if (conditioningSet.find(desc) != conditioningSet.end()) {
                return true;
            }
        }
        return false;
    } else {
        // For chains and forks, node must NOT be in conditioning set
        return !nodeInCondSet;
    }
}

bool BNInferenceEngine::hasActivePathDFS(unsigned long start, unsigned long end,
                                       const std::set<unsigned long>& conditioningSet,
                                       std::vector<unsigned long>& currentPath,
                                       std::set<unsigned long>& visited) const {
    if (start == end && currentPath.size() > 1) {
        // Check if the path is active
        for (size_t i = 1; i < currentPath.size() - 1; ++i) {
            if (!isActive(currentPath[i], conditioningSet, currentPath, visited)) {
                return false;
            }
        }
        return true;
    }
    
    // Get neighbors (both parents and children)
    std::vector<unsigned long> neighbors;
    auto parents = getParents(start);
    auto children = getChildren(start);
    neighbors.insert(neighbors.end(), parents.begin(), parents.end());
    neighbors.insert(neighbors.end(), children.begin(), children.end());
    
    for (auto neighbor : neighbors) {
        if (visited.find(neighbor) != visited.end()) continue;
        
        visited.insert(neighbor);
        currentPath.push_back(neighbor);
        
        if (hasActivePathDFS(neighbor, end, conditioningSet, currentPath, visited)) {
            return true;
        }
        
        currentPath.pop_back();
        visited.erase(neighbor);
    }
    
    return false;
}

bool BNInferenceEngine::isDConnected(unsigned long start, unsigned long end,
                                   const std::set<unsigned long>& conditioningSet) const {
    std::vector<unsigned long> currentPath = {start};
    std::set<unsigned long> visited = {start};
    
    return hasActivePathDFS(start, end, conditioningSet, currentPath, visited);
}

void BNInferenceEngine::calculateBeliefs(std::map<long, SituationInstance>& instanceMap, simtime_t current) {
    // First verify that the Bayesian network is valid
    if (_bn->number_of_nodes() == 0) {
        throw std::runtime_error("Empty Bayesian network");
    }

    // Perform belief propagation using join tree algorithm
    bayesian_network_join_tree solution(*_bn, *_joinTree);
    
    // Update beliefs and states for each node
    for (auto& [id, instance] : instanceMap) {
        // Skip if not in Bayesian network
        const std::string nodeName = std::to_string(id);
        if (_nodeMap.find(nodeName) == _nodeMap.end()) continue;
        
        unsigned long nodeIdx = _nodeMap[nodeName];
        
        // Only process UNDETERMINED nodes
        if (instance.state == SituationInstance::UNDETERMINED) {
            // Get marginal probability for this node being true
            double beliefTrue = solution.probability(nodeIdx)(1);
            instance.beliefValue = beliefTrue;
            
            // Get the node's threshold from situation graph
            const SituationNode& node = _sg.getNode(id);
            
            // Check if belief exceeds threshold and evidence counter condition is met
            bool hasHigherCounterEvidence = false;
            for (const auto& evidenceId : node.evidences) {
                const auto& evidenceInstance = instanceMap[evidenceId];
                if (instance.counter < evidenceInstance.counter) {
                    hasHigherCounterEvidence = true;
                    break;
                }
            }
            
            // Update state based on belief value and evidence counter condition
            if (beliefTrue >= node.threshold && hasHigherCounterEvidence) {
                instance.state = SituationInstance::TRIGGERED;
                instance.counter++;
                instance.next_start = current;
            } else {
                instance.state = SituationInstance::UNTRIGGERED;
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
            long nodeIdCopy = nodeId;
            nodes->add(nodeIdCopy);
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
    auto connectedNodes = std::make_unique<dlib::set<long>::kernel_1a>();
    
    // Get the node ID from the map
    const std::string nodeName = std::to_string(node.id);
    if (_nodeMap.find(nodeName) == _nodeMap.end()) {
        return connectedNodes;
    }
    unsigned long nodeIdx = _nodeMap[nodeName];
    
    // Create conditioning set from all nodes except the current node
    std::set<unsigned long> conditioningSet;
    for (const auto& [name, idx] : _nodeMap) {
        if (idx != nodeIdx) {
            conditioningSet.insert(idx);
        }
    }
    
    // Check d-connection with all other nodes
    for (const auto& [name, idx] : _nodeMap) {
        if (idx != nodeIdx && isDConnected(nodeIdx, idx, conditioningSet)) {
            long nodeId = std::stol(name);
            connectedNodes->add(nodeId);
        }
    }
    
    return connectedNodes;
}

std::vector<unsigned long> BNInferenceEngine::getParents(unsigned long nodeIdx) const {
    std::vector<unsigned long> parents;
    
    // Get the number of parents for this node
    unsigned long num_parents = _bn->node(nodeIdx).number_of_parents();
    
    // For each parent index, get the actual parent node index
    for (unsigned long i = 0; i < num_parents; ++i) {
        unsigned long parent = _bn->node(nodeIdx).parent(i).index();
        parents.push_back(parent);
    }
    
    return parents;
}

std::vector<unsigned long> BNInferenceEngine::getChildren(unsigned long nodeIdx) const {
    std::vector<unsigned long> children;
    
    // Iterate through all nodes to find those that have this node as a parent
    for (unsigned long i = 0; i < _bn->number_of_nodes(); ++i) {
        // Skip self
        if (i == nodeIdx) continue;
        
        // Check if nodeIdx is a parent of node i
        unsigned long num_parents = _bn->node(i).number_of_parents();
        for (unsigned long j = 0; j < num_parents; ++j) {
            if (_bn->node(i).parent(j).index() == nodeIdx) {
                children.push_back(i);
                break;  // Found as parent, no need to check other parent positions
            }
        }
    }
    
    return children;
}
