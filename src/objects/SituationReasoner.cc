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

#include "../common/Util.h"
#include "SituationReasoner.h"

SituationReasoner::SituationReasoner() :
        SituationEvolution() {
    // TODO Auto-generated constructor stub

}

SituationReasoner::~SituationReasoner() {
    // TODO Auto-generated destructor stub
}

set<long> SituationReasoner::reason(set<long> triggered, simtime_t current) {
    set<long> tOperational;

//    cout << "show triggered: ";
//    util::printSet(triggered);

    int numOfLayers = sg.modelHeight();
    // trigger bottom layer situations
    DirectedGraph g = sg.getLayer(numOfLayers - 1);
    vector<long> bottoms = g.topo_sort();
    for (auto bottom : bottoms) {
        SituationInstance& instance = instanceMap[bottom];
        auto it = triggered.find(bottom);
        if (it != triggered.end()) {
            instance.state = SituationInstance::TRIGGERED;
            instance.counter++;
            instance.next_start = current;
        }
    }

    for (int i = numOfLayers - 1; i > 0; i--) {
        DirectedGraph g1 = sg.getLayer(i - 1);
        vector<long> uppers = g1.topo_sort();
        for (auto upper : uppers) {
            SituationInstance& instance = instanceMap[upper];
            SituationNode node = sg.getNode(instance.id);
            bool toTrigger = true;
            for (auto evidence : node.evidences) {
                SituationInstance& es = instanceMap[evidence];
                if (es.counter <= instance.counter) {
                    toTrigger = false;
                    break;
                }
            }
            if (toTrigger) {
                instance.state = SituationInstance::TRIGGERED;
                instance.counter++;
                instance.next_start = current;
            }
        }
    }

    // get operational situations from the bottom layer
    for (auto bottom : bottoms) {
        SituationInstance& instance = instanceMap[bottom];
        if (instance.state == SituationInstance::TRIGGERED
                && instance.next_start == current) {
            tOperational.insert(instance.id);
        }
    }

    // reset transient situations
    for (auto& si : instanceMap) {
        if (si.second.next_start + si.second.duration <= current) {
            si.second.state = SituationInstance::UNTRIGGERED;
        }
    }


//    cout << "print situation graph instance" << endl;
//    print();

    return tOperational;
}

void SituationReasoner::checkState(simtime_t current) {
    // reset transient situations
    for (auto si : instanceMap) {
        if (si.second.next_start + si.second.duration <= current) {
            si.second.state = SituationInstance::UNTRIGGERED;
        }
    }
}
