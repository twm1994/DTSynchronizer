# DT Synchronization OMNeT++ Simulation

## 1. Libraries & Dependencies

External library linked: Boost (https://www.boost.org/)

## 2. Situation Modeling

SG.json defines a situation model (i.e., situation graph).It contains multiple layers, devided into JSON arrayes. The first layer is the top layer of situation graph, called layer 0, while the last layer is the bottom layer.
 
In each layer, there are multipe JSON objects, representing situation nodes. Each situation is uniquely identified by ID.
 
Predecessors and Children contain the causes and evidences of a situation respectively. They could be null if empty. 

Duration is the length of a situation, which could be 0 for a transient situation. Cycle is the time gap between two sitaution occurrences, which could be set to empty or 0 to represent no-gap.