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

#include "LatencyGenerator.h"

LatencyGenerator::LatencyGenerator() {
    // TODO Auto-generated constructor stub

}

simtime_t LatencyGenerator::generator_latency(){
    // assume minimum latency is 50 ms
    double min_latency = 50;
    // mu = 1 in the lognormal distribution of jitter
    double mu = 3;
    // sigma = 0.5 in the lognormal distribution of jitter
    double sigma = 1;
    // calculate one-way jitter
    double var_latency = lognormal(getEnvir()->getRNG(0), mu, sigma) / 2;
    // calculate the variable latency
//    double var_latency = exponential(getEnvir()->getRNG(0), 1 / jitter);
    simtime_t delay = (min_latency + var_latency) / 1000;

//    cout << "delay " << delay << ", var_latency " << var_latency << endl;

    return delay;
}

LatencyGenerator::~LatencyGenerator() {
    // TODO Auto-generated destructor stub
}

