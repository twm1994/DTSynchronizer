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
    /*
     * Construct a situation graph and its instance
     */
    sa.initModel("../files/SG.json");

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

    // schedule IoT event generation
    scheduleAt(min_event_cycle, EGTimeout);
}

void EventSource::handleMessage(cMessage *msg) {
    if (msg->isName(msg::EG_TIMEOUT)) {

        vector<PhysicalOperation> operations = sa.arrange(simTime());
        for(auto operation : operations){
            IoTEvent* event = new IoTEvent(msg::IOT_EVENT);

            event->setEventID(operation.id);
            event->setToTrigger(operation.toTrigger);
            event->setTimestamp(operation.timestamp);

            simtime_t latency = lg.generator_latency();
            // send out the message
            sendDelayed(event, latency, "out");

        }

        scheduleAt(simTime() + min_event_cycle, EGTimeout);
    }
}
