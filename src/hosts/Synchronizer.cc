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

#include "../common/Constants.h"
#include "../messages/event_m.h"
#include "Synchronizer.h"

Define_Module(Synchronizer);

void Synchronizer::initialize() {
    // TODO - Generated method body
}

void Synchronizer::handleMessage(cMessage *msg) {
    if (msg->isName(msg::IOT_EVENT)) {
        Event *event = check_and_cast<Event*>(msg);

        cout << "event (" << event->getEventID() << "): toTrigger "
                << event->getToTrigger() << ", timestamp "
                << event->getTimestamp() << endl;

        // delete the received msg
        delete event;
    }
}
