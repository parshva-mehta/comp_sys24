CC=gcc

all: project1BFS project1DFS BFS_part2 DFS_part2

project1BFS: project1BFS.c
	$(CC) project1BFS.c -o project1BFS

project1DFS: project1DFS.c
	$(CC) project1DFS.c -o project1DFS

BFS_part2: BFS_part2.c
	$(CC) BFS_part2.c -o BFS_part2

DFS_part2: DFS_part2.c
	$(CC) DFS_part2.c -o DFS_part2

clean:
	rm -f project1BFS project1DFS BFS_part2 DFS_part2