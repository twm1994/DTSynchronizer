#ifndef BNINFENGINE_H_
#define BNINFENGINE_H_

#include <memory>
#include <string>
#include <map>
#include <vector>
#include <set>
#include <utility>
#include <dlib/directed_graph.h>
#include <dlib/graph_utils.h>
#include <dlib/bayes_utils.h>
#include <dlib/set.h>

#include "SituationGraph.h"
#include "SituationNode.h"
#include "SituationInstance.h"
#include "SituationRelation.h"
#include "../utils/ReasonerLogger.h"

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

using bayes_network = dlib::directed_graph<dlib::bayes_node>::kernel_1a_c;

class BNInferenceEngine {
private:
    typedef dlib::set<unsigned long>::kernel_1a set_type;
    typedef bayes_network bn_type;
    typedef dlib::graph<set_type, set_type>::kernel_1a_c join_tree_type;
    
    SituationGraph _sg;
    std::unique_ptr<bn_type> _bn;
    std::map<std::string, unsigned long> _nodeMap;
    std::unique_ptr<join_tree_type> _joinTree;
    
    // Solution object for inference
    std::unique_ptr<dlib::bayesian_network_join_tree> _solution;
    
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

    void calculateBeliefs(std::map<long, SituationInstance>& instanceMap, simtime_t current);

    // Helper functions for d-separation
    bool isCollider(unsigned long node, const std::vector<unsigned long>& path) const;
    bool isActive(unsigned long node, const std::set<unsigned long>& conditioningSet, 
                 const std::vector<unsigned long>& path,
                 std::set<unsigned long>& visited) const;
    bool hasActivePathDFS(unsigned long start, unsigned long end,
                        const std::set<unsigned long>& conditioningSet,
                        std::vector<unsigned long>& currentPath,
                        std::set<unsigned long>& visited) const;
    bool isDConnected(unsigned long start, unsigned long end,
                     const std::set<unsigned long>& conditioningSet) const;
    std::vector<unsigned long> getDescendants(unsigned long node) const;

public:
    BNInferenceEngine();
    virtual ~BNInferenceEngine();
    
    void loadModel(SituationGraph sg);
    void reason(SituationGraph sg,
               std::map<long, SituationInstance> &instanceMap,
               simtime_t current,
               std::shared_ptr<ReasonerLogger> logger = nullptr);
    void convertGraphToBN(const SituationGraph& sg);
    
    // Print functions
    void printNetwork(std::ostream& out = std::cout) const;
    void printProbabilities(std::ostream& out = std::cout) const;
};

#endif /* BNINFENGINE_H_ */
