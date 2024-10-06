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

#include <boost/json.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "../objects/SituationGraph.h"
#include "../messages/event_m.h"
#include "EventSource.h"

Define_Module(EventSource);

EventSource::EventSource(){
    // 500 ms
    min_event_cycle = 0.5;

    EETimeout = new cMessage(msg::EE_TIMEOUT);
}

EventSource::~EventSource(){
    if (EETimeout != NULL) {
        cancelAndDelete(EETimeout);
    }
}

void EventSource::initialize() {

    /*
     * Create event emitters
     */
    // Create a root
    pt::ptree root;
    // Load the json file in this ptree
    pt::read_json("../files/SG.json", root);
    for (pt::ptree::value_type &layer : root.get_child("layers")) {
        for (pt::ptree::value_type &node : layer.second) {
            if(!node.second.get_child("Cycle").empty()){
                Sensor sensor;
                sensor.id = node.second.get<long>("ID");
                double cycle = node.second.get<long>("Cycle");
                // cycle is in millisecond
                sensor.cycle = SimTime(cycle/1000.0);
                sensors[sensor.id] = sensor;
            }
        }
    }

    // schedule event emission
    scheduleAt(min_event_cycle, EETimeout);

    /*
     * Construct a situation graph
     */
    SituationGraph sg;
    sg.loadModel("../files/SG.json");

    vector<SituationNode> situations = sg.getAllOperationalSitutions();
//    cout << "all operational situations: " << endl;
    for(auto situation : situations){
//        cout << situation;
    }

//    situations = sg.getOperationalSitutions(203);
//    cout << "specific operational situations: " << endl;
//    for(auto situation : situations){
//        cout << situation;
//    }
}

void EventSource::handleMessage(cMessage *msg) {
    if (msg->isName(msg::EE_TIMEOUT)) {
        Event* event = new Event(msg::IOT_EVENT);

        // send out the message
        sendDelayed(event, 0.3, "out");

        scheduleAt(simTime() + min_event_cycle, EETimeout);
    }
}
