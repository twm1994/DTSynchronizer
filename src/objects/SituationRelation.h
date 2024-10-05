/*
 * SituationRelation.h
 *
 *  Created on: 2024年10月4日
 *      Author: sunni
 */

#ifndef OBJECTS_SITUATIONRELATION_H_
#define OBJECTS_SITUATIONRELATION_H_

#include <iostream>
#include "SituationNode.h"

using namespace std;

class SituationRelation {
public:
    enum Type {
        V, H
    };
    enum Relation {
        SOLE, AND, OR
    };

    long src;
    long dest;
    Type type;
    Relation relation;
    double weight;
public:
    SituationRelation();
    virtual ~SituationRelation();
};

inline std::ostream& operator<<(std::ostream &os, const SituationRelation &r) {
    return os << "relation (" << r.src << " -> " << r.dest << "): type "
            << r.type << ", relation " << r.relation << ", weight " << r.weight;
}

#endif /* OBJECTS_SITUATIONRELATION_H_ */
