#include "BayesianNetworkDlib.h"
#include <stdexcept>
#include <dlib/bayes_utils.h>

BayesianNetworkDlib::BayesianNetworkDlib() : _joinTree(std::make_unique<join_tree_type>()) {}

BayesianNetworkDlib::~BayesianNetworkDlib() = default;

void BayesianNetworkDlib::createNetworkFromSituationGraph(const std::string& jsonFile) {
    try {
        boost::property_tree::ptree jsonTree;
        boost::property_tree::read_json(jsonFile, jsonTree);

        // Clear existing network
        _bn.clear();
        _nodeMap.clear();
        _conditionals.clear();
        _evidence.clear();
        _joinTree = std::make_unique<join_tree_type>();

        // Process each layer
        for (const auto& layer : jsonTree.get_child("layers")) {
            for (const auto& node : layer.second) {
                parseNode(node.second);
            }
        }

        // Process edges after all nodes are created
        for (const auto& layer : jsonTree.get_child("layers")) {
            for (const auto& node : layer.second) {
                parseEdges(node.second);
            }
        }

        // Build join tree for inference
        buildJoinTree();
    } catch (const std::exception& e) {
        throw std::runtime_error("Error creating network from JSON: " + std::string(e.what()));
    }
}

void BayesianNetworkDlib::createNetworkFromSituationGraph(const SituationGraph& graph) {
    // Clear existing network
    _bn.clear();
    _nodeMap.clear();
    _conditionals.clear();
    _evidence.clear();
    _joinTree = std::make_unique<join_tree_type>();
    
    // Step 1: Add nodes
    for (int layer = 0; layer < graph.modelHeight(); layer++) {
        DirectedGraph currentLayer = graph.getLayer(layer);
        for (const auto& nodeId : currentLayer.getVertices()) {
            const SituationNode& node = graph.getNode(nodeId);
            std::string varName = std::to_string(node.id);
            initializeNode(varName);
        }
    }
    
    // Step 2: Add edges
    for (int layer = 0; layer < graph.modelHeight(); layer++) {
        DirectedGraph currentLayer = graph.getLayer(layer);
        for (const auto& nodeId : currentLayer.getVertices()) {
            const SituationNode& node = graph.getNode(nodeId);
            for (const auto& childId : node.evidences) {
                addEdge(std::to_string(childId), std::to_string(node.id));
            }
        }
    }

    // Build join tree for inference
    buildJoinTree();
}

void BayesianNetworkDlib::initializeNode(const std::string& name) {
    unsigned long idx = _bn.add_node();
    _nodeMap[name] = idx;
    
    // Initialize node with binary states
    auto& node = _bn.node(idx);
    node.data.table().set_size(2, 1);  // Binary node (True/False)
    node.data.table()(0,0) = 0.5;      // Initial probability for False
    node.data.table()(1,0) = 0.5;      // Initial probability for True
}

void BayesianNetworkDlib::addEdge(const std::string& parent, const std::string& child) {
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

void BayesianNetworkDlib::buildJoinTree() {
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

void BayesianNetworkDlib::performInference() {
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

void BayesianNetworkDlib::addEvidence(const std::string& nodeName, size_t value) {
    if (_nodeMap.find(nodeName) == _nodeMap.end()) {
        throw std::runtime_error("Node " + nodeName + " not found in network");
    }
    if (value > 1) { // Binary nodes only accept 0 or 1
        throw std::runtime_error("Invalid evidence value. Must be 0 or 1");
    }
    _evidence[nodeName] = value;
}

void BayesianNetworkDlib::eraseEvidence(const std::string& nodeName) {
    if (_nodeMap.find(nodeName) == _nodeMap.end()) {
        throw std::runtime_error("Node " + nodeName + " not found in network");
    }
    
    auto it = _evidence.find(nodeName);
    if (it != _evidence.end()) {
        _evidence.erase(it);
        // Reset node's CPT to uniform distribution
        unsigned long node_idx = _nodeMap[nodeName];
        auto& node = _bn.node(node_idx);
        node.data.table().set_size(2, 1);
        node.data.table()(0,0) = 0.5;
        node.data.table()(1,0) = 0.5;
    }
}

void BayesianNetworkDlib::eraseAllEvidence() {
    _evidence.clear();
    // Reset all nodes to uniform distribution
    for (unsigned long i = 0; i < _bn.number_of_nodes(); ++i) {
        auto& node = _bn.node(i);
        node.data.table().set_size(2, 1);
        node.data.table()(0,0) = 0.5;
        node.data.table()(1,0) = 0.5;
    }
}

std::vector<double> BayesianNetworkDlib::getPosterior(const std::string& nodeName) {
    if (_nodeMap.find(nodeName) == _nodeMap.end()) {
        throw std::runtime_error("Node " + nodeName + " not found in network");
    }
    
    // Ensure inference has been performed with current evidence
    performInference();
    
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

dlib::matrix<double> BayesianNetworkDlib::calculateJointProbability(const std::string& nodeName) {
    unsigned long node_idx = _nodeMap[nodeName];
    auto& node = _bn.node(node_idx);
    
    // If node has no parents, return its probability table
    if (getParents(node_idx).empty()) {
        return node.data.table();
    }
    
    // Otherwise, calculate joint probability using join tree
    dlib::bayesian_network_join_tree solution(_bn, *_joinTree);
    solution.join_tree = *_joinTree;
    solution.compute_net_probabilities();
    
    return solution.node_value(node_idx);
}

std::vector<unsigned long> BayesianNetworkDlib::getParents(unsigned long node_idx) const {
    std::vector<unsigned long> parents;
    for (unsigned long i = 0; i < _bn.number_of_nodes(); ++i) {
        if (_bn.has_edge(i, node_idx)) {
            parents.push_back(i);
        }
    }
    return parents;
}

std::vector<unsigned long> BayesianNetworkDlib::getChildren(unsigned long node_idx) const {
    std::vector<unsigned long> children;
    for (unsigned long i = 0; i < _bn.number_of_nodes(); ++i) {
        if (_bn.has_edge(node_idx, i)) {
            children.push_back(i);
        }
    }
    return children;
}

void BayesianNetworkDlib::updateRefinement(const SituationGraph& graph, 
                                         std::map<long, SituationInstance>& instanceMap) {
    // Learn network structure from data
    learnStructure(instanceMap);
    
    // Update beliefs
    updateBeliefs(instanceMap);
    
    // Build new join tree for inference
    buildJoinTree();
}

void BayesianNetworkDlib::learnStructure(const std::map<long, SituationInstance>& instanceMap) {
    // Use mutual information to learn edges
    for (unsigned long i = 0; i < _bn.number_of_nodes(); ++i) {
        for (unsigned long j = i + 1; j < _bn.number_of_nodes(); ++j) {
            double mi = calculateMutualInformation(i, j, instanceMap);
            if (mi > 0.1) {  // Add edge if mutual information is significant
                _bn.add_edge(i, j);
            }
        }
    }
    
    // Prune weak edges
    pruneWeakEdges();
}

double BayesianNetworkDlib::calculateMutualInformation(unsigned long node1, unsigned long node2,
                                                     const std::map<long, SituationInstance>& instanceMap) {
    // Simple mutual information calculation based on instance states
    double mi = 0.0;
    // Implementation depends on how instance data is stored
    // This is a placeholder - implement based on your data structure
    return mi;
}

void BayesianNetworkDlib::pruneWeakEdges(double threshold) {
    std::vector<std::pair<unsigned long, unsigned long>> edges_to_remove;
    
    // Find weak edges based on conditional probabilities
    for (unsigned long i = 0; i < _bn.number_of_nodes(); ++i) {
        auto children = getChildren(i);
        for (auto child : children) {
            auto& child_node = _bn.node(child);
            double max_impact = 0.0;
            
            // Calculate maximum impact of parent on child
            for (long row = 0; row < child_node.data.table().nr(); ++row) {
                for (long col = 0; col < child_node.data.table().nc(); ++col) {
                    max_impact = std::max(max_impact, std::abs(child_node.data.table()(row,col) - 0.5));
                }
            }
            
            if (max_impact < threshold) {
                edges_to_remove.emplace_back(i, child);
            }
        }
    }
    
    // Remove weak edges
    for (const auto& edge : edges_to_remove) {
        _bn.remove_edge(edge.first, edge.second);
    }
}

void BayesianNetworkDlib::updateBeliefs(std::map<long, SituationInstance>& instanceMap) {
    performInference();
    
    // Update instance beliefs based on inference results
    for (auto& [id, instance] : instanceMap) {
        std::string node_name = std::to_string(id);
        if (_nodeMap.find(node_name) != _nodeMap.end()) {
            auto posterior = getPosterior(node_name);
            instance.belief = posterior[1];  // Probability of True state
        }
    }
}

bool BayesianNetworkDlib::isIndependent(unsigned long node1, unsigned long node2) const {
    return isIndependent(node1, node2, std::set<unsigned long>());
}

bool BayesianNetworkDlib::isIndependent(unsigned long node1, unsigned long node2, 
                                      const std::set<unsigned long>& conditioningSet) const {
    // Get all paths between node1 and node2
    std::vector<std::vector<unsigned long>> allPaths = findAllPaths(node1, node2);
    
    // If no paths exist, nodes are independent
    if (allPaths.empty()) {
        return true;
    }
    
    // Check if all paths are blocked given the conditioning set
    for (const auto& path : allPaths) {
        if (!isPathBlocked(path, conditioningSet)) {
            return false;  // Found an active path, nodes are dependent
        }
    }
    
    return true;  // All paths are blocked, nodes are independent
}

std::vector<std::vector<unsigned long>> BayesianNetworkDlib::findAllPaths(
    unsigned long start, unsigned long end) const {
    std::vector<std::vector<unsigned long>> allPaths;
    std::vector<unsigned long> currentPath;
    std::set<unsigned long> visited;
    
    findPathsDFS(start, end, visited, currentPath, allPaths);
    return allPaths;
}

void BayesianNetworkDlib::findPathsDFS(unsigned long current, unsigned long end,
                                      std::set<unsigned long>& visited,
                                      std::vector<unsigned long>& currentPath,
                                      std::vector<std::vector<unsigned long>>& allPaths) const {
    visited.insert(current);
    currentPath.push_back(current);
    
    if (current == end) {
        allPaths.push_back(currentPath);
    } else {
        // Get all neighbors (both parents and children)
        std::vector<unsigned long> neighbors;
        for (unsigned long i = 0; i < _bn.number_of_nodes(); ++i) {
            if (_bn.has_edge(current, i) || _bn.has_edge(i, current)) {
                neighbors.push_back(i);
            }
        }
        
        for (unsigned long neighbor : neighbors) {
            if (visited.find(neighbor) == visited.end()) {
                findPathsDFS(neighbor, end, visited, currentPath, allPaths);
            }
        }
    }
    
    visited.erase(current);
    currentPath.pop_back();
}

bool BayesianNetworkDlib::isPathBlocked(const std::vector<unsigned long>& path,
                                       const std::set<unsigned long>& conditioningSet) const {
    if (path.size() < 2) return false;
    
    for (size_t i = 1; i < path.size() - 1; ++i) {
        unsigned long prev = path[i-1];
        unsigned long current = path[i];
        unsigned long next = path[i+1];
        
        ConnectionType connection = getConnectionType(prev, current, next);
        bool isConditioned = (conditioningSet.find(current) != conditioningSet.end());
        
        switch (connection) {
            case ConnectionType::Chain:
                // Chain: X -> Y -> Z or X <- Y <- Z
                if (isConditioned) {
                    return true;  // Path is blocked
                }
                break;
                
            case ConnectionType::Fork:
                // Fork: X <- Y -> Z
                if (isConditioned) {
                    return true;  // Path is blocked
                }
                break;
                
            case ConnectionType::Collider:
                // Collider: X -> Y <- Z
                if (!isConditioned && !hasConditionedDescendant(current, conditioningSet)) {
                    return true;  // Path is blocked
                }
                break;
        }
    }
    
    return false;  // Path is active
}

BayesianNetworkDlib::ConnectionType BayesianNetworkDlib::getConnectionType(
    unsigned long prev, unsigned long current, unsigned long next) const {
    bool prevToCurrent = _bn.has_edge(prev, current);
    bool currentToPrev = _bn.has_edge(current, prev);
    bool currentToNext = _bn.has_edge(current, next);
    bool nextToCurrent = _bn.has_edge(next, current);
    
    if ((prevToCurrent && currentToNext) || (currentToPrev && nextToCurrent)) {
        return ConnectionType::Chain;
    }
    if (currentToPrev && currentToNext) {
        return ConnectionType::Fork;
    }
    if (prevToCurrent && nextToCurrent) {
        return ConnectionType::Collider;
    }
    
    // Default to Chain if connection type is ambiguous
    return ConnectionType::Chain;
}

bool BayesianNetworkDlib::hasConditionedDescendant(
    unsigned long node, const std::set<unsigned long>& conditioningSet) const {
    std::set<unsigned long> visited;
    return hasConditionedDescendantDFS(node, conditioningSet, visited);
}

bool BayesianNetworkDlib::hasConditionedDescendantDFS(
    unsigned long current, const std::set<unsigned long>& conditioningSet,
    std::set<unsigned long>& visited) const {
    visited.insert(current);
    
    // Check if current node is in conditioning set
    if (conditioningSet.find(current) != conditioningSet.end()) {
        return true;
    }
    
    // Get children
    std::vector<unsigned long> children;
    for (unsigned long i = 0; i < _bn.number_of_nodes(); ++i) {
        if (_bn.has_edge(current, i) && visited.find(i) == visited.end()) {
            children.push_back(i);
        }
    }
    
    // Recursively check children
    for (unsigned long child : children) {
        if (hasConditionedDescendantDFS(child, conditioningSet, visited)) {
            return true;
        }
    }
    
    return false;
}

// Utility methods
dlib::matrix<double> BayesianNetworkDlib::normalizeMatrix(const dlib::matrix<double>& m) {
    dlib::matrix<double> normalized = m;
    for (long col = 0; col < m.nc(); ++col) {
        double sum = 0;
        for (long row = 0; row < m.nr(); ++row) {
            sum += m(row,col);
        }
        if (sum > 0) {
            for (long row = 0; row < m.nr(); ++row) {
                normalized(row,col) = m(row,col) / sum;
            }
        }
    }
    return normalized;
}

std::vector<double> BayesianNetworkDlib::matrixToVector(const dlib::matrix<double>& m) {
    std::vector<double> v;
    v.reserve(m.nr() * m.nc());
    for (long row = 0; row < m.nr(); ++row) {
        for (long col = 0; col < m.nc(); ++col) {
            v.push_back(m(row,col));
        }
    }
    return v;
}

dlib::matrix<double> BayesianNetworkDlib::vectorToMatrix(const std::vector<double>& v) {
    size_t rows = 2;  // Binary nodes
    size_t cols = v.size() / rows;
    dlib::matrix<double> m(rows, cols);
    for (size_t i = 0; i < v.size(); ++i) {
        m(i / cols, i % cols) = v[i];
    }
    return m;
}
