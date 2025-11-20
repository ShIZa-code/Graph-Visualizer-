# Graph-Visualizer-
This C++ program is a Graphical Graph Editor and Visualization Tool using WinBGIm. It enables users to build and modify weighted/directed graphs interactively (adding/deleting nodes and edges). Crucially, it provides a dynamic, step-by-step visualization of the Breadth-First Search (BFS) and Depth-First Search (DFS) traversal algorithms.

This is a powerful graphical application developed in C++ using the WinBGIm Graphics Library for interactively building, modifying, and visualizing graph structures and algorithms.
Interactive Graph Construction:
     Node/Vertex Management: Add and delete nodes by simple mouse clicks.  Nodes are automatically labeled (0, 1, 2, ...).
     Edge Management: Add and delete edges, including support for self-loops.
Graph Customization:
     Supports creation of Weighted/Unweighted and Directed/Undirected graphs (configured at startup).
     A popup window allows users to input custom edge weights when in weighted mode.
Algorithm Visualization (Core Feature):
  Provides dynamic, step-by-step visualizations for fundamental graph traversals:
     Breadth-First Search (BFS)
     Depth-First Search (DFS)
     Nodes change color in real-time to indicate the order in which they are visited.
User Experience (UX):
   Smooth Rendering: Uses double buffering (WinBGIm) for flicker-free graphical updates.
   Keyboard Shortcuts: Press U for Undo and Press R for Redo.
   Toolbar Interface: All modes (Add Node, Add Edge, BFS, DFS, Clear, etc.) are easily accessible via a clickable toolbar.

The project relies on efficient C++ Standard Library containers to model the graph, implement core algorithms, and manage the application state. The graph structure itself is primarily represented using an Adjacency List (std::vector<std::vector<std::pair<int, int>>> adj), which stores all connections (edges) and their associated weights, while the vertices are held in a std::vector<Node>, detailing each node's position, label, and state. For the critical traversal algorithms, a std::queue<int> is employed to maintain the FIFO (First-In, First-Out) order required by the Breadth-First Search (BFS), and a std::stack<int> is used to enforce the LIFO (Last-In, First-Out) behavior of the Depth-First Search (DFS). Finally, the Undo/Redo functionality is achieved by using two separate std::vector<Snapshot> containers, which behave as stacks to store and retrieve complete historical states of the graph structure.
