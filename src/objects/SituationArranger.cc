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

#include "../common/RandomClass.h"
#include "../common/Util.h"
#include "SituationArranger.h"

SituationArranger::SituationArranger() : SituationEvolution() {
}

vector<PhysicalOperation> SituationArranger::arrange(simtime_t current) {

    cout << endl << "current time in Arranger: " << current << endl;

    vector<PhysicalOperation> operations;

    /*
     * Build a list of triggerable top-layer situations
     */
    set<long> triggerables;
    DirectedGraph top = sg.getLayer(0);
    vector<long> topNodes = top.topo_sort();

    for (auto node : topNodes) {
        SituationNode s = sg.getNode(node);
        SituationInstance &si = instanceMap[node];
        if (s.causes.empty()) {
            triggerables.insert(si.id);
        } else {
            bool toTrigger = true;
            for (auto cause : s.causes) {
                SituationInstance cs = instanceMap[cause];
                if (cs.counter <= si.counter) {
                    toTrigger = false;
                    break;
                }
            }
            if (toTrigger) {
                triggerables.insert(si.id);
            }
        }
    }

    /*
     * build a list of triggerable operational situations
     */
    for (auto triggerable : triggerables) {

        // top instance
        SituationInstance &ti = instanceMap[triggerable];

        if (ti.state == SituationInstance::UNTRIGGERED) {
            if (ti.next_start <= current && Random.NextDecimal() > 0.7) {

                cout << "trigger situation " << ti.id << endl;

                ti.state = SituationInstance::TRIGGERED;
                vector<long> tBottoms = sg.getOperationalSitutions(triggerable);
                for (auto tBottom : tBottoms) {
                    // bottom instance
                    SituationInstance &bi = instanceMap[tBottom];
                    bi.state = SituationInstance::TRIGGERED;
                    tOpStiuations.insert(tBottom);
                }
            }
        } else {
            /*
             * check whether all bottom situations have been triggered,
             * which means they have experience the transition from triggered
             * to untriggered.
             */
            bool allTriggered = true;
            vector<long> tBottoms = sg.getOperationalSitutions(triggerable);
            for (auto tBottom : tBottoms) {
                // bottom instance
                SituationInstance &bi = instanceMap[tBottom];
                if (bi.state == SituationInstance::TRIGGERED
                        || bi.counter <= ti.counter) {
                    allTriggered = false;
                    break;
                }
            }

            if (allTriggered && ti.next_start + ti.duration <= current) {

                cout << "reset situation " << ti.id << endl;

                ti.state = SituationInstance::UNTRIGGERED;
                ti.counter++;
                ti.next_start = current + ti.cycle;

                for (auto tBottom : tBottoms) {
                    tOpStiuations.erase(tOpStiuations.find(tBottom));
                }
            } else {
                vector<long> tBottoms = sg.getOperationalSitutions(triggerable);
                for (auto tBottom : tBottoms) {
                    // bottom instance
                    SituationInstance &bi = instanceMap[tBottom];
                    if (bi.state == SituationInstance::UNTRIGGERED
                            && bi.counter <= ti.counter) {
                        bi.state = SituationInstance::TRIGGERED;
                        tOpStiuations.insert(tBottom);
                    }
                }
            }
        }
    }

//    cout << "print triggerable operational stiuations: ";
//    util::printSet(tOpStiuations);

    vector<long> bottoms = sg.getAllOperationalSitutions();

    for (auto bottom : bottoms) {
        SituationInstance &bi = instanceMap[bottom];

        // cycle match check
        simtime_t value = fmod(current, bi.cycle);
        if (value == 0) {
            PhysicalOperation s;
            s.id = bi.id;
            s.timestamp = current;
            s.toTrigger = false;
            auto it = tOpStiuations.find(bi.id);
            if (it != tOpStiuations.end()) {
                if (bi.state == SituationInstance::TRIGGERED) {
                    bi.counter++;
                    bi.state = SituationInstance::UNTRIGGERED;
                    s.toTrigger = true;
                }
            }
            operations.push_back(s);
        } else {
            bi.state = SituationInstance::UNTRIGGERED;
        }
    }

    return operations;
}

SituationArranger::~SituationArranger() {

}
