#ifndef OBJECTS_BNINFERENCEENGINE_H_
#define OBJECTS_BNINFERENCEENGINE_H_

#include <dlib/bayes_utils.h>
#include <dlib/graph_utils.h>
#include <dlib/graph.h>
#include <dlib/directed_graph.h>
#include <dlib/set.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <omnetpp.h>
#include "SituationGraph.h"
#include "SituationInstance.h"

using namespace omnetpp;
using namespace dlib::bayes_node_utils;

class BNInferenceEngine {
private:
    // Type definitions using dlib
    using bn_type = dlib::directed_graph<dlib::bayes_node>::kernel_1a;
    using join_tree_type = dlib::graph<dlib::matrix<double>>::kernel_1a;
    using set_type = dlib::set<unsigned long>::kernel_1a_c;
    
    // Core data structures
    bn_type _bn;  // Bayesian network
    std::unique_ptr<join_tree_type> _joinTree;  // Join tree for inference
    std::map<std::string, unsigned long> _nodeMap;  // Maps node names to indices

    // Helper methods
    void buildJoinTree();
    void addNode(const std::string& name, const SituationNode& node);
    void addEdge(const std::string& parent, const std::string& child, double weight);
    std::vector<double> getPosterior(const std::string& nodeName);
    void setNodeValue(const std::string& nodeName, unsigned long value);
    void setNodeAsEvidence(const std::string& nodeName);
    
    // Utility methods
    std::vector<unsigned long> getParents(unsigned long node_idx) const;
    std::vector<unsigned long> getChildren(unsigned long node_idx) const;

public:
    void loadModel(SituationGraph sg);
    void reason(SituationGraph sg,
            std::map<long, SituationInstance> &instanceMap, simtime_t current);
    BNInferenceEngine();
    virtual ~BNInferenceEngine();
};

#endif /* OBJECTS_BNINFERENCEENGINE_H_ */
