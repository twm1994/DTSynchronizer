#ifndef BNINFENGINE_H_
#define BNINFENGINE_H_

#include <memory>
#include <string>
#include <map>
#include <vector>
#include <set>
#include <dlib/directed_graph.h>
#include <dlib/bayes_utils.h>
#include <dlib/graph_utils.h>
#include "SituationGraph.h"
#include "SituationInstance.h"

using namespace dlib;
using namespace dlib::bayes_node_utils;

class BNInferenceEngine {
private:
    typedef dlib::directed_graph<bayes_node>::kernel_1a bn_type;
    typedef dlib::directed_graph<std::set<unsigned long> >::kernel_1a join_tree_type;
    
    SituationGraph _sg;
    std::unique_ptr<bn_type> _bn;
    std::map<std::string, unsigned long> _nodeMap;
    std::unique_ptr<join_tree_type> _joinTree;
    
    void addNode(const std::string& name, const SituationNode& node);
    void addEdge(const std::string& parentName, const std::string& childName, double weight);
    void buildJoinTree();
    void setNodeValue(const std::string& name, unsigned long value);
    void setNodeAsEvidence(const std::string& name);
    std::vector<double> getPosterior(const std::string& name);
    std::vector<unsigned long> getParents(unsigned long nodeIdx) const;
    std::vector<unsigned long> getChildren(unsigned long nodeIdx) const;
    double normalizeWeight(double weight) const;
    bool areNodesConnected(unsigned long node1, unsigned long node2) const;
    void handleMixedRelations(const SituationNode& node,
                            const std::vector<long>& andNodes,
                            const std::vector<long>& orNodes);
    double calculateAndProbability(const SituationNode& node,
                                 const std::vector<long>& nodes);
    double calculateOrProbability(const SituationNode& node,
                                const std::vector<long>& nodes);
    void constructCPT(const SituationNode& node,
                     const std::map<long, SituationInstance>& instanceMap);
    bool determineNodeState(const SituationNode& node,
                          SituationInstance& instance,
                          const std::map<long, SituationInstance>& instanceMap);
    std::set<long> findConnectedNodes(const SituationNode& node);
    std::pair<std::set<long>, std::set<std::pair<long, long>>>
    discoverCausalStructure(const std::map<long, SituationInstance>& instanceMap);
    void calculateBeliefs(std::map<long, SituationInstance>& instanceMap);

public:
    BNInferenceEngine();
    virtual ~BNInferenceEngine();
    
    void loadModel(SituationGraph sg);
    void reason(SituationGraph sg,
               std::map<long, SituationInstance> &instanceMap,
               simtime_t current);
};

#endif /* BNINFENGINE_H_ */
