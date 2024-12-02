#include "BayesianNetwork.h"
#include <stdexcept>

BayesianNetwork::BayesianNetwork() {
    // Initialize with LazyPropagation by default
    setInferenceEngine<LazyPropagationEngine<double>>();
}

BayesianNetwork::~BayesianNetwork() = default;

void BayesianNetwork::createNetworkFromSituationGraph(const std::string& jsonFile) {
    try {
        boost::property_tree::ptree jsonTree;
        boost::property_tree::read_json(jsonFile, jsonTree);

        // Clear existing network
        _bn = gum::BayesNet<double>();

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
    } catch (const std::exception& e) {
        throw std::runtime_error("Error creating network from JSON: " + std::string(e.what()));
    }
}

void BayesianNetwork::createNetworkFromSituationGraph(const SituationGraph& graph) {
    // Clear existing network
    _bn = gum::BayesNet<double>();
    
    // Step 1: Add nodes
    for (int layer = 0; layer < graph.modelHeight(); layer++) {
        DirectedGraph currentLayer = graph.getLayer(layer);
        for (const auto& nodeId : currentLayer.getVertices()) {
            const SituationNode& node = graph.getNode(nodeId);
            std::string varName = std::to_string(node.id);
            // Create variable with correct labels
            _bn.add(gum::LabelizedVariable(varName, "", {"False", "True"}));
        }
    }
    
    // Step 2: Add edges and CPTs
    for (int layer = 0; layer < graph.modelHeight(); layer++) {
        DirectedGraph currentLayer = graph.getLayer(layer);
        for (const auto& nodeId : currentLayer.getVertices()) {
            const SituationNode& node = graph.getNode(nodeId);
            constructCPT(node, std::map<long, SituationInstance>());
        }
    }
}

void BayesianNetwork::parseNode(const boost::property_tree::ptree& node) {
    std::string id = std::to_string(node.get<int>("ID"));
    _bn.add(gum::LabelizedVariable(id, "Node " + id, {"False", "True"}));
}

void BayesianNetwork::parseEdges(const boost::property_tree::ptree& node) {
    std::string sourceId = std::to_string(node.get<int>("ID"));

    // Process predecessors
    if (auto predecessors = node.get_child_optional("Predecessors")) {
        for (const auto& pred : *predecessors) {
            std::string predId = std::to_string(pred.second.get<int>("ID"));
            _bn.addArc(predId, sourceId);
            setCPT(sourceId, pred.second);
        }
    }

    // Process children
    if (auto children = node.get_child_optional("Children")) {
        for (const auto& child : *children) {
            std::string childId = std::to_string(child.second.get<int>("ID"));
            _bn.addArc(sourceId, childId);
            setCPT(childId, child.second);
        }
    }
}

void BayesianNetwork::setCPT(const std::string& nodeName, const boost::property_tree::ptree& node) {
    double weight = node.get<double>("Weight-x", node.get<double>("Weight-y", 0.5));
    gum::Potential<double> pot;
    pot.add(_bn.variable(nodeName));
    pot.fillWith({weight, 1.0 - weight});
    _bn.cpt(nodeName).fillWith(pot);
}

void BayesianNetwork::performInference() {
    if (!_inferenceEngine) {
        throw std::runtime_error("No inference engine set");
    }
    _inferenceEngine->makeInference();
}

void BayesianNetwork::addEvidence(const std::string& nodeName, size_t value) {
    if (!_inferenceEngine) {
        throw std::runtime_error("No inference engine set");
    }
    _inferenceEngine->addEvidence(nodeName, value);
}

void BayesianNetwork::eraseEvidence(const std::string& nodeName) {
    if (!_inferenceEngine) {
        throw std::runtime_error("No inference engine set");
    }
    _inferenceEngine->eraseEvidence(nodeName);
}

void BayesianNetwork::eraseAllEvidence() {
    if (!_inferenceEngine) {
        throw std::runtime_error("No inference engine set");
    }
    _inferenceEngine->eraseAllEvidence();
}

gum::Potential<double> BayesianNetwork::getPosterior(const std::string& nodeName) {
    if (!_inferenceEngine) {
        throw std::runtime_error("No inference engine set");
    }
    return _inferenceEngine->posterior(nodeName);
}

void BayesianNetwork::loadFromBIF(const std::string& filename) {
    gum::BayesNet<double> temp;
    gum::BIFReader<double> reader(&temp, filename);
    reader.proceed();
    _bn = std::move(temp);
}

void BayesianNetwork::saveToBIF(const std::string& filename) {
    gum::BIFWriter<double> writer;
    writer.write(filename, _bn);
}

void BayesianNetwork::loadNetwork(const std::string& filename) {
    loadFromBIF(filename);
}

void BayesianNetwork::saveNetwork(const std::string& filename) {
    saveToBIF(filename);
}

void BayesianNetwork::constructCPT(const SituationNode& node, 
                                 const std::map<long, SituationInstance>& instanceMap) {
    std::string varName = std::to_string(node.id);
    
    // Create potential for this node
    gum::Potential<double> pot;
    pot.add(_bn.variable(varName));

    // For each relation, add the corresponding variable to the potential
    for (const auto& [otherId, relation] : _sg.getOutgoingRelations(node.id)) {
        pot.add(_bn.variable(std::to_string(otherId)));
    }
    
    std::vector<long> predecessors;
    std::vector<long> children;
    std::vector<long> andNodes;
    std::vector<long> orNodes;
    
    // Collect predecessors (TYPE-H) and children (TYPE-V)
    for (const auto& predId : node.causes) {
        const SituationRelation* relation = _sg.getRelation(predId, node.id);
        if (relation && relation->type == SituationRelation::H) {
            predecessors.push_back(predId);
            if (relation->relation == SituationRelation::AND) {
                andNodes.push_back(predId);
            } else if (relation->relation == SituationRelation::OR) {
                orNodes.push_back(predId);
            }
        }
    }
    
    for (const auto& childId : node.evidences) {
        const SituationRelation* relation = _sg.getRelation(node.id, childId);
        if (relation && relation->type == SituationRelation::V) {
            children.push_back(childId);
            if (relation->relation == SituationRelation::AND) {
                andNodes.push_back(childId);
            } else if (relation->relation == SituationRelation::OR) {
                orNodes.push_back(childId);
            }
        }
    }
    
    // Case 1: No predecessors or children
    if (predecessors.empty() && children.empty()) {
        pot.fillWith({0.0, 1.0}); // [UNTRIGGERED, TRIGGERED]
        _bn.cpt(varName).fillWith(pot);
        return;
    }
    
    // Case 2: Single predecessor/child with SOLE relation
    if (predecessors.size() + children.size() == 1 && andNodes.empty() && orNodes.empty()) {
        long otherId = predecessors.empty() ? children[0] : predecessors[0];
        const SituationRelation* relation = _sg.getRelation(predecessors.empty() ? node.id : otherId,
                                                          predecessors.empty() ? otherId : node.id);
        if (relation && relation->relation == SituationRelation::SOLE) {
            pot.add(_bn.variable(std::to_string(otherId)));
            pot.fillWith({1.0 - relation->weight, relation->weight,
                         1.0 - relation->weight, relation->weight});
            _bn.cpt(varName).fillWith(pot);
            return;
        }
    }
    
    // Case 3: All AND relations
    if (!andNodes.empty() && orNodes.empty()) {
        double prob = calculateAndProbability(node, andNodes);
        for (const auto& nodeId : andNodes) {
            pot.add(_bn.variable(std::to_string(nodeId)));
        }
        // Fill CPT based on AND logic
        std::vector<double> cptValues;
        size_t combinations = 1 << andNodes.size();
        for (size_t i = 0; i < combinations; ++i) {
            cptValues.push_back(i == combinations - 1 ? prob : 0.0);
        }
        pot.fillWith(cptValues);  // First fill the potential with values
        _bn.cpt(varName).fillWith(pot);  // Then copy the filled potential to the CPT
        return;
    }
    
    // Case 4: All OR relations
    if (andNodes.empty() && !orNodes.empty()) {
        double prob = calculateOrProbability(node, orNodes);
        for (const auto& nodeId : orNodes) {
            pot.add(_bn.variable(std::to_string(nodeId)));
        }
        // Fill CPT based on OR logic
        std::vector<double> cptValues;
        size_t combinations = 1 << orNodes.size();
        for (size_t i = 0; i < combinations; ++i) {
            cptValues.push_back(i == 0 ? 0.0 : prob);
        }
        pot.fillWith(cptValues);  // First fill the potential with values
        _bn.cpt(varName).fillWith(pot);  // Then copy the filled potential to the CPT
        return;
    }
    
    // Case 5: Mixed AND and OR relations
    if (!andNodes.empty() && !orNodes.empty()) {
        handleMixedRelations(node, andNodes, orNodes);
    }
}

double BayesianNetwork::calculateAndProbability(const SituationNode& node, const std::vector<long>& nodes) {
    double prob = 1.0;
    for (const auto& otherId : nodes) {
        const SituationRelation* relation = _sg.getRelation(otherId, node.id);
        if (relation) {
            prob *= relation->weight;
        }
    }
    return prob;
}

double BayesianNetwork::calculateOrProbability(const SituationNode& node, const std::vector<long>& nodes) {
    double m = 1.0;
    for (const auto& otherId : nodes) {
        const SituationRelation* relation = _sg.getRelation(otherId, node.id);
        if (relation) {
            m *= (1.0 - relation->weight);
        }
    }
    return 1.0 - m;
}

void BayesianNetwork::handleMixedRelations(const SituationNode& node, 
                                         const std::vector<long>& andNodes,
                                         const std::vector<long>& orNodes) {
    // Create intermediary nodes u and v
    std::string uName = "u_" + std::to_string(node.id);
    std::string vName = "v_" + std::to_string(node.id);
    
    // Add intermediary nodes to Bayesian network
    _bn.add(gum::LabelizedVariable(uName, "AND intermediary", {"False", "True"}));
    _bn.add(gum::LabelizedVariable(vName, "AND intermediary", {"False", "True"}));
    
    // Calculate probabilities for intermediary nodes
    double probU = calculateAndProbability(node, andNodes);
    double probV = calculateOrProbability(node, orNodes);
    
    // Create CPTs for intermediary nodes
    gum::Potential<double> potU;
    // First add all AND nodes (parents)
    for (const auto& nodeId : andNodes) {
        potU.add(_bn.variable(std::to_string(nodeId)));
    }
    // Then add U node (child)
    potU.add(_bn.variable(uName));
    
    // Fill CPT: P(U|parents) = 1 only when all parents are true
    std::vector<double> cptValuesU;
    size_t combinationsU = 1 << andNodes.size();
    for (size_t i = 0; i < combinationsU; ++i) {
        // Value is 1 only when all parents are true (i.e., last combination)
        cptValuesU.push_back(i == combinationsU - 1 ? probU : 0.0);
    }
    potU.fillWith(cptValuesU);  // First fill the potential with values
    _bn.cpt(uName).fillWith(potU);  // Then copy the filled potential to the CPT

    gum::Potential<double> potV;
    // First add all OR nodes (parents)
    for (const auto& nodeId : orNodes) {
        potV.add(_bn.variable(std::to_string(nodeId)));
    }
    // Then add V node (child)
    potV.add(_bn.variable(vName));
    
    // Fill CPT: P(V|parents) = 1 when any parent is true
    std::vector<double> cptValuesV;
    size_t combinationsV = 1 << orNodes.size();
    for (size_t i = 0; i < combinationsV; ++i) {
        // Value is 0 only when no parents are true (i.e., first combination)
        cptValuesV.push_back(i == 0 ? 0.0 : probV);
    }
    potV.fillWith(cptValuesV);  // First fill the potential with values
    _bn.cpt(vName).fillWith(potV);  // Then copy the filled potential to the CPT
    
    // Create CPT for node A with P(A|u,v)
    gum::Potential<double> potA;
    // First add U and V (parents)
    potA.add(_bn.variable(uName));
    potA.add(_bn.variable(vName));
    // Then add A (child)
    potA.add(_bn.variable(std::to_string(node.id)));
    
    // Fill CPT: P(A|U,V) = 1 only when both U and V are true
    // Order of combinations: !U!V, !UV, U!V, UV
    std::vector<double> cptValuesA = {
        0.0,  // P(A|!U,!V) = 0
        0.0,  // P(A|!U,V) = 0
        0.0,  // P(A|U,!V) = 0
        1.0   // P(A|U,V) = 1
    };
    potA.fillWith(cptValuesA);  // First fill the potential with values
    _bn.cpt(std::to_string(node.id)).fillWith(potA);  // Then copy the filled potential to the CPT
}

std::set<long> BayesianNetwork::findConnectedNodes(const SituationNode& node) {
    std::set<long> connected;
    // Use aGrUM's d-connection algorithm
    gum::DAG dag = _bn.dag();
    std::string nodeId = std::to_string(node.id);
    
    // Convert DAG nodes to gum::NodeSet
    gum::NodeSet allNodes;
    for (const auto& node : dag.nodes()) {
        allNodes.insert(node);
    }
    
    // Find d-connected nodes
    gum::NodeId nodeIdNum = _bn.idFromName(nodeId);  // Convert string ID to NodeId
    gum::dSeparationAlgorithm dsep;
    for (const auto& targetId : allNodes) {
        if (targetId == nodeIdNum) continue;  // Skip self
        
        gum::NodeSet query, hardEvidence, softEvidence, requisite;
        query.insert(nodeIdNum);
        hardEvidence.insert(targetId);
        dsep.requisiteNodes(dag, query, hardEvidence, softEvidence, requisite);
        
        if (!requisite.empty()) {  // If there are requisite nodes, they are d-connected
            connected.insert(std::stol(_bn.variable(targetId).name()));
        }
    }
    
    return connected;
}

std::pair<std::set<long>, std::set<std::pair<long, long>>> 
BayesianNetwork::discoverCausalStructure(const std::map<long, SituationInstance>& instanceMap) {
    std::set<long> nodes;
    std::set<std::pair<long, long>> edges;
    
    // Step 1: Collect nodes that are NOT in UNDETERMINED state
    std::set<long> setA;
    for (const auto& pair : instanceMap) {
        if (pair.second.state != SituationInstance::UNDETERMINED) {
            setA.insert(pair.first);
        }
    }
    
    // Step 2: First iteration - collect related nodes
    std::set<long> setB;
    for (const auto& nodeId : setA) {
        const SituationNode& node = _sg.getNode(nodeId);
        for (const auto& predId : node.causes) {
            // Only add if the node is NOT in UNDETERMINED state
            auto it = instanceMap.find(predId);
            if (it != instanceMap.end() && it->second.state != SituationInstance::UNDETERMINED) {
                setB.insert(predId);
            }
        }
        for (const auto& childId : node.evidences) {
            // Only add if the node is NOT in UNDETERMINED state
            auto it = instanceMap.find(childId);
            if (it != instanceMap.end() && it->second.state != SituationInstance::UNDETERMINED) {
                setB.insert(childId);
            }
        }
    }
    
    // Step 3: Second iteration - find d-connected nodes
    std::set<long> setV;
    for (const auto& nodeId : setB) {
        const SituationNode& node = _sg.getNode(nodeId);
        // Only process if node is NOT in UNDETERMINED state
        auto it = instanceMap.find(nodeId);
        if (it != instanceMap.end() && it->second.state == SituationInstance::UNDETERMINED) {
            std::set<long> connected = findConnectedNodes(node);
            setV.insert(connected.begin(), connected.end());
        }
    }
    
    // Step 4: Collect edges between nodes in setV
    for (const auto& nodeId : setV) {
        const SituationNode& node = _sg.getNode(nodeId);
        // Check causes (incoming edges)
        for (const auto& causeId : node.causes) {
            if (setV.find(causeId) != setV.end()) {
                edges.insert(std::make_pair(causeId, nodeId));
            }
        }
        // Check evidences (outgoing edges)
        for (const auto& evidenceId : node.evidences) {
            if (setV.find(evidenceId) != setV.end()) {
                edges.insert(std::make_pair(nodeId, evidenceId));
            }
        }
    }
    
    nodes = setV;  // Final set of nodes
    return std::make_pair(nodes, edges);
}

void BayesianNetwork::calculateBeliefs(std::map<long, SituationInstance>& instanceMap) {
    // First pass: Set evidence for TRIGGERED and UNTRIGGERED nodes
    for (const auto& pair : instanceMap) {
        if (pair.second.state != SituationInstance::UNDETERMINED) {
            _inferenceEngine->addEvidence(std::to_string(pair.first),
                                        pair.second.state == SituationInstance::TRIGGERED ? 1 : 0);
        }
    }
    
    // Perform inference once with all evidence
    _inferenceEngine->makeInference();
    
    // Second pass: Calculate posteriors only for UNDETERMINED nodes
    for (auto& pair : instanceMap) {
        if (pair.second.state == SituationInstance::UNDETERMINED) {
            const SituationNode& node = _sg.getNode(pair.first);
            gum::Potential<double> posterior = _inferenceEngine->posterior(std::to_string(pair.first));
            
            // Create instantiation for the posterior
            gum::Instantiation inst(posterior);
            inst.chgVal(0, 1);  // Set to true state
            pair.second.beliefValue = posterior.get(inst);
            
            // Update state based on belief and other conditions
            if (determineNodeState(node, pair.second, instanceMap)) {
                pair.second.state = SituationInstance::TRIGGERED;
                pair.second.counter++;
            } else {
                pair.second.state = SituationInstance::UNTRIGGERED;
            }
        }
    }
    
    // Clear all evidence for next iteration
    _inferenceEngine->eraseAllEvidence();
}

bool BayesianNetwork::determineNodeState(const SituationNode& node, SituationInstance& instance,
                                       const std::map<long, SituationInstance>& instanceMap) {
    // Check if belief meets threshold
    if (instance.beliefValue < instance.beliefThreshold) {
        return false;
    }
    
    // Check counter conditions
    for (const auto& childId : node.evidences) {
        const auto& childInstance = instanceMap.find(childId);
        if (childInstance != instanceMap.end() && 
            instance.counter < childInstance->second.counter) {
            return true;
        }
    }
    
    return false;
}

void BayesianNetwork::updateRefinement(const SituationGraph& graph, 
                                     std::map<long, SituationInstance>& instanceMap) {
    _sg = graph;  // Store the current graph
    
    // Step 1: Construct CPTs for all nodes
    for (const auto& pair : instanceMap) {
        const SituationNode& node = _sg.getNode(pair.first);
        constructCPT(node, instanceMap);
    }
    
    // Step 2: Discover causal structure
    auto [nodes, edges] = discoverCausalStructure(instanceMap);
    
    // Step 3: Calculate beliefs and update states
    calculateBeliefs(instanceMap);
}
