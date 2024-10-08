/*
 * SituationNode.h
 *
 *  Created on: 2024年10月4日
 *      Author: sunni
 */

#ifndef OBJECTS_SITUATIONNODE_H_
#define OBJECTS_SITUATIONNODE_H_

#include <iostream>
#include <vector>
using namespace std;

class SituationNode {
public:
    long id;
    double threshold;
    vector<long> causes;
    vector<long> evidences;
public:
    SituationNode();
    virtual ~SituationNode();
};

inline std::ostream& operator<<(std::ostream &os, const SituationNode &s) {
    os << "situation (" << s.id << "): threshold " << s.threshold << endl;
    os << "causes (" << s.causes.size() << "): ";
    for(auto cause : s.causes){
        os << cause << ", ";
    }
    os << endl;
    os << "evidences: (" << s.evidences.size() << "): ";
    for(auto evidence : s.evidences){
            os << evidence << ", ";
    }
    os << endl;
    return os;
}

#endif /* OBJECTS_SITUATIONNODE_H_ */
