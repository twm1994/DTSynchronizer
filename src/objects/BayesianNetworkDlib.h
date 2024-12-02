#ifndef BAYESIAN_NETWORK_DLIB_H_
#define BAYESIAN_NETWORK_DLIB_H_

/*
#include <dlib/bayes_utils.h>
#include <dlib/graph.h>
#include <dlib/directed_graph.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include "SituationGraph.h"
#include "SituationInstance.h"

class BayesianNetworkDlib {
public:
    BayesianNetworkDlib();
    ~BayesianNetworkDlib();

    // Network construction methods
    void createNetworkFromSituationGraph(const std::string& jsonFile);
    void createNetworkFromSituationGraph(const SituationGraph& graph);

    // Node and edge management
    void initializeNode(const std::string& name);
    void addEdge(const std::string& parent, const std::string& child);

    // Inference methods
    void performInference();
    void addEvidence(const std::string& nodeName, size_t value);
    void eraseEvidence(const std::string& nodeName);
    void eraseAllEvidence();
    std::vector<double> getPosterior(const std::string& nodeName);

    // Structure learning and refinement
    void updateRefinement(const SituationGraph& graph, std::map<long, SituationInstance>& instanceMap);
    void learnStructure(const std::map<long, SituationInstance>& instanceMap);
    bool isIndependent(unsigned long node1, unsigned long node2) const;
    bool isIndependent(unsigned long node1, unsigned long node2, const std::set<unsigned long>& conditioningSet) const;

private:
    // Type definitions using dlib
    using bn_type = dlib::directed_graph<dlib::bayes_node>::kernel_1a_c;
    using join_tree_type = dlib::graph<dlib::set<unsigned long>::compare_1b_c, dlib::set<unsigned long>::compare_1b_c>::kernel_1a_c;
    
    // Core data structures
    bn_type _bn;  // Bayesian network
    std::unique_ptr<join_tree_type> _joinTree;  // Join tree for inference
    std::map<std::string, unsigned long> _nodeMap;  // Maps node names to indices
    std::map<std::string, size_t> _evidence;  // Current evidence

    // Helper methods for network operations
    void buildJoinTree();
    dlib::matrix<double> calculateJointProbability(const std::string& nodeName);
    void pruneWeakEdges(double threshold = 0.1);
    void updateBeliefs(std::map<long, SituationInstance>& instanceMap);
    double calculateMutualInformation(unsigned long node1, unsigned long node2,
                                    const std::map<long, SituationInstance>& instanceMap);

    // Helper methods for d-separation
    enum class ConnectionType {
        Chain,      // X -> Y -> Z or X <- Y <- Z
        Fork,       // X <- Y -> Z
        Collider    // X -> Y <- Z
    };

    std::vector<std::vector<unsigned long>> findAllPaths(unsigned long start, unsigned long end) const;
    void findPathsDFS(unsigned long current, unsigned long end,
                     std::set<unsigned long>& visited,
                     std::vector<unsigned long>& currentPath,
                     std::vector<std::vector<unsigned long>>& allPaths) const;
    bool isPathBlocked(const std::vector<unsigned long>& path,
                      const std::set<unsigned long>& conditioningSet) const;
    ConnectionType getConnectionType(unsigned long prev, unsigned long current, unsigned long next) const;
    bool hasConditionedDescendant(unsigned long node,
                                const std::set<unsigned long>& conditioningSet) const;
    bool hasConditionedDescendantDFS(unsigned long current,
                                   const std::set<unsigned long>& conditioningSet,
                                   std::set<unsigned long>& visited) const;

    // Helper methods for JSON parsing
    void parseNode(const boost::property_tree::ptree& node);
    void parseEdges(const boost::property_tree::ptree& node);

    // Utility methods
    std::vector<unsigned long> getParents(unsigned long node_idx) const;
    std::vector<unsigned long> getChildren(unsigned long node_idx) const;
    dlib::matrix<double> normalizeMatrix(const dlib::matrix<double>& m);
    std::vector<double> matrixToVector(const dlib::matrix<double>& m);
    dlib::matrix<double> vectorToMatrix(const std::vector<double>& v);
};
*/

#endif // BAYESIAN_NETWORK_DLIB_H_
