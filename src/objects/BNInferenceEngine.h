#ifndef OBJECTS_BNINFERENCEENGINE_H_
#define OBJECTS_BNINFERENCEENGINE_H_

#include <dlib/bayes_utils.h>
#include <dlib/graph.h>
#include <dlib/directed_graph.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <omnetpp.h>
#include "SituationGraph.h"
#include "SituationInstance.h"

using namespace omnetpp;

class BNInferenceEngine {
private:
    // Type definitions using dlib
    using bn_type = dlib::directed_graph<dlib::bayes_node>::kernel_1a_c;
    using join_tree_type = dlib::graph<dlib::set<unsigned long>::compare_1b_c, dlib::set<unsigned long>::compare_1b_c>::kernel_1a_c;
    
    // Core data structures
    bn_type _bn;  // Bayesian network
    std::unique_ptr<join_tree_type> _joinTree;  // Join tree for inference
    std::map<std::string, unsigned long> _nodeMap;  // Maps node names to indices
    std::map<std::string, size_t> _evidence;  // Current evidence

    // Helper methods
    void buildJoinTree();
    void addNode(const std::string& name);
    void addEdge(const std::string& parent, const std::string& child);
    void setEvidence(const std::string& nodeName, size_t value);
    std::vector<double> getPosterior(const std::string& nodeName);
    void performInference();
    
    // Utility methods
    std::vector<unsigned long> getParents(unsigned long node_idx) const;
    std::vector<unsigned long> getChildren(unsigned long node_idx) const;
    double calculateMutualInformation(unsigned long node1, unsigned long node2,
                                    const std::map<long, SituationInstance>& instanceMap);
    void pruneWeakEdges(double threshold = 0.1);

public:
    void loadModel(SituationGraph sg);
    void reason(SituationGraph sg,
            std::map<long, SituationInstance> &instanceMap, simtime_t current);
    BNInferenceEngine();
    virtual ~BNInferenceEngine();
};

#endif /* OBJECTS_BNINFERENCEENGINE_H_ */
