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

#include "../common/Constants.h"
#include "../common/Util.h"
#include "../messages/IoTEvent_m.h"
#include "../messages/SimEvent_m.h"
#include "Synchronizer.h"

Define_Module(Synchronizer);

Synchronizer::Synchronizer() {
    /*
     * Construct a situation graph and a situation inference engine
     */
    sr.initModel("../files/SG2.json");
    sog.setModel(sr.getModel());
    sog.setModelInstance(&sr);

    // 500 ms
    check_cycle = 0.5;
    // 3000 ms
    slice_cycle = 3;

    SETimeout = new cMessage(msg::SE_TIMEOUT);
    SCTimeout = new cMessage(msg::SC_TIMEOUT);
}

Synchronizer::~Synchronizer() {
    if (SETimeout != NULL) {
        cancelAndDelete(SETimeout);
    }
    if (SCTimeout != NULL) {
        cancelAndDelete(SCTimeout);
    }
}

void Synchronizer::initialize() {
    // schedule situation evolution
    scheduleAt(check_cycle, SCTimeout);
    scheduleAt(slice_cycle, SETimeout);
}

void Synchronizer::handleMessage(cMessage *msg) {
    if (msg->isName(msg::IOT_EVENT)) {
        IoTEvent *event = check_and_cast<IoTEvent*>(msg);

        cout << "IoT event (" << event->getEventID() << "): toTrigger "
                << event->getToTrigger() << ", timestamp "
                << event->getTimestamp() << endl;

        /*
         * By rights, all received IoT events needs to be cached for regression if needed.
         * Here, temporarily only triggering events are maintained for simplicity.
         */
        if (event->getToTrigger()) {
            sog.cacheEvent(event->getEventID(), event->getToTrigger(),
                    event->getTimestamp());

            long id = event->getEventID();
            if (bufferCounters.count(id)) {
                bufferCounters[id]++;
            } else {
                bufferCounters[id] = 1;
            }
        }

        // free up memory space
        delete event;
    } else if (msg->isName(msg::SE_TIMEOUT)) {

        simtime_t current = simTime();

        cout << endl << "current time slice: " << current << endl;

        std::set<long> triggered;

//        cout << "print buffer counters: ";
//        util::printMap(bufferCounters);

        for (auto bufferCounter : bufferCounters) {
            if (bufferCounter.second > 0) {
                triggered.insert(bufferCounter.first);
                bufferCounters[bufferCounter.first]--;
            }
        }

        /*
         * The reasoning result contains a list of triggered observable situations,
         * which is supposed to tell SOG to generate the corresponding simulation events.
         */
        std::set<long> tOperations = sr.reason(triggered, current);

        std::queue<std::vector<VirtualOperation>> opSets = sog.generateOperations(
                tOperations);

        cout << "Operation sets are: " << endl;
        util::printComplexQueue(opSets);

        while (!opSets.empty()) {
            std::vector<VirtualOperation> operations = opSets.front();
            for (auto op : operations) {
                SimEvent *event = new SimEvent(msg::SIM_EVENT);
                event->setEventID(op.id);
                event->setTimestamp(op.timestamp);
                event->setCount(op.count);
                simtime_t latency = lg.generator_latency();
                // send out the message
                sendDelayed(event, latency, "out");
            }

            opSets.pop();
        }

        scheduleAt(simTime() + slice_cycle, SETimeout);
    } else if (msg->isName(msg::SC_TIMEOUT)) {
        sr.checkState(SimTime());
        scheduleAt(simTime() + check_cycle, SCTimeout);
    }
}
