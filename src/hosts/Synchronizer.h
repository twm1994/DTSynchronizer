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

#ifndef __DTSYNCHRONIZER_SYNCHRONIZER_H_
#define __DTSYNCHRONIZER_SYNCHRONIZER_H_

//#include <bits/stdc++.h>
#include <cmath>
#include <omnetpp.h>
#include <nlohmann/json.hpp>

#include "../objects/OperationGenerator.h"
#include "../objects/SituationReasoner.h"
#include "../transport/LatencyGenerator.h"
#include "../utils/ReasonerLogger.h"
#include "../common/Util.h"

using namespace omnetpp;
using namespace std;
using json = nlohmann::json;

class Synchronizer: public cSimpleModule {
private:
    int slice;
    // cycle to check durable situations
    simtime_t check_cycle;
    // time slice
    simtime_t slice_cycle;
    // situation evolution timeout
    cMessage* SETimeout;
    // situation check timeout
    cMessage* SCTimeout;

    SituationReasoner sr;
    OperationGenerator sog;
    LatencyGenerator lg;
    // <situation_ID, trigger_counter>
    std::map<long, int> bufferCounters;
    // <situation_ID, trigger_coutner> for actual observable situations
    std::map<long, int> actOBCounters;
    // <observable_situation_ID, trigger_coutner> for simulated observable situations
    std::map<long, int> simOBCounters;
    // situation instance ID: <situation_id, counter>
    typedef std::pair<long, int> si_id;
    // cause counters of actual situations: <si_id, list_of_cause_counter>
    std::map<si_id, std::map<long, int>> m_actCauseCounts;
    // cause counters of simulated situations: <si_id, list_of_cause_counter>
    std::map<si_id, std::map<long, int>> m_simCauseCounts;    

protected:
    virtual void initialize() override;
    virtual void finish() override;
    virtual void handleMessage(cMessage *msg) override;

public:
    Synchronizer();
    virtual ~Synchronizer();
};

#endif
