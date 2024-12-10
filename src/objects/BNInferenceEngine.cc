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
    std::cout << "\nLoading Bayesian Network Model..." << std::endl;
    _sg = sg;  // Store the graph
    
    // Create new network
    _bn = std::make_unique<bn_type>();  // Reset network
    _nodeMap.clear();
    _joinTree.reset();
    
    // Step 1: Add nodes for each situation
    std::cout << "Adding nodes to Bayesian Network:" << std::endl;
    for (int layer = 0; layer < sg.modelHeight(); layer++) {
        DirectedGraph currentLayer = sg.getLayer(layer);
        std::cout << "Layer " << layer << ":" << std::endl;
        for (const auto& nodeId : currentLayer.getVertices()) {
            const SituationNode& node = sg.getNode(nodeId);
            std::cout << "  Adding node ID: " << node.id << std::endl;
            addNode(std::to_string(node.id), node);
        }
    }
    
    // Step 2: Add edges based on relations
    std::cout << "\nAdding edges to Bayesian Network:" << std::endl;
    for (int layer = 0; layer < sg.modelHeight(); layer++) {
        DirectedGraph currentLayer = sg.getLayer(layer);
        for (const auto& nodeId : currentLayer.getVertices()) {
            const SituationNode& node = sg.getNode(nodeId);
            // Add edges for both vertical and horizontal relations
            for (const auto& [childId, relation] : sg.getOutgoingRelations(node.id)) {
                std::cout << "  Adding edge: " << node.id << " -> " << childId << " (weight: " << relation.weight << ")" << std::endl;
                addEdge(std::to_string(node.id), std::to_string(childId), relation.weight);
            }
        }
    }
    std::cout << "Bayesian Network Model loading complete.\n" << std::endl;
}

void BNInferenceEngine::reason(SituationGraph sg, std::map<long, SituationInstance>& instanceMap, simtime_t current, std::shared_ptr<ReasonerLogger> logger) {
    _sg = sg;  // Update stored graph
    
    // Step 1: Discover causal structure and create subgraph
    auto [nodeSet, edgeSet] = findCausallyConnectedNodes(instanceMap);
    
    // Create sets for buildReachabilityMatrix
    std::set<long> vertices;
    std::set<SituationGraph::edge_id> edges;
    
    // Convert dlib sets to std::set for reachability matrix
    nodeSet->reset();
    while (nodeSet->move_next()) {
        vertices.insert(nodeSet->element());
    }
    
    edgeSet->reset();
    while (edgeSet->move_next()) {
        edges.insert(edgeSet->element());
    }
    
    // Create subgraph and build its reachability matrix
    SituationGraph causalGraph = sg.createSubgraph(*nodeSet, *edgeSet);
    causalGraph.buildReachabilityMatrix(vertices, edges);
    
    // Create instance map for causal subgraph
    std::map<long, SituationInstance> causalInstanceMap;
    nodeSet->reset();
    while (nodeSet->move_next()) {
        long nodeId = nodeSet->element();
        causalInstanceMap[nodeId] = instanceMap[nodeId];
    }
    
    // Step 2: Create new Bayesian network for causal subgraph
    _bn = std::make_unique<bn_type>();
    _nodeMap.clear();
    _joinTree.reset();
    
    // First, create a sorted list of node IDs to ensure consistent indexing
    std::vector<long> sortedNodeIds;
    for (const auto& [id, _] : causalGraph.situationMap) {
        sortedNodeIds.push_back(id);
    }
    std::sort(sortedNodeIds.begin(), sortedNodeIds.end());
    
    // Create mapping from situation ID to BN index
    std::map<long, unsigned long> idToIndexMap;  // Maps situation ID to BN index
    std::cout << "\nCreating node index mapping:" << std::endl;
    for (size_t i = 0; i < sortedNodeIds.size(); ++i) {
        long id = sortedNodeIds[i];
        unsigned long idx = _bn->add_node();
        idToIndexMap[id] = idx;
        _nodeMap[std::to_string(id)] = idx;
        std::cout << "  Node ID " << id << " -> BN index " << idx << std::endl;
    }
    
    // Add edges using the consistent index mapping
    std::cout << "\nAdding edges using mapped indices:" << std::endl;
    for (const auto& [edge, relation] : causalGraph.relationMap) {
        long parentId = edge.first;
        long childId = edge.second;
        
        // Verify both nodes exist in our mapping
        if (idToIndexMap.find(parentId) == idToIndexMap.end() ||
            idToIndexMap.find(childId) == idToIndexMap.end()) {
            std::cerr << "Warning: Edge " << parentId << "->" << childId 
                     << " references non-existent node(s)" << std::endl;
            continue;
        }
        
        unsigned long parentIdx = idToIndexMap[parentId];
        unsigned long childIdx = idToIndexMap[childId];
        
        _bn->add_edge(parentIdx, childIdx);
        std::cout << "  Added edge: " << parentId << " (index " << parentIdx 
                 << ") -> " << childId << " (index " << childIdx << ")" << std::endl;
    }
    
    // Print final network structure for verification
    std::cout << "\nVerifying network structure:" << std::endl;
    for (const auto& [id, idx] : idToIndexMap) {
        std::cout << "Node " << id << " (index " << idx << "):" << std::endl;
        std::cout << "  Parents:";
        for (unsigned long p = 0; p < _bn->node(idx).number_of_parents(); ++p) {
            unsigned long parentIdx = _bn->node(idx).parent(p).index();
            for (const auto& [pid, pidx] : idToIndexMap) {
                if (pidx == parentIdx) {
                    std::cout << " " << pid;
                    break;
                }
            }
        }
        std::cout << "\n  Children:";
        for (unsigned long c = 0; c < _bn->node(idx).number_of_children(); ++c) {
            unsigned long childIdx = _bn->node(idx).child(c).index();
            for (const auto& [cid, cidx] : idToIndexMap) {
                if (cidx == childIdx) {
                    std::cout << " " << cid;
                    break;
                }
            }
        }
        std::cout << std::endl;
    }
    
    // Step 3: Construct CPTs for nodes in causal subgraph
    for (const auto& [id, node] : causalGraph.situationMap) {
        constructCPT(node, causalInstanceMap);
    }
    
    // Step 4: Create join tree and calculate beliefs
    try {
        std::cout << "\nCreating join tree..." << std::endl;
        std::cout << "Creating empty join tree" << std::endl;
        _joinTree = std::make_unique<join_tree_type>();
        
        std::cout << "Creating moral graph..." << std::endl;
        std::cout << "BN nodes: " << _bn->number_of_nodes() << std::endl;
        
        // Count edges by summing up each node's children
        size_t edgeCount = 0;
        for (unsigned long i = 0; i < _bn->number_of_nodes(); ++i) {
            edgeCount += _bn->node(i).number_of_children();
        }
        std::cout << "BN edges: " << edgeCount << std::endl;
        
        create_moral_graph(*_bn, *_joinTree);
        std::cout << "Moral graph created successfully" << std::endl;
        
        std::cout << "Creating join tree from moral graph..." << std::endl;
        create_join_tree(*_joinTree, *_joinTree);
        std::cout << "Join tree created successfully" << std::endl;
        
        std::cout << "Starting belief calculation..." << std::endl;
        calculateBeliefs(instanceMap, current);
    } catch (const std::exception& e) {
        std::cerr << "Error creating join tree or calculating beliefs: " << e.what() << std::endl;
        std::cerr << "Current state:" << std::endl;
        std::cerr << "  BN nodes: " << _bn->number_of_nodes() << std::endl;
        
        // Count edges in error state too
        size_t edgeCount = 0;
        for (unsigned long i = 0; i < _bn->number_of_nodes(); ++i) {
            edgeCount += _bn->node(i).number_of_children();
        }
        std::cerr << "  BN edges: " << edgeCount << std::endl;
        
        std::cerr << "  NodeMap size: " << _nodeMap.size() << std::endl;
        for (const auto& [name, idx] : _nodeMap) {
            std::cerr << "    Node " << name << " -> " << idx << std::endl;
        }
        throw;  // Re-throw the exception
    }
}

void BNInferenceEngine::addNode(const std::string& nodeName, const SituationNode& node) {
    try {
        // Add node to Bayesian network
        unsigned long nodeIndex = _bn->add_node();
        _nodeMap[nodeName] = nodeIndex;
        
        // Initialize node with binary values (0 = false, 1 = true)
        dlib::bayes_node_utils::set_node_num_values(*_bn, nodeIndex, 2);
        
        // Set initial probabilities for root nodes
        assignment empty_assignment;
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIndex, 0, empty_assignment, 0.5);  // P(node=false)
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIndex, 1, empty_assignment, 0.5);  // P(node=true)
        
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

void BNInferenceEngine::constructCPT(const SituationNode& node, const std::map<long, SituationInstance>& instanceMap) {
    try {
        unsigned long nodeIdx = _nodeMap[std::to_string(node.id)];
        std::cout << "\nConstructing CPT for node " << node.id << " (index: " << nodeIdx << ")" << std::endl;
        
        // Set number of values for the node (binary)
        dlib::bayes_node_utils::set_node_num_values(*_bn, nodeIdx, 2);
        std::cout << "Set number of values to 2" << std::endl;

        // Get parents of the node
        std::vector<unsigned long> parents = getParents(nodeIdx);
        std::cout << "Node has " << parents.size() << " parents" << std::endl;

        // Constants for numerical stability
        const double EPSILON = 1e-6;
        
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
        
        // Case 1: No predecessors or children
        if (andNodes.empty() && orNodes.empty()) {
            assignment empty_assignment;
            dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, empty_assignment, 1.0 - EPSILON);  // P(NOT A) = 1
            dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, empty_assignment, EPSILON);  // P(A) = 0
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
                    double w = std::max(EPSILON, std::min(1.0 - EPSILON, relation->weight));  // Clamp weight
                    
                    assignment a;
                    a.add(connectedIdx, 0);  // B = false
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0 - EPSILON);
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, EPSILON);
                    
                    a[connectedIdx] = 1;     // B = true
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0 - w);
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, w);
                    return;
                }
            }
            
            for (const auto& evidence : node.evidences) {
                const SituationRelation* relation = _sg.getRelation(node.id, evidence);
                if (relation && relation->relation == SituationRelation::SOLE) {
                    std::string connectedName = std::to_string(evidence);
                    if (_nodeMap.find(connectedName) == _nodeMap.end()) continue;
                    unsigned long connectedIdx = _nodeMap[connectedName];
                    double w = std::max(EPSILON, std::min(1.0 - EPSILON, relation->weight));  // Clamp weight
                    
                    assignment a;
                    a.add(connectedIdx, 0);
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0 - EPSILON);
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, EPSILON);
                    
                    a[connectedIdx] = 1;
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0 - w);
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
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0 - w);  // P(NOT A|B) = 1-w
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
                    dlib::bayes_node_utils::set_node_probability(*_bn, suIdx, 0, au, 1.0 - w_u);
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
                    try {
                        assignment a_temp;
                        for (const auto& orNode : orNodes) {
                            std::string nodeName = std::to_string(orNode);
                            if (_nodeMap.find(nodeName) == _nodeMap.end()) continue;
                            unsigned long idx = _nodeMap[nodeName];
                            a_temp.add(idx, av[idx]);
                        }
                        dlib::bayes_node_utils::set_node_probability(*_bn, svIdx, 0, a_temp, w_v);
                        dlib::bayes_node_utils::set_node_probability(*_bn, svIdx, 1, a_temp, 1.0-w_v);
                        std::cout << "Successfully set probabilities: " << w_v << " and " << (1.0-w_v) << std::endl;
                    } catch (const std::exception& e) {
                        std::cerr << "Error setting probabilities for node " << svIdx << ": " << e.what() << std::endl;
                    }
                } else {
                    try {
                        assignment a_temp;
                        for (const auto& orNode : orNodes) {
                            std::string nodeName = std::to_string(orNode);
                            if (_nodeMap.find(nodeName) == _nodeMap.end()) continue;
                            unsigned long idx = _nodeMap[nodeName];
                            a_temp.add(idx, av[idx]);
                        }
                        dlib::bayes_node_utils::set_node_probability(*_bn, svIdx, 0, a_temp, 1.0);
                        dlib::bayes_node_utils::set_node_probability(*_bn, svIdx, 1, a_temp, 0.0);
                        std::cout << "Successfully set default probabilities: 1.0 and 0.0" << std::endl;
                    } catch (const std::exception& e) {
                        std::cerr << "Error setting default probabilities for node " << svIdx << ": " << e.what() << std::endl;
                    }
                }
            } while(dlib::bayes_node_utils::node_next_parent_assignment(*_bn, svIdx, av));
        }
        
        // Validate CPT after construction
        if (!dlib::bayes_node_utils::node_cpt_filled_out(*_bn, nodeIdx)) {
            std::cerr << "Warning: CPT not fully specified for node " << node.id << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in CPT construction for node " << node.id << ": " << e.what() << std::endl;
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
    try {
        std::cout << "\nSetting evidence in Bayesian network..." << std::endl;
        std::cout << "Instance map size: " << instanceMap.size() << std::endl;
        
        // Set evidence for each node based on instance map
        for (const auto& [id, instance] : instanceMap) {
            try {
                const std::string nodeName = std::to_string(id);
                std::cout << "Processing node " << nodeName << "..." << std::endl;
                
                if (_nodeMap.find(nodeName) == _nodeMap.end()) {
                    std::cout << "Node " << nodeName << " not found in _nodeMap, skipping" << std::endl;
                    continue;
                }
                
                unsigned long nodeIdx = _nodeMap[nodeName];
                std::cout << "Found node index: " << nodeIdx << std::endl;
                
                if (instance.state == SituationInstance::TRIGGERED) {
                    std::cout << "Setting node " << id << " (index " << nodeIdx << ") to TRIGGERED" << std::endl;
                    set_node_value(*_bn, nodeIdx, 1);
                } else if (instance.state == SituationInstance::UNTRIGGERED) {
                    std::cout << "Setting node " << id << " (index " << nodeIdx << ") to UNTRIGGERED" << std::endl;
                    set_node_value(*_bn, nodeIdx, 0);
                }
            } catch (const std::out_of_range& e) {
                std::cerr << "Error: Node " << id << " not found in _nodeMap" << std::endl;
                throw;
            } catch (const std::exception& e) {
                std::cerr << "Error setting evidence for node " << id << ": " << e.what() << std::endl;
                throw;
            }
        }
        
        std::cout << "Evidence set successfully. Starting belief propagation..." << std::endl;
        bayesian_network_join_tree solution(*_bn, *_joinTree);
        std::cout << "Belief propagation completed successfully" << std::endl;
        
        // Print final probabilities for verification
        std::cout << "\nFinal node probabilities:" << std::endl;
        for (const auto& [nodeName, nodeIdx] : _nodeMap) {
            try {
                auto prob = solution.probability(nodeIdx)(1);
                std::cout << "  " << nodeName << " (idx=" << nodeIdx << "): " << prob << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error getting probability for node " << nodeName << ": " << e.what() << std::endl;
            }
        }
        
        // Update beliefs and states for each node
        std::cout << "\nUpdating beliefs and states..." << std::endl;
        for (auto& [id, instance] : instanceMap) {
            // Skip if not in Bayesian network
            const std::string nodeName = std::to_string(id);
            std::cout << "Processing node " << nodeName << " for belief update..." << std::endl;
            
            if (_nodeMap.find(nodeName) == _nodeMap.end()) {
                std::cout << "Node " << nodeName << " not found in _nodeMap, skipping belief update" << std::endl;
                continue;
            }
            
            unsigned long nodeIdx = _nodeMap[nodeName];
            std::cout << "Found node index for belief update: " << nodeIdx << std::endl;
            
            // Only process UNDETERMINED nodes
            if (instance.state == SituationInstance::UNDETERMINED) {
                try {
                    std::cout << "Getting probability for node " << id << " (index " << nodeIdx << ")" << std::endl;
                    // Get marginal probability for this node being true
                    double beliefTrue = solution.probability(nodeIdx)(1);
                    instance.beliefValue = beliefTrue;
                    std::cout << "Node " << id << " belief: " << beliefTrue << std::endl;
                    
                    // Get the node's threshold from situation graph
                    const SituationNode& node = _sg.getNode(id);
                    std::cout << "Node threshold: " << node.threshold << std::endl;
                    
                    // Check if belief exceeds threshold and evidence counter condition is met
                    bool hasHigherCounterEvidence = false;
                    for (const auto& evidenceId : node.evidences) {
                        const auto& evidenceInstance = instanceMap[evidenceId];
                        std::cout << "Checking evidence " << evidenceId << " (counter: " << evidenceInstance.counter 
                                << " vs current: " << instance.counter << ")" << std::endl;
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
                        std::cout << "Node " << id << " TRIGGERED (belief: " << beliefTrue 
                                << " >= threshold: " << node.threshold 
                                << " and has higher counter evidence)" << std::endl;
                    } else {
                        instance.state = SituationInstance::UNTRIGGERED;
                        std::cout << "Node " << id << " UNTRIGGERED (belief: " << beliefTrue 
                                << " < threshold: " << node.threshold 
                                << " or no higher counter evidence)" << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error calculating belief for node " << id << ": " << e.what() << std::endl;
                    throw;  // Re-throw to be caught by outer try-catch
                }
            } else {
                std::cout << "Node " << id << " is not UNDETERMINED, skipping belief update" << std::endl;
            }
        }
        std::cout << "Belief calculation completed successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\nError in calculateBeliefs: " << e.what() << std::endl;
        std::cerr << "Current state:" << std::endl;
        std::cerr << "  BN nodes: " << _bn->number_of_nodes() << std::endl;
        
        // Count edges by summing up each node's children
        size_t edgeCount = 0;
        for (unsigned long i = 0; i < _bn->number_of_nodes(); ++i) {
            edgeCount += _bn->node(i).number_of_children();
        }
        std::cerr << "  BN edges: " << edgeCount << std::endl;
        
        std::cerr << "  NodeMap size: " << _nodeMap.size() << std::endl;
        for (const auto& [name, idx] : _nodeMap) {
            std::cerr << "    Node " << name << " -> " << idx << std::endl;
        }
        throw;  // Re-throw the exception
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
