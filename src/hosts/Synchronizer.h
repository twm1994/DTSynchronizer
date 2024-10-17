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

#include <omnetpp.h>
#include "../objects/SituationReasoner.h"
#include "../objects/TriggeringEventGenerator.h"
#include "../transport/LatencyGenerator.h"

using namespace omnetpp;
using namespace std;

/**
 * TODO - Generated class
 */
class Synchronizer: public cSimpleModule {
private:
    // cycle to check durable situations
    simtime_t check_cycle;
    // time slice
    simtime_t slice_cycle;
    // situation evolution timeout
    cMessage* SETimeout;
    // situation check timeout
    cMessage* SCTimeout;

    SituationReasoner sr;
    TriggeringEventGenerator teg;
    LatencyGenerator lg;
    // <situation_ID, trigger_counter>
    map<long, int> bufferCounters;

protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;

public:
    Synchronizer();
    virtual ~Synchronizer();
};

#endif
