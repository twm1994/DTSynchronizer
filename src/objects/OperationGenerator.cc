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
#include "OperationGenerator.h"

OperationGenerator::OperationGenerator() {

}

void OperationGenerator::setModel(SituationGraph sg){
    this->sg = sg;
}

void OperationGenerator::setModelInstance(SituationEvolution* se){
    this->se = se;
}

void OperationGenerator::cacheEvent(long eventId, bool toTrigger,
        simtime_t timestamp) {
    OperationalEvent event;
    event.id = eventId;
    event.toTrigger = toTrigger;
    event.timestamp = timestamp;
    eventQueues[eventId].push_back(event);
}

queue<vector<VirtualOperation>> OperationGenerator::generateOperations(set<long> cycleTriggered) {
    /*
     * Event merge: here is only an over-simplified implementation
     */
    map<long, OperationalEvent> mergedEvents;
    set<long> toRemoveFront;
    for(auto a : eventQueues){
        long id = a.first;
        if(!a.second.empty()){
            OperationalEvent event = a.second[0];
            mergedEvents[id] = event;
            toRemoveFront.insert(id);
        }
    }

    //    cout << "mergedEvents: ";
    //    util::printMap(mergedEvents);
        cout << "print eventQueues: " << endl;
        util::printComplexMap(eventQueues);

    // to remove the first event from cache, which is supposed to be transmitted to simulator
    for(auto a : toRemoveFront){
        eventQueues[a].erase(eventQueues[a].begin());
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
        vo.count = se->getInstance(a.first).counter;
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

            /*
             * search for vo's cause situations
             */
            // a flag indicating whether a situation has a cause in the same slice
            bool sameSlice = false;
            for(auto& vo1 : topMap){
                if(vo1.first != id && sg.isReachable(vo1.first, id) && !sg.isReachable(id, vo1.first)){
                    SituationInstance& cInstance = se->getInstance(vo1.first);
                    /*
                     * check whether causes have been triggered in the last time slice
                     */
                    if(cInstance.counter == instance.counter){
                        newVoMap[vo1.first] = vo1.second;
                        sameSlice = true;
                        hasCause = true;
//                        cout << "find cause situation (" << vo.second << ")'s cause (" << vo1.second << endl;
                    }
                }
            }
            if(!sameSlice){
                // migrate causes and no-cause operations to a new operation set
                newVoMap[id] = vo.second;
            }

        }

        if(hasCause){
            sorted.push(newVoMap);
            // delete causes and no-cause events from the checked Set
            for(auto vo : newVoMap){
                topMap.erase(vo.first);
            }

            cout << "migrate operation set" << endl;
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

OperationGenerator::~OperationGenerator() {
    // TODO Auto-generated destructor stub
}

