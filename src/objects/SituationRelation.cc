/*
 * SituationRelation.cpp
 *
 *  Created on: 2024年10月4日
 *      Author: sunni
 */

#include "SituationRelation.h"

SituationRelation::SituationRelation() {
    src = 0;
    dest = 0;
    type = Type::V;
    relation = Relation::SOLE;
    weight = 1;
}

SituationRelation::~SituationRelation() {
    // TODO Auto-generated destructor stub
}

