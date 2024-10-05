/*
 * SituationNode.h
 *
 *  Created on: 2024年10月4日
 *      Author: sunni
 */

#ifndef OBJECTS_SITUATIONNODE_H_
#define OBJECTS_SITUATIONNODE_H_

#include <iostream>
using namespace std;

class SituationNode {
public:
    long id;
    int counter;
    short state; // 0 - untriggered, 1 - triggered
    double threshold;
public:
    SituationNode();
    virtual ~SituationNode();
};

inline std::ostream& operator<<(std::ostream &os, const SituationNode &s) {
    return os << "situation (" << s.id << "): counter " << s.counter
            << ", state " << s.state << ", threshold " << s.threshold;
}

#endif /* OBJECTS_SITUATIONNODE_H_ */
