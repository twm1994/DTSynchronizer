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

BNInferenceEngine::BNInferenceEngine() : _BNet(std::make_unique<BayesianNetwork>()) {}

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
        for (const auto& nodeId : result.nodes) {
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
            const auto& relInfo = relIt->second;
            if (relInfo.relations.type == RelationType::MIXED) {
                hasMixedRelations = true;
                mixedRelationNodes.push_back(nodeId);
                std::cout << "Node " << nodeId << " has mixed AND/OR relations" << std::endl;
            }
        }
    }
    
    // Step 3: Flatten causal subgraph into single DirectedGraph
    std::cout << "\nStep 3: Flattening causal subgraph..." << std::endl;
    DirectedGraph causalDGraph;
    
    // First add all vertices
    std::cout << "Adding vertices to directed graph..." << std::endl;
    for (const auto& [nodeId, _] : causalGraph.situationMap) {
        causalDGraph.add_vertex(nodeId);
        std::cout << "  Added vertex: " << nodeId << std::endl;
    }
    
    // Then add all edges using cached relations
    std::cout << "\nAdding edges using cached relations..." << std::endl;
    for (const auto& [nodeId, node] : causalGraph.situationMap) {
        std::cout << "Processing node " << nodeId << std::endl;
        
        // Get all outgoing relations for this node
        const auto& outgoingRels = _sg.getOutgoingRelations(nodeId);
        for (const auto& [destId, rel] : outgoingRels) {
            if (causalGraph.situationMap.find(destId) != causalGraph.situationMap.end()) {
                std::cout << "  Processing " << rel.relation << " relation..." << std::endl;
                causalDGraph.add_edge(nodeId, destId);
                std::cout << "    Added edge: " << nodeId << " -> " << destId << std::endl;
            }
        }
        
        // Get all incoming relations for this node
        const auto& incomingRels = _sg.getIncomingRelations(nodeId);
        for (const auto& [srcId, rel] : incomingRels) {
            if (causalGraph.situationMap.find(srcId) != causalGraph.situationMap.end()) {
                std::cout << "  Processing incoming " << rel.relation << " relation..." << std::endl;
                causalDGraph.add_edge(srcId, nodeId);
                std::cout << "    Added edge: " << srcId << " -> " << nodeId << std::endl;
            }
        }
    }
    
    // Step 4: Perform subgraph completion if needed
    if (hasMixedRelations) {
        std::cout << "\nStep 4: Mixed relations detected, performing subgraph completion..." << std::endl;
        std::cout << "Found " << mixedRelationNodes.size() << " nodes with mixed relations" << std::endl;
                  
        _mixedNodeInfo.clear();
        
        for (long nodeId : mixedRelationNodes) {
            std::cout << "\nProcessing mixed relations for node " << nodeId << std::endl;
            auto relIt = _relationCache.find(nodeId);
            if (relIt != _relationCache.end()) {
                std::cout << "  Found relations in cache, completing subgraph..." << std::endl;
                completeMixedRelationSubgraph(nodeId, causalDGraph, relIt->second.relations, _mixedNodeInfo);
                
                // Log the M and N nodes created
                auto mnIt = _mixedNodeInfo.find(nodeId);
                if (mnIt != _mixedNodeInfo.end()) {
                    std::cout << "  Created M node: " << mnIt->second.first << std::endl;
                    std::cout << "  Created N node: " << mnIt->second.second << std::endl;
                }
            } else {
                std::cout << "  No relations found in cache for node " << nodeId << std::endl;
            }
        }
    } else {
        std::cout << "\nStep 4: No mixed relations detected, skipping subgraph completion" << std::endl;
    }
    
    // Print the final causal subgraph
    std::cout << "\nFinal causal subgraph:" << std::endl;
    causalDGraph.print();

    // Step 5: Create Bayesian network for the subgraph
    std::cout << "Creating Bayesian Network structure..." << std::endl;

    // Clear existing mappings
    nodeToIndex.clear();
    indexToNode.clear();

    // Build the Bayesian network
    const auto& vertices = causalDGraph.getVertices();
    std::set<long> nodes;
    std::set<std::pair<long, long>> edges;
    
    // Create sequential index mapping for nodes
    unsigned long nextIndex = 0;
    for (const auto& vertex : vertices) {
        nodeToIndex[vertex] = nextIndex;
        indexToNode[nextIndex] = vertex;
        nodes.insert(nextIndex);
        nextIndex++;
    }

    // Print node mapping after it's created
    std::cout << "\nNode to BN index mapping:" << std::endl;
    std::cout << "Total nodes: " << nodeToIndex.size() << std::endl;
    for (const auto& [nodeId, idx] : nodeToIndex) {
        std::cout << "Node ID: " << nodeId << " -> " << idx << std::endl;
    }
    std::cout << std::endl;
    
    // Add all edges using the mapped indices
    for (const auto& vertex : vertices) {
        try {
            const auto& adjList = causalDGraph.getAdjacencyList(vertex);
            for (const auto& dest : adjList) {
                edges.insert(std::make_pair(nodeToIndex[vertex], nodeToIndex[dest]));
            }
        } catch (const std::out_of_range&) {
            continue;
        }
    }
    
    _BNet->buildBNGraph(nodes, edges);

    _BNet->printNetwork();

    // Step 6: Construct CPTs for the Bayesian Network
    std::cout << "Constructing Conditional Probability Tables..." << std::endl;
    std::cout << "Number of nodes in causalInstanceMap: " << causalInstanceMap.size() << std::endl;

    for (const auto& [nodeId, instance] : causalInstanceMap) {
        std::cout << "Processing node " << nodeId << std::endl;
        auto nodeIt = _nodeCache.find(nodeId);
        if (nodeIt == _nodeCache.end()) {
            std::cout << "Node " << nodeId << " not found in _nodeCache" << std::endl;
            continue;
        }
        
        const SituationNode& node = nodeIt->second;
        if (_mixedNodeInfo.find(nodeId) != _mixedNodeInfo.end()) {
            std::cout << "Node " << nodeId << " has mixed relations" << std::endl;
            // Case 5: Mixed relations - use M and N nodes
            constructMixedRelationCPT(node, _mixedNodeInfo[nodeId], causalInstanceMap);
        } else {
            std::cout << "Node " << nodeId << " has regular relations" << std::endl;
            // Cases 1-4: Regular CPT construction
            constructCPT(node, causalInstanceMap);
        }
    }

    // Debug output for cptCache content
    std::cout << "\n=== CPT Cache Contents ===" << std::endl;
    if (cptCache.empty()) {
        std::cout << "CPT Cache is empty!" << std::endl;
    } else {
        for (const auto& entry : cptCache) {
            const auto& key = entry.first;
            const auto& value = entry.second;
            std::cout << "\nCPT Entry:" << std::endl;
            std::cout << "  Node ID: " << std::get<0>(key) << std::endl;
            std::cout << "  State: " << std::get<1>(key) << std::endl;
            std::cout << "  Parent States: [";
            for (const auto& parent : std::get<2>(key)) {
                std::cout << "(node=" << parent.first << ", state=" << parent.second << ") ";
            }
            std::cout << "]" << std::endl;
            std::cout << "  Probability: " << value << std::endl;
        }
    }
    std::cout << "=======================\n" << std::endl;

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
        
        // Skip if we don't have a mapping for this node
        if (nodeToIndex.find(sid) == nodeToIndex.end()) {
            continue;
        }
        
        if (si.state == SituationInstance::TRIGGERING || si.state == SituationInstance::TRIGGERED) {
            // Use the mapped index instead of the original node ID
            evidences[nodeToIndex[sid]] = 1;
            cout << "set evidence of node " << sid << " (index " << nodeToIndex[sid] << "): 1" << endl;
        } else if(si.state == SituationInstance::UNTRIGGERED) {
            evidences[nodeToIndex[sid]] = 0;
            cout << "set evidence of node " << sid << " (index " << nodeToIndex[sid] << "): 0" << endl;
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
    std::string nodeName = std::to_string(node.id);
    auto nodeMapIt = _nodeMap.find(nodeName);
    if (nodeMapIt == _nodeMap.end()) {
        std::cout << "Error: Node " << node.id << " not found in _nodeMap" << std::endl;
        return;
    }
    unsigned long nodeIdx = nodeMapIt->second;
    
    std::cout << "\nConstructing CPT for node " << node.id << " (mapped to index " << nodeIdx << ")" << std::endl;
    std::cout << "Relation type (0: NONE, 1: SOLE, 2: AND_ONLY, 3: OR_ONLY, 4: MIXED): " << static_cast<int>(relations.type) << std::endl;
    
    // Handle each case based on relation type
    switch (relations.type) {
        case RelationType::NONE: {
            // Add to cptCache with empty set
            std::set<std::pair<long, long>> empty_set;
            /*
            * NOTE: in the paper this case is determined by either IoT data (node is observable) or expert (node is not observable)
            * For the first case, p is either 1 or 0
            * For the second case, p is determined by the expert
            * Here, we assume the first case and set node is untriggered by default    
            */ 
            unsigned long mappedNodeIdx = nodeIdx;
            std::tuple<long, long, std::set<std::pair<long, long>>> setting_0(mappedNodeIdx, 0, empty_set);
            cptCache[setting_0] = 1.0;
            std::cout << "Added CPT entry for node " << mappedNodeIdx << " state 0 with empty parent set (NONE case): 1.0" << std::endl;
            
            std::tuple<long, long, std::set<std::pair<long, long>>> setting_1(mappedNodeIdx, 1, empty_set);
            cptCache[setting_1] = 0.0;
            std::cout << "Added CPT entry for node " << mappedNodeIdx << " state 1 with empty parent set (NONE case): 0.0" << std::endl;
            break;
        }
        
        case RelationType::SOLE: {
            auto [connectedId, relation] = relations.soleNodes[0];
            double weight = _weightCache[{connectedId, node.id}];
            
            // Get mapped indices
            unsigned long mappedNodeIdx = nodeIdx;
            unsigned long mappedConnectedIdx = nodeToIndex[connectedId]; // Add to cptCache - P(NOT A|NOT B) = 1, P(A|NOT B) = 0
            std::set<std::pair<long, long>> parent_set_false;
            parent_set_false.insert(std::make_pair(mappedConnectedIdx, 0));
            std::tuple<long, long, std::set<std::pair<long, long>>> setting_false_0(mappedNodeIdx, 0, parent_set_false);
            cptCache[setting_false_0] = 1.0;
            std::cout << "Added CPT entry for node " << mappedNodeIdx << " state 0 with parent " << mappedConnectedIdx << " state 0: 1.0" << std::endl;
            
            std::tuple<long, long, std::set<std::pair<long, long>>> setting_false_1(mappedNodeIdx, 1, parent_set_false);
            cptCache[setting_false_1] = 0.0;
            std::cout << "Added CPT entry for node " << mappedNodeIdx << " state 1 with parent " << mappedConnectedIdx << " state 0: 0.0" << std::endl;

            // Add to cptCache - P(NOT A|B) = 1-w, P(A|B) = w
            std::set<std::pair<long, long>> parent_set_true;
            parent_set_true.insert(std::make_pair(mappedConnectedIdx, 1));
            std::tuple<long, long, std::set<std::pair<long, long>>> setting_true_0(mappedNodeIdx, 0, parent_set_true);
            cptCache[setting_true_0] = 1.0-weight;
            std::cout << "Added CPT entry for node " << mappedNodeIdx << " state 0 with parent " << mappedConnectedIdx << " state 1: " << 1.0-weight << std::endl;
            
            std::tuple<long, long, std::set<std::pair<long, long>>> setting_true_1(mappedNodeIdx, 1, parent_set_true);
            cptCache[setting_true_1] = weight;
            std::cout << "Added CPT entry for node " << mappedNodeIdx << " state 1 with parent " << mappedConnectedIdx << " state 1: " << weight << std::endl;
            break;
        }
        
        case RelationType::AND_ONLY: {
            // Get list of AND nodes with their weights
            std::vector<long> andNodes(relations.andNodes.begin(), relations.andNodes.end());
            std::vector<double> parentWeights;
            std::vector<unsigned long> mappedParentIds;
            
            std::cout << "Processing AND_ONLY relation for node " << node.id << ":" << std::endl;
            std::cout << "  Number of AND parents: " << andNodes.size() << std::endl;
            
            // First, collect all parent weights and mapped IDs
            for (const auto& andNode : andNodes) {
                auto weightIt = _weightCache.find({andNode, node.id});
                if (weightIt != _weightCache.end()) {
                    parentWeights.push_back(weightIt->second);
                    std::cout << "  Parent " << andNode << " weight: " << weightIt->second << std::endl;
                } else {
                    parentWeights.push_back(1.0); // Default weight if not found
                    std::cout << "  Parent " << andNode << " using default weight: 1.0" << std::endl;
                }
                
                // Get mapped parent ID
                std::string nodeName = std::to_string(andNode);
                auto nodeMapIt = _nodeMap.find(nodeName);
                if (nodeMapIt != _nodeMap.end()) {
                    mappedParentIds.push_back(nodeMapIt->second);
                    std::cout << "  Parent " << andNode << " mapped to index: " << nodeMapIt->second << std::endl;
                } else {
                    std::cout << "  Warning: Parent " << andNode << " not found in node map" << std::endl;
                    // Add default entries with empty parent set
                    std::set<std::pair<long, long>> empty_set;
                    std::tuple<long, long, std::set<std::pair<long, long>>> setting_0(nodeIdx, 0, empty_set);
                    std::tuple<long, long, std::set<std::pair<long, long>>> setting_1(nodeIdx, 1, empty_set);
                    cptCache[setting_0] = 1.0;
                    cptCache[setting_1] = 0.0;
                    std::cout << "Added default CPT entries for node " << nodeIdx << " with empty parent set" << std::endl;
                    return;
                }
            }
            
            // Generate all possible parent state combinations
            std::vector<int> andAssignment(andNodes.size(), 0);
            size_t numCombinations = 1 << andNodes.size();
            
            for (size_t combination = 0; combination < numCombinations; ++combination) {
                // Generate this combination's assignment
                for (size_t i = 0; i < andNodes.size(); ++i) {
                    andAssignment[i] = (combination & (1 << i)) ? 1 : 0;
                }
                
                // Build parent states set
                std::set<std::pair<long, long>> parent_states;
                bool allTriggered = true;
                double prob = 1.0;
                
                std::cout << "\nProcessing combination " << combination << ":" << std::endl;
                for (size_t i = 0; i < andNodes.size(); ++i) {
                    parent_states.insert(std::make_pair(mappedParentIds[i], andAssignment[i]));
                    if (andAssignment[i] == 1) {
                        prob *= parentWeights[i];
                    } else {
                        allTriggered = false;
                    }
                    std::cout << "  Parent " << andNodes[i] << ": state=" << andAssignment[i];
                    if (andAssignment[i] == 1) std::cout << ", weight=" << parentWeights[i];
                    std::cout << std::endl;
                }
                
                // Set probabilities based on AND logic
                double p0 = allTriggered ? (1.0 - prob) : 1.0;
                double p1 = allTriggered ? prob : 0.0;
                
                // Add CPT entries
                std::tuple<long, long, std::set<std::pair<long, long>>> setting_0(nodeIdx, 0, parent_states);
                std::tuple<long, long, std::set<std::pair<long, long>>> setting_1(nodeIdx, 1, parent_states);
                
                cptCache[setting_0] = p0;
                cptCache[setting_1] = p1;
                
                std::cout << "  P(" << node.id << "=0|parents) = " << p0 << std::endl;
                std::cout << "  P(" << node.id << "=1|parents) = " << p1 << std::endl;
            }
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
                // Add to cptCache with empty set
                std::set<std::pair<long, long>> empty_set;
                unsigned long mappedNodeIdx = nodeIdx;
                std::tuple<long, long, std::set<std::pair<long, long>>> setting_0(mappedNodeIdx, 0, empty_set);
                cptCache[setting_0] = 1.0;
                std::tuple<long, long, std::set<std::pair<long, long>>> setting_1(mappedNodeIdx, 1, empty_set);
                cptCache[setting_1] = 0.0;
                break;
            }
            
            std::vector<long> orNodes(relations.orNodes.begin(), relations.orNodes.end());
            
            // Calculate total number of combinations (2^n for n parents)
            size_t totalCombinations = 1 << orNodes.size();
            std::cout << "\nProcessing OR relations for node " << node.id << ":" << std::endl;
            std::cout << "  Total parents: " << orNodes.size() << std::endl;
            std::cout << "  Total combinations: " << totalCombinations << std::endl;
            std::cout << "  Parents: ";
            for (const auto& parent : orNodes) {
                std::cout << parent << " ";
            }
            std::cout << std::endl;
            
            // Process each possible combination of parent states
            for (size_t combination = 0; combination < totalCombinations; ++combination) {
                std::vector<int> orAssignment(orNodes.size());
                
                // Convert combination number to binary assignment
                for (size_t i = 0; i < orNodes.size(); ++i) {
                    orAssignment[i] = (combination & (1 << i)) ? 1 : 0;
                }
                
                std::set<std::pair<long, long>> parent_states;
                std::vector<double> weights;
                bool hasTriggeredParent = false;
                
                // Process current parent states and calculate probability
                double prob = 0.0;  // P(A|parents) for OR relation
                double untriggeredProb = 1.0;  // Product of (1-w_i) for triggered parents
                
                // First pass - collect weights and check states
                std::vector<double> parentWeights(orNodes.size(), 0.0);
                for (size_t i = 0; i < orNodes.size(); ++i) {
                    auto weightIt = _weightCache.find({orNodes[i], node.id});
                    if (weightIt != _weightCache.end()) {
                        parentWeights[i] = weightIt->second;
                    }
                }
                
                // Second pass - calculate probability
                for (size_t i = 0; i < orNodes.size(); ++i) {
                    std::string nodeName = std::to_string(orNodes[i]);
                    unsigned long idx = _nodeMap[nodeName];
                    
                    if (orAssignment[i] == 1) {
                        // Parent is in state 1, apply its weight
                        untriggeredProb *= (1.0 - parentWeights[i]);
                        hasTriggeredParent = true;
                    }
                    
                    // Add to parent states set with mapped index
                    unsigned long mappedParentId = nodeToIndex[orNodes[i]];
                    parent_states.insert(std::make_pair(mappedParentId, orAssignment[i]));
                }
                
                // Calculate final probability using noisy-OR formula
                if (hasTriggeredParent) {
                    prob = 1.0 - untriggeredProb;
                    std::cout << "OR probability calculation for node " << node.id << ":" << std::endl;
                    std::cout << "  Parent states and weights:" << std::endl;
                    for (size_t i = 0; i < orNodes.size(); ++i) {
                        std::cout << "    Parent " << orNodes[i] << ": state=" << orAssignment[i] 
                                  << ", weight=" << parentWeights[i] << std::endl;
                    }
                    std::cout << "  untriggeredProb = " << untriggeredProb << std::endl;
                    std::cout << "  final prob = " << prob << std::endl;
                }
                
                // Add CPT entries with detailed logging
                unsigned long mappedNodeIdx = nodeIdx;
                
                // Calculate final probability if not already done
                if (hasTriggeredParent && prob == 0.0) {
                    prob = 1.0 - untriggeredProb;
                }
                
                // Add CPT entries for both child states
                std::tuple<long, long, std::set<std::pair<long, long>>> setting_0(mappedNodeIdx, 0, parent_states);
                cptCache[setting_0] = 1.0 - prob;
                
                std::tuple<long, long, std::set<std::pair<long, long>>> setting_1(mappedNodeIdx, 1, parent_states);
                cptCache[setting_1] = prob;
                
                // Log the entries
                std::cout << "\nAdded CPT entries for combination " << combination << ":" << std::endl;
                std::cout << "  Parent states:";
                for (size_t i = 0; i < orNodes.size(); ++i) {
                    std::cout << " (" << orNodes[i] << ": " << orAssignment[i] << ")";
                }
                std::cout << std::endl;
                
                if (hasTriggeredParent) {
                    std::cout << "  OR probability calculation:" << std::endl;
                    std::cout << "    untriggeredProb = " << untriggeredProb << std::endl;
                    std::cout << "    P(" << node.id << "=0|parents) = " << 1.0-prob << std::endl;
                    std::cout << "    P(" << node.id << "=1|parents) = " << prob << std::endl;
                } else {
                    std::cout << "  No triggered parents:" << std::endl;
                    std::cout << "    P(" << node.id << "=0|parents) = 1.0" << std::endl;
                    std::cout << "    P(" << node.id << "=1|parents) = 0.0" << std::endl;
                }
            }
            break;
        }
        
        case RelationType::MIXED: {
            break;
        }
    }
    
    // Validate that all required CPT entries exist
    std::vector<unsigned long> parents = getParents(nodeIdx);
    
    // For each possible parent state combination
    size_t numParents = parents.size();
    size_t numCombinations = 1 << numParents;
    
    // Get the mapped index for this node (we already have it from earlier)
    unsigned long mappedNodeIdx = nodeIdx;
    
    std::cout << "\nValidating CPT entries for node " << node.id << " (mapped index " << mappedNodeIdx << ")" << std::endl;
    std::cout << "Number of parents: " << numParents << std::endl;
    std::cout << "Total combinations to check: " << numCombinations << std::endl;
    
    // Print parent mapping for debugging
    std::cout << "Parent mapping:" << std::endl;
    for (size_t j = 0; j < numParents; ++j) {
        auto origId = indexToNode[parents[j]];
        std::cout << "  Parent " << j << ": Original ID " << origId << " -> Mapped ID " << parents[j] << std::endl;
    }
    
    // Debug relation info
    std::cout << "Relation info for node " << node.id << ":" << std::endl;
    auto relIt = _relationCache.find(node.id);
    if (relIt != _relationCache.end()) {
        const auto& relInfo = relIt->second;
        std::cout << "  AND nodes: ";
        for (const auto& andNode : relInfo.andNodes) std::cout << andNode << " ";
        std::cout << std::endl;
        std::cout << "  OR nodes: ";
        for (const auto& orNode : relInfo.orNodes) std::cout << orNode << " ";
        std::cout << std::endl;
        std::cout << "  SOLE nodes: ";
        for (const auto& soleNode : relInfo.soleNodes) std::cout << soleNode << " ";
        std::cout << std::endl;
    } else {
        std::cout << "  No relations found in cache" << std::endl;
    }
    
    for (size_t i = 0; i < numCombinations; ++i) {
        std::set<std::pair<long, long>> parent_states;
        for (size_t j = 0; j < numParents; ++j) {
            unsigned long parentId = parents[j];  // Already mapped index
            int state = (i & (1 << j)) ? 1 : 0;
            parent_states.insert(std::make_pair(parentId, state));
        }
        
        // Check both child states (0 and 1)
        for (int childState = 0; childState <= 1; ++childState) {
            std::tuple<long, long, std::set<std::pair<long, long>>> key(mappedNodeIdx, childState, parent_states);
            if (cptCache.find(key) == cptCache.end()) {
                // Missing entry - add default based on relation type
                double prob = 0.0;
                
                if (relations.type == RelationType::NONE || parent_states.empty()) {
                    // No parents - default state is 0
                    prob = (childState == 0) ? 1.0 : 0.0;
                } else if (relations.type == RelationType::SOLE) {
                    // For SOLE relations, check if parent is triggered
                    bool parentTriggered = false;
                    for (const auto& [parentId, state] : parent_states) {
                        if (state == 1) {
                            parentTriggered = true;
                            break;
                        }
                    }
                    if (parentTriggered) {
                        // Use the weight from the relation
                        auto origParentId = indexToNode[parent_states.begin()->first];
                        auto weightIt = _weightCache.find({origParentId, node.id});
                        if (weightIt != _weightCache.end()) {
                            prob = (childState == 1) ? weightIt->second : (1.0 - weightIt->second);
                        }
                    } else {
                        prob = (childState == 0) ? 1.0 : 0.0;
                    }
                } else {
                    // For AND/OR relations, default to untriggered
                    prob = (childState == 0) ? 1.0 : 0.0;
                }
                
                cptCache[key] = prob;
                std::cout << "Added missing CPT entry for node " << node.id 
                        << " (mapped " << mappedNodeIdx << ") state " << childState 
                        << " with parent states: ";
                for (const auto& parent : parent_states) {
                    auto origParentId = indexToNode[parent.first];
                    std::cout << "(" << origParentId << ": " << parent.second << ") ";
                }
                std::cout << ", probability: " << prob << std::endl;
            }
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

// TODO: Check if this is handled in _BNet->buildSolution() or other methods
// void BNInferenceEngine::calculateBeliefs(std::map<long, SituationInstance>& instanceMap, simtime_t current) {
//     // First verify that the Bayesian network is valid
//     if (_bn->number_of_nodes() == 0) {
//         throw std::runtime_error("Empty Bayesian network");
//     }

//     // Ensure we have a valid solution object
//     if (!_solution) {
//         buildJoinTree();
//     }
    
//     // Update beliefs and states for each node
//     for (auto& [id, instance] : instanceMap) {
//         // Skip if not in Bayesian network
//         const std::string nodeName = std::to_string(id);
//         if (_nodeMap.find(nodeName) == _nodeMap.end()) continue;
        
//         unsigned long nodeIdx = _nodeMap[nodeName];
        
//         // Only process UNDETERMINED nodes
//         if (instance.state == SituationInstance::UNDETERMINED) {
//             try {
//                 // Get marginal probability for this node being true
//                 double beliefTrue = _solution->probability(nodeIdx)(1);
//                 instance.beliefValue = beliefTrue;
                
//                 // Get the node's threshold from situation graph
//                 const SituationNode& node = _sg.getNode(id);
                
//                 // Check if belief exceeds threshold and evidence counter condition is met
//                 bool hasHigherCounterEvidence = false;
//                 for (const auto& evidenceId : node.evidences) {
//                     const auto& evidenceInstance = instanceMap[evidenceId];
//                     if (instance.counter < evidenceInstance.counter) {
//                         hasHigherCounterEvidence = true;
//                         break;
//                     }
//                 }
                
//                 // Update state based on belief value and evidence counter condition
//                 if (beliefTrue >= node.threshold && hasHigherCounterEvidence) {
//                     instance.state = SituationInstance::TRIGGERED;
//                     instance.counter++;
//                     instance.next_start = current;
//                 } else {
//                     instance.state = SituationInstance::UNTRIGGERED;
//                 }
//             } catch (const std::exception& e) {
//                 // If we get an error accessing probability, log it and skip this node
//                 std::cerr << "Error calculating belief for node " << id << ": " << e.what() << std::endl;
//                 continue;
//             }
//         }
//     }
// }

CausalConnection BNInferenceEngine::findCausallyConnectedNodes(const std::map<long, SituationInstance>& instanceMap) {
    std::cout << "\nFinding causally connected nodes..." << std::endl;
    
    CausalConnection result;
    
    try {
        // First, find all triggered nodes and their neighbors
        std::cout << "Processing " << instanceMap.size() << " instances" << std::endl;
        for (const auto& [nodeId, instance] : instanceMap) {
            try {
                std::cout << "Checking node " << nodeId << " (state: " << instance.state << ")" << std::endl;
                
                // Verify node exists in graph
                if (!_sg.hasNode(nodeId)) {
                    std::cout << "Warning: Node " << nodeId << " not found in graph" << std::endl;
                    continue;
                }
                
                if (instance.state == SituationInstance::TRIGGERED) {
                    std::cout << "  Node " << nodeId << " is triggered" << std::endl;
                    const SituationNode& trigNode = _sg.getNode(nodeId);
                    std::cout << "  Retrieved node from graph. Finding connected nodes..." << std::endl;
                    
                    auto connectedNodes = findConnectedNodes(trigNode);
                    std::cout << "  Found " << connectedNodes.size() << " connected nodes" << std::endl;
                    
                    result.nodes.insert(connectedNodes.begin(), connectedNodes.end());
                    result.nodes.insert(nodeId);  // Add the triggered node itself
                    std::cout << "  Added nodes to result set. Current size: " << result.nodes.size() << std::endl;
                }
            } catch (const std::exception& e) {
                std::cout << "Error processing node " << nodeId << ": " << e.what() << std::endl;
            }
        }
        
        std::cout << "\nProcessing edges between nodes..." << std::endl;
        // Then find edges between these nodes
        for (const auto& nodeId : result.nodes) {
            try {
                // Check causes
                std::cout << "Checking causes for node " << nodeId << std::endl;
                const SituationNode& node = _sg.getNode(nodeId);
                for (const auto& causeId : node.causes) {
                    if (_sg.hasNode(causeId)) {
                        auto causeIt = std::find(result.nodes.begin(), result.nodes.end(), causeId);
                        if (causeIt != result.nodes.end()) {
                            result.edges.insert(std::make_pair(*causeIt, nodeId));  // cause -> node
                            std::cout << "Added edge: " << (*causeIt) << " -> " << nodeId << std::endl;
                        }
                    }
                }
                
                // Check evidences
                std::cout << "Checking evidences for node " << nodeId << std::endl;
                for (const auto& evidenceId : node.evidences) {
                    if (_sg.hasNode(evidenceId)) {
                        auto evidenceIt = std::find(result.nodes.begin(), result.nodes.end(), evidenceId);
                        if (evidenceIt != result.nodes.end()) {
                            result.edges.insert(std::make_pair(*evidenceIt, nodeId));  // evidence -> node
                            std::cout << "Added edge: " << (*evidenceIt) << " -> " << nodeId << std::endl;
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cout << "Error processing edges for node " << nodeId << ": " << e.what() << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cout << "Fatal error in findCausallyConnectedNodes: " << e.what() << std::endl;
    }
    
    std::cout << "Found " << result.nodes.size() << " nodes and " << result.edges.size() << " edges" << std::endl;
    return result;
}

std::set<long> BNInferenceEngine::findConnectedNodes(const SituationNode& node) {
    std::set<long> connectedNodes;
    
    std::cout << "Finding connected nodes for node " << node.id << std::endl;
    
    // Add causes
    for (const auto& cause : node.causes) {
        if (_sg.hasNode(cause)) {
            connectedNodes.insert(cause);
            std::cout << "  Added cause: " << cause << std::endl;
        }
    }
    
    // Add evidences
    for (const auto& evidence : node.evidences) {
        if (_sg.hasNode(evidence)) {
            connectedNodes.insert(evidence);
            std::cout << "  Added evidence: " << evidence << std::endl;
        }
    }
    
    // Add effects (nodes where this node is a cause)
    for (const auto& [id, otherNode] : _sg.situationMap) {
        if (std::find(otherNode.causes.begin(), otherNode.causes.end(), node.id) != otherNode.causes.end()) {
            if (_sg.hasNode(id)) {
                connectedNodes.insert(id);
                std::cout << "  Added effect: " << id << std::endl;
            }
        }
    }
    
    std::cout << "Found " << connectedNodes.size() << " connected nodes" << std::endl;
    return connectedNodes;
}

std::vector<unsigned long> BNInferenceEngine::getParents(unsigned long nodeIdx) const {
    std::vector<unsigned long> parents;
    
    // Convert nodeIdx back to actual node ID
    auto nodeIdIt = indexToNode.find(nodeIdx);
    if (nodeIdIt == indexToNode.end()) {
        std::cout << "Warning: Node index " << nodeIdx << " not found in indexToNode map" << std::endl;
        return parents;
    }
    long nodeId = nodeIdIt->second;
    
    std::cout << "\nGetting parents for node " << nodeId << " (mapped index " << nodeIdx << ")" << std::endl;
    
    // Get relations from cache
    auto relIt = _relationCache.find(nodeId);
    if (relIt == _relationCache.end()) {
        std::cout << "Warning: Node " << nodeId << " not found in relation cache" << std::endl;
        return parents;
    }
    
    const auto& relInfo = relIt->second;
    
    // Add all parent nodes (AND, OR, and SOLE relations)
    for (const auto& parentId : relInfo.andNodes) {
        std::string parentName = std::to_string(parentId);
        auto it = _nodeMap.find(parentName);
        if (it != _nodeMap.end()) {
            parents.push_back(it->second);
            std::cout << "  Found AND parent: " << parentId << " (mapped index " << it->second << ")" << std::endl;
        } else {
            std::cout << "  Warning: AND parent " << parentId << " not found in _nodeMap" << std::endl;
        }
    }
    
    for (const auto& parentId : relInfo.orNodes) {
        std::string parentName = std::to_string(parentId);
        auto it = _nodeMap.find(parentName);
        if (it != _nodeMap.end()) {
            parents.push_back(it->second);
            std::cout << "  Found OR parent: " << parentId << " (mapped index " << it->second << ")" << std::endl;
        } else {
            std::cout << "  Warning: OR parent " << parentId << " not found in _nodeMap" << std::endl;
        }
    }
    
    for (const auto& parentId : relInfo.soleNodes) {
        std::string parentName = std::to_string(parentId);
        auto it = _nodeMap.find(parentName);
        if (it != _nodeMap.end()) {
            parents.push_back(it->second);
            std::cout << "  Found SOLE parent: " << parentId << " (mapped index " << it->second << ")" << std::endl;
        } else {
            std::cout << "  Warning: SOLE parent " << parentId << " not found in _nodeMap" << std::endl;
        }
    }
    
    return parents;
}

std::vector<unsigned long> BNInferenceEngine::getChildren(unsigned long nodeIdx) const {
    std::vector<unsigned long> children;
    
    // Iterate through all nodes to find those that have this node as a parent
    for (unsigned long i = 0; i < _BNet->number_of_nodes(); ++i) {
        // Skip self
        if (i == nodeIdx) continue;
        
        // Check if nodeIdx is a parent of the current node
        unsigned long num_parents = _BNet->number_of_parents(i);
        for (unsigned long j = 0; j < num_parents; ++j) {
            if (_BNet->get_parent(i, j) == nodeIdx) {
                children.push_back(i);
                break;  // Found as parent, no need to check other parent positions
            }
        }
    }
    
    return children;
}

void BNInferenceEngine::initializeCaches(std::map<long, SituationInstance>& instanceMap) {
    // Clear existing caches
    _nodeCache.clear();
    _relationCache.clear();
    _stateCache.clear();
    _weightCache.clear();
    
    // First pass: Cache all nodes and initialize node mapping
    unsigned long idx = 0;
    for (const auto& [nodeId, node] : _sg.situationMap) {
        _nodeCache[nodeId] = node;
        
        // Create bidirectional mapping between node IDs and indices
        std::string nodeName = std::to_string(nodeId);
        _nodeMap[nodeName] = idx;
        nodeToIndex[nodeId] = idx;
        indexToNode[idx] = nodeId;
        idx++;
        
        std::cout << "Mapped node " << nodeId << " to index " << idx-1 << std::endl;
    }
    
    // Second pass: Process relations after all nodes are mapped
    for (const auto& [nodeId, node] : _sg.situationMap) {
        RelationInfo& relInfo = _relationCache[nodeId];
        
        std::cout << "\nProcessing relations for node " << nodeId << ":" << std::endl;
        
        // Get all incoming relations
        auto incomingRelations = _sg.getIncomingRelations(nodeId);
        for (const auto& [parentId, relation] : incomingRelations) {
            // Add incoming relation to current node's sets
            if (relation.relation == SituationRelation::AND) {
                relInfo.andNodes.insert(parentId);
                std::cout << "  Added AND parent: " << parentId << std::endl;
            } else if (relation.relation == SituationRelation::OR) {
                relInfo.orNodes.insert(parentId);
                std::cout << "  Added OR parent: " << parentId << std::endl;
            } else if (relation.relation == SituationRelation::SOLE) {
                relInfo.soleNodes.insert(parentId);
                std::cout << "  Added SOLE parent: " << parentId << std::endl;
            }
            _weightCache[{parentId, nodeId}] = relation.weight;
        }
        
        // Get all outgoing relations
        auto outgoingRelations = _sg.getOutgoingRelations(nodeId);
        for (const auto& [childId, relation] : outgoingRelations) {
            RelationInfo& childRelInfo = _relationCache[childId];
            if (relation.relation == SituationRelation::AND) {
                childRelInfo.andNodes.insert(nodeId);
                std::cout << "  Added as AND parent to: " << childId << std::endl;
            } else if (relation.relation == SituationRelation::OR) {
                childRelInfo.orNodes.insert(nodeId);
                std::cout << "  Added as OR parent to: " << childId << std::endl;
            } else if (relation.relation == SituationRelation::SOLE) {
                childRelInfo.soleNodes.insert(nodeId);
                std::cout << "  Added as SOLE parent to: " << childId << std::endl;
            }
            _weightCache[{nodeId, childId}] = relation.weight;
        }
        
        // Pre-compute relations for the node
        relInfo.relations = analyzeNodeRelations(node);
        std::cout << "  Relation type: " << static_cast<int>(relInfo.relations.type) << std::endl;
        std::cout << "  AND nodes: " << relInfo.andNodes.size() << std::endl;
        std::cout << "  OR nodes: " << relInfo.orNodes.size() << std::endl;
        std::cout << "  SOLE nodes: " << relInfo.soleNodes.size() << std::endl;
        
        // Pre-compute relations for the node after all relations are properly cached
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
    
    // Get mapped indices for all nodes
    unsigned long mappedNodeIdx = nodeToIndex[nodeIdx];
    unsigned long mappedMNodeId = nodeToIndex[mNodeId];
    unsigned long mappedNNodeId = nodeToIndex[nNodeId];

    // Add to cptCache - M=1, N=1, P(S|M,N) = 1, P(NOT S|M,N) = 0
    std::set<std::pair<long, long>> mn_true;
    mn_true.insert(std::make_pair(mappedMNodeId, 1));
    mn_true.insert(std::make_pair(mappedNNodeId, 1));
    cptCache[std::make_tuple(mappedNodeIdx, 1, mn_true)] = 1.0;
    cptCache[std::make_tuple(mappedNodeIdx, 0, mn_true)] = 0.0;
    
    // Add to cptCache - M=1, N=0, P(S|M,NOT N) = 0, P(NOT S|M,NOT N) = 1
    std::set<std::pair<long, long>> mn_10;
    mn_10.insert(std::make_pair(mappedMNodeId, 1));
    mn_10.insert(std::make_pair(mappedNNodeId, 0));
    cptCache[std::make_tuple(mappedNodeIdx, 1, mn_10)] = 0.0;
    cptCache[std::make_tuple(mappedNodeIdx, 0, mn_10)] = 1.0;
    
    // Add to cptCache - M=0, N=1,  P(S|NOT M,N) = 0, P(NOT S|NOT M,N) = 1
    std::set<std::pair<long, long>> mn_01;
    mn_01.insert(std::make_pair(mappedMNodeId, 0));
    mn_01.insert(std::make_pair(mappedNNodeId, 1));
    cptCache[std::make_tuple(mappedNodeIdx, 1, mn_01)] = 0.0;
    cptCache[std::make_tuple(mappedNodeIdx, 0, mn_01)] = 1.0;
    
    // Add to cptCache - M=0, N=0, P(S|NOT M,NOT N) = 0, P(NOT S|NOT M,NOT N) = 1
    std::set<std::pair<long, long>> mn_false;
    mn_false.insert(std::make_pair(mappedMNodeId, 0));
    mn_false.insert(std::make_pair(mappedNNodeId, 0));
    cptCache[std::make_tuple(mappedNodeIdx, 1, mn_false)] = 0.0;
    cptCache[std::make_tuple(mappedNodeIdx, 0, mn_false)] = 1.0;

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
