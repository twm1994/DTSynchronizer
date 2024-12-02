#ifndef UTILS_REASONERLOGGER_H_
#define UTILS_REASONERLOGGER_H_

#include <string>
#include <fstream>
#include <map>
#include <vector>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "../objects/SituationInstance.h"
#include "../objects/SituationGraph.h"

class ReasonerLogger {
public:
    struct ReasoningStep {
        std::string phase;           // beliefPropagation, backwardRetrospection, or downwardRetrospection
        simtime_t timestamp;         // Current simulation time
        long situationId;           // ID of the situation
        double beliefValue;         // Current belief value
        double temporalWeight;      // Temporal weight used
        std::vector<double> childrenBeliefs;    // Beliefs from children
        std::vector<double> predecessorBeliefs; // Beliefs from predecessors
        SituationInstance::State state;  // Current state
    };

private:
    std::string csvFilePath;
    std::string jsonFilePath;
    std::ofstream csvFile;
    std::vector<ReasoningStep> steps;
    bool headerWritten;

public:
    ReasonerLogger(const std::string& baseFilePath);
    ~ReasonerLogger();

    // Log a reasoning step
    void logStep(const std::string& phase, 
                simtime_t timestamp,
                long situationId,
                double beliefValue,
                double temporalWeight,
                const std::vector<double>& childrenBeliefs,
                const std::vector<double>& predecessorBeliefs,
                SituationInstance::State state);

    // Log graph state
    void logGraphState(const SituationGraph& graph, 
                      const std::map<long, SituationInstance>& instances,
                      simtime_t timestamp);

    // Log causal reasoning
    void logCausalReasoning(long causeId, long effectId, double beliefValue, simtime_t timestamp);

    // Log instance state
    void logInstanceState(long instanceId, 
                         const std::vector<double>& childrenBeliefs,
                         const std::vector<double>& predecessorBeliefs,
                         SituationInstance::State state,
                         simtime_t timestamp);

    // Save logs to files
    void flush();

private:
    void writeCSVHeader();
    void writeCSVStep(const ReasoningStep& step);
    boost::property_tree::ptree stepToPtree(const ReasoningStep& step);
    void saveToJson();
};

#endif /* UTILS_REASONERLOGGER_H_ */
