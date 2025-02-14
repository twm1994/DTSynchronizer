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

#include <algorithm>
#include <boost/json.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "../common/Constants.h"
#include "../objects/PhysicalOperation.h"
#include "../messages/IoTEvent_m.h"
#include "EventSource.h"

Define_Module(EventSource);

EventSource::EventSource(){
    MAX_TRIGGER_LIMIT = 8;
    toltalOperations = 0;
    toltalSituations = 0;    
    /*
     * Construct a situation graph and its instance
     */
    sa.initModel("../files/SG_test_case_4.json");

//    sa.print();

    // 500 ms
    min_event_cycle = 0.5;

    EGTimeout = new cMessage(msg::EG_TIMEOUT);
}

EventSource::~EventSource(){
    if (EGTimeout != NULL) {
        cancelAndDelete(EGTimeout);
    }
}

void EventSource::initialize() {
    generatedOperations.setName("Actual Generated Operations");
    generatedSituations.setName("Actual Generated Situations");
    // schedule IoT event generation
    scheduleAt(min_event_cycle, EGTimeout);
}

void EventSource::finish() {
    recordScalar("Actual Operations", toltalOperations);
    recordScalar("Actual Situations", toltalSituations);
    int consistency = sa.numOfConsistentOperation();
    recordScalar("Actual Consistent Operations", consistency);
}

void EventSource::handleMessage(cMessage *msg) {
    if (msg->isName(msg::EG_TIMEOUT)) {
        simtime_t current = simTime();
        vector<PhysicalOperation> operations = sa.arrange(MAX_TRIGGER_LIMIT, current);
        int operationCount = operations.size();
        int situationCount = 0;
        for(auto operation : operations){
            IoTEvent* event = new IoTEvent(msg::IOT_EVENT);
            long opID = operation.id;
            event->setEventID(opID);
            event->setToTrigger(operation.toTrigger);
            event->setTimestamp(operation.timestamp);
            event->setType(operation.type);
            event->setCounter(operation.counter);

            /*
             * Check the explicit cause only
             */
//            vector<long> causes = sa.getModel().getNode(operation.id).causes;
//            map<long, int> causeCounts;
//            for(auto cause : causes){
//                causeCounts[cause] = sa.getInstance(cause).counter;
//            }
            /*
             * Check both explicit cause and implicit cause
             */
            vector<long> operations =
                    sa.getModel().getAllOperationalSitutions();
            map<long, int> causeCounts;
            for (auto op2 : operations) {
                if (op2 != opID && sa.getModel().isReachable(op2, opID)
                        && !sa.getModel().isReachable(opID, op2)) {
                    causeCounts[op2] = sa.getInstance(op2).counter;
                }
            }
            json j_causeCounts(causeCounts);
            event->setCauseCounts(j_causeCounts.dump().c_str());

            simtime_t latency = lg.generator_latency();
            // send out the message
            sendDelayed(event, latency, "out");

            if(operation.toTrigger){
                situationCount++;
            }
        }

        generatedOperations.record(operationCount);
        generatedSituations.record(situationCount);
        toltalOperations += operationCount;
        toltalSituations += situationCount;
        scheduleAt(simTime() + min_event_cycle, EGTimeout);
    }
}
