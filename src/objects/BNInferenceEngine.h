#ifndef BNINFENGINE_H_
#define BNINFENGINE_H_

#include <memory>
#include <string>
#include <map>
#include <vector>
#include <set>
#include "SituationGraph.h"
#include "SituationInstance.h"
#include <dlib/directed_graph.h>
#include <dlib/graph_utils.h>
#include <dlib/bayes_utils.h>
#include <dlib/set.h>

using namespace dlib;
using namespace dlib::bayes_node_utils;

// Custom comparison operators for dlib sets
namespace dlib {
    template<typename T, typename alloc>
    bool operator==(const set_kernel_1<T,alloc>& lhs, const set_kernel_1<T,alloc>& rhs) {
        if (lhs.size() != rhs.size()) return false;
        
        // Manual element comparison since we can't use iterators directly
        bool equal = true;
        lhs.reset();
        T item;
        while (lhs.move_next()) {
            item = lhs.element();
            if (!rhs.is_member(item)) {
                equal = false;
                break;
            }
        }
        return equal;
    }
    
    template<typename T, typename alloc>
    bool operator<(const set_kernel_1<T,alloc>& lhs, const set_kernel_1<T,alloc>& rhs) {
        if (lhs.size() >= rhs.size()) return false;
        
        bool is_subset = true;
        lhs.reset();
        T item;
        while (lhs.move_next()) {
            item = lhs.element();
            if (!rhs.is_member(item)) {
                is_subset = false;
                break;
            }
        }
        return is_subset;
    }
}

class BNInferenceEngine {
private:
    typedef dlib::set<unsigned long>::kernel_1a set_type;
    typedef dlib::directed_graph<bayes_node>::kernel_1a bn_type;
    typedef dlib::graph_kernel_1<set_type, set_type> join_tree_type;
    
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
    std::unique_ptr<dlib::set<long>::kernel_1a> findConnectedNodes(const SituationNode& node);
    /**
     * Find causally connected nodes in the Bayesian network based on TRIGGERED states.
     * This method identifies nodes that are causally connected through causes and evidences,
     * and also discovers d-connections between nodes.
     *
     * @param instanceMap Map of situation instances
     * @return A pair of sets: (nodes, edges) where nodes are causally connected nodes
     *         and edges represent the causal connections between them
     */
    std::pair<std::unique_ptr<dlib::set<long>::kernel_1a>, std::unique_ptr<dlib::set<std::pair<long, long>>::kernel_1a>>
    findCausallyConnectedNodes(const std::map<long, SituationInstance>& instanceMap);

    /**
     * Discover causal structure in the Bayesian network.
     * @deprecated Use findCausallyConnectedNodes instead
     */
    std::pair<dlib::set<long>::kernel_1a, dlib::set<std::pair<long, long>>::kernel_1a>
    discoverCausalStructure(const std::map<long, SituationInstance>& instanceMap);

    // void calculateBeliefs(std::map<long, SituationInstance>& instanceMap);

public:
    BNInferenceEngine();
    virtual ~BNInferenceEngine();
    
    void loadModel(SituationGraph sg);
    void reason(SituationGraph sg,
               std::map<long, SituationInstance> &instanceMap,
               simtime_t current);
};

#endif /* BNINFENGINE_H_ */
