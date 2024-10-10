# DT Synchronization OMNeT++ Simulation

## 1. Libraries & Dependencies

External library linked: Boost (https://www.boost.org/)

## 2. Situation Modeling

SG.json defines a situation model (i.e., situation graph).It contains multiple layers, devided into JSON arrayes. The first layer is the top layer of situation graph, called layer 0, while the last layer is the bottom layer.
 
In each layer, there are multipe JSON objects, representing situation nodes. Each situation is uniquely identified by ID.
 
Predecessors and Children contain the causes and evidences of a situation respectively. They could be null if empty. 

Duration is the length of a situation, which could be 0 for a transient situation. Cycle is the time gap between two sitaution occurrences, which could be set to empty or 0 to represent no-gap.

Currently, ***it is assumed that each observable situaiton is only related to one state variable, which, however, is not restricted in principle***.

## Notes: most functions are not fully implemented, or implemented in a workaround way, including: 

1) In SituationReasoner, the reason() funciton hasn't implemented situation inference.

2) In Synchronzier, the reasoning result from se.reason() contains a list of triggered observable situations,  which is supposed to tell TEG to generate the corresponding simulation events.

3) In the generateTriggeringEvents() method of TriggeringEventGenerator, the event merge function is only an over-simplified implementation.

4) The generateTriggeringEvents() method of TriggeringEventGenerator is supposed to use the function parameter *cycleTriggered* to generate events for sync failure, and add them to mergedEvents, which hasn't been implemented yet.