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

vector<PhysicalOperation> SituationArranger::arrange(int max_trigger_limit, simtime_t current) {
    set<long> causes;

    cout << endl << "current time in Arranger: " << current << endl;

    vector<PhysicalOperation> operations;

    /*
     * 1. Build a list of triggerable top-layer situations: A top-down approach to generate situations
     */
    set<long> triggerables;
    DirectedGraph top = sg.getLayer(0);
    vector<long> topNodes = top.topo_sort();

    for (auto node : topNodes) {
        SituationNode s = sg.getNode(node);
        SituationInstance &si = instanceMap[node];
        if(si.counter < max_trigger_limit){
            if (s.causes.empty()) {
                        triggerables.insert(si.id);
                    } else {
                        // a top-layer situation is to be triggered only if its trigger count is less than all causes
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
    }

    /*
     * 2. Build a list of triggerable operational situations
     */
    for (auto triggerable : triggerables) {

        // ti: top-layer instance
        SituationInstance &ti = instanceMap[triggerable];

        if (ti.state == SituationInstance::UNTRIGGERED) {
            if (ti.next_start <= current && Random.NextDecimal() > 0) {

//                cout << "trigger situation " << ti.id << endl;

                // trigger a top-layer situation, if it is not triggered
                ti.state = SituationInstance::TRIGGERING;

                // trigger all related bottom-layer situations
                vector<long> tBottoms = sg.getOperationalSitutions(triggerable);
                for (auto tBottom : tBottoms) {
                    // bottom instance
                    SituationInstance &bi = instanceMap[tBottom];
                    bi.state = SituationInstance::TRIGGERING;
                    tOpStiuations.insert(tBottom);
                }
            }
        } else {

            /*
             * Reset the top-layer situation's state, if all its bottom-layer evidences have been triggered
             * and its situation life cycle has ended. Otherwise, the state of them are left unchanged
             */

            /*
             * Check whether all bottom situations have been triggered,
             * which means they have experienced the transition from triggered
             * to untriggered.
             */
            bool allTriggered = true;
            vector<long> tBottoms = sg.getOperationalSitutions(triggerable);
            for (auto tBottom : tBottoms) {
                // bi: bottom-layer instance
                SituationInstance &bi = instanceMap[tBottom];
                if (bi.state == SituationInstance::TRIGGERING
                        || bi.counter <= ti.counter) {
                    allTriggered = false;
                    break;
                }
            }

            if (allTriggered && ti.next_start + ti.duration <= current) {

                /*
                 * Untrigger a top-layer situation and set its bottom-layer evidences non-triggerable
                 */
//                cout << "reset situation " << ti.id << endl;
                ti.state = SituationInstance::UNTRIGGERED;
                ti.counter++;
                ti.next_start = current + ti.cycle;

                for (auto tBottom : tBottoms) {
                    tOpStiuations.erase(tOpStiuations.find(tBottom));
                }
            } else {

                /*
                 * Leave the top-layer situation triggered and its bottom-layer evidences triggerable,
                 * if some bottom-layer evidence has not been triggered
                 */
                vector<long> tBottoms = sg.getOperationalSitutions(triggerable);
                for (auto tBottom : tBottoms) {
                    SituationInstance &bi = instanceMap[tBottom];
                    // leave the bottom-layer evidence triggered
                    if (bi.state == SituationInstance::UNTRIGGERED
                            && bi.counter <= ti.counter) {
                        bi.state = SituationInstance::TRIGGERING;
                        tOpStiuations.insert(tBottom);
                    }
                }
            }
        }
    }

//    cout << "print triggerable operational stiuations: ";
//    util::printSet(tOpStiuations);

    /*
     * 3. Pick triggerable operational situations if their triggering cycle has been reached and they are observable
     */
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

            /*
             * Reset the triggerable operational situation to untriggered for top-layer situation state reset condition check
             */
            auto it = tOpStiuations.find(bi.id);
            if (it != tOpStiuations.end()) {
                if (bi.state == SituationInstance::TRIGGERING) {
                    bi.counter++;
                    bi.state = SituationInstance::UNTRIGGERED;
                    s.toTrigger = true;
                }
            }

            s.counter = bi.counter;
            s.type = bi.type;

            // here, hidden situation are also transmitted, but not triggered, for result analysis
//            if(bi.type != SituationInstance::HIDDEN){
            // only send observable operations
            operations.push_back(s);
//            }
        } else {
            bi.state = SituationInstance::UNTRIGGERED;
        }
    }

    return operations;
}

SituationArranger::~SituationArranger() {

}
