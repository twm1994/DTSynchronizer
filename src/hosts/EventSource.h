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

#ifndef __DTSYNCHRONIZER_EVENTSOURCE_H_
#define __DTSYNCHRONIZER_EVENTSOURCE_H_

#include <omnetpp.h>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>
#include "../objects/SituationArranger.h"
#include "../transport/LatencyGenerator.h"

using namespace std;
using namespace omnetpp;
using json = nlohmann::json;

/**
 *
 */
class EventSource: public cSimpleModule {
private:
    // specify the maximal situation counter value;
    int MAX_COUNT;
    simtime_t min_event_cycle;
    // event generation timeout
    cMessage* EGTimeout;
    LatencyGenerator lg;
    SituationArranger sa;

protected:
    cOutVector generatedOperations;
    cOutVector generatedSituations;
    int toltalOperations;
    int toltalSituations;
    virtual void initialize() override;
    virtual void finish() override;    
    virtual void handleMessage(cMessage *msg) override;

public:
    EventSource();
    virtual ~EventSource();
};

#endif
