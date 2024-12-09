#include "ReasonerLogger.h"
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <boost/property_tree/json_parser.hpp>

ReasonerLogger::ReasonerLogger(const std::string& baseFilePath) 
    : headerWritten(false) {
    // Create logs directory if it doesn't exist
    std::filesystem::path dir = std::filesystem::path(baseFilePath).parent_path();
    std::filesystem::create_directories(dir);

    // Set file paths
    csvFilePath = baseFilePath + ".csv";
    jsonFilePath = baseFilePath + ".json";

    // Open CSV file
    csvFile.open(csvFilePath, std::ios::out);
    if (!csvFile.is_open()) {
        throw std::runtime_error("Failed to open CSV file: " + csvFilePath);
    }
}

ReasonerLogger::~ReasonerLogger() {
    if (csvFile.is_open()) {
        csvFile.close();
    }
    // Save JSON data before destruction
    saveToJson();
}

void ReasonerLogger::logStep(const std::string& phase, 
                           simtime_t timestamp,
                           long situationId,
                           double beliefValue,
                           const std::vector<double>& childrenBeliefs,
                           const std::vector<double>& predecessorBeliefs,
                           SituationInstance::State state) {
    ReasoningStep step{
        phase, timestamp, situationId, beliefValue,
        childrenBeliefs, predecessorBeliefs, state
    };
    
    steps.push_back(step);
    
    if (!headerWritten) {
        writeCSVHeader();
    }
    writeCSVStep(step);
}

void ReasonerLogger::logGraphState(const SituationGraph& graph, 
                                 const std::map<long, SituationInstance>& instances,
                                 simtime_t timestamp) {
    boost::property_tree::ptree graphState;
    graphState.put("timestamp", timestamp.dbl());
    boost::property_tree::ptree nodesArray;
    
    for (int layer = 0; layer < graph.modelHeight(); layer++) {
        DirectedGraph currentLayer = graph.getLayer(layer);
        std::vector<long> nodes = currentLayer.topo_sort();
        
        for (long nodeId : nodes) {
            boost::property_tree::ptree nodeState;
            nodeState.put("id", nodeId);
            nodeState.put("layer", layer);
            
            const SituationNode& node = graph.getNode(nodeId);
            nodeState.put("threshold", node.threshold);
            
            // Add causes array
            boost::property_tree::ptree causesArray;
            for (const auto& cause : node.causes) {
                boost::property_tree::ptree causeValue;
                causeValue.put("", cause);
                causesArray.push_back(std::make_pair("", causeValue));
            }
            nodeState.add_child("causes", causesArray);
            
            // Add evidences array
            boost::property_tree::ptree evidencesArray;
            for (const auto& evidence : node.evidences) {
                boost::property_tree::ptree evidenceValue;
                evidenceValue.put("", evidence);
                evidencesArray.push_back(std::make_pair("", evidenceValue));
            }
            nodeState.add_child("evidences", evidencesArray);
            
            auto it = instances.find(nodeId);
            if (it != instances.end()) {
                const SituationInstance& instance = it->second;
                nodeState.put("state", instance.state);
                nodeState.put("beliefValue", instance.beliefValue);
                nodeState.put("duration", instance.duration.dbl());
                nodeState.put("cycle", instance.cycle.dbl());
                nodeState.put("next_start", instance.next_start.dbl());
            }
            
            nodesArray.push_back(std::make_pair("", nodeState));
        }
    }
    
    graphState.add_child("nodes", nodesArray);
    
    // Append to JSON file
    std::ofstream jsonFile(jsonFilePath, std::ios::app);
    boost::property_tree::write_json(jsonFile, graphState);
    jsonFile.close();
}

void ReasonerLogger::logCausalReasoning(long causeId, long effectId, double beliefValue, simtime_t timestamp) {
    if (!headerWritten) {
        writeCSVHeader();
    }

    // Log to CSV
    csvFile << "causalReasoning," << timestamp << "," << causeId << "," << effectId << "," 
            << beliefValue << ",,,," << std::endl;

    // Create JSON entry
    boost::property_tree::ptree entry;
    entry.put("type", "causalReasoning");
    entry.put("timestamp", timestamp.dbl());
    entry.put("causeId", causeId);
    entry.put("effectId", effectId);
    entry.put("beliefValue", beliefValue);

    // Add to steps
    ReasoningStep step;
    step.phase = "causalReasoning";
    step.timestamp = timestamp;
    step.situationId = effectId;
    step.beliefValue = beliefValue;
    steps.push_back(step);
}

void ReasonerLogger::logInstanceState(long instanceId,
                                    const std::vector<double>& childrenBeliefs,
                                    const std::vector<double>& predecessorBeliefs,
                                    SituationInstance::State state,
                                    simtime_t timestamp) {
    if (!headerWritten) {
        writeCSVHeader();
    }

    // Convert beliefs to string
    std::string childrenBeliefsStr;
    for (const auto& belief : childrenBeliefs) {
        if (!childrenBeliefsStr.empty()) childrenBeliefsStr += ",";
        childrenBeliefsStr += std::to_string(belief);
    }

    std::string predecessorBeliefsStr;
    for (const auto& belief : predecessorBeliefs) {
        if (!predecessorBeliefsStr.empty()) predecessorBeliefsStr += ",";
        predecessorBeliefsStr += std::to_string(belief);
    }

    // Log to CSV
    csvFile << "instanceState," << timestamp << "," << instanceId << ",,," 
            << static_cast<int>(state) << "," 
            << childrenBeliefsStr << "," << predecessorBeliefsStr << std::endl;

    // Create JSON entry
    boost::property_tree::ptree entry;
    entry.put("type", "instanceState");
    entry.put("timestamp", timestamp.dbl());
    entry.put("instanceId", instanceId);
    entry.put("state", static_cast<int>(state));

    boost::property_tree::ptree childrenArray;
    for (const auto& belief : childrenBeliefs) {
        boost::property_tree::ptree value;
        value.put("", belief);
        childrenArray.push_back(std::make_pair("", value));
    }
    entry.add_child("childrenBeliefs", childrenArray);

    boost::property_tree::ptree predecessorArray;
    for (const auto& belief : predecessorBeliefs) {
        boost::property_tree::ptree value;
        value.put("", belief);
        predecessorArray.push_back(std::make_pair("", value));
    }
    entry.add_child("predecessorBeliefs", predecessorArray);

    // Add to steps
    ReasoningStep step;
    step.phase = "instanceState";
    step.timestamp = timestamp;
    step.situationId = instanceId;
    step.childrenBeliefs = childrenBeliefs;
    step.predecessorBeliefs = predecessorBeliefs;
    step.state = state;
    steps.push_back(step);
}

void ReasonerLogger::writeCSVHeader() {
    csvFile << "Timestamp,Phase,SituationID,BeliefValue,State,"
            << "ChildrenBeliefs,PredecessorBeliefs" << std::endl;
    headerWritten = true;
}

void ReasonerLogger::writeCSVStep(const ReasoningStep& step) {
    csvFile << step.timestamp.dbl() << ","
            << step.phase << ","
            << step.situationId << ","
            << std::fixed << std::setprecision(6) << step.beliefValue << ","
            << (step.state == SituationInstance::TRIGGERED ? "TRIGGERED" : "UNTRIGGERED") << ",\"";
    
    // Write children beliefs
    for (size_t i = 0; i < step.childrenBeliefs.size(); ++i) {
        if (i > 0) csvFile << ";";
        csvFile << step.childrenBeliefs[i];
    }
    csvFile << "\",\"";
    
    // Write predecessor beliefs
    for (size_t i = 0; i < step.predecessorBeliefs.size(); ++i) {
        if (i > 0) csvFile << ";";
        csvFile << step.predecessorBeliefs[i];
    }
    csvFile << "\"" << std::endl;
}

boost::property_tree::ptree ReasonerLogger::stepToPtree(const ReasoningStep& step) {
    boost::property_tree::ptree pt;
    pt.put("timestamp", step.timestamp.dbl());
    pt.put("phase", step.phase);
    pt.put("situationId", step.situationId);
    pt.put("beliefValue", step.beliefValue);
    pt.put("state", (step.state == SituationInstance::TRIGGERED ? "TRIGGERED" : "UNTRIGGERED"));
    
    // Add children beliefs array
    boost::property_tree::ptree childrenArray;
    for (const auto& belief : step.childrenBeliefs) {
        boost::property_tree::ptree beliefValue;
        beliefValue.put("", belief);
        childrenArray.push_back(std::make_pair("", beliefValue));
    }
    pt.add_child("childrenBeliefs", childrenArray);
    
    // Add predecessor beliefs array
    boost::property_tree::ptree predecessorArray;
    for (const auto& belief : step.predecessorBeliefs) {
        boost::property_tree::ptree beliefValue;
        beliefValue.put("", belief);
        predecessorArray.push_back(std::make_pair("", beliefValue));
    }
    pt.add_child("predecessorBeliefs", predecessorArray);
    
    return pt;
}

void ReasonerLogger::saveToJson() {
    boost::property_tree::ptree root;
    boost::property_tree::ptree stepsArray;
    
    for (const auto& step : steps) {
        stepsArray.push_back(std::make_pair("", stepToPtree(step)));
    }
    
    root.add_child("steps", stepsArray);
    
    std::ofstream jsonFile(jsonFilePath);
    boost::property_tree::write_json(jsonFile, root);
    jsonFile.close();
}

void ReasonerLogger::flush() {
    csvFile.flush();
    saveToJson();
}
