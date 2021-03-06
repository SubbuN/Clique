# Clique

This application finds the maximum clique size and at least one instance of maximum clique of given graph.

Input : Graph in DIMACS binary format.

Output : Maximum clique size plus an instance of maximum clique


## Explanation of the algorithm

[pdf](https://github.com/SubbuN/Clique/tree/master/doc/slides/MaximumClique.pdf)

[tex](https://github.com/SubbuN/Clique/tree/master/doc/slides/MaximumClique.tex)


## Algorithm

[FindClique] (https://github.com/SubbuN/Clique/tree/master/src/Clique.cpp)

The main algorithm is implemented in [TryFindClique](https://github.com/SubbuN/Clique/tree/master/src/Clique.cpp)

Notes:
	For performance and simplicity, a self loop edge is created for each vertex in the graph. Therefore
input graph is cloned for processing before being passed to TryFindClique. The actual input graph is never modified.

	Callbacks and filters to avoid exploring certain path are not added in this implementation for simplicity.

