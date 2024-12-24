#ifndef BNINFENGINETEST_H_
#define BNINFENGINETEST_H_

#include "BNInferenceEngine.h"

class BNInferenceEngineTest : public BNInferenceEngine {
private:
    // Virtual nodes for case 5
    struct VirtualNodes {
        unsigned long M;  // Virtual AND node
        unsigned long N;  // Virtual OR node
        std::vector<unsigned long> andNodes;
        std::vector<unsigned long> orNodes;
    };
    std::map<unsigned long, VirtualNodes> _virtualNodes;

    // New methods for case 5
    void createVirtualNodes(unsigned long nodeIdx, const std::vector<unsigned long>& andNodes, const std::vector<unsigned long>& orNodes);
    void setupVirtualNodesCPT(unsigned long nodeIdx, const VirtualNodes& vNodes, const std::map<std::string, double>& weights);
    void handleCase5(const SituationNode& node, const std::map<long, SituationInstance>& instanceMap);

protected:
    // Override parent class method to handle case 5
    virtual void constructCPT(const SituationNode& node, const std::map<long, SituationInstance>& instanceMap) override;

public:
    BNInferenceEngineTest(const SituationGraph& sg);
    virtual ~BNInferenceEngineTest();
};

#endif /* BNINFENGINETEST_H_ */
