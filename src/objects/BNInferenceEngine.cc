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
#include <iostream>
#include <algorithm>
#include <queue>
#include <memory>
#include <cmath>

using namespace omnetpp;
using namespace dlib;
using namespace dlib::bayes_node_utils;

BNInferenceEngine::BNInferenceEngine() : _BNet(std::make_unique<BayesianNetwork>()), _bn(std::make_unique<bn_type>()), _joinTree(nullptr), _solution(nullptr) {}

BNInferenceEngine::~BNInferenceEngine() = default;

void BNInferenceEngine::loadModel(SituationGraph sg, std::map<long, SituationInstance>& instanceMap) {
    std::cout << "\nLoading Bayesian Network Model..." << std::endl;
    _sg = sg;  // Store the graph
    
    // Initialize caches first
    initializeCaches(instanceMap);
    
    // Step 1: Discover causal structure and create subgraph
    SituationGraph causalGraph;
    std::map<long, SituationInstance> causalInstanceMap;
    
    auto result = findCausallyConnectedNodes(instanceMap);
    if (result.nodes.empty()) {
        std::cout << "No causally connected nodes found, using full graph..." << std::endl;
        // Use cached nodes directly
        for (const auto& [nodeId, node] : _nodeCache) {
            causalGraph.situationMap[nodeId] = node;
            if (instanceMap.find(nodeId) != instanceMap.end()) {
                causalInstanceMap[nodeId] = instanceMap[nodeId];
            }
        }
    } else {
        std::cout << "Found " << result.nodes.size() << " causally connected nodes and " 
                  << result.edges.size() << " edges" << std::endl;
        
        // Create new graph with only causally connected nodes
        for (const auto& node : result.nodes) {
            long nodeId = node->id;
            auto it = _nodeCache.find(nodeId);
            if (it != _nodeCache.end()) {
                causalGraph.situationMap[nodeId] = it->second;
                if (instanceMap.find(nodeId) != instanceMap.end()) {
                    causalInstanceMap[nodeId] = instanceMap[nodeId];
                }
            }
        }
    }
    
    // Step 2: Analyze node relations
    std::cout << "Analyzing node relations..." << std::endl;
    bool hasMixedRelations = false;
    std::vector<long> mixedRelationNodes;
    
    // Use cached relations
    for (const auto& [nodeId, node] : causalGraph.situationMap) {
        auto relIt = _relationCache.find(nodeId);
        if (relIt != _relationCache.end()) {
            const auto& relations = relIt->second.relations;
            if (relations.type == RelationType::MIXED) {
                hasMixedRelations = true;
                mixedRelationNodes.push_back(nodeId);
                std::cout << "Node " << nodeId << " has mixed AND/OR relations" << std::endl;
            }
        }
    }
    
    // Step 3: Flatten causal subgraph into single DirectedGraph
    std::cout << "Flattening causal subgraph..." << std::endl;
    DirectedGraph causalDGraph;
    
    // First add all vertices
    for (const auto& [nodeId, _] : causalGraph.situationMap) {
        causalDGraph.add_vertex(nodeId);
    }
    
    // Then add all edges using cached relations
    for (const auto& [nodeId, node] : causalGraph.situationMap) {
        auto relIt = _relationCache.find(nodeId);
        if (relIt == _relationCache.end()) continue;
        
        const auto& relInfo = relIt->second;
        
        // Add edges from all relation types
        auto addEdgesFromSet = [&](const std::set<long>& nodes) {
            for (long parentId : nodes) {
                if (causalGraph.situationMap.find(parentId) != causalGraph.situationMap.end()) {
                    causalDGraph.add_edge(parentId, nodeId);
                }
            }
        };
        
        addEdgesFromSet(relInfo.soleNodes);
        addEdgesFromSet(relInfo.andNodes);
        addEdgesFromSet(relInfo.orNodes);
    }
    
    // Step 4: Perform subgraph completion if needed
    if (hasMixedRelations) {
        std::cout << "Mixed relations detected, performing subgraph completion for " 
                  << mixedRelationNodes.size() << " nodes..." << std::endl;
                  
        _mixedNodeInfo.clear();
        
        for (long nodeId : mixedRelationNodes) {
            auto relIt = _relationCache.find(nodeId);
            if (relIt != _relationCache.end()) {
                completeMixedRelationSubgraph(nodeId, causalDGraph, relIt->second.relations, _mixedNodeInfo);
            }
        }
    }
    
    // Step 5: Create Bayesian network for the subgraph
    std::cout << "Creating Bayesian Network structure..." << std::endl;
    _bn = std::make_unique<bn_type>();
    _nodeMap.clear();
    _joinTree.reset();
    _solution.reset();
    
    // First add all nodes
    for (const auto& [nodeId, instance] : causalInstanceMap) {
        auto nodeIt = _nodeCache.find(nodeId);
        if (nodeIt != _nodeCache.end()) {
            addNode(std::to_string(nodeId), nodeIt->second);
        }
    }
    
    // Then add all edges using the flattened graph's adjacency lists and cached weights
    const auto& vertices = causalDGraph.getVertices();
    for (const auto& childId : vertices) {
        try {
            const auto& parents = causalDGraph.getAdjacencyList(childId);
            for (const auto& parentId : parents) {
                auto weightIt = _weightCache.find({parentId, childId});
                if (weightIt != _weightCache.end()) {
                    addEdge(std::to_string(parentId), std::to_string(childId), weightIt->second);
                }
            }
        } catch (const std::out_of_range&) {
            continue;
        }
    }

    // Build the Bayesian network
    std::set<long> nodes;
    std::set<std::pair<long, long>> edges;
    
    // Add all vertices
    for (const auto& vertex : vertices) {
        nodes.insert(vertex);
    }
    
    // Add all edges
    for (const auto& vertex : vertices) {
        try {
            const auto& adjList = causalDGraph.getAdjacencyList(vertex);
            for (const auto& dest : adjList) {
                edges.insert(std::make_pair(vertex, dest));
            }
        } catch (const std::out_of_range&) {
            continue;
        }
    }
    
    _BNet->buildBNGraph(nodes, edges);

    // Step 6: Construct CPTs for the Bayesian Network
    std::cout << "Constructing Conditional Probability Tables..." << std::endl;

    for (const auto& [nodeId, instance] : causalInstanceMap) {
        auto nodeIt = _nodeCache.find(nodeId);
        if (nodeIt == _nodeCache.end()) continue;
        
        const SituationNode& node = nodeIt->second;
        if (_mixedNodeInfo.find(nodeId) != _mixedNodeInfo.end()) {
            // Case 5: Mixed relations - use M and N nodes
            constructMixedRelationCPT(node, _mixedNodeInfo[nodeId], causalInstanceMap);
        } else {
            // Cases 1-4: Regular CPT construction
            constructCPT(node, causalInstanceMap);
        }
    }
    _BNet->buildCPT(cptCache);
    std::cout << "Bayesian Network Model loading complete.\n" << std::endl;
}

void BNInferenceEngine::reason(SituationGraph sg, std::map<long, SituationInstance> &instanceMap, simtime_t current, std::shared_ptr<ReasonerLogger> logger) {
    /*
     * Build a Bayesian network solution
     */
    std::map<long, long> evidences;
    for (auto instance : instanceMap) {
        long sid = instance.first;
        SituationInstance si = instance.second;
        if (si.state == SituationInstance::TRIGGERING || si.state == SituationInstance::TRIGGERED) {
            // TODO here, instance alignment is included, correct and fully implemented?
            evidences[sid] = 1;

            cout << "set evidence of node " << sid << ": " << 1 << endl;
        }else if(si.state == SituationInstance::UNTRIGGERED){
            evidences[sid] = 0;
            cout << "set evidence of node " << sid << ": " << 0 << endl;
        }
    }
    _BNet->buildSolution(evidences);

    /*
     * Bayesian network-based state inference
     */
    for (auto& instance : instanceMap) {
        long sid = instance.first;
        SituationInstance &si = instance.second;
        // probability of triggering
        double p_tr = _BNet->getProbability(sid, 1);
        if(si.state == SituationInstance::UNDETERMINED){
            if (p_tr >= sg.situationMap[sid].threshold) {
                si.state = SituationInstance::TRIGGERING;
                si.counter++;
                si.next_start = current;
            } else {
                si.state = SituationInstance::UNTRIGGERED;
            }
                cout << "probability of triggering node " << sid << ": " << p_tr << endl;
                cout << "state of undetermined node " << sid << ": " << si.state << endl;
                cout << "counter of node " << sid << ": " << si.counter << endl;
        }
    }
    /*
     * Clear the solution
     */
    _BNet->clearSolution();
}

void BNInferenceEngine::addNode(const std::string& nodeName, const SituationNode& node) {
    try {
        // Add node to Bayesian network
        unsigned long nodeIndex = _bn->add_node();
        _nodeMap[nodeName] = nodeIndex;
        
        // Initialize node with binary values (0 = false, 1 = true)
        dlib::bayes_node_utils::set_node_num_values(*_bn, nodeIndex, 2);
        
        std::cout << "    Successfully added node " << nodeName << " at index " << nodeIndex << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error adding node " << nodeName << ": " << e.what() << std::endl;
        throw;
    }
}

void BNInferenceEngine::addEdge(const std::string& parentName, const std::string& childName, double weight) {
    try {
        if (_nodeMap.find(parentName) == _nodeMap.end()) {
            std::cerr << "Error: Parent node " << parentName << " not found in node map" << std::endl;
            throw std::runtime_error("Parent node not found");
        }
        if (_nodeMap.find(childName) == _nodeMap.end()) {
            std::cerr << "Error: Child node " << childName << " not found in node map" << std::endl;
            throw std::runtime_error("Child node not found");
        }
        
        unsigned long parentIndex = _nodeMap[parentName];
        unsigned long childIndex = _nodeMap[childName];
        
        // Add the edge to the network
        _bn->add_edge(parentIndex, childIndex);
        
        std::cout << "    Successfully added edge from " << parentName << " (index " << parentIndex 
                 << ") to " << childName << " (index " << childIndex << ")" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error adding edge from " << parentName << " to " << childName << ": " << e.what() << std::endl;
        throw;
    }
}

bool BNInferenceEngine::hasMixedRelations(const SituationNode& node) const {
    auto it = _relationCache.find(node.id);
    if (it != _relationCache.end()) {
        const auto& relInfo = it->second;
        return !relInfo.andNodes.empty() && !relInfo.orNodes.empty();
    }
    return false;
}

NodeRelations BNInferenceEngine::analyzeNodeRelations(const SituationNode& node) {
    auto it = _relationCache.find(node.id);
    if (it != _relationCache.end()) {
        const auto& relInfo = it->second;
        NodeRelations relations;
        relations.hasMixedRelations = !relInfo.andNodes.empty() && !relInfo.orNodes.empty();
        
        // Convert sets to vectors
        relations.andNodes.assign(relInfo.andNodes.begin(), relInfo.andNodes.end());
        relations.orNodes.assign(relInfo.orNodes.begin(), relInfo.orNodes.end());
        
        // Convert sole nodes
        for (const auto& soleId : relInfo.soleNodes) {
            const SituationRelation* relation = _sg.getRelation(soleId, node.id);
            if (!relation) {
                relation = _sg.getRelation(node.id, soleId);
            }
            if (relation) {
                relations.soleNodes.emplace_back(soleId, relation);
            }
        }
        
        // Determine relation type
        if (relations.andNodes.empty() && relations.orNodes.empty() && relations.soleNodes.empty()) {
            relations.type = RelationType::NONE;
        } else if (relations.andNodes.empty() && relations.orNodes.empty() && relations.soleNodes.size() == 1) {
            relations.type = RelationType::SOLE;
        } else if (!relations.andNodes.empty() && relations.orNodes.empty() && relations.soleNodes.empty()) {
            relations.type = RelationType::AND_ONLY;
        } else if (relations.andNodes.empty() && !relations.orNodes.empty() && relations.soleNodes.empty()) {
            relations.type = RelationType::OR_ONLY;
        } else {
            relations.type = RelationType::MIXED;
        }
        
        return relations;
    }
    
    // Return empty relations if node not found in cache
    NodeRelations emptyRelations;
    emptyRelations.type = RelationType::NONE;
    emptyRelations.hasMixedRelations = false;
    return emptyRelations;
}

void BNInferenceEngine::constructCPTFromRelations(const SituationNode& node, const NodeRelations& relations) {
    // Get node index in Bayesian network
    const std::string nodeName = std::to_string(node.id);
    if (_nodeMap.find(nodeName) == _nodeMap.end()) return;
    unsigned long nodeIdx = _nodeMap[nodeName];
    
    // Handle each case based on relation type
    switch (relations.type) {
        case RelationType::NONE: {
            assignment empty_assignment;
            empty_assignment.clear();
            dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, empty_assignment, 1.0);
            dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, empty_assignment, 0.0);

            // Create a tuple with an empty assignment
            std::set<std::pair<long, long>> empty_set;
            std::tuple<long, long, std::set<std::pair<long, long>>> setting_0(nodeIdx, 0, empty_set);
            cptCache[setting_0] = 1.0;
            std::tuple<long, long, std::set<std::pair<long, long>>> setting_1(nodeIdx, 1, empty_set);
            cptCache[setting_1] = 0.0;
            break;
        }
        
        case RelationType::SOLE: {
            auto [connectedId, relation] = relations.soleNodes[0];
            double weight = _weightCache[{connectedId, node.id}];
            std::string connectedName = std::to_string(connectedId);
            if (_nodeMap.find(connectedName) == _nodeMap.end()) return;
            unsigned long connectedIdx = _nodeMap[connectedName];
            
            assignment a;
            a.clear();
            a.add(connectedIdx, 0);  // B = false
            dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0);  // P(NOT A|NOT B) = 1
            dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, 0.0);  // P(A|NOT B) = 0

            // Create a tuple with a single parent-state pair
            std::set<std::pair<long, long>> parent_set_false;
            parent_set_false.insert(std::make_pair(connectedIdx, 0));
            std::tuple<long, long, std::set<std::pair<long, long>>> setting_false_0(nodeIdx, 0, parent_set_false);
            cptCache[setting_false_0] = 1.0;
            std::tuple<long, long, std::set<std::pair<long, long>>> setting_false_1(nodeIdx, 1, parent_set_false);
            cptCache[setting_false_1] = 0.0;

            a[connectedIdx] = 1;     // B = true
            dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0-weight);  // P(NOT A|B) = 1-w
            dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, weight);      // P(A|B) = w

            // Create a tuple with a single parent-state pair
            std::set<std::pair<long, long>> parent_set_true;
            parent_set_true.insert(std::make_pair(connectedIdx, 1));
            std::tuple<long, long, std::set<std::pair<long, long>>> setting_true_0(nodeIdx, 0, parent_set_true);
            cptCache[setting_true_0] = 1.0-weight;
            std::tuple<long, long, std::set<std::pair<long, long>>> setting_true_1(nodeIdx, 1, parent_set_true);
            cptCache[setting_true_1] = weight;
            break;
        }
        
        case RelationType::AND_ONLY: {
            // First, make sure all parent nodes exist in the map
            bool allParentsValid = true;
            for (const auto& andNode : relations.andNodes) {
                std::string nodeName = std::to_string(andNode);
                if (_nodeMap.find(nodeName) == _nodeMap.end()) {
                    allParentsValid = false;
                    break;
                }
            }
            
            if (!allParentsValid) {
                assignment empty_assignment;
                empty_assignment.clear();
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, empty_assignment, 1.0);
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, empty_assignment, 0.0);
                
                // Create a tuple with an empty assignment
                std::set<std::pair<long, long>> empty_set;
                std::tuple<long, long, std::set<std::pair<long, long>>> setting_0(nodeIdx, 0, empty_set);
                cptCache[setting_0] = 1.0;
                std::tuple<long, long, std::set<std::pair<long, long>>> setting_1(nodeIdx, 1, empty_set);
                cptCache[setting_1] = 0.0;
                break;
            }
            
            assignment and_assignment;
            and_assignment.clear();
            
            // Add all parents to the assignment
            for (const auto& andNode : relations.andNodes) {
                std::string nodeName = std::to_string(andNode);
                unsigned long idx = _nodeMap[nodeName];
                and_assignment.add(idx, 0);
            }
            
            do {
                std::vector<double> weights;
                bool hasTriggeredParent = false;
                bool allParentsTriggered = true;
                
                // Check all parents and collect weights of triggered ones
                for (const auto& andNode : relations.andNodes) {
                    std::string nodeName = std::to_string(andNode);
                    unsigned long idx = _nodeMap[nodeName];
                    if (and_assignment[idx] == 1) {
                        hasTriggeredParent = true;
                        double weight = _weightCache[{andNode, node.id}];
                        weights.push_back(weight);
                    } else {
                        allParentsTriggered = false;
                    }
                }
                
                // Create set of parent assignments for cptCache
                std::set<std::pair<long, long>> parent_states;
                for (const auto& andNode : relations.andNodes) {
                    std::string nodeName = std::to_string(andNode);
                    unsigned long idx = _nodeMap[nodeName];
                    parent_states.insert(std::make_pair(andNode, and_assignment[idx]));
                }
                
                if (!hasTriggeredParent || !allParentsTriggered) {
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, and_assignment, 1.0);
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, and_assignment, 0.0);
                    
                    // Add to cptCache
                    std::tuple<long, long, std::set<std::pair<long, long>>> setting_0(nodeIdx, 0, parent_states);
                    cptCache[setting_0] = 1.0;
                    std::tuple<long, long, std::set<std::pair<long, long>>> setting_1(nodeIdx, 1, parent_states);
                    cptCache[setting_1] = 0.0;
                } else {
                    double prob = 1.0;
                    for (double w : weights) {
                        prob *= w;
                    }
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, and_assignment, 1.0-prob);
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, and_assignment, prob);
                    
                    // Add to cptCache
                    std::tuple<long, long, std::set<std::pair<long, long>>> setting_0(nodeIdx, 0, parent_states);
                    cptCache[setting_0] = 1.0-prob;
                    std::tuple<long, long, std::set<std::pair<long, long>>> setting_1(nodeIdx, 1, parent_states);
                    cptCache[setting_1] = prob;
                }
            } while(dlib::bayes_node_utils::node_next_parent_assignment(*_bn, nodeIdx, and_assignment));
            break;
        }
        
        case RelationType::OR_ONLY: {
            // First, make sure all parent nodes exist in the map
            bool allParentsValid = true;
            for (const auto& orNode : relations.orNodes) {
                std::string nodeName = std::to_string(orNode);
                if (_nodeMap.find(nodeName) == _nodeMap.end()) {
                    allParentsValid = false;
                    break;
                }
            }
            
            if (!allParentsValid) {
                assignment empty_assignment;
                empty_assignment.clear();
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, empty_assignment, 1.0);
                dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, empty_assignment, 0.0);

                // Create a tuple with an empty assignment
                std::set<std::pair<long, long>> empty_set;
                std::tuple<long, long, std::set<std::pair<long, long>>> setting_0(nodeIdx, 0, empty_set);
                cptCache[setting_0] = 1.0;
                std::tuple<long, long, std::set<std::pair<long, long>>> setting_1(nodeIdx, 1, empty_set);
                cptCache[setting_1] = 0.0;
                break;
            }
            
            assignment or_assignment;
            or_assignment.clear();
            
            // Add all parents to the assignment
            for (const auto& orNode : relations.orNodes) {
                std::string nodeName = std::to_string(orNode);
                unsigned long idx = _nodeMap[nodeName];
                or_assignment.add(idx, 0);
            }
            
            do {
                std::vector<double> weights;
                bool hasTriggeredParent = false;
                
                // Check all parents and collect weights of triggered ones
                for (const auto& orNode : relations.orNodes) {
                    std::string nodeName = std::to_string(orNode);
                    unsigned long idx = _nodeMap[nodeName];
                    if (or_assignment[idx] == 1) {
                        hasTriggeredParent = true;
                        double weight = _weightCache[{orNode, node.id}];
                        weights.push_back(weight);
                    }
                }

                // Create set of parent assignments for cptCache
                std::set<std::pair<long, long>> parent_states;
                for (const auto& orNode : relations.orNodes) {
                    std::string nodeName = std::to_string(orNode);
                    unsigned long idx = _nodeMap[nodeName];
                    parent_states.insert(std::make_pair(orNode, or_assignment[idx]));
                }
                
                if (!hasTriggeredParent) {
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, or_assignment, 1.0);
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, or_assignment, 0.0);
                    
                    // Add to cptCache
                    std::tuple<long, long, std::set<std::pair<long, long>>> setting_0(nodeIdx, 0, parent_states);
                    cptCache[setting_0] = 1.0;
                    std::tuple<long, long, std::set<std::pair<long, long>>> setting_1(nodeIdx, 1, parent_states);
                    cptCache[setting_1] = 0.0;
                } else {
                    double not_prob = 1.0;
                    for (double w : weights) {
                        not_prob *= (1.0 - w);
                    }
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, or_assignment, not_prob);
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, or_assignment, 1.0-not_prob);
                    
                    // Add to cptCache
                    std::tuple<long, long, std::set<std::pair<long, long>>> setting_0(nodeIdx, 0, parent_states);
                    cptCache[setting_0] = not_prob;
                    std::tuple<long, long, std::set<std::pair<long, long>>> setting_1(nodeIdx, 1, parent_states);
                    cptCache[setting_1] = 1.0-not_prob;
                }
            } while(dlib::bayes_node_utils::node_next_parent_assignment(*_bn, nodeIdx, or_assignment));
            break;
        }
        
        case RelationType::MIXED: {
            // Set default probabilities for now
            assignment empty_assignment;
            empty_assignment.clear();
            break;
        }
    }
}

void BNInferenceEngine::constructCPT(const SituationNode& node, const std::map<long, SituationInstance>& instanceMap) {
    // First analyze the relations
    NodeRelations relations = analyzeNodeRelations(node);
    
    // Then construct the CPT based on the analyzed relations
    constructCPTFromRelations(node, relations);
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
    ::std::queue<unsigned long> queue;  // Explicitly use global namespace std::queue
    
    // Start with all children of the node
    auto children = getChildren(node);
    for (auto child : children) {
        queue.push(child);
        visited.insert(child);
        descendants.push_back(child);
    }
    
    while (!queue.empty()) {
        unsigned long current = queue.front();
        queue.pop();
        
        // Get children of current node
        children = getChildren(current);
        for (auto child : children) {
            if (visited.find(child) == visited.end()) {
                queue.push(child);
                visited.insert(child);
                descendants.push_back(child);
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

    // Ensure we have a valid solution object
    if (!_solution) {
        buildJoinTree();
    }
    
    // Update beliefs and states for each node
    for (auto& [id, instance] : instanceMap) {
        // Skip if not in Bayesian network
        const std::string nodeName = std::to_string(id);
        if (_nodeMap.find(nodeName) == _nodeMap.end()) continue;
        
        unsigned long nodeIdx = _nodeMap[nodeName];
        
        // Only process UNDETERMINED nodes
        if (instance.state == SituationInstance::UNDETERMINED) {
            try {
                // Get marginal probability for this node being true
                double beliefTrue = _solution->probability(nodeIdx)(1);
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
            } catch (const std::exception& e) {
                // If we get an error accessing probability, log it and skip this node
                std::cerr << "Error calculating belief for node " << id << ": " << e.what() << std::endl;
                continue;
            }
        }
    }
}

CausalConnection BNInferenceEngine::findCausallyConnectedNodes(const std::map<long, SituationInstance>& instanceMap) {
    std::cout << "Finding causally connected nodes..." << std::endl;
    
    CausalConnection result;
    
    // First, find all triggered nodes and their neighbors
    for (const auto& [nodeId, instance] : instanceMap) {
        std::cout << "Checking node " << nodeId << " (state: " << instance.state << ")" << std::endl;
        if (instance.state == SituationInstance::TRIGGERED) {
            std::cout << "  Node " << nodeId << " is triggered" << std::endl;
            const SituationNode& trigNode = _sg.getNode(nodeId);
            auto connectedNodes = findConnectedNodes(trigNode);
            result.nodes.insert(connectedNodes.begin(), connectedNodes.end());
            result.nodes.insert(&trigNode);  // Add the triggered node itself
        }
    }
    
    // Then find edges between these nodes
    for (const auto& node : result.nodes) {
        // Check causes
        for (const auto& causeId : node->causes) {
            const SituationNode& causeNode = _sg.getNode(causeId);
            auto causeIt = std::find_if(result.nodes.begin(), result.nodes.end(),
                [&causeNode](const SituationNode* n) { return n->id == causeNode.id; });
            if (causeIt != result.nodes.end()) {
                result.edges.insert(std::make_pair(*causeIt, node));
                std::cout << "Added edge: " << (*causeIt)->id << " -> " << node->id << std::endl;
            }
        }
        
        // Check evidences
        for (const auto& evidenceId : node->evidences) {
            const SituationNode& evidenceNode = _sg.getNode(evidenceId);
            auto evidenceIt = std::find_if(result.nodes.begin(), result.nodes.end(),
                [&evidenceNode](const SituationNode* n) { return n->id == evidenceNode.id; });
            if (evidenceIt != result.nodes.end()) {
                result.edges.insert(std::make_pair(node, *evidenceIt));
                std::cout << "Added edge: " << node->id << " -> " << (*evidenceIt)->id << std::endl;
            }
        }
    }
    
    std::cout << "Found " << result.nodes.size() << " nodes and " << result.edges.size() << " edges" << std::endl;
    return result;
}

std::set<const SituationNode*> BNInferenceEngine::findConnectedNodes(const SituationNode& node) {
    std::set<const SituationNode*> connectedNodes;
    
    std::cout << "Finding connected nodes for node " << node.id << std::endl;
    
    // Add causes
    for (const auto& cause : node.causes) {
        const SituationNode& causeNode = _sg.getNode(cause);
        connectedNodes.insert(&causeNode);
        std::cout << "  Added cause: " << causeNode.id << std::endl;
    }
    
    // Add evidences
    for (const auto& evidence : node.evidences) {
        const SituationNode& evidenceNode = _sg.getNode(evidence);
        connectedNodes.insert(&evidenceNode);
        std::cout << "  Added evidence: " << evidenceNode.id << std::endl;
    }
    
    // Add effects (nodes where this node is a cause)
    for (const auto& [id, otherNode] : _sg.situationMap) {
        if (std::find(otherNode.causes.begin(), otherNode.causes.end(), node.id) != otherNode.causes.end()) {
            const SituationNode& effectNode = _sg.getNode(id);
            connectedNodes.insert(&effectNode);
            std::cout << "  Added effect: " << effectNode.id << std::endl;
        }
    }
    
    std::cout << "Found " << connectedNodes.size() << " connected nodes" << std::endl;
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

void BNInferenceEngine::buildJoinTree() {
    // Verify we have a valid network
    if (!_bn || _bn->number_of_nodes() == 0) {
        throw std::runtime_error("Cannot build join tree: Network is empty or invalid");
    }
    
    EV << "Building join tree for network with " << _bn->number_of_nodes() << " nodes" << endl;
    
    // Create a new join tree
    _joinTree.reset(new join_tree_type());
    EV << "Created new join tree" << endl;
    
    // Create moral graph and join tree using dlib's algorithms
    try {
        EV << "Creating moral graph..." << endl;
        create_moral_graph(*_bn, *_joinTree);
        EV << "Creating join tree..." << endl;
        create_join_tree(*_joinTree, *_joinTree);
        EV << "Join tree created successfully" << endl;
        
        // Create solution object for inference
        EV << "Creating solution object..." << endl;
        _solution.reset(new dlib::bayesian_network_join_tree(*_bn, *_joinTree));
        EV << "Solution object created successfully" << endl;
    } catch (const std::exception& e) {
        std::cerr << "Error during join tree creation: " << e.what() << std::endl;
        throw;
    }
}

void BNInferenceEngine::printNetwork(std::ostream& out) const {
    if (!_bn) {
        out << "Bayesian Network is not initialized." << std::endl;
        return;
    }

    out << "Bayesian Network Structure:" << std::endl;
    out << "Number of nodes: " << _bn->number_of_nodes() << std::endl;
    
    // Print nodes and their connections
    for (unsigned long i = 0; i < _bn->number_of_nodes(); ++i) {
        out << "Node " << i << " (";
        
        // Find and print the original node ID from _nodeMap
        for (const auto& pair : _nodeMap) {
            if (pair.second == i) {
                out << "ID: " << pair.first;
                break;
            }
        }
        out << "):" << std::endl;
        
        // Print parents using number_of_parents() and parent()
        out << "  Parents: ";
        if (_bn->node(i).number_of_parents() == 0) {
            out << "none";
        } else {
            for (unsigned long p = 0; p < _bn->node(i).number_of_parents(); ++p) {
                out << _bn->node(i).parent(p).index() << " ";
            }
        }
        out << std::endl;
        
        // Print children using number_of_children() and child()
        out << "  Children: ";
        if (_bn->node(i).number_of_children() == 0) {
            out << "none";
        } else {
            for (unsigned long c = 0; c < _bn->node(i).number_of_children(); ++c) {
                out << _bn->node(i).child(c).index() << " ";
            }
        }
        out << std::endl;
    }
}

void BNInferenceEngine::printProbabilities(std::ostream& out) const {
    if (!_bn || !_joinTree) {
        out << "Bayesian Network or Join Tree is not initialized." << std::endl;
        return;
    }

    out << "\nBayesian Network Probabilities:" << std::endl;
    
    // Create a join tree solution to compute probabilities
    dlib::bayesian_network_join_tree solution(*_bn, *_joinTree);
    
    // Print probabilities for each node
    for (unsigned long i = 0; i < _bn->number_of_nodes(); ++i) {
        out << "Node " << i << " (";
        
        // Find and print the original node ID from _nodeMap
        for (const auto& pair : _nodeMap) {
            if (pair.second == i) {
                out << "ID: " << pair.first;
                break;
            }
        }
        out << "):" << std::endl;
        
        // Print marginal probabilities
        out << "  Marginal probabilities:" << std::endl;
        for (unsigned long val = 0; val < 2; ++val) {  // Assuming binary nodes
            out << "    P(Node=" << val << ") = " << solution.probability(i)(val) << std::endl;
        }
        
        // Print conditional probabilities if the node has parents
        if (_bn->node(i).number_of_parents() > 0) {
            out << "  Conditional probabilities:" << std::endl;
            
            // Create an assignment for parent states
            dlib::assignment parent_state;
            parent_state.clear();
            for (unsigned long p = 0; p < _bn->node(i).number_of_parents(); ++p) {
                parent_state.add(_bn->node(i).parent(p).index(), 0);
            }
            
            // Iterate through all possible parent combinations
            bool done = false;
            while (!done) {
                out << "    P(Node=1 | ";
                // Print parent states
                for (unsigned long p = 0; p < _bn->node(i).number_of_parents(); ++p) {
                    unsigned long parent_idx = _bn->node(i).parent(p).index();
                    out << "Parent" << parent_idx << "=" << parent_state[parent_idx] << " ";
                }
                out << ") = " << dlib::bayes_node_utils::node_probability(*_bn, i, 1, parent_state) << std::endl;
                
                // Update parent states (like counting in binary)
                done = true;
                for (unsigned long p = 0; p < _bn->node(i).number_of_parents(); ++p) {
                    unsigned long parent_idx = _bn->node(i).parent(p).index();
                    if (parent_state[parent_idx] == 0) {
                        parent_state[parent_idx] = 1;
                        done = false;
                        break;
                    }
                    parent_state[parent_idx] = 0;
                }
            }
        }
        out << std::endl;
    }
}

void BNInferenceEngine::initializeCaches(std::map<long, SituationInstance>& instanceMap) {
    // Clear existing caches
    _nodeCache.clear();
    _relationCache.clear();
    _stateCache.clear();
    _weightCache.clear();
    
    // Cache all nodes
    for (const auto& [nodeId, node] : _sg.situationMap) {
        _nodeCache[nodeId] = node;
        
        // Initialize relation info for this node
        RelationInfo& relInfo = _relationCache[nodeId];
        
        // Process causes
        for (const auto& cause : node.causes) {
            const SituationRelation* relation = _sg.getRelation(cause, nodeId);
            if (relation) {
                if (relation->relation == SituationRelation::AND) {
                    relInfo.andNodes.insert(cause);
                } else if (relation->relation == SituationRelation::OR) {
                    relInfo.orNodes.insert(cause);
                } else if (relation->relation == SituationRelation::SOLE) {
                    relInfo.soleNodes.insert(cause);
                }
                _weightCache[{cause, nodeId}] = relation->weight;
            }
        }
        
        // Process evidences
        for (const auto& evidence : node.evidences) {
            const SituationRelation* relation = _sg.getRelation(nodeId, evidence);
            if (relation) {
                if (relation->relation == SituationRelation::AND) {
                    relInfo.andNodes.insert(evidence);
                } else if (relation->relation == SituationRelation::OR) {
                    relInfo.orNodes.insert(evidence);
                } else if (relation->relation == SituationRelation::SOLE) {
                    relInfo.soleNodes.insert(evidence);
                }
                _weightCache[{nodeId, evidence}] = relation->weight;
            }
        }
        
        // Pre-compute relations for the node
        relInfo.relations = analyzeNodeRelations(node);
        
        // Cache instance state if available
        auto instIt = instanceMap.find(nodeId);
        if (instIt != instanceMap.end()) {
            const auto& instance = instIt->second;
            std::vector<std::pair<std::string, SituationInstance::State>> states;
            // Store the current state
            states.emplace_back("state", instance.state);
            _stateCache[nodeId] = states;
        }
    }
}

void BNInferenceEngine::constructMixedRelationCPT(const SituationNode& node, 
                                               const std::pair<long, long>& mnNodes,
                                               const std::map<long, SituationInstance>& instanceMap) {
    // Get node index in Bayesian network
    const std::string nodeName = std::to_string(node.id);
    if (_nodeMap.find(nodeName) == _nodeMap.end()) return;
    unsigned long nodeIdx = _nodeMap[nodeName];                                                
    const long& mNodeId = mnNodes.first;
    const long& nNodeId = mnNodes.second;

    // First set CPT for S based on M,N nodes (AND relation)
    // P(S|M,N) has 8 combinations as specified
    assignment a;
    
    // Case M=1, N=1
    a.clear();
    a.add(mNodeId, 1);  // M = true
    a.add(nNodeId, 1);  // N = true
    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, 1.0);    // P(S|M,N) = 1
    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 0.0);    // P(NOT S|M,N) = 0
    
    // Add to cptCache - M=1, N=1
    std::set<std::pair<long, long>> mn_true;
    mn_true.insert(std::make_pair(mNodeId, 1));
    mn_true.insert(std::make_pair(nNodeId, 1));
    cptCache[std::make_tuple(nodeIdx, 1, mn_true)] = 1.0;
    cptCache[std::make_tuple(nodeIdx, 0, mn_true)] = 0.0;

    // Case M=1, N=0
    a.clear();
    a.add(mNodeId, 1);  // M = true
    a.add(nNodeId, 0);  // N = false
    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, 0.0);    // P(S|M,NOT N) = 0
    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0);    // P(NOT S|M,NOT N) = 1
    
    // Add to cptCache - M=1, N=0
    std::set<std::pair<long, long>> mn_10;
    mn_10.insert(std::make_pair(mNodeId, 1));
    mn_10.insert(std::make_pair(nNodeId, 0));
    cptCache[std::make_tuple(nodeIdx, 1, mn_10)] = 0.0;
    cptCache[std::make_tuple(nodeIdx, 0, mn_10)] = 1.0;

    // Case M=0, N=1
    a.clear();
    a.add(mNodeId, 0);  // M = false
    a.add(nNodeId, 1);  // N = true
    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, 0.0);    // P(S|NOT M,N) = 0
    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0);    // P(NOT S|NOT M,N) = 1
    
    // Add to cptCache - M=0, N=1
    std::set<std::pair<long, long>> mn_01;
    mn_01.insert(std::make_pair(mNodeId, 0));
    mn_01.insert(std::make_pair(nNodeId, 1));
    cptCache[std::make_tuple(nodeIdx, 1, mn_01)] = 0.0;
    cptCache[std::make_tuple(nodeIdx, 0, mn_01)] = 1.0;

    // Case M=0, N=0
    a.clear();
    a.add(mNodeId, 0);  // M = false
    a.add(nNodeId, 0);  // N = false
    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, 0.0);    // P(S|NOT M,NOT N) = 0
    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0);    // P(NOT S|NOT M,NOT N) = 1
    
    // Add to cptCache - M=0, N=0
    std::set<std::pair<long, long>> mn_false;
    mn_false.insert(std::make_pair(mNodeId, 0));
    mn_false.insert(std::make_pair(nNodeId, 0));
    cptCache[std::make_tuple(nodeIdx, 1, mn_false)] = 0.0;
    cptCache[std::make_tuple(nodeIdx, 0, mn_false)] = 1.0;

    // Get relations for M and N nodes
    NodeRelations relations = analyzeNodeRelations(node);
    
    // Set CPT for M node (AND relation with its parents)
    if (!relations.andNodes.empty()) {
        // Create temporary node to reuse AND CPT construction
        SituationNode mNode;
        mNode.id = mNodeId;
        mNode.causes = relations.andNodes;
        
        // Copy weights from original relations
        std::vector<std::pair<long, const SituationRelation*>> mRelations;
        for (const auto& andNodeId : relations.andNodes) {
            if (const SituationRelation* rel = _sg.getRelation(andNodeId, node.id)) {
                mRelations.push_back(std::make_pair(andNodeId, rel));
            }
        }
        
        // Construct AND CPT for M node
        NodeRelations mNodeRelations;
        mNodeRelations.type = RelationType::AND_ONLY;
        mNodeRelations.andNodes = relations.andNodes;
        mNodeRelations.soleNodes = mRelations;
        constructCPTFromRelations(mNode, mNodeRelations);
    }
    
    // Set CPT for N node (OR relation with its parents)
    if (!relations.orNodes.empty()) {
        // Create temporary node to reuse OR CPT construction
        SituationNode nNode;
        nNode.id = nNodeId;
        nNode.causes = relations.orNodes;
        
        // Copy weights from original relations
        std::vector<std::pair<long, const SituationRelation*>> nRelations;
        for (const auto& orNodeId : relations.orNodes) {
            if (const SituationRelation* rel = _sg.getRelation(orNodeId, node.id)) {
                nRelations.push_back(std::make_pair(orNodeId, rel));
            }
        }
        
        // Construct OR CPT for N node
        NodeRelations nNodeRelations;
        nNodeRelations.type = RelationType::OR_ONLY;
        nNodeRelations.orNodes = relations.orNodes;
        nNodeRelations.soleNodes = nRelations;
        constructCPTFromRelations(nNode, nNodeRelations);
    }
}

void BNInferenceEngine::completeMixedRelationSubgraph(long nodeId, DirectedGraph& causalDGraph, 
                                                   const NodeRelations& relations,
                                                   std::map<long, std::pair<long, long>>& mixedNodeInfo) {
    // Create M and N nodes with IDs based on original node
    long mNodeId = nodeId * 10 + 1;  // M node ID
    long nNodeId = nodeId * 10 + 2;  // N node ID
    
    // Add M and N nodes to graph
    causalDGraph.add_vertex(mNodeId);
    causalDGraph.add_vertex(nNodeId);
    
    // Add edges from M and N to S
    causalDGraph.add_edge(mNodeId, nodeId);
    causalDGraph.add_edge(nNodeId, nodeId);
    
    // Store M,N node info for later CPT construction
    mixedNodeInfo[nodeId] = std::make_pair(mNodeId, nNodeId);
    
    // Create relation info for M and N nodes
    RelationInfo mRelInfo, nRelInfo;
    mRelInfo.relations.type = RelationType::AND_ONLY;
    nRelInfo.relations.type = RelationType::OR_ONLY;
    
    // Update edges: AND nodes -> M, OR nodes -> N
    // Redirect AND node edges to M
    for (const auto& andNodeId : relations.andNodes) {
        // Get adjacency list to check if edge exists
        try {
            const auto& adjList = causalDGraph.getAdjacencyList(andNodeId);
            // If there's a direct edge to nodeId, remove it and add edge to M
            if (std::find(adjList.begin(), adjList.end(), nodeId) != adjList.end()) {
                causalDGraph.remove_edge(andNodeId, nodeId);
                causalDGraph.add_edge(andNodeId, mNodeId);
            }
        } catch (const std::out_of_range&) {
            // Node doesn't exist in graph, skip it
            continue;
        }
    }
    
    // Redirect OR node edges to N
    for (const auto& orNodeId : relations.orNodes) {
        // Get adjacency list to check if edge exists
        try {
            const auto& adjList = causalDGraph.getAdjacencyList(orNodeId);
            // If there's a direct edge to nodeId, remove it and add edge to N
            if (std::find(adjList.begin(), adjList.end(), nodeId) != adjList.end()) {
                causalDGraph.remove_edge(orNodeId, nodeId);
                causalDGraph.add_edge(orNodeId, nNodeId);
            }
        } catch (const std::out_of_range&) {
            // Node doesn't exist in graph, skip it
            continue;
        }
    }
}
