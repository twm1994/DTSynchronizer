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

#ifndef OBJECTS_TRIGGERINGEVENTGENERATOR_H_
#define OBJECTS_TRIGGERINGEVENTGENERATOR_H_

#include <vector>
#include <queue>
#include "SituationGraph.h"
#include "SituationEvolution.h"
#include "OperationalEvent.h"
#include "VirtualOperation.h"

class TriggeringEventGenerator {
private:
    SituationGraph sg;
    SituationEvolution* se;
    map<long, vector<OperationalEvent>> eventQueue;
public:
    TriggeringEventGenerator();
    void setModel(SituationGraph sg);
    void setModelInstance(SituationEvolution* se);
    void cacheEvent(long eventId, bool toTrigger, simtime_t timestamp);
    queue<vector<VirtualOperation>> generateTriggeringEvents(set<long> cycleTriggered);
    virtual ~TriggeringEventGenerator();
};

#endif /* OBJECTS_TRIGGERINGEVENTGENERATOR_H_ */
