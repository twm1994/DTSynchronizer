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

#include "../common/Util.h"
#include "SituationReasoner.h"

SituationReasoner::SituationReasoner() :
        SituationEvolution() {
}

SituationReasoner::~SituationReasoner() {
}

void SituationReasoner::initializeLogger(const std::string& logBasePath) {
    logger = std::make_unique<ReasonerLogger>(logBasePath);
    
    // Initialize instances for all nodes in the graph starting from bottom layer
    for (int layer = sg.modelHeight() - 1; layer >= 0; layer--) {
        DirectedGraph currentLayer = sg.getLayer(layer);
        std::vector<long> nodes = currentLayer.topo_sort();
        for (long nodeId : nodes) {
            if (instanceMap.find(nodeId) == instanceMap.end()) {
                addInstance(nodeId);
            }
        }
    }
}

void SituationReasoner::beliefPropagation(SituationGraph& graph) {
    // Process each layer from bottom to top in the situation graph
    // Evidence nodes are in lower layers, hypothesis nodes are in upper layers
    int numLayers = graph.modelHeight();
    
    for (int layer = numLayers - 1; layer >= 0; layer--) {
        DirectedGraph currentLayer = graph.getLayer(layer);
        std::vector<long> nodes = currentLayer.topo_sort();
        
        // Process each node in the current layer as a hypothesis
        for (long nodeId : nodes) {
            const SituationNode& hypothesisNode = graph.getNode(nodeId);
            SituationInstance& hypothesisInstance = instanceMap[nodeId];
            
            // Collect evidence nodes from the layer below (type-V relations)
            std::vector<long> evidenceNodes;
            for (long evidenceId : hypothesisNode.evidences) {
                const SituationRelation* relation = graph.getRelation(nodeId, evidenceId);
                if (relation && relation->type == SituationRelation::V) {
                    evidenceNodes.push_back(evidenceId);
                }
            }
            
            // Case 1: Base hypothesis (no evidence nodes)
            // Set belief to expert-defined measure
            if (evidenceNodes.empty()) {
                hypothesisInstance.beliefValue = 0.8;  // Expert-defined measure
            }
            // Case 2: Single evidence with SOLE relation
            // Belief = evidence_belief * relation_weight
            else if (evidenceNodes.size() == 1) {
                long evidenceId = evidenceNodes[0];
                const SituationRelation* relation = graph.getRelation(nodeId, evidenceId);
                if (relation && relation->relation == SituationRelation::SOLE) {
                    SituationInstance& evidenceInstance = instanceMap[evidenceId];
                    hypothesisInstance.beliefValue = evidenceInstance.beliefValue * relation->weight;
                }
            }
            // Cases 3 & 4: Multiple evidence nodes with OR or AND relations
            else {
                bool allOr = true;
                bool allAnd = true;
                
                // Verify relation types
                for (long evidenceId : evidenceNodes) {
                    const SituationRelation* relation = graph.getRelation(nodeId, evidenceId);
                    if (relation) {
                        if (relation->relation != SituationRelation::OR) allOr = false;
                        if (relation->relation != SituationRelation::AND) allAnd = false;
                    }
                }
                
                // Case 3: All OR relations
                // Belief = max(evidence_belief * relation_weight)
                if (allOr) {
                    double maxWeightedBelief = 0.0;
                    for (long evidenceId : evidenceNodes) {
                        const SituationRelation* relation = graph.getRelation(nodeId, evidenceId);
                        if (relation) {
                            SituationInstance& evidenceInstance = instanceMap[evidenceId];
                            double weightedBelief = evidenceInstance.beliefValue * relation->weight;
                            maxWeightedBelief = std::max(maxWeightedBelief, weightedBelief);
                        }
                    }
                    hypothesisInstance.beliefValue = maxWeightedBelief;
                }
                // Case 4: All AND relations
                // Belief = Dempster's combination of weighted beliefs
                else if (allAnd) {
                    // Start with first evidence's weighted belief
                    long firstEvidenceId = evidenceNodes[0];
                    const SituationRelation* firstRelation = graph.getRelation(nodeId, firstEvidenceId);
                    SituationInstance& firstEvidence = instanceMap[firstEvidenceId];
                    double combinedBelief = firstEvidence.beliefValue * firstRelation->weight;
                    
                    // Combine remaining evidence beliefs using Dempster's rule
                    for (size_t i = 1; i < evidenceNodes.size(); i++) {
                        long evidenceId = evidenceNodes[i];
                        const SituationRelation* relation = graph.getRelation(nodeId, evidenceId);
                        if (!relation) continue;
                        
                        SituationInstance& evidenceInstance = instanceMap[evidenceId];
                        double weightedBelief = evidenceInstance.beliefValue * relation->weight;
                        
                        // k = conflict measure between beliefs
                        double k = (1 - combinedBelief) * weightedBelief + combinedBelief * (1 - weightedBelief);
                        
                        // m₁₂(A) = (m₁(A) × m₂(A)) / (1 - k)
                        if (k < 1.0) {
                            combinedBelief = (combinedBelief * weightedBelief) / (1 - k);
                        } else {
                            combinedBelief = 0.0;  // Complete conflict case
                            break;
                        }
                    }
                    hypothesisInstance.beliefValue = combinedBelief;
                }
            }
            
            // Log belief update
            if (logger) {
                logger->logStep("beliefPropagation", current, nodeId,
                              hypothesisInstance.beliefValue, 1.0,
                              convertMapToVector(hypothesisInstance.childrenBeliefs),
                              convertMapToVector(hypothesisInstance.predecessorBeliefs),
                              hypothesisInstance.state);
            }
        }
    }
}

void SituationReasoner::backwardRetrospection(SituationGraph& graph) {
    int numLayers = graph.modelHeight();
    
    // Process each layer from top to bottom
    for (int layer = 0; layer < numLayers; layer++) {
        DirectedGraph currentLayer = graph.getLayer(layer);
        std::vector<long> nodes = currentLayer.topo_sort();
        
        // First iteration: collect triggered situations in this layer
        std::vector<long> triggeredEffects;
        for (long nodeId : nodes) {
            SituationInstance& instance = instanceMap[nodeId];
            if (instance.state == SituationInstance::TRIGGERED) {
                triggeredEffects.push_back(nodeId);
                // Update state buffer with current state
                instance.addStateToBuffer(instance.state);
            } else {
                instance.addStateToBuffer(SituationInstance::UNTRIGGERED);
            }
        }
        
        // Second and third iterations
        while (!triggeredEffects.empty()) {
            // Get and remove last triggered effect
            long effectId = triggeredEffects.back();
            triggeredEffects.pop_back();
            
            const SituationNode& effectNode = graph.getNode(effectId);
            
            // Second iteration: collect cause situations with horizontal relations
            std::vector<long> causeSituations;
            for (long causeId : effectNode.causes) {
                const SituationRelation* relation = graph.getRelation(causeId, effectId);
                if (relation && relation->type == SituationRelation::H) {
                    causeSituations.push_back(causeId);
                }
            }
            
            // Third iteration: process cause situations
            if (!causeSituations.empty()) {
                for (long causeId : causeSituations) {
                    SituationInstance& causeInstance = instanceMap[causeId];
                    
                    if (causeInstance.state == SituationInstance::UNTRIGGERED) {
                        // Determine state based on effect node
                        SituationInstance::State newState = determineState(causeId, effectId, graph);
                        causeInstance.addStateToBuffer(newState);
                        
                        if (newState == SituationInstance::TRIGGERED) {
                            triggeredEffects.push_back(causeId);
                        }
                        
                        if (logger) {
                            logger->logStep("backwardRetrospection: state determined", 
                                          current, causeId,
                                          causeInstance.beliefValue, 1.0,
                                          convertMapToVector(causeInstance.childrenBeliefs),
                                          convertMapToVector(causeInstance.predecessorBeliefs),
                                          newState);
                        }
                    } else if (causeInstance.state == SituationInstance::TRIGGERED) {
                        triggeredEffects.push_back(causeId);
                    }
                }
            }
            
            if (logger) {
                SituationInstance& instance = instanceMap[effectId];
                SituationInstance::State lastState = instance.stateBuffer.empty() ? 
                    instance.state : instance.stateBuffer.back();
                logger->logStep("backwardRetrospection", current, effectId,
                              instance.beliefValue, 1.0,
                              convertMapToVector(instance.childrenBeliefs),
                              convertMapToVector(instance.predecessorBeliefs),
                              lastState);
            }
        }
    }
}

void SituationReasoner::downwardRetrospection(SituationGraph& graph) {
    int numLayers = graph.modelHeight();
    
    // Process each layer from top to bottom
    for (int layer = 0; layer < numLayers; layer++) {
        DirectedGraph currentLayer = graph.getLayer(layer);
        std::vector<long> nodes = currentLayer.topo_sort();
        
        // First iteration: collect triggered situations in this layer
        std::vector<long> triggeredSituations;
        for (long nodeId : nodes) {
            SituationInstance& instance = instanceMap[nodeId];
            if (instance.state == SituationInstance::TRIGGERED) {
                triggeredSituations.push_back(nodeId);
                // Initialize state buffer with current state
                instance.addStateToBuffer(instance.state);
            } else {
                instance.addStateToBuffer(SituationInstance::UNTRIGGERED);
            }
        }
        
        // Second and third iterations
        while (!triggeredSituations.empty()) {
            // Get and remove last triggered situation
            long parentId = triggeredSituations.back();
            triggeredSituations.pop_back();
            
            const SituationNode& parentNode = graph.getNode(parentId);
            
            // Second iteration: collect child situations with vertical relations
            std::vector<long> childSituations;
            for (long childId : parentNode.evidences) {
                const SituationRelation* relation = graph.getRelation(parentId, childId);
                if (relation && relation->type == SituationRelation::V) {
                    childSituations.push_back(childId);
                }
            }
            
            // Third iteration: process child situations
            if (!childSituations.empty()) {
                for (long childId : childSituations) {
                    SituationInstance& childInstance = instanceMap[childId];
                    
                    // Determine state based on parent node
                    SituationInstance::State newState = determineChildState(parentId, childId, graph);
                    childInstance.addStateToBuffer(newState);
                    
                    if (logger) {
                        logger->logStep("downwardRetrospection: state determined", 
                                      current, childId,
                                      childInstance.beliefValue, 1.0,
                                      convertMapToVector(childInstance.childrenBeliefs),
                                      convertMapToVector(childInstance.predecessorBeliefs),
                                      newState);
                    }
                }
            }
            
            if (logger) {
                SituationInstance& instance = instanceMap[parentId];
                SituationInstance::State lastState = instance.stateBuffer.empty() ? 
                    instance.state : instance.stateBuffer.back();
                logger->logStep("downwardRetrospection", current, parentId,
                              instance.beliefValue, 1.0,
                              convertMapToVector(instance.childrenBeliefs),
                              convertMapToVector(instance.predecessorBeliefs),
                              lastState);
            }
        }
    }
}

SituationInstance::State SituationReasoner::determineState(long causeId, long effectId, SituationGraph& graph) {
    // const SituationNode& causeNode = graph.getNode(causeId);
    SituationInstance& effectInstance = instanceMap[effectId];
    
    // Condition 1: Effect node must be TRIGGERED
    if (effectInstance.state != SituationInstance::TRIGGERED) {
        return SituationInstance::UNDETERMINED;
    }
    
    // Get all effects with type-H relations from this cause
    std::vector<long> effects;
    std::vector<SituationRelation::Relation> effectRelations;
    for (const auto& [nodeId, relation] : graph.getOutgoingRelations(causeId)) {
        if (relation.type == SituationRelation::H) {
            effects.push_back(nodeId);
            effectRelations.push_back(relation.relation);
        }
    }
    
    // Check conditions 2.1, 2.2, and 2.3
    bool condition2_1 = false;  // This cause is the SOLE cause of the input effect
    bool condition2_2 = false;  // All effects of this cause have OR relations
    bool condition2_3 = false;  // All effects have AND relations and non-input effects are UNTRIGGERED
    
    // Condition 2.1: Check if this cause is the SOLE cause of the input effect
    const SituationNode& effectNode = graph.getNode(effectId);
    bool isSoleCause = true;
    for (long otherCauseId : effectNode.causes) {
        if (otherCauseId != causeId) {
            const SituationRelation* relation = graph.getRelation(otherCauseId, effectId);
            if (relation && relation->type == SituationRelation::H) {
                isSoleCause = false;
                break;
            }
        }
    }
    condition2_1 = isSoleCause;
    
    // Check if all relations are of the same type
    bool allOr = true;
    bool allAnd = true;
    for (auto rel : effectRelations) {
        if (rel != SituationRelation::OR) allOr = false;
        if (rel != SituationRelation::AND) allAnd = false;
    }
    
    // Condition 2.2: All effects of this cause have OR relations
    condition2_2 = allOr;
    
    // Condition 2.3: All effects have AND relations and non-input effects are UNTRIGGERED
    if (allAnd) {
        bool allOtherEffectsUntriggered = true;
        for (long otherEffectId : effects) {
            if (otherEffectId != effectId) {
                SituationInstance& otherEffectInstance = instanceMap[otherEffectId];
                if (otherEffectInstance.state != SituationInstance::UNTRIGGERED) {
                    allOtherEffectsUntriggered = false;
                    break;
                }
            }
        }
        condition2_3 = allOtherEffectsUntriggered;
    }
    
    // Final condition: Condition 1 AND (Condition 2.1 OR Condition 2.2 OR Condition 2.3)
    // Note: Condition 1 is already checked at the start
    if (condition2_1 || condition2_2 || condition2_3) {
        return SituationInstance::TRIGGERED;
    }
    
    return SituationInstance::UNDETERMINED;
}

SituationInstance::State SituationReasoner::determineChildState(long parentId, long childId, SituationGraph& graph) {
    const SituationNode& parentNode = graph.getNode(parentId);
    SituationInstance& parentInstance = instanceMap[parentId];
    
    // Condition 1: Parent node must be TRIGGERED
    if (parentInstance.state != SituationInstance::TRIGGERED) {
        return SituationInstance::UNDETERMINED;
    }
    
    // Get all V-type child relations
    std::vector<long> vChildren;
    std::vector<SituationRelation::Relation> vRelations;
    for (long evId : parentNode.evidences) {
        const SituationRelation* relation = graph.getRelation(parentId, evId);
        if (relation && relation->type == SituationRelation::V) {
            vChildren.push_back(evId);
            vRelations.push_back(relation->relation);
        }
    }
    
    // Check conditions 2.a, 2.b, and 2.c
    bool condition2a = false;  // Node B is the only child
    bool condition2b = false;  // All OR relations and others UNTRIGGERED
    bool condition2c = false;  // All AND relations and others TRIGGERED
    
    // Condition 2.a: Node B is the only child
    condition2a = (vChildren.size() == 1 && vChildren[0] == childId);
    
    // Check if all relations are of the same type
    bool allOr = true;
    bool allAnd = true;
    for (auto rel : vRelations) {
        if (rel != SituationRelation::OR) allOr = false;
        if (rel != SituationRelation::AND) allAnd = false;
    }
    
    // Condition 2.b: All relations are OR (conjunction) and others UNTRIGGERED
    if (allOr) {
        bool allOthersUntriggered = true;
        for (long evId : vChildren) {
            if (evId != childId) {
                SituationInstance& evInstance = instanceMap[evId];
                if (evInstance.state != SituationInstance::UNTRIGGERED) {
                    allOthersUntriggered = false;
                    break;
                }
            }
        }
        condition2b = allOthersUntriggered;
    }
    
    // Condition 2.c: All relations are AND (disjunction) and others TRIGGERED
    if (allAnd) {
        bool allOthersTriggered = true;
        for (long evId : vChildren) {
            if (evId != childId) {
                SituationInstance& evInstance = instanceMap[evId];
                if (evInstance.state != SituationInstance::TRIGGERED) {
                    allOthersTriggered = false;
                    break;
                }
            }
        }
        condition2c = allOthersTriggered;
    }
    
    // Final condition: Condition 1 AND (Condition 2.a OR Condition 2.b OR Condition 2.c)
    // Note: Condition 1 is already checked at the start
    if (condition2a || condition2b || condition2c) {
        return SituationInstance::TRIGGERED;
    }
    
    return SituationInstance::UNDETERMINED;
}

std::vector<double> SituationReasoner::convertMapToVector(const std::map<long, double>& beliefMap) {
    std::vector<double> result;
    for (const auto& [id, belief] : beliefMap) {
        result.push_back(belief);
    }
    return result;
}

void SituationReasoner::logCausalReasoning(const SituationInstance& causeInstance,
                                         const SituationInstance& effectInstance,
                                         double beliefValue) {
    logger->logCausalReasoning(causeInstance.id, effectInstance.id, beliefValue, simTime());
}

void SituationReasoner::logInstanceState(const SituationInstance& instance) {
    logger->logInstanceState(instance.id,
                           convertMapToVector(instance.childrenBeliefs),
                           convertMapToVector(instance.predecessorBeliefs),
                           instance.state,
                           simTime());
}

SituationInstance::State SituationReasoner::combineStates(std::vector<SituationInstance::State>& stateBuffer) {
    if (stateBuffer.empty()) {
        return SituationInstance::UNTRIGGERED;
    }
    
    // Take first element as initial temporary result
    SituationInstance::State tr = stateBuffer[0];
    
    // Process all remaining states in buffer
    for (size_t i = 1; i < stateBuffer.size(); i++) {
        SituationInstance::State currentState = stateBuffer[i];
        
        // Rule 1: If one is TRIGGERED, result is TRIGGERED
        if (tr == SituationInstance::TRIGGERED || currentState == SituationInstance::TRIGGERED) {
            tr = SituationInstance::TRIGGERED;
        }
        // Rule 2: If both are UNDETERMINED, result is UNDETERMINED
        else if (tr == SituationInstance::UNDETERMINED && currentState == SituationInstance::UNDETERMINED) {
            tr = SituationInstance::UNDETERMINED;
        }
        // Rule 3: If one is UNDETERMINED and other is UNTRIGGERED, result is UNTRIGGERED
        else if ((tr == SituationInstance::UNDETERMINED && currentState == SituationInstance::UNTRIGGERED) ||
                 (tr == SituationInstance::UNTRIGGERED && currentState == SituationInstance::UNDETERMINED)) {
            tr = SituationInstance::UNTRIGGERED;
        }
        // Both UNTRIGGERED case - result stays UNTRIGGERED
    }
    
    return tr;
}

std::set<long> SituationReasoner::reason(std::set<long> triggered, simtime_t current) {
    std::set<long> tOperational;
    
    // Create a working copy of the situation graph
    SituationGraph workingGraph = sg;

//    cout << "show triggered: ";
//    util::printSet(triggered);

    int numOfLayers = sg.modelHeight();
    // trigger bottom layer situations
    DirectedGraph g = sg.getLayer(numOfLayers - 1);
    std::vector<long> bottoms = g.topo_sort();
    for (auto bottom : bottoms) {
        SituationInstance &instance = instanceMap[bottom];
        auto it = triggered.find(bottom);
        if (it != triggered.end()) {
            instance.state = SituationInstance::TRIGGERED;
            instance.counter++;
            instance.next_start = current;
        }
    }
    for (int i = workingGraph.modelHeight() - 1; i > 0; i--) {
        DirectedGraph g1 = sg.getLayer(i - 1);
        std::vector<long> uppers = g1.topo_sort();
        for (auto upper : uppers) {
            SituationInstance &instance = instanceMap[upper];
            SituationNode node = sg.getNode(instance.id);
            bool toTrigger = true;
            for (auto evidence : node.evidences) {
                SituationInstance &es = instanceMap[evidence];
                if (es.counter <= instance.counter) {
                    toTrigger = false;
                    break;
                }
            }
            if (toTrigger) {
                instance.state = SituationInstance::TRIGGERED;
                instance.counter++;
                instance.next_start = current;
            }
        }
    }

    // Run belief propagation and retrospection
    // beliefPropagation(workingGraph);
    // backwardRetrospection(workingGraph);
    // downwardRetrospection(workingGraph);
    
    // Combine states from buffer for each situation
    // for (auto& [id, instance] : instanceMap) {
    //     instance.state = combineStates(instance.stateBuffer);
    //     // Clear buffer after combining
    //     instance.stateBuffer.clear();
    // }

    // compute UNDETERMINED state
    for (int i = 0; i < sg.modelHeight(); i++) {
        DirectedGraph g = sg.getLayer(i);
        std::vector<long> sortedNodes = g.topo_sort();
        std::reverse(sortedNodes.begin(), sortedNodes.end());
        for (auto node : sortedNodes) {
            SituationInstance &si = instanceMap[node];
            if (si.state == SituationInstance::TRIGGERED
                    || si.state == SituationInstance::UNDETERMINED) {
                std::vector<long> causes = sg.getNode(node).causes;
                for (auto cause : causes) {
                    SituationInstance &ci = instanceMap[cause];
                    if (ci.state != SituationInstance::TRIGGERED) {
                        ci.state = SituationInstance::UNDETERMINED;
//                        cout << "situation " << ci.id << " is undetermined" << endl;
                    }
                }
            }
        }
    }

    // update refinement
    BNInferenceEngine engine;
    engine.loadModel(sg);
    engine.reason(sg, instanceMap, current);

    // get operational situations from the bottom layer
    for (auto bottom : bottoms) {
        SituationInstance& instance = instanceMap[bottom];
        if (instance.state == SituationInstance::TRIGGERED && instance.next_start == current) {
            tOperational.insert(instance.id);
        }
    }
    
    checkState(current);

    // reset transient situations
    for (auto &si : instanceMap) {
        if (si.second.next_start + si.second.duration <= current) {
            si.second.state = SituationInstance::UNTRIGGERED;
        }
    }

//    cout << "print situation graph instance" << endl;
//    print();

    return tOperational;
}

void SituationReasoner::checkState(simtime_t current) {
    // reset transient situations
    for (auto si : instanceMap) {
        if (si.second.next_start + si.second.duration <= current) {
            si.second.state = SituationInstance::UNTRIGGERED;
        }
    }
}

void SituationReasoner::updateRefinement(SituationGraph& graph) {
    // Create a Bayesian Network and perform update refinement
//    BayesianNetwork bn;
//    bn.updateRefinement(graph, instanceMap);
}
