//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#ifndef OBJECTS_SITUATIONREASONER_H_
#define OBJECTS_SITUATIONREASONER_H_

#include <omnetpp.h>

// Project includes
#include "SituationEvolution.h"

using namespace omnetpp;
using namespace std;

/**
 * The SituationReasoner class is responsible for reasoning about situations.
 * It extends the SituationEvolution class and provides methods for belief propagation,
 * backward and downward retrospection, and situation refinement using Bayesian Networks.
 */
class SituationReasoner: public SituationEvolution {
private:
    std::unique_ptr<ReasonerLogger> logger;

    /**
     * Determine the state of a cause situation based on a child situation.
     * 
     * @param causeId The ID of the cause situation.
     * @param childId The ID of the child situation.
     * @param graph The situation graph.
     * @return The state of the cause situation.
     */
    SituationInstance::State determineState(long causeId, long childId, SituationGraph& graph);

    /**
     * Determine the state of a child situation based on a parent situation.
     * 
     * @param parentId The ID of the parent situation.
     * @param childId The ID of the child situation.
     * @param graph The situation graph.
     * @return The state of the child situation.
     */
    SituationInstance::State determineChildState(long parentId, long childId, SituationGraph& graph);

    /**
     * Combine backward and downward retrospection states.
     * 
     * @param stateBuffer A vector of states to combine.
     * @return The combined state.
     */
    SituationInstance::State combineStates(vector<SituationInstance::State>& stateBuffer);

    static std::vector<double> convertMapToVector(const std::map<long, double>& beliefMap);
    void logCausalReasoning(const SituationInstance& causeInstance,
                           const SituationInstance& effectInstance,
                           double beliefValue);
    void logInstanceState(const SituationInstance& instance);

protected:
    /**
     * The current simulation time.
     */
    simtime_t current;

    /**
     * Perform belief propagation from bottom to top layers.
     * For each situation node n in a layer:
     * 1. If n has no type-H children: belief = 0.8 (expert measure)
     * 2. If n has one type-H child with SOLE relation: belief = child_belief * weight
     * 3. If n has multiple type-H children with OR relations: belief = max(child_belief * weight)
     * 4. If n has multiple type-H children with AND relations: belief = Dempster's combination of weighted beliefs
     * 
     * @param graph The situation graph.
     */
    void beliefPropagation(SituationGraph& graph);
    
    /**
     * Perform backward retrospection within each layer.
     * 
     * @param graph The situation graph.
     */
    void backwardRetrospection(SituationGraph& graph);
    
    /**
     * Perform downward retrospection from top to bottom layers.
     * 
     * @param graph The situation graph.
     */
    void downwardRetrospection(SituationGraph& graph);
    
    /**
     * Update situation refinement using Bayesian Network.
     * 
     * @param graph The situation graph.
     */
    void updateRefinement(SituationGraph& graph);

public:
    /**
     * Constructor.
     */
    SituationReasoner();
    // return a set of triggered operational situations
    set<long> reason(set<long> triggered, simtime_t current);
    // reset durable situations if timeout
    void checkState(simtime_t current);
    
    /**
     * Initialize the logger.
     * 
     * @param logBasePath The base path for log files.
     */
    void initializeLogger(const std::string& logBasePath);
    
    /**
     * Destructor.
     */
    virtual ~SituationReasoner();
};

#endif /* OBJECTS_SITUATIONREASONER_H_ */
