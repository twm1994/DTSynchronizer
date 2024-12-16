#!/usr/bin/env python3

import json
import random
import time
import os
from typing import List, Dict, Any, Optional

class SituationGraphGenerator:
    def __init__(self, num_layers: int, nodes_per_layer: List[int], binary_tree: bool = False):
        """
        Initialize the generator with the number of layers and nodes per layer.
        
        Args:
            num_layers: Number of layers in the graph
            nodes_per_layer: List containing the number of nodes for each layer
            binary_tree: If True, generates a binary tree structure with AND relations
        """
        self.num_layers = num_layers
        self.nodes_per_layer = nodes_per_layer
        self.binary_tree = binary_tree
        self.node_ids = self._generate_node_ids()
        
    def _generate_node_ids(self) -> Dict[int, List[int]]:
        """Generate unique IDs for nodes in each layer."""
        node_ids = {}
        for layer in range(self.num_layers):
            base = (layer + 1) * 100  # Layer 0 -> 100s, Layer 1 -> 200s, etc.
            node_ids[layer] = [base + i + 1 for i in range(self.nodes_per_layer[layer])]
        return node_ids
    
    def _generate_node(self, layer: int, node_id: int, predecessors: Optional[List[Dict[str, Any]]], children: Optional[List[Dict[str, Any]]]) -> Dict[str, Any]:
        """Generate a node with the given parameters."""
        return {
            "ID": node_id,
            "Predecessors": predecessors,
            "Children": children,
            "Duration": random.choice([1000, 2000, 3000]),
            "Cycle": random.choice([0, 1000, 2000]),
            "type": random.randint(0, 1)  # 0 for NORMAL, 1 for HIDDEN
        }
    
    def _generate_connection(self, target_id: int, is_vertical: bool) -> Dict[str, Any]:
        """Generate a connection between two nodes."""
        weight = round(random.uniform(0.95, 0.99), 2)
        # For vertical connections in binary tree, use AND relation (1)
        # For other connections, randomly choose between SOLE (0), AND (1), or OR (2)
        relation = 1 if (is_vertical and self.binary_tree) else random.randint(0, 2)
        weight_key = "Weight-y" if is_vertical else "Weight-x"
        
        return {
            "ID": target_id,
            weight_key: weight,
            "Relation": relation
        }
    
    def generate(self) -> Dict[str, List[List[Dict[str, Any]]]]:
        """Generate a complete situation graph."""
        if self.binary_tree:
            return self._generate_binary_tree()
        else:
            return self._generate_random_graph()
            
    def _generate_binary_tree(self) -> Dict[str, List[List[Dict[str, Any]]]]:
        """Generate a binary tree structure with AND relations."""
        layers = []
        current_id = 100
        nodes_by_id = {}  # Keep track of all nodes by ID
        
        # First pass: create all nodes without connections
        layer_start_ids = []  # Keep track of starting ID for each layer
        for layer in range(self.num_layers):
            layer_nodes = []
            num_nodes = 2 ** layer
            layer_start_ids.append(current_id)
            
            for i in range(num_nodes):
                node_id = current_id + i
                node = {
                    "ID": node_id,
                    "Predecessors": None,
                    "Children": None,
                    "Duration": random.choice([1000, 2000, 3000]),
                    "Cycle": random.choice([0, 1000, 2000]),
                    "type": random.randint(0, 1)
                }
                layer_nodes.append(node)
                nodes_by_id[node_id] = node
            
            layers.append(layer_nodes)
            current_id += num_nodes
        
        # Second pass: establish connections
        for layer in range(self.num_layers - 1):  # Skip last layer as it has no children
            num_nodes = 2 ** layer
            layer_start_id = layer_start_ids[layer]
            next_layer_start = layer_start_ids[layer + 1]
            
            for i in range(num_nodes):
                node_id = layer_start_id + i
                node = nodes_by_id[node_id]
                
                # Add children connections
                child1_id = next_layer_start + i * 2
                child2_id = next_layer_start + i * 2 + 1
                node["Children"] = [
                    {
                        "ID": child1_id,
                        "Weight-y": round(random.uniform(0.95, 0.99), 2),
                        "Relation": 1  # AND relation
                    },
                    {
                        "ID": child2_id,
                        "Weight-y": round(random.uniform(0.95, 0.99), 2),
                        "Relation": 1  # AND relation
                    }
                ]
        
        return {"layers": layers}
    
    def _generate_random_graph(self) -> Dict[str, List[List[Dict[str, Any]]]]:
        """Generate a random graph structure."""
        layers = []
        nodes_by_id = {}
        
        # First pass: create all nodes without connections
        current_id = 100
        for layer in range(self.num_layers):
            layer_nodes = []
            for i in range(self.nodes_per_layer[layer]):
                node_id = current_id + i
                node = {
                    "ID": node_id,
                    "Predecessors": None,
                    "Children": None,
                    "Duration": random.choice([1000, 2000, 3000]),
                    "Cycle": random.choice([0, 1000, 2000]),
                    "type": random.randint(0, 1)
                }
                layer_nodes.append(node)
                nodes_by_id[node_id] = node
            layers.append(layer_nodes)
            current_id += 100
        
        # Second pass: establish vertical connections (between layers)
        for layer in range(self.num_layers - 1):  # Skip last layer as it has no children
            current_layer_nodes = layers[layer]
            next_layer_nodes = layers[layer + 1]
            
            for node in current_layer_nodes:
                # Each node should connect to 1-3 nodes in next layer
                num_children = random.randint(1, min(3, len(next_layer_nodes)))
                child_nodes = random.sample(next_layer_nodes, num_children)
                
                # If only one child, use SOLE relation
                # If multiple children, randomly choose between AND/OR
                relation = 0 if num_children == 1 else random.randint(1, 2)
                
                node["Children"] = [
                    {
                        "ID": child_node["ID"],
                        "Weight-y": round(random.uniform(0.95, 0.99), 2),
                        "Relation": relation
                    }
                    for child_node in child_nodes
                ]
        
        # Third pass: establish horizontal connections (within layers)
        for layer in range(self.num_layers):
            layer_nodes = layers[layer]
            for i in range(len(layer_nodes) - 1):
                if random.random() < 0.3:  # 30% chance of horizontal connection
                    source_node = layer_nodes[i]
                    target_node = layer_nodes[i + 1]
                    
                    # For horizontal connections, randomly choose between SOLE/AND/OR
                    relation = random.randint(0, 2)
                    
                    # Add connection as predecessor to target node
                    if target_node["Predecessors"] is None:
                        target_node["Predecessors"] = []
                    target_node["Predecessors"].append({
                        "ID": source_node["ID"],
                        "Weight-x": round(random.uniform(0.95, 0.99), 2),
                        "Relation": relation
                    })
        
        return {"layers": layers}
    
    def generate_specific_graph(self, relation_type: int) -> Dict[str, List[List[Dict[str, Any]]]]:
        """Generate a specific graph structure with A<-B, B<-C, B<-D, B<-E, and C->D.
        relation_type: 1 for AND, 2 for OR for B's children"""
        
        # Create nodes with specific IDs
        node_a = {
            "ID": 100,
            "Predecessors": None,
            "Children": [{
                "ID": 200,
                "Weight-y": 0.98,
                "Relation": 0  # SOLE
            }],
            "Duration": 1000,
            "Cycle": 0,
            "type": 0
        }
        
        node_b = {
            "ID": 200,
            "Predecessors": None,
            "Children": [
                {
                    "ID": 301,
                    "Weight-y": 0.98,
                    "Relation": relation_type  # AND/OR based on file type
                },
                {
                    "ID": 302,
                    "Weight-y": 0.98,
                    "Relation": relation_type
                },
                {
                    "ID": 303,
                    "Weight-y": 0.98,
                    "Relation": relation_type
                }
            ],
            "Duration": 1000,
            "Cycle": 0,
            "type": 0
        }
        
        node_c = {
            "ID": 301,
            "Predecessors": None,
            "Children": None,
            "Duration": 1000,
            "Cycle": 0,
            "type": 0
        }
        
        node_d = {
            "ID": 302,
            "Predecessors": [{
                "ID": 301,
                "Weight-x": 0.98,
                "Relation": 0  # SOLE
            }],
            "Children": None,
            "Duration": 1000,
            "Cycle": 0,
            "type": 0
        }
        
        node_e = {
            "ID": 303,
            "Predecessors": None,
            "Children": None,
            "Duration": 1000,
            "Cycle": 0,
            "type": 0
        }
        
        return {
            "layers": [
                [node_a],           # Layer 1
                [node_b],           # Layer 2
                [node_c, node_d, node_e]  # Layer 3
            ]
        }

    def save_to_file(self, graph: Dict[str, Any], output_dir: str) -> str:
        """Save the generated graph to a JSON file."""
        timestamp = int(time.time())
        graph_type = "BinaryTree" if self.binary_tree else "Random"
        filename = f"SG_{graph_type}_L{self.num_layers}_N{'_'.join(map(str, self.nodes_per_layer))}_{timestamp}.json"
        filepath = os.path.join(output_dir, filename)
        
        os.makedirs(output_dir, exist_ok=True)
        with open(filepath, 'w') as f:
            json.dump(graph, f, indent=2)
        
        return filepath

def main():
    # Create output directory
    output_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(__file__))), 'files')
    os.makedirs(output_dir, exist_ok=True)
    timestamp = int(time.time())
    
    # Generate binary tree example (3 layers: 1-2-4 nodes)
    binary_generator = SituationGraphGenerator(3, [1, 2, 4], binary_tree=True)
    binary_graph = binary_generator.generate()
    binary_file = f"SG_BinaryTree_L3_{timestamp}.json"
    binary_path = os.path.join(output_dir, binary_file)
    with open(binary_path, 'w') as f:
        json.dump(binary_graph, f, indent=2)
    print(f"Generated binary tree graph saved to: {binary_path}")
    
    time.sleep(1)  # Ensure different timestamps
    
    # Generate random graph example (3 layers: 2-3-2 nodes)
    timestamp = int(time.time())
    random_generator = SituationGraphGenerator(3, [2, 3, 2], binary_tree=False)
    random_graph = random_generator.generate()
    random_file = f"SG_Random_L3_{timestamp}.json"
    random_path = os.path.join(output_dir, random_file)
    with open(random_path, 'w') as f:
        json.dump(random_graph, f, indent=2)
    print(f"Generated random graph saved to: {random_path}")

if __name__ == "__main__":
    main()
