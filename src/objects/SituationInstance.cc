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

#include "SituationInstance.h"

SituationInstance::SituationInstance() {
    id = -1;
    counter = 0;
    state = State::UNTRIGGERED;
    cycle = 0;
    next_start = 0;
}

SituationInstance::SituationInstance(long id, simtime_t duration, simtime_t cycle){
    this->id = id;
    this->counter = 0;
    this->state = State::UNTRIGGERED;
    this->duration = duration;
    this->cycle = cycle;
    this->next_start = cycle;
}

SituationInstance::~SituationInstance() {
    // TODO Auto-generated destructor stub
}

