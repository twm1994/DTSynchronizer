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

#include "../common/Util.h"
#include "TriggeringEventGenerator.h"

TriggeringEventGenerator::TriggeringEventGenerator() {

}

void TriggeringEventGenerator::setModel(SituationGraph sg){
    this->sg = sg;
}

void TriggeringEventGenerator::setModelInstance(SituationEvolution* se){
    this->se = se;
}

void TriggeringEventGenerator::cacheEvent(long eventId, bool toTrigger,
        simtime_t timestamp) {
    OperationalEvent event;
    event.id = eventId;
    event.toTrigger = toTrigger;
    event.timestamp = timestamp;
    eventQueue[eventId].push_back(event);
}

queue<vector<VirtualOperation>> TriggeringEventGenerator::generateTriggeringEvents(set<long> cycleTriggered) {
    /*
     * Event merge: here is only an over-simplified implementation
     */
    map<long, OperationalEvent> mergedEvents;
    set<long> toRemoveFront;
    for(auto a : eventQueue){
        long id = a.first;
        if(!a.second.empty()){
            OperationalEvent event = a.second[0];
            mergedEvents[id] = event;
            toRemoveFront.insert(id);
        }
    }

    //    cout << "mergedEvents: ";
    //    util::printMap(mergedEvents);
        cout << "print eventQueue: " << endl;
        util::printComplexMap(eventQueue);

    // to remove the first event from cache, which is supposed to be transmitted to simulator
    for(auto a : toRemoveFront){
        eventQueue[a].erase(eventQueue[a].begin());
    }

    /*
     * TODO use cycleTriggered to generate events for sync failure, and add them to mergedEvents
     */

    /*
     * Event sort
     */
    stack<map<long, VirtualOperation>> sorted;
    map<long, VirtualOperation> voMap;
    for(auto a : mergedEvents){
        VirtualOperation vo;
        vo.id = a.first;
        vo.timestamp = a.second.timestamp;
        voMap[vo.id] = vo;
    }
    sorted.push(voMap);

    bool hasCause = false;
    do{
        hasCause = false;
        map<long, VirtualOperation>& topMap = sorted.top();
        map<long, VirtualOperation> newVoMap;
        for(auto vo : topMap){
            long id = vo.first;
            SituationNode node = sg.getNode(id);
            SituationInstance& instance = se->getInstance(id);
            vector<long> causes = node.causes;
            if(!causes.empty()){
                bool sameSlice = false;
                for(auto cause : causes){
                    if(topMap.count(cause) > 0){
                        SituationInstance& cInstance = se->getInstance(cause);
                        /*
                         * check whether causes have been triggered in the last time slice
                         */
                        if(cInstance.counter == instance.counter){
                            VirtualOperation& co = topMap[cause];
                            newVoMap[cause] = co;
                            sameSlice = true;
                        }
                    }else{
                        break;
                    }
                }
                if(sameSlice){
                    hasCause = true;
                }
            }else{
                newVoMap[id] = vo.second;
            }
        }

        if(hasCause){
            sorted.push(newVoMap);
            // delete causes and no-cause events from the checked Set
            for(auto vo : newVoMap){
                topMap.erase(vo.first);
            }
        }

    }while(hasCause);


    /*
     * Divide events into different sets
     */
    queue<vector<VirtualOperation>> opSets;
    while(!sorted.empty()){
        map<long, VirtualOperation>& voMap = sorted.top();
        vector<VirtualOperation> operations;
        for(auto a : voMap){
            operations.push_back(a.second);
        }
        opSets.push(operations);
        sorted.pop();
    }

    return opSets;
}

TriggeringEventGenerator::~TriggeringEventGenerator() {
    // TODO Auto-generated destructor stub
}

