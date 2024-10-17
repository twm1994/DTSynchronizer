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

#ifndef OBJECTS_OPERATIONALEVENT_H_
#define OBJECTS_OPERATIONALEVENT_H_

#include "Operation.h"

class OperationalEvent: public Operation {
public:
    // state variable ID
    long svId;
    bool toTrigger;
public:
    OperationalEvent();
    virtual ~OperationalEvent();
};

inline std::ostream& operator<<(std::ostream &os, const OperationalEvent &o) {
    os << "Operational Event [" << o.id << "]: svID " << o.svId << " toTrigger "
            << o.toTrigger << " timestamp " << o.timestamp;
    return os;
}

#endif /* OBJECTS_OPERATIONALEVENT_H_ */
