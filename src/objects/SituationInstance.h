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

#ifndef OBJECTS_SITUATIONINSTANCE_H_
#define OBJECTS_SITUATIONINSTANCE_H_

#include <omnetpp.h>
#include <iostream>

using namespace std;
using namespace omnetpp;

class SituationInstance {
public:
    enum State {
        TRIGGERED, UNTRIGGERED
    };
    long id;
    int counter;
    State state; // 0 - untriggered, 1 - triggered
    simtime_t duration;
    simtime_t cycle;
    simtime_t next_start;

public:
    SituationInstance();
    SituationInstance(long id, simtime_t duration, simtime_t cycle);
    virtual ~SituationInstance();
};

inline std::ostream& operator<<(std::ostream &os, const SituationInstance &si) {
    os << "situation (" << si.id << "): counter " << si.counter << ", state "
            << si.state << ", duration " << si.duration << ", cycle "
            << si.cycle << ", next_start " << si.next_start << endl;
    return os;
}

#endif /* OBJECTS_SITUATIONINSTANCE_H_ */
