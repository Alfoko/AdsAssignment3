#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#include "ai.h"
#include "gate.h"
#include "radix.h"
#include "utils.h"

#define DEBUG 0

#define UP 'u'
#define DOWN 'd'
#define LEFT 'l'
#define RIGHT 'r'
char directions[] = {UP, DOWN, LEFT, RIGHT};
char invertedDirections[] = {DOWN, UP, RIGHT, LEFT};
char pieceNames[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

/**
 * Given a game state, work out the number of bytes required to store the state.
*/
int getPackedSize(gate_t *gate);

/**
 * Store state of puzzle in map.
*/
void packMap(gate_t *gate, unsigned char *packedMap);

/**
 * Check if the given state is in a won state.
 */
bool winning_state(gate_t gate);

gate_t* duplicate_state(gate_t* gate) {
	gate_t* duplicate = (gate_t*)malloc(sizeof(gate_t));
	/* Fill in */
	/*
	Hint: copy the src state as well as the dynamically allocated map and solution
	*/
	*duplicate = *gate;
	duplicate->map = (char **)malloc(sizeof(char*) * gate->lines);
	for (int i = 0; i < gate->lines; i++) {
		duplicate->map[i] = strdup(gate->map[i]);
	}
	
	if (gate->soln != NULL) {
		duplicate->soln = strdup(gate->soln);
	} else {
		duplicate->soln = strdup("");
	}

	return duplicate;
}

/**
 * Without lightweight states, the second argument may not be required.
 * Its use is up to your decision.
 */
void free_state(gate_t* stateToFree, gate_t *init_data) {
	//Fill in
	/*
	Hint:
	Free all of:
	dynamically allocated map strings
	dynamically allocated map string pointers
	solution string
	state
	*/

	// free dynamically allocated map strings
	for (int i = 0; i < stateToFree->lines; i++) {
		free(stateToFree->map[i]);
	}
	// free dynamically allocated map string pointers
	free(stateToFree->map);
	// free solution string
	free(stateToFree->soln);
	// free state
	free(stateToFree);
}

void free_initial_state(gate_t *init_data) {
	/* Frees dynamic elements of initial state data - including 
		unchanging state. */
	/* 
	Hint:
	Unchanging state:
	buffer
	map_save
	*/
	// free buffer
	free(init_data->buffer);
	// free map_save strings
	for (int i = 0; i < init_data->lines; i++) {
		free(init_data->map_save[i]);
	}
	// free map_save string pointers
	free(init_data->map_save);

	for (int i = 0; i < init_data->lines; i++) {
		free(init_data->map[i]);
	}
	free(init_data->map);

	if (init_data->soln) {
		free(init_data->soln);
	}
}

/* queue node for BFS */
struct qnode {
	gate_t *state;
	struct qnode *next;
};

/* simple queue head/tail for storing enqueued states */
static struct qnode *queue_head = NULL;
static struct qnode *queue_tail = NULL;

void enqueue_state(gate_t* stateToEnqueue) {
	struct qnode *node = (struct qnode*)malloc(sizeof(struct qnode));
	if (!node) {
		/* allocation failed - do nothing */
		return;
	}
	node->state = stateToEnqueue;
	node->next = NULL;

	if (queue_tail) {
		queue_tail->next = node;
		queue_tail = node;
	} else {
		queue_head = queue_tail = node;
	}
}

/**
 * Find a solution by exploring all possible paths
 */
void find_solution(gate_t* init_data) {
	/* Location for packedMap. */
	int packedBytes = getPackedSize(init_data);
	unsigned char *packedMap = (unsigned char *) calloc(packedBytes, sizeof(unsigned char));
	assert(packedMap);

	bool has_won = false;
	int dequeued = 0;
	int enqueued = 0;
	int duplicatedNodes = 0;
	char *soln = "";
	double start = now();
	double elapsed;
	
	// Algorithm 1 is a width n + 1 search
	int w = init_data->num_pieces + 1;

	// Initialize solution string
	init_data->soln = NULL;

	/*
	 * Algorithm 1:
	 * n <- createInitNode(init_data)
	 * numPieces <- getNumPieces(init_data)
	 * Enqueue(n)
	 * while not Empty(Queue)
	 *     n <- Dequeue()
	 *    exploredNodes <- exploredNodes + 1
	 *     if WinningCondition(n) then
	 *         solution <- SaveSolution(n)
	 * 	       solutionSize <- n.depth
	 *         break
	 *    for each move action a (u, d, l, r) * numPieces do
	 * 	      pieceMoved <- ApplyAction(n, NewNode, a)
	 * 	      generatedNodes <- generatedNodes + 1
	 *        if !pieceMoved then
	 *            Free(NewNode)
	 *            continue
	 * 	      Queue.Enqueue(NewNode)
	 */
	
	// n <- createInitNode(init_data)
	gate_t* n = duplicate_state(init_data);
	gate_t* winning_state_ptr = NULL;
	// numPieces <- getNumPieces(init_data)
	int numPieces = init_data->num_pieces;
	// Enqueue(n)
	enqueue_state(n);
	enqueued++;

	// while not Empty(Queue)
	while (queue_head != NULL) {
		// n <- Dequeue()
		gate_t* n = queue_head->state;
		struct qnode *old_head = queue_head;
		queue_head = queue_head->next;
		if (queue_head == NULL) {
			queue_tail = NULL;
		}
		free(old_head);
		dequeued++;

		// if WinningCondition(n) then
		if (winning_state(*n)) {
			// solution <- SaveSolution(n)
			soln = strdup(n->soln);
			winning_state_ptr = n;
			// solutionSize <- n.depth
			has_won = true;
			break;
		}

		// for each move action a (u, d, l, r) * numPieces do
		for (int piece = 0; piece < numPieces; piece++) {
			for (int dir = 0; dir < 4; dir++) {
				// pieceMoved <- ApplyAction(n, NewNode, a)
				gate_t* newNode = duplicate_state(n);
				*newNode = attempt_move(*newNode, pieceNames[piece], directions[dir]);
				enqueued++;
				// if !pieceMoved then
				if (memcmp(newNode, n, sizeof(gate_t)) == 0) {
					// Free(NewNode)
					free_state(newNode, init_data);
					duplicatedNodes++;
					continue;
				}

				// Format: piece name + direction (e.g., "0u", "1d", "2l", "3r")
				int oldLen = strlen(newNode->soln);
				char* newSoln = (char*)malloc(oldLen + 3); // +2 for piece+dir, +1 for null
				strcpy(newSoln, newNode->soln);
				newSoln[oldLen] = pieceNames[piece];
				newSoln[oldLen + 1] = directions[dir];
				newSoln[oldLen + 2] = '\0';
				free(newNode->soln);
				newNode->soln = newSoln;

				// Enqueue(NewNode)
				enqueue_state(newNode);
			}
		}
		free_state(n, init_data);
	}

	/* Output statistics */
	elapsed = now() - start;
	printf("Solution path: ");
	printf("%s\n", soln);
	printf("Execution time: %lf\n", elapsed);
	printf("Expanded nodes: %d\n", dequeued);
	printf("Generated nodes: %d\n", enqueued);
	printf("Duplicated nodes: %d\n", duplicatedNodes);
	int memoryUsage = 0;
	// Algorithm 2: Memory usage, uncomment to add.
	// memoryUsage += queryRadixMemoryUsage(radixTree);
	// Algorithm 3: Memory usage, uncomment to add.
	// for(int i = 0; i < w; i++) {
	//	memoryUsage += queryRadixMemoryUsage(rts[i]);
	// }
	printf("Auxiliary memory usage (bytes): %d\n", memoryUsage);
	printf("Number of pieces in the puzzle: %d\n", init_data->num_pieces);
	printf("Number of steps in solution: %ld\n", strlen(soln)/2);
	int emptySpaces = 0;
	
	// count the number of empty spaces in the map
	for (int i = 0; i < init_data->lines; i++) {
		for (int j = 0; init_data->map_save[i][j] != '\0'; j++) {
			if (init_data->map_save[i][j] == ' ') {
				emptySpaces++;
			}
		}
	}
	
	printf("Number of empty spaces: %d\n", emptySpaces);
	printf("Solved by IW(%d)\n", w);
	printf("Number of nodes expanded per second: %lf\n", (dequeued + 1) / elapsed);

	// Free remaining states in the queue
	while (queue_head != NULL) {
		gate_t* stateToFree = queue_head->state;
		struct qnode *old_head = queue_head;
		queue_head = queue_head->next;
		free_state(stateToFree, init_data);
		free(old_head);
	}
	queue_tail = NULL;

	if (winning_state_ptr) {
		free_state(winning_state_ptr, init_data);
	}

	if (soln) {
		free(soln);
	}

	/* Free associated memory. */
	if(packedMap) {
		free(packedMap);
	}
	/* Free initial map. */
	free_initial_state(init_data);
}

/**
 * Given a game state, work out the number of bytes required to store the state.
*/
int getPackedSize(gate_t *gate) {
	int pBits = calcBits(gate->num_pieces);
    int hBits = calcBits(gate->lines);
    int wBits = calcBits(gate->num_chars_map / gate->lines);
    int atomSize = pBits + hBits + wBits;
	int bitCount = atomSize * gate->num_pieces;
	return bitCount;
}

/**
 * Store state of puzzle in map.
*/
void packMap(gate_t *gate, unsigned char *packedMap) {
	int pBits = calcBits(gate->num_pieces);
    int hBits = calcBits(gate->lines);
    int wBits = calcBits(gate->num_chars_map / gate->lines);
	int bitIdx = 0;
	for(int i = 0; i < gate->num_pieces; i++) {
		for(int j = 0; j < pBits; j++) {
			if(((i >> j) & 1) == 1) {
				bitOn( packedMap, bitIdx );
			} else {
				bitOff( packedMap, bitIdx );
			}
			bitIdx++;
		}
		for(int j = 0; j < hBits; j++) {
			if(((gate->piece_y[i] >> j) & 1) == 1) {
				bitOn( packedMap, bitIdx );
			} else {
				bitOff( packedMap, bitIdx );
			}
			bitIdx++;
		}
		for(int j = 0; j < wBits; j++) {
			if(((gate->piece_x[i] >> j) & 1) == 1) {
				bitOn( packedMap, bitIdx );
			} else {
				bitOff( packedMap, bitIdx );
			}
			bitIdx++;
		}
	}
}

/**
 * Check if the given state is in a won state.
 */
bool winning_state(gate_t gate) {
	for (int i = 0; i < gate.lines; i++) {
		for (int j = 0; gate.map_save[i][j] != '\0'; j++) {
			if (gate.map[i][j] == 'G' || (gate.map[i][j] >= 'I' && gate.map[i][j] <= 'Q')) {
				return false;
			}
		}
	}
	return true;
}

void solve(char const *path)
{
	/**
	 * Load Map
	*/
	gate_t gate = make_map(path, gate);
	
	/**
	 * Verify map is valid
	*/
	map_check(gate);

	/**
	 * Locate player x, y position
	*/
	gate = find_player(gate);

	/**
	 * Locate each piece.
	*/
	gate = find_pieces(gate);
	
	gate.base_path = path;

	find_solution(&gate);

}
