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
#include <vector>
#include <map>
#include <string>
#include "SituationNode.h"
#include <omnetpp.h>

using namespace std;
using namespace omnetpp;

class SituationInstance {
public:
    /**
     * Possible states of a situation instance.
     */
    enum State {
        UNTRIGGERED,    // Situation is not active
        TRIGGERED,      // Situation is active
        UNDETERMINED    // Situation state is unknown
    };
    enum Type {
        NORMAL, HIDDEN
    };
    long id;                      // Unique identifier
    int counter;                  // Occurrence counter
    vector<State> stateBuffer;    // Buffer for state calculations
    simtime_t duration;          // Duration of the situation
    simtime_t cycle;             // Cycle time of the situation    
    Type type;
    State state;

    /*
     * a multi-purpose variable:
     * 1) next situation start time in Arranger, and
     * 2) current situation start time in Reasoner.
     */
    simtime_t next_start;

    // Belief-related members
    double beliefValue;          // Current belief from evidence combination
    double beliefThreshold;      // Threshold for state transition
    bool beliefUpdated;          // Tracks if belief was updated in current cycle
    std::map<long, double> childrenBeliefs;
    std::map<long, double> predecessorBeliefs;
    
    // Methods to manage beliefs
    void updateChildBelief(long childId, double newBelief) {
        childrenBeliefs[childId] = newBelief;
    }
    
    void updatePredecessorBelief(long predId, double newBelief) {
        predecessorBeliefs[predId] = newBelief;
    }
    
    double getChildBelief(long childId) const {
        auto it = childrenBeliefs.find(childId);
        return it != childrenBeliefs.end() ? it->second : 0.0;
    }
    
    double getPredecessorBelief(long predId) const {
        auto it = predecessorBeliefs.find(predId);
        return it != predecessorBeliefs.end() ? it->second : 0.0;
    }

public:
    /**
     * Default constructor.
     * Initializes a situation instance with default values.
     */
    SituationInstance() : 
        id(-1), counter(0), state(UNTRIGGERED), type(NORMAL),
        duration(0), cycle(0), next_start(0),
        beliefValue(0.0), beliefThreshold(0.7), beliefUpdated(false) {
        stateBuffer.clear();
    }
        
    /**
     * Constructor with parameters.
     * @param id Unique identifier for the situation
     * @param duration Duration of the situation
     * @param cycle Cycle time of the situation
     */
    SituationInstance(long id, Type type, simtime_t duration, simtime_t cycle) :
        id(id), counter(0), state(UNTRIGGERED), type(NORMAL),
        duration(duration), cycle(cycle), next_start(0),
        beliefValue(0.0), beliefThreshold(0.7), beliefUpdated(false) {
        stateBuffer.clear();
    }
    // SituationInstance();
    // SituationInstance(long id, Type type, simtime_t duration, simtime_t cycle);
    virtual ~SituationInstance();

    /**
     * Updates the belief value and marks it as updated.
     * @param newBelief The new belief value to set
     */
    void updateBelief(double newBelief) {
        beliefValue = newBelief;
        beliefUpdated = true;
    }

    /**
     * Resets belief value and update flag.
     */
    void resetBelief() {
        beliefValue = 0.0;
        beliefUpdated = false;
    }

    /**
     * Adds a new state to the state buffer.
     * @param newState The state to add to the buffer
     */
    void addStateToBuffer(State newState) {
        stateBuffer.push_back(newState);
    }
};

inline std::ostream& operator<<(std::ostream &os, const SituationInstance &si) {
    os << "situation (" << si.id << "): counter " << si.counter << ", type "
            << si.type << ", state " << si.state << ", duration " << si.duration
            << ", cycle " << si.cycle << ", next_start " << si.next_start
            << endl;
    return os;
}

#endif /* OBJECTS_SITUATIONINSTANCE_H_ */
