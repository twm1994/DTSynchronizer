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

package abstract;

import hosts.*;

network Simulation
{
    parameters:
        @display("bgb=600,400");
    submodules:
        event_source: EventSource {
            @display("p=100,150");
        }
        synchronizer: Synchronizer {
            @display("p=250,150");
        }
        simulator: Simulator {
            @display("p=400,150");
        }

    connections:
        event_source.out --> synchronizer.in;
        synchronizer.out --> simulator.in;
}
