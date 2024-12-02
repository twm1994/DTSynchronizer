#ifndef BAYESIAN_NETWORK_H
#define BAYESIAN_NETWORK_H

// Standard library includes
#include <cstdio>
#include <iostream>
#include <memory>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// Platform-specific includes
#include "../common/PlatformDefs.h"

// Boost includes
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

// aGrUM includes
#include <agrum/BN/BayesNet.h>
#include <agrum/BN/inference/lazyPropagation.h>
#include <agrum/BN/inference/tools/BayesNetInference.h>
#include <agrum/BN/io/BIF/BIFReader.h>
#include <agrum/BN/io/BIF/BIFWriter.h>
#include <agrum/BN/algorithms/dSeparationAlgorithm.h>
#include <agrum/tools/core/types.h>
#include <agrum/tools/multidim/potential.h>
#include <agrum/tools/variables/discretizedVariable.h>
#include <agrum/tools/variables/labelizedVariable.h>

// Project includes
#include "SituationEvolution.h"
#include "SituationGraph.h"

// Abstract base class for inference engines
template<typename T = double>
class InferenceEngine {
public:
    virtual ~InferenceEngine() = default;
    virtual void makeInference() = 0;
    virtual gum::Potential<T> posterior(const std::string& nodeName) = 0;
    virtual void addEvidence(const std::string& nodeName, size_t val) = 0;
    virtual void eraseEvidence(const std::string& nodeName) = 0;
    virtual void eraseAllEvidence() = 0;
};

// Concrete implementation for Lazy Propagation
template<typename T = double>
class LazyPropagationEngine : public InferenceEngine<T> {
public:
    explicit LazyPropagationEngine(const gum::BayesNet<T>& bn) 
        : _inference(&bn) {}
    
    void makeInference() override { _inference.makeInference(); }
    
    gum::Potential<T> posterior(const std::string& nodeName) override {
        _inference.makeInference();
        return _inference.posterior(nodeName);
    }
    
    void addEvidence(const std::string& nodeName, size_t val) override {
        _inference.addEvidence(nodeName, val);
    }
    
    void eraseEvidence(const std::string& nodeName) override {
        _inference.eraseEvidence(nodeName);
    }
    
    void eraseAllEvidence() override {
        _inference.eraseAllEvidence();
    }

private:
    gum::LazyPropagation<T> _inference;
};

class BayesianNetwork {
public:
    BayesianNetwork();
    ~BayesianNetwork();

    // Network creation and modification
    void createNetworkFromSituationGraph(const std::string& jsonFile);
    void createNetworkFromSituationGraph(const SituationGraph& graph);
    
    // Inference methods
    template<typename Engine>
    void setInferenceEngine();
    void performInference();
    void addEvidence(const std::string& nodeName, size_t value);
    void eraseEvidence(const std::string& nodeName);
    void eraseAllEvidence();
    gum::Potential<double> getPosterior(const std::string& nodeName);
    
    // File I/O
    void loadNetwork(const std::string& filename);
    void saveNetwork(const std::string& filename);
    void loadFromBIF(const std::string& filename);
    void saveToBIF(const std::string& filename);
    
    // Refinement methods
    void updateRefinement(const SituationGraph& graph, std::map<long, SituationInstance>& instanceMap);

private:
    gum::BayesNet<double> _bn;
    std::unique_ptr<InferenceEngine<double>> _inferenceEngine;
    SituationGraph _sg;
    SituationEvolution _se;

    // Helper methods for CPT construction
    void constructCPT(const SituationNode& node, const std::map<long, SituationInstance>& instanceMap);
    double calculateAndProbability(const SituationNode& node, const std::vector<long>& nodes);
    double calculateOrProbability(const SituationNode& node, const std::vector<long>& nodes);
    void handleMixedRelations(const SituationNode& node, const std::vector<long>& andNodes, 
                             const std::vector<long>& orNodes);
    
    // Helper methods for causal structure discovery
    std::pair<std::set<long>, std::set<std::pair<long, long>>> 
    discoverCausalStructure(const std::map<long, SituationInstance>& instanceMap);
    std::set<long> findConnectedNodes(const SituationNode& node);
    
    // Helper methods for belief calculation
    void calculateBeliefs(std::map<long, SituationInstance>& instanceMap);
    bool determineNodeState(const SituationNode& node, SituationInstance& instance, 
                          const std::map<long, SituationInstance>& instanceMap);
    
    // Helper methods for JSON parsing
    void parseNode(const boost::property_tree::ptree& node);
    void parseEdges(const boost::property_tree::ptree& node);
    void setCPT(const std::string& nodeName, const boost::property_tree::ptree& node);
};

// Template method implementation
template<typename Engine>
void BayesianNetwork::setInferenceEngine() {
    _inferenceEngine = std::make_unique<Engine>(_bn);
}

#endif // BAYESIAN_NETWORK_H
