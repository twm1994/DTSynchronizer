# DT Synchronization OMNeT++ Simulation

## 1. Libraries & Dependencies

External library linked: JSON for Modern C++ (https://github.com/nlohmann/json), dlib (https://dlib.net/)

## 2. Situation Modeling

SG.json defines a situation model (i.e., situation graph).It contains multiple layers, devided into JSON arrayes. The first layer is the top layer of situation graph, called layer 0, while the last layer is the bottom layer.
 
In each layer, there are multiple JSON objects, representing situation nodes. Each situation is uniquely identified by ID.
 
Predecessors and Children contain the causes and evidences of a situation respectively. They could be null if empty. 

Duration is the length of a situation, which could be 0 for a transient situation. Cycle is the time gap between two sitaution occurrences, which could be set to empty or 0 to represent no-gap.

Currently, ***it is assumed that each observable situation is only related to one state variable, which, however, is not restricted in principle***.

## 3. Bayesian Network Inference

Bayesian Network is implemented with dlib 19.24. Specifically, BayesianNetwork encapsulates dlib methods for network management and inference, while BNInferenceEngine handles the task of converting a situation graph to a Bayesian Network and reasoning over the network using node-triggering states of the situation graph as evidences.

In BNInferenceEngine, several helper functions are defined for d-separation algorithm, including *isCollider*, *isActive*, *hasActivePathDFS*, and *isDConnected*. D-separation is used for determining a minimal BN for update refinement in the manuscript. But in this project the *findCausallyConnectedNodes* method simplifies finding a minimal BN as finding all triggered nodes and their neighbors.

Also, the dlib library does not implement LazyPropagation natively, but use *join_tree* for inference task.

The library aGrUM (https://agrum.gitlab.io/) provides better support for Bayesian Network tasks, however it does not work well with OMNet++.

## 4. Implementation Notes

Most functions are not fully implemented, or implemented in a workaround way, including: 

1. In SituationReasoner, the *reason* method hasn't implemented situation inference. (Update: situation inference has been implemented with three functions: *beliefPropagation*, *backwardRetrospection*, and *downwardRetrospection*)

2. In Synchronzier, the reasoning result from calling SituationReasoner's *reason* method contains a list of triggered observable situations,  which is supposed to tell *TEG* to generate the corresponding simulation events.

3. In the *generateTriggeringEvents* method of TriggeringEventGenerator, the event merge function is only an over-simplified implementation.

4. The *generateTriggeringEvents* method of TriggeringEventGenerator is supposed to use the function parameter *cycleTriggered* to generate events for sync failure, and add them to mergedEvents, which hasn't been implemented yet.

5. In the *loadModel* method of BNInferenceEngine, subgraph completion (step 4) cannot consistently handle mixed relations in test cases (i.e. a node has both AND-type and OR-type predecessors or children).

6. In the *loadModel* method of BNInferenceEngine, when mixed relations occur, the correct CPT table or network structure cannot be determined.

7. All test cases are in the *files* folder. Currently there is no test case that produce UNDETERMINED nodes to trigger situation refinement in the *reason* method of SituationReasoner.