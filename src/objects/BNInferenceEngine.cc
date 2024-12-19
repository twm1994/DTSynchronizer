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

BNInferenceEngine::BNInferenceEngine() : _bn(std::make_unique<bn_type>()), _joinTree(nullptr), _solution(nullptr) {}

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

void BNInferenceEngine::reason(SituationGraph sg, std::map<long, SituationInstance> &instanceMap, simtime_t current, std::shared_ptr<ReasonerLogger> logger) {
    _sg = sg;  // Update stored graph
    _logger = logger;
    
    std::cout << "\nInitializing Bayesian Network..." << std::endl;
    
    // Step 1: Discover causal structure and create subgraph
    SituationGraph causalGraph;
    std::map<long, SituationInstance> causalInstanceMap;
    
    auto [nodeSet, edgeSet] = findCausallyConnectedNodes(instanceMap);
    if (!nodeSet || !edgeSet) {
        std::cout << "No causally connected nodes found, using full graph..." << std::endl;
        causalGraph = sg;
        causalInstanceMap = instanceMap;
    } else {
        std::cout << "Found " << nodeSet->size() << " causally connected nodes and " 
                  << edgeSet->size() << " edges" << std::endl;
        
        // Create new graph with only causally connected nodes
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
            DirectedGraph layer;
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
    }
    
    // Step 2: Create new Bayesian network for causal subgraph
    std::cout << "Creating Bayesian Network structure..." << std::endl;
    _bn = std::make_unique<bn_type>();
    _nodeMap.clear();
    _joinTree.reset();
    _solution.reset();
    
    // First add all nodes
    for (const auto& [id, instance] : causalInstanceMap) {
        addNode(std::to_string(id), causalGraph.getNode(id));
    }
    
    // Then add all edges
    for (const auto& [nodeId, node] : causalGraph.situationMap) {
        // Add edges from causes
        for (const auto& cause : node.causes) {
            if (causalGraph.situationMap.find(cause) != causalGraph.situationMap.end()) {
                const SituationRelation* relation = causalGraph.getRelation(cause, nodeId);
                if (relation) {
                    addEdge(std::to_string(cause), std::to_string(nodeId), relation->weight);
                }
            }
        }
        
        // Add edges from evidences
        for (const auto& evidence : node.evidences) {
            if (causalGraph.situationMap.find(evidence) != causalGraph.situationMap.end()) {
                const SituationRelation* relation = causalGraph.getRelation(nodeId, evidence);
                if (relation) {
                    addEdge(std::to_string(nodeId), std::to_string(evidence), relation->weight);
                }
            }
        }
    }
    
    // Step 3: Now that network structure is complete, construct CPTs
    std::cout << "Constructing Conditional Probability Tables..." << std::endl;
    for (const auto& [id, instance] : causalInstanceMap) {
        const SituationNode& node = causalGraph.getNode(id);
        constructCPT(node, causalInstanceMap);
    }
    
    if (_logger) {
        _logger->logStep("BN Structure Created", 
                       current, 
                       -1, 
                       0.0, 
                       {}, 
                       {}, 
                       SituationInstance::UNDETERMINED);
    }

    // Step 4: Build join tree and create solution
    try {
        std::cout << "Building join tree..." << std::endl;
        buildJoinTree();
        std::cout << "Join tree built successfully" << std::endl;
        
        if (_logger) {
            _logger->logStep("BN Join Tree Created", 
                           current, 
                           -1, 
                           0.0, 
                           {}, 
                           {}, 
                           SituationInstance::UNDETERMINED);
        }
        
        // Step 5: Calculate beliefs
        std::cout << "Calculating beliefs..." << std::endl;
        calculateBeliefs(instanceMap, current);
        std::cout << "Beliefs calculated successfully" << std::endl;
        
        if (_logger) {
            _logger->logStep("BN Beliefs Calculated", 
                           current, 
                           -1, 
                           0.0, 
                           {}, 
                           {}, 
                           SituationInstance::UNDETERMINED);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error during join tree creation or belief calculation: " << e.what() << std::endl;
        throw;
    }
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

void BNInferenceEngine::constructCPT(const SituationNode& node, const std::map<long, SituationInstance>& instanceMap) {
    std::vector<long> andNodes;
    std::vector<long> orNodes;
    std::vector<std::pair<long, const SituationRelation*>> soleNodes;
    
    // Step 1: Classify nodes into AND, OR, and SOLE relations
    auto classifyNode = [&](long nodeId, const SituationRelation* relation) {
        if (relation) {
            if (relation->relation == SituationRelation::AND) {
                andNodes.push_back(nodeId);
            } else if (relation->relation == SituationRelation::OR) {
                orNodes.push_back(nodeId);
            } else if (relation->relation == SituationRelation::SOLE) {
                soleNodes.push_back({nodeId, relation});
            }
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
    
    // Case 1: No relations at all
    if (andNodes.empty() && orNodes.empty() && soleNodes.empty()) {
        assignment empty_assignment;
        empty_assignment.clear();
        // Node untriggered by default
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, empty_assignment, 1.0);
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, empty_assignment, 0.0);
        return;
    }
    
    // Case 2: Single SOLE relation
    if (andNodes.empty() && orNodes.empty() && soleNodes.size() == 1) {
        auto [connectedId, relation] = soleNodes[0];
        std::string connectedName = std::to_string(connectedId);
        if (_nodeMap.find(connectedName) == _nodeMap.end()) return;
        unsigned long connectedIdx = _nodeMap[connectedName];
        double w = relation->weight;
        
        assignment a;
        a.clear();
        a.add(connectedIdx, 0);  // B = false
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0);    // P(NOT A|NOT B) = 1
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, 0.0);    // P(A|NOT B) = 0
        
        a[connectedIdx] = 1;     // B = true
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, a, 1.0-w);  // P(NOT A|B) = 1-w
        dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, a, w);      // P(A|B) = w
        return;
    }
    
    // Case 3: Only AND relations
    if (!andNodes.empty() && orNodes.empty() && soleNodes.empty()) {
        EV << "Starting AND case for node " << nodeIdx << endl;
        
        // First, make sure all parent nodes exist in the map
        bool allParentsValid = true;
        for (const auto& andNode : andNodes) {
            std::string nodeName = std::to_string(andNode);
            if (_nodeMap.find(nodeName) == _nodeMap.end()) {
                EV << "Warning: Parent node " << andNode << " not found in node map" << endl;
                allParentsValid = false;
                break;
            }
            unsigned long idx = _nodeMap[nodeName];
            EV << "Parent " << andNode << " maps to index " << idx << endl;
        }
        
        if (!allParentsValid) {
            EV << "Some parents are invalid, setting default probabilities" << endl;
            assignment empty_assignment;
            empty_assignment.clear();
            dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, empty_assignment, 1.0);
            dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, empty_assignment, 0.0);
            return;
        }
        
        assignment and_assignment;
        and_assignment.clear();
        
        // Manually add all parents to the assignment
        for (const auto& andNode : andNodes) {
            std::string nodeName = std::to_string(andNode);
            unsigned long idx = _nodeMap[nodeName];
            EV << "Adding parent " << andNode << " (index " << idx << ") to assignment" << endl;
            and_assignment.add(idx, 0);
        }
        
        // Debug: Print number of parents
        EV << "AND Node " << node.id << " has " << andNodes.size() << " parents" << endl;
        EV << "Initial assignment size: " << and_assignment.size() << endl;
        
        do {
            std::vector<double> weights;
            bool hasTriggeredParent = false;
            bool allParentsTriggered = true;
            
            // Debug: Print current assignment
            EV << "AND Assignment state: ";
            for (const auto& andNode : andNodes) {
                std::string nodeName = std::to_string(andNode);
                unsigned long idx = _nodeMap[nodeName];
                EV << "Parent " << andNode << "(idx=" << idx << ")=" << and_assignment[idx] << " ";
            }
            EV << endl;

            // Check all parents and collect weights of triggered ones
            for (const auto& andNode : andNodes) {
                std::string nodeName = std::to_string(andNode);
                unsigned long idx = _nodeMap[nodeName];
                try {
                    if (and_assignment[idx] == 1) {  // Parent is triggered
                        hasTriggeredParent = true;
                        const SituationRelation* relation = _sg.getRelation(andNode, node.id);
                        if (relation) {
                            weights.push_back(relation->weight);
                        }
                    } else {
                        allParentsTriggered = false;  // Found an untriggered parent
                    }
                } catch (const std::exception& e) {
                    EV << "Error accessing assignment for parent " << andNode << " (idx=" << idx << "): " << e.what() << endl;
                    throw;
                }
            }
            
            try {
                if (!hasTriggeredParent) {
                    // No parents are triggered -> node is untriggered
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, and_assignment, 1.0);
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, and_assignment, 0.0);
                    EV << "Setting AND prob for untriggered parents: P(0)=1.0, P(1)=0.0" << endl;
                } else if (!allParentsTriggered) {
                    // Some parents triggered but not all -> node is untriggered
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, and_assignment, 1.0);
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, and_assignment, 0.0);
                    EV << "Setting AND prob for partially triggered: P(0)=1.0, P(1)=0.0" << endl;
                } else {
                    // All parents are triggered -> calculate probability
                    double prob = 1.0;
                    for (double w : weights) {
                        prob *= w;
                    }
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, and_assignment, 1.0-prob);
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, and_assignment, prob);
                    EV << "Setting AND prob for all triggered: P(0)=" << 1.0-prob << ", P(1)=" << prob << endl;
                }
            } catch (const std::exception& e) {
                EV << "Error setting probability: " << e.what() << endl;
                throw;
            }
        } while(dlib::bayes_node_utils::node_next_parent_assignment(*_bn, nodeIdx, and_assignment));
        return;
    }

    // Case 4: Only OR relations
    if (andNodes.empty() && !orNodes.empty() && soleNodes.empty()) {
        EV << "Starting OR case for node " << nodeIdx << endl;
        
        // First, make sure all parent nodes exist in the map
        bool allParentsValid = true;
        for (const auto& orNode : orNodes) {
            std::string nodeName = std::to_string(orNode);
            if (_nodeMap.find(nodeName) == _nodeMap.end()) {
                EV << "Warning: Parent node " << orNode << " not found in node map" << endl;
                allParentsValid = false;
                break;
            }
            unsigned long idx = _nodeMap[nodeName];
            EV << "Parent " << orNode << " maps to index " << idx << endl;
        }
        
        if (!allParentsValid) {
            EV << "Some parents are invalid, setting default probabilities" << endl;
            assignment empty_assignment;
            empty_assignment.clear();
            dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, empty_assignment, 1.0);
            dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, empty_assignment, 0.0);
            return;
        }
        
        assignment or_assignment;
        or_assignment.clear();
        
        // Manually add all parents to the assignment
        for (const auto& orNode : orNodes) {
            std::string nodeName = std::to_string(orNode);
            unsigned long idx = _nodeMap[nodeName];
            EV << "Adding parent " << orNode << " (index " << idx << ") to assignment" << endl;
            or_assignment.add(idx, 0);
        }

        // Debug: Print number of parents
        EV << "OR Node " << node.id << " has " << orNodes.size() << " parents" << endl;
        EV << "Initial assignment size: " << or_assignment.size() << endl;
        
        do {
            std::vector<double> weights;
            bool hasTriggeredParent = false;
            
            // Debug: Print current assignment
            EV << "OR Assignment state: ";
            for (const auto& orNode : orNodes) {
                std::string nodeName = std::to_string(orNode);
                unsigned long idx = _nodeMap[nodeName];
                EV << "Parent " << orNode << "(idx=" << idx << ")=" << or_assignment[idx] << " ";
            }
            EV << endl;

            // Check all parents and collect weights of triggered ones
            for (const auto& orNode : orNodes) {
                std::string nodeName = std::to_string(orNode);
                unsigned long idx = _nodeMap[nodeName];
                try {
                    if (or_assignment[idx] == 1) {  // Parent is triggered
                        hasTriggeredParent = true;
                        const SituationRelation* relation = _sg.getRelation(orNode, node.id);
                        if (relation) {
                            weights.push_back(relation->weight);
                        }
                    }
                } catch (const std::exception& e) {
                    EV << "Error accessing assignment for parent " << orNode << " (idx=" << idx << "): " << e.what() << endl;
                    throw;
                }
            }
            
            try {
                if (!hasTriggeredParent) {
                    // No parents are triggered -> node is untriggered
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, or_assignment, 1.0);
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, or_assignment, 0.0);
                    EV << "Setting OR prob for untriggered: P(0)=1.0, P(1)=0.0" << endl;
                } else {
                    // At least one parent is triggered -> calculate probability
                    double not_prob = 1.0;
                    for (double w : weights) {
                        not_prob *= (1.0 - w);
                    }
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, or_assignment, not_prob);
                    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, or_assignment, 1.0-not_prob);
                    EV << "Setting OR prob: P(0)=" << not_prob << ", P(1)=" << 1.0-not_prob << endl;
                }
            } catch (const std::exception& e) {
                EV << "Error setting probability: " << e.what() << endl;
                throw;
            }
        } while(dlib::bayes_node_utils::node_next_parent_assignment(*_bn, nodeIdx, or_assignment));
        return;
    }
    
    // Case 5: Mixed relations - not supported yet
    EV << "Warning: Mixed relations not supported for node " << node.id << endl;
    EV << "AND nodes: " << andNodes.size() << ", OR nodes: " << orNodes.size() 
       << ", SOLE nodes: " << soleNodes.size() << endl;
       
    // Set default probabilities
    assignment empty_assignment;
    empty_assignment.clear();
    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 0, empty_assignment, 1.0);
    dlib::bayes_node_utils::set_node_probability(*_bn, nodeIdx, 1, empty_assignment, 0.0);
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

std::pair<std::unique_ptr<dlib::set<long>::kernel_1a>, std::unique_ptr<dlib::set<std::pair<long, long>>::kernel_1a>> 
BNInferenceEngine::findCausallyConnectedNodes(const std::map<long, SituationInstance>& instanceMap) {
    std::cout << "Finding causally connected nodes..." << std::endl;
    
    auto nodeSet = std::make_unique<dlib::set<long>::kernel_1a>();
    auto edgeSet = std::make_unique<dlib::set<std::pair<long, long>>::kernel_1a>();
    
    // First, find all triggered nodes and their neighbors
    for (const auto& [nodeId, instance] : instanceMap) {
        std::cout << "Checking node " << nodeId << " (state: " << instance.state << ")" << std::endl;
        if (instance.state == SituationInstance::TRIGGERED) {
            std::cout << "  Node " << nodeId << " is triggered" << std::endl;
            auto connectedNodes = findConnectedNodes(_sg.getNode(nodeId));
            if (connectedNodes) {
                connectedNodes->reset();
                while (connectedNodes->move_next()) {
                    long connectedId = connectedNodes->element();
                    nodeSet->add(connectedId);
                    std::cout << "  Added connected node " << connectedId << std::endl;
                }
                long trigNodeId = nodeId;
                nodeSet->add(trigNodeId);  // Add the triggered node itself
            }
        }
    }
    
    // Then find edges between these nodes
    for (const auto& [nodeId, node] : _sg.situationMap) {
        if (!nodeSet->is_member(nodeId)) continue;
        
        // Check causes
        for (const auto& cause : node.causes) {
            if (nodeSet->is_member(cause)) {
                std::pair<long, long> edge(cause, nodeId);
                edgeSet->add(edge);
                std::cout << "Added edge: " << cause << " -> " << nodeId << std::endl;
            }
        }
        
        // Check evidences
        for (const auto& evidence : node.evidences) {
            if (nodeSet->is_member(evidence)) {
                std::pair<long, long> edge(nodeId, evidence);
                edgeSet->add(edge);
                std::cout << "Added edge: " << nodeId << " -> " << evidence << std::endl;
            }
        }
    }
    
    std::cout << "Found " << nodeSet->size() << " nodes and " << edgeSet->size() << " edges" << std::endl;
    if (nodeSet->size() == 0) {
        std::cout << "Warning: No triggered nodes found in the network" << std::endl;
        return {nullptr, nullptr};
    }
    
    return {std::move(nodeSet), std::move(edgeSet)};
}

std::unique_ptr<dlib::set<long>::kernel_1a> BNInferenceEngine::findConnectedNodes(const SituationNode& node) {
    auto connectedNodes = std::make_unique<dlib::set<long>::kernel_1a>();
    
    std::cout << "Finding connected nodes for node " << node.id << std::endl;
    
    // Add causes
    for (const auto& cause : node.causes) {
        long causeId = cause;  // Create a copy
        connectedNodes->add(causeId);
        std::cout << "  Added cause: " << causeId << std::endl;
    }
    
    // Add evidences
    for (const auto& evidence : node.evidences) {
        long evidenceId = evidence;  // Create a copy
        connectedNodes->add(evidenceId);
        std::cout << "  Added evidence: " << evidenceId << std::endl;
    }
    
    // Add effects (nodes where this node is a cause)
    for (const auto& [id, otherNode] : _sg.situationMap) {
        if (std::find(otherNode.causes.begin(), otherNode.causes.end(), node.id) != otherNode.causes.end()) {
            long effectId = id;  // Create a copy
            connectedNodes->add(effectId);
            std::cout << "  Added effect: " << effectId << std::endl;
        }
    }
    
    std::cout << "Found " << connectedNodes->size() << " connected nodes" << std::endl;
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

void BNInferenceEngine::convertGraphToBN(const SituationGraph& sg) {
    // Clear existing network if any
    _bn.reset(new bayes_network());
    _nodeMap.clear();
    _sg = sg;

    // First pass: Create nodes for each SituationNode
    for (const auto& [id, node] : sg.situationMap) {
        unsigned long nodeIdx = _bn->add_node();
        _nodeMap[std::to_string(id)] = nodeIdx;
        dlib::bayes_node_utils::set_node_num_values(*_bn, nodeIdx, 2);  // Binary nodes (0 or 1)
    }

    // Second pass: Add edges and construct CPTs
    for (const auto& [id, node] : sg.situationMap) {
        unsigned long childIdx = _nodeMap[std::to_string(id)];
        
        // Add edges from causes (same layer)
        for (const auto& causeId : node.causes) {
            if (_nodeMap.find(std::to_string(causeId)) != _nodeMap.end()) {
                unsigned long parentIdx = _nodeMap[std::to_string(causeId)];
                _bn->add_edge(parentIdx, childIdx);
            }
        }
        
        // Add edges from evidences (layer below)
        for (const auto& evidenceId : node.evidences) {
            if (_nodeMap.find(std::to_string(evidenceId)) != _nodeMap.end()) {
                unsigned long parentIdx = _nodeMap[std::to_string(evidenceId)];
                _bn->add_edge(parentIdx, childIdx);
            }
        }
        
        // Create a temporary instance map with default instances for all nodes
        std::map<long, SituationInstance> tempInstanceMap;
        for (const auto& [nodeId, _] : sg.situationMap) {
            SituationInstance instance;
            instance.id = nodeId;
            instance.state = SituationInstance::UNDETERMINED;
            instance.beliefValue = 0.5;  // Default belief value
            tempInstanceMap[nodeId] = instance;
        }
        
        // Construct CPT for this node
        constructCPT(node, tempInstanceMap);
    }
    
    // Build join tree for inference
    buildJoinTree();
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
