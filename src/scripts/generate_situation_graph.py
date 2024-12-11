#!/usr/bin/env python3

import json
import random
import time
import os
from typing import List, Dict, Any

class SituationGraphGenerator:
    def __init__(self, num_layers: int, nodes_per_layer: List[int]):
        """
        Initialize the generator with the number of layers and nodes per layer.
        
        Args:
            num_layers: Number of layers in the graph
            nodes_per_layer: List containing the number of nodes for each layer
        """
        self.num_layers = num_layers
        self.nodes_per_layer = nodes_per_layer
        self.node_ids = self._generate_node_ids()
        
    def _generate_node_ids(self) -> Dict[int, List[int]]:
        """Generate unique IDs for nodes in each layer."""
        node_ids = {}
        for layer in range(self.num_layers):
            base = (layer + 1) * 100  # Layer 0 -> 100s, Layer 1 -> 200s, etc.
            node_ids[layer] = [base + i + 1 for i in range(self.nodes_per_layer[layer])]
        return node_ids
    
    def _generate_node(self, layer: int, node_id: int, children: List[Dict], predecessors: List[Dict]) -> Dict[str, Any]:
        """Generate a single node with its properties."""
        duration = random.choice([0, 1000, 3000])
        cycle = random.choice([0, 1000, 1500, 2000, 2500]) if duration != 3000 else 0
        node_type = random.randint(0, 1)
        
        return {
            "ID": node_id,
            "Predecessors": predecessors,
            "Children": children,
            "Duration": duration,
            "Cycle": cycle,
            "type": node_type
        }
    
    def _generate_connection(self, source_id: int, target_id: int, is_vertical: bool) -> Dict[str, Any]:
        """Generate a connection between two nodes."""
        weight = round(random.uniform(0.95, 0.99), 2)
        # Only use relation types 1 and 2 for vertical connections
        relation = random.randint(1, 2) if is_vertical else 1  # Changed to always use case 1 for horizontal
        weight_key = "Weight-y" if is_vertical else "Weight-x"
        
        return {
            "ID": target_id,
            weight_key: weight,
            "Relation": relation
        }
    
    def generate(self) -> Dict[str, List[List[Dict[str, Any]]]]:
        """Generate a complete situation graph."""
        layers = []
        connections = {layer: {} for layer in range(self.num_layers)}
        
        # First pass: create vertical connections (between layers)
        for layer in range(self.num_layers - 1):
            current_layer_nodes = self.node_ids[layer]
            next_layer_nodes = self.node_ids[layer + 1]
            
            # Ensure 95-100% connectivity
            min_connections = max(1, int(0.95 * len(next_layer_nodes)))
            
            for source_id in current_layer_nodes:
                num_connections = random.randint(min_connections, len(next_layer_nodes))
                targets = random.sample(next_layer_nodes, num_connections)
                connections[layer][source_id] = [
                    self._generate_connection(source_id, target, True)
                    for target in targets
                ]
        
        # Second pass: create horizontal connections (within layers)
        for layer in range(self.num_layers):
            layer_nodes = self.node_ids[layer]
            for i in range(len(layer_nodes) - 1):
                if random.random() < 0.3:  # 30% chance of horizontal connection
                    source_id = layer_nodes[i]
                    target_id = layer_nodes[i + 1]
                    if layer not in connections:
                        connections[layer] = {}
                    if target_id not in connections[layer]:
                        connections[layer][target_id] = []
                    connections[layer][target_id].append(
                        self._generate_connection(source_id, target_id, False)
                    )
        
        # Third pass: create the final graph structure
        for layer in range(self.num_layers):
            layer_nodes = []
            for node_id in self.node_ids[layer]:
                children = connections[layer].get(node_id, None)
                predecessors = []
                
                # Find predecessors from previous layer
                if layer > 0:
                    for prev_id, prev_connections in connections[layer - 1].items():
                        for conn in prev_connections:
                            if conn["ID"] == node_id:
                                predecessors.append(self._generate_connection(prev_id, node_id, False))
                
                # Find horizontal predecessors
                if layer in connections:
                    for source_id, horiz_connections in connections[layer].items():
                        for conn in horiz_connections:
                            if conn["ID"] == node_id:
                                predecessors.append(conn)
                
                node = self._generate_node(
                    layer,
                    node_id,
                    children,
                    predecessors if predecessors else None
                )
                layer_nodes.append(node)
            
            layers.append(layer_nodes)
        
        return {"layers": layers}
    
    def save_to_file(self, graph: Dict[str, Any], output_dir: str) -> str:
        """Save the generated graph to a JSON file."""
        timestamp = int(time.time())
        filename = f"SG_L{self.num_layers}_N{'_'.join(map(str, self.nodes_per_layer))}_{timestamp}.json"
        filepath = os.path.join(output_dir, filename)
        
        os.makedirs(output_dir, exist_ok=True)
        with open(filepath, 'w') as f:
            json.dump(graph, f, indent=2)
        
        return filepath

def main():
    # Example usage
    num_layers = 3
    nodes_per_layer = [1, 3, 6]  # Similar to SG2.json structure
    
    generator = SituationGraphGenerator(num_layers, nodes_per_layer)
    graph = generator.generate()
    
    # Save to files directory
    output_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(__file__))), 'files')
    filepath = generator.save_to_file(graph, output_dir)
    print(f"Generated situation graph saved to: {filepath}")

if __name__ == "__main__":
    main()
