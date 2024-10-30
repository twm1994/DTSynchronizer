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

#ifndef OBJECTS_VIRTUALOPERATION_H_
#define OBJECTS_VIRTUALOPERATION_H_

#include "Operation.h"

class VirtualOperation: public Operation {
public:
    int count;
protected:
    // print has to be a constant method, as the caller is a constant
    void print(ostream& os) const{
        Operation::print(os);
        os << " count " << count;
    }
public:
    VirtualOperation();
    virtual ~VirtualOperation();
};

#endif /* OBJECTS_VIRTUALOPERATION_H_ */
