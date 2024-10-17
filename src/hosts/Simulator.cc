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
#include "../messages/SimEvent_m.h"
#include "Simulator.h"

Define_Module(Simulator);

void Simulator::initialize()
{
    // TODO - Generated method body
}

void Simulator::handleMessage(cMessage *msg)
{
    if (msg->isName(msg::SIM_EVENT)) {
        SimEvent *event = check_and_cast<SimEvent*>(msg);

        cout << "Simulation event (" << event->getEventID() << "): timestamp "
                << event->getTimestamp() << endl;

        // delete the received msg
        delete event;
    }
}
