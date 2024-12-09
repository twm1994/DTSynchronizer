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

#ifndef OBJECTS_SITUATIONEVOLUTION_H_
#define OBJECTS_SITUATIONEVOLUTION_H_

#include <omnetpp.h>
#include <map>
#include <set>
#include <vector>
#include "SituationInstance.h"
#include "PhysicalOperation.h"
#include "SituationGraph.h"

using namespace omnetpp;
using namespace std;

class SituationEvolution {
protected:
    SituationGraph sg;
std::map<long, SituationInstance> instanceMap;
public:
    SituationEvolution();
    void initModel(const char *model_path);
    // return a list of operations as operational situations
    void addInstance(long id, SituationInstance::Type type =
            SituationInstance::NORMAL, simtime_t duration = 0, simtime_t cycle =
            0);
    SituationInstance& getInstance(long id);
    SituationGraph& getModel();
    void print();
    virtual ~SituationEvolution();
};

#endif /* OBJECTS_SITUATIONEVOLUTION_H_ */
