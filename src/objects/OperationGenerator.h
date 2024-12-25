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

#ifndef OBJECTS_OPERATIONGENERATOR_H_
#define OBJECTS_OPERATIONGENERATOR_H_

#include <queue>
#include <vector>
#include <map>
#include <stack>
#include <omnetpp.h>
#include "SituationGraph.h"
#include "SituationEvolution.h"
#include "VirtualOperation.h"
#include "OperationalEvent.h"

using namespace std;
using namespace omnetpp;

/**
 * Generates operations based on the situation graph and current state.
 * Handles synchronization failures by monitoring cyclic situations and 
 * generating appropriate failure events when cycles are missed.
 */
class OperationGenerator {
private:
    SituationGraph sg;
    SituationEvolution* se;
    map<long, vector<OperationalEvent>> eventQueues;
public:
    OperationGenerator();
    void setModel(SituationGraph sg);
    void setModelInstance(SituationEvolution* se);
    void cacheEvent(long eventId, bool toTrigger, simtime_t timestamp);
    
    /**
     * Generates operations based on current events and cycle triggers.
     * Also handles synchronization failures for cyclic situations.
     * 
     * @param cycleTriggered Set of situation IDs that have triggered their cycles
     * @return Queue of operation sets to be executed
     */
    queue<vector<VirtualOperation>> generateOperations(set<long> cycleTriggered);
    virtual ~OperationGenerator();
};

#endif /* OBJECTS_OPERATIONGENERATOR_H_ */
