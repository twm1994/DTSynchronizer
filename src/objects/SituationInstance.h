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

#include <iostream>
#include <omnetpp.h>

using namespace std;
using namespace omnetpp;

class SituationInstance {
public:
    enum Type {
        NORMAL, HIDDEN
    };
    enum State {
        UNTRIGGERED, TRIGGERED, UNDETERMINED
    };
    long id;
    int counter;
    // 0 - normal source, 1 - hidden source
    Type type;
    // 0 - untriggered, 1 - triggered, 2 - UNDETERMINED
    State state;
    simtime_t duration;
    simtime_t cycle;
    /*
     * a multi-purpose variable:
     * 1) next situation start time in Arranger, and
     * 2) current situation start time in Reasoner.
     */
    simtime_t next_start;

public:
    SituationInstance();
    SituationInstance(long id, Type type, simtime_t duration, simtime_t cycle);
    virtual ~SituationInstance();
};

inline std::ostream& operator<<(std::ostream &os, const SituationInstance &si) {
    os << "situation (" << si.id << "): counter " << si.counter << ", type "
            << si.type << ", state " << si.state << ", duration " << si.duration
            << ", cycle " << si.cycle << ", next_start " << si.next_start
            << endl;
    return os;
}

#endif /* OBJECTS_SITUATIONINSTANCE_H_ */
