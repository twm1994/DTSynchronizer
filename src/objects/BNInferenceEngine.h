#ifndef BNINFENGINE_H_
#define BNINFENGINE_H_

#include <memory>
#include <string>
#include <map>
#include <vector>
#include <set>
#include <utility>

#include "BayesianNetwork.h"
#include "SituationGraph.h"
#include "SituationNode.h"
#include "SituationInstance.h"
#include "SituationRelation.h"
#include "OperationalEvent.h"

struct CausalConnection {
    std::set<long> nodes;  // Store node IDs instead of pointers
    std::set<std::pair<long, long>> edges;  // Store node ID pairs instead of pointer pairs
};

struct RelationType {
    enum Type {
        NONE,       // No relations
        SOLE,       // Single SOLE relation
        AND_ONLY,   // Only AND relations
        OR_ONLY,    // Only OR relations
        MIXED       // Mixed relations
    };
};

struct NodeRelations {
    RelationType::Type type;
    std::vector<long> andNodes;
    std::vector<long> orNodes;
    std::vector<std::pair<long, const SituationRelation*>> soleNodes;
    bool hasMixedRelations;  // True if any node has both causes and evidences
};

class BNInferenceEngine {
private:
 
    SituationGraph _sg;
    std::unique_ptr<BayesianNetwork> _BNet;
    std::map<std::tuple<long, long, std::set<std::pair<long, long>>>, double> cptCache;
    std::map<std::string, unsigned long> _nodeMap;
    
    // Solution object for inference
    std::unique_ptr<dlib::bayesian_network_join_tree> _solution;
    
    
    // Maps node ID to its M,N node IDs for mixed relation handling
    std::map<long, std::pair<long, long>> _mixedNodeInfo;
    
    // New lookup tables
    std::unordered_map<long, SituationNode> _nodeCache;  // Node ID -> SituationNode
    
    // Relation cache for each node's causes and evidences
    struct RelationInfo {
        std::set<long> soleNodes;
        std::set<long> andNodes;
        std::set<long> orNodes;
        NodeRelations relations;  // Pre-computed relations
    };
    std::unordered_map<long, RelationInfo> _relationCache;  // Node ID -> RelationInfo
    
    // State cache for triggering conditions
    std::unordered_map<long, std::vector<std::pair<std::string, SituationInstance::State>>> _stateCache;  // Node ID -> [(param, value)]
    
    // Weight cache for relations
    using EdgeKey = std::pair<long, long>;
    struct EdgeKeyHash {
        std::size_t operator()(const EdgeKey& k) const {
            return std::hash<long>()(k.first) ^ (std::hash<long>()(k.second) << 1);
        }
    };
    std::unordered_map<EdgeKey, double, EdgeKeyHash> _weightCache;  // (srcID, destID) -> weight

    // Node ID mapping
    std::map<long, unsigned long> nodeToIndex;  // Maps node IDs to sequential indices
    std::map<unsigned long, long> indexToNode;  // Maps sequential indices back to node IDs

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

protected:
    std::vector<unsigned long> getParents(unsigned long nodeIdx) const;
    std::vector<unsigned long> getChildren(unsigned long nodeIdx) const;

    std::set<long> findConnectedNodes(const SituationNode& node);
    CausalConnection findCausallyConnectedNodes(const std::map<long, SituationInstance>& instanceMap);
    
    /**
     * Complete the subgraph for a node with mixed relations (case 5)
     * Creates M and N nodes and updates edges accordingly
     */
    void completeMixedRelationSubgraph(long nodeId, DirectedGraph& causalDGraph, 
                                     const NodeRelations& relations,
                                     std::map<long, std::pair<long, long>>& mixedNodeInfo);
    
    /**
     * Construct CPT for a node with mixed relations (case 5)
     * Uses M and N nodes with AND connection
     */
    void constructMixedRelationCPT(const SituationNode& node, 
                                 const std::pair<long, long>& mnNodes,
                                 const std::map<long, SituationInstance>& instanceMap);

    // void calculateBeliefs(std::map<long, SituationInstance>& instanceMap, simtime_t current);
    std::vector<unsigned long> getDescendants(unsigned long node) const;
    virtual void constructCPT(const SituationNode& node,
                     const std::map<long, SituationInstance>& instanceMap);
    
    /**
     * Analyze the relations of a node and classify them into different types
     */
    NodeRelations analyzeNodeRelations(const SituationNode& node);
    
    /**
     * Construct the CPT based on the analyzed relations
     */
    void constructCPTFromRelations(const SituationNode& node, const NodeRelations& relations);
    
    /**
     * Check if a node has mixed relations (both causes and evidences)
     */
    bool hasMixedRelations(const SituationNode& node) const;

    // Helper method to initialize caches
    void initializeCaches(std::map<long, SituationInstance>& instanceMap);

public:
    BNInferenceEngine();
    virtual ~BNInferenceEngine();
    
    void loadModel(SituationGraph sg, std::map<long, SituationInstance>& instanceMap);
    void reason(SituationGraph sg,
               std::map<long, SituationInstance> &instanceMap,
               simtime_t current);
};

// Custom comparison operators for dlib sets
// namespace dlib {
//     template<typename T, typename alloc>
//     bool operator==(const set_kernel_1<T,alloc>& lhs, const set_kernel_1<T,alloc>& rhs) {
//         if (lhs.size() != rhs.size()) return false;
        
//         // Manual element comparison since we can't use iterators directly
//         bool equal = true;
//         set_kernel_1<T,alloc> temp_lhs(lhs); // Create copies to avoid modifying originals
//         T item;
//         while (temp_lhs.move_next()) {
//             item = temp_lhs.element();
//             if (!rhs.is_member(item)) {
//                 equal = false;
//                 break;
//             }
//         }
//         return equal;
//     }
    
//     template<typename T, typename alloc>
//     bool operator<(const set_kernel_1<T,alloc>& lhs, const set_kernel_1<T,alloc>& rhs) {
//         if (lhs.size() >= rhs.size()) return false;
        
//         bool is_subset = true;
//         set_kernel_1<T,alloc> temp_lhs(lhs); // Create copies to avoid modifying originals
//         T item;
//         while (temp_lhs.move_next()) {
//             item = temp_lhs.element();
//             if (!rhs.is_member(item)) {
//                 is_subset = false;
//                 break;
//             }
//         }
//         return is_subset;
//     }
// }

#endif /* BNINFENGINE_H_ */
