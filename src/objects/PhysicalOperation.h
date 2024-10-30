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

#ifndef OBJECTS_PHYSICALOPERATION_H_
#define OBJECTS_PHYSICALOPERATION_H_

#include "Operation.h"

class PhysicalOperation: public Operation {
public:
    bool toTrigger;
protected:
    // print has to be a constant method, as the caller is a constant
    void print(ostream &os) const {
        Operation::print(os);
        os << " toTrigger " << toTrigger;
    }
public:
    PhysicalOperation();
    virtual ~PhysicalOperation();
};

#endif /* OBJECTS_PHYSICALOPERATION_H_ */
