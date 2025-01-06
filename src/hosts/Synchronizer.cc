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
    sr.initModel("../files/SG_OR.json");
    sog.setModel(sr.getModel());
    sog.setModelInstance(&sr);
    // Create logs directory if it doesn't exist
    std::string logDir = "../logs";
    if (system(("mkdir -p " + logDir).c_str()) != 0) {
        EV_ERROR << "Failed to create logs directory" << endl;
    }

    // Initialize the reasoner's logger
    sr.initializeLogger(logDir + "/reasoner");
    slice = 0;
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

void Synchronizer::finish() {
    int consistency = sr.numOfConsistentOperation();
    recordScalar("Recognized Consistent Operations", consistency);
    /*
     * Calculate situation occurrence fidelity
     * TODO: currently, simulated and actual observable situations are not fully aligned here
     */
    std::vector<long> operations = sr.getModel().getAllOperationalSitutions();
    double sum_sqr_diff = 0;
    for(auto op : operations){
        double so_count = (double)sr.getInstance(op).counter;
        double ao_count = 0;
        if(actOBCounters.count(op) > 0){
            ao_count = (double)actOBCounters[op];
            if(actOBCounters[op] > slice){
                ao_count = slice;
            }
        }
        sum_sqr_diff += pow((so_count - ao_count), 2);
        cout << "actual situation " << op << " count = " << ao_count << endl;
        cout << "simulated situation " << op << " count = " << so_count << endl;
    }
    double fidelity_occ = sqrt(sum_sqr_diff / (double)operations.size());
    recordScalar("Situation Occurrence Fidelity", fidelity_occ);
    /*
     * Calculate situation alignment fidelity
     */
    double sum_sqr_max_diff = 0;
    for(auto simCauseCounts : m_simCauseCounts){
        long id = simCauseCounts.first.first;
        long count = simCauseCounts.first.second;
        si_id iid(id, count);
        std::vector<int> countDiff;
        for(auto causeCount : simCauseCounts.second){
            long cause = causeCount.first;
            int simCount = causeCount.second;
            int actCount = m_actCauseCounts[iid][cause];
            countDiff.push_back(abs(simCount - actCount));
        }
        if(countDiff.size() > 0){
            int max_diff = *std::max_element(countDiff.begin(), countDiff.end());
            sum_sqr_max_diff += pow(max_diff, 2);
            cout << "max situation (" << id << ", " << count << ") max misalignment = " << max_diff << endl;
        }else{
            cout << "no cause of situation (" << id << ", " << count << ")" << endl;
        }
    }
    double fidelity_ali = sqrt(sum_sqr_max_diff / (double)m_simCauseCounts.size());
    recordScalar("Situation Alignment Fidelity", fidelity_ali);
}

void Synchronizer::handleMessage(cMessage *msg) {
    if (msg->isName(msg::IOT_EVENT)) {
        IoTEvent *event = check_and_cast<IoTEvent*>(msg);

        cout << "IoT event (" << event->getEventID() << "): toTrigger "
                << event->getToTrigger() << ", counter " << event->getCounter()
                << ", type " << event->getType() << ", cause counts "
                << event->getCauseCounts() << ", timestamp "
                << event->getTimestamp() << endl;

        /*
         * By rights, all received IoT events needs to be cached for regression if needed.
         * Here, temporarily only triggering events are maintained for simplicity.
         */
        if (event->getToTrigger() && event->getType() == SituationInstance::NORMAL) {
            sog.cacheEvent(event->getEventID(), event->getToTrigger(),
                    event->getTimestamp());

            long id = event->getEventID();
            if (bufferCounters.count(id)) {
                bufferCounters[id]++;
            } else {
                bufferCounters[id] = 1;
            }
        }

        /*
         * Update actual observable situation counter and cause counters for occurrence fidelity analysis
         */
        if (event->getToTrigger()){
            long id = event->getEventID();
            actOBCounters.count(id) > 0 ? actOBCounters[id]++ : (actOBCounters[id] = 1);
            json j_causeCounts = json::parse(string(event->getCauseCounts()));
            std::map<long, int> causeCounts = j_causeCounts.get<std::map<long, int>>();
//            std::vector<si_id> actOBCauseCounts;
//            std::copy(causeCounts.begin(), causeCounts.end(), std::back_inserter(actOBCauseCounts));
            int count = actOBCounters[id];
            si_id actOBId(id, count);
            m_actCauseCounts[actOBId] = causeCounts;
        }

        // free up memory space
        delete event;
    } else if (msg->isName(msg::SE_TIMEOUT)) {

        simtime_t current = simTime();
        slice = (int) (current / slice_cycle);
        cout << endl << "current time slice: " << current << "(" << slice << ")"
                << endl;

        std::set<long> triggered;

//        cout << "print buffer counters: ";
//        util::printMap(bufferCounters);

        for (auto bufferCounter : bufferCounters) {
            if (bufferCounter.second > 0) {
                cout << "triggered ID " << bufferCounter.first << ", counter " << bufferCounter.second << endl;                
                triggered.insert(bufferCounter.first);
                bufferCounters[bufferCounter.first]--;
            }
        }

        cout << "Triggered events: ";
        util::printSet(triggered);
        cout << endl;

        /*
         * The reasoning result contains a list of triggered observable situations,
         * which is supposed to tell SOG to generate the corresponding simulation events.
         */
        std::set<long> tOperations = sr.reason(triggered, current);
        
        /*
         * Update simulated observable situation counter and cause counters for alignment fidelity analysis
         */
        for(auto op : tOperations){
            std::map<long, int> causeCounts;
            /*
             * Check the explicit cause only
             */
//            std::vector<long> causes = sr.getModel().getNode(op).causes;
//            for(auto cause : causes){
//                causeCounts[cause] = sr.getInstance(cause).counter;
//            }
            /*
             * Check both explicit cause and implicit cause
             */
            std::vector<long> operations = sr.getModel().getAllOperationalSitutions();
            for(auto op2 : operations){
                if(op2 != op && sr.getModel().isReachable(op2, op) && !sr.getModel().isReachable(op, op2)){
                    causeCounts[op2] = sr.getInstance(op2).counter;
                }
            }
//            std::vector<si_id> simOBCauseCounts;
//            std::copy(causeCounts.begin(), causeCounts.end(), std::back_inserter(simOBCauseCounts));
            int count = sr.getInstance(op).counter;
            si_id simOBId(op, count);
            m_simCauseCounts[simOBId] = causeCounts;
        }

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
