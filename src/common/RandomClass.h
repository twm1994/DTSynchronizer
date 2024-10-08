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

#ifndef COMMON_RANDOMCLASS_H_
#define COMMON_RANDOMCLASS_H_

#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */

class RandomClass {
public:
    RandomClass() {
        /* initialize random seed: */
        srand(time(0));
    }

    double NextDecimal() {
        double r = ((double) rand() / (RAND_MAX)) + 1;
        return r;
    }
};

RandomClass Random;

#endif /* COMMON_RANDOMCLASS_H_ */
