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

#include "SituationEvolution.h"

SituationEvolution::SituationEvolution() {

}

SituationEvolution::~SituationEvolution() {}

void SituationEvolution::initModel(const char *model_path) {
    sg.loadModel(model_path, this);
}

void SituationEvolution::addInstance(long id, SituationInstance::Type type,
        simtime_t duration, simtime_t cycle, double baseBelief) {
    SituationInstance si(id, type, duration, cycle, baseBelief);
    instanceMap[si.id] = si;
}

int SituationEvolution::numOfConsistentOperation(){
    int consistency = 0;
    vector<long> operations = sg.getAllOperationalSitutions();
    for(auto op : operations){
        bool inconsistency = false;
        /*
         * Check the explicit cause only
         */
//        vector<long> causes = sg.getNode(op).causes;
//        for(auto cause : causes){
//            int causeCounter = instanceMap[cause].counter;
//            if(causeCounter < instanceMap[op].counter){
//                inconsistency = true;
//                break;
//            }
//        }
        /*
         * Check both explicit cause and implicit cause
         */
        for(auto op2 : operations){
            if(op2 != op && sg.isReachable(op2, op) && !sg.isReachable(op, op2)){
                int causeCounter = instanceMap[op2].counter;
                if(causeCounter < instanceMap[op].counter){
                    inconsistency = true;
                    break;
                }
            }
        }
        if(!inconsistency){
            consistency++;
        }
    }
    return consistency;
}

SituationInstance& SituationEvolution::getInstance(long id) {
    return instanceMap[id];
}

SituationGraph& SituationEvolution::getModel() {
    return sg;
}

void SituationEvolution::print() {
    for (auto m : instanceMap) {
        cout << m.second;
    }
}
