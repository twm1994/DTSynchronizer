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

#ifndef OBJECTS_SITUATIONREASONER_H_
#define OBJECTS_SITUATIONREASONER_H_

#include <omnetpp.h>
#include "SituationEvolution.h"

using namespace omnetpp;
using namespace std;

class SituationReasoner: public SituationEvolution {
private:
public:
    SituationReasoner();
    // return a set of triggered operational situations
    set<long> reason(set<long> triggered, simtime_t current);
    // reset durable situations if timeout
    void checkState(simtime_t current);
    virtual ~SituationReasoner();
};

#endif /* OBJECTS_SITUATIONREASONER_H_ */
