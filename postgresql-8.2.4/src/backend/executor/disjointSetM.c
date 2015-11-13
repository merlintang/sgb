/*
 * disjointSetM.c
 *
 *  Created on: Sep 2, 2013
 *      Author: merlin
 */

#include "executor/disjointSetM.h"


void printElement(element *ptr) {
	if (ptr != NULL )
		printf("id is %d \n", ptr->index);

}

bool InsertElement(tuple *ptr, int index, DisJointSets *dsets, element *eptr) {
	if (index > dsets->capacity || dsets == NULL || ptr == NULL )
		return false;

	eptr = &(dsets->m_nodes[index]);

	eptr->childListEnd = NULL;
	eptr->childListHead = NULL;
	eptr->data = ptr;
	eptr->index = index;
	eptr->nextChild = NULL;
	eptr->parent = NULL;
	eptr->rank = 0;
	eptr->visited = 0;
	dsets->num_elements++;
	dsets->num_sets++;
	return true;
}

void FreeDJset(DisJointSets *dssets) {
	int i = 0;
	while (i < dssets->num_elements) {
		//printf("set id is %d \n", FindSet(i,dssets));
		if (dssets->m_nodes[i].data != NULL ) {
			free(dssets->m_nodes[i].data);
		}

		i++;
	}

	free(dssets);
}



int FindSet(int tupleid1, DisJointSets *dssets) {

	if (tupleid1 > dssets->num_elements) {
		printf("tuple range error !!! \n");
		return 0;
	}

	element *currtnode = &(dssets->m_nodes[tupleid1]);

	while (currtnode->parent != NULL )
		currtnode = currtnode->parent;

	element* root = currtnode;

	// Walk to the root, updating the parents of `elementId`. Make those elements the direct
	// children of `root`. This optimizes the tree for future FindSet invokations.
	currtnode = &(dssets->m_nodes[tupleid1]);

	while (currtnode != root&& root!=currtnode->parent) {

		element* next = currtnode->parent;
		currtnode->parent = root;

		if (root->childListEnd == NULL ) {
			root->childListHead = currtnode;
			root->childListEnd = currtnode;
		} else {
			root->childListEnd->nextChild = currtnode;
			root->childListEnd = currtnode;
		}

		currtnode = next;
	}

	return root->index;
}

int unionSet(int setId1, int setId2, DisJointSets *dssets) {

	if (setId1 > dssets->num_elements || setId2 > dssets->num_elements) {
		printf("tuple range error !!! \n");
		return 0;
	}

	if (setId1 == setId2)
		return -1; // already unioned

	element* set1 = &(dssets->m_nodes[setId1]);
	element* set2 = &(dssets->m_nodes[setId2]);

	// Determine which node representing a set has a higher rank. The node with the higher rank is
	// likely to have a bigger subtree so in order to better balance the tree representing the
	// union, the node with the higher rank is made the parent of the one with the lower rank and
	// not the other way around.

	int setid = 0;

	if (set1->rank > set2->rank) {
		set2->parent = set1;
		if (set1->childListEnd == NULL ) {
			set1->childListHead = set2;
			set1->childListEnd = set2;
		} else {
			set1->childListEnd->nextChild = set2;
			set1->childListEnd = set2;
		}

		set1->rank = set1->rank + set2->rank;
		setid = set1->index;
	} else if (set1->rank < set2->rank) {
		set1->parent = set2;

		if (set2->childListEnd == NULL ) {
			set2->childListHead = set1;
			set2->childListEnd = set1;
		} else {
			set2->childListEnd->nextChild = set1;
			set2->childListEnd = set1;
		}

		set2->rank = set1->rank + set2->rank;
		setid = set2->index;
	} else // set1->rank == set2->rank
	{
		set2->parent = set1;

		if (set1->childListEnd == NULL ) {
			set1->childListHead = set2;
			set1->childListEnd = set2;
		} else {
			set1->childListEnd->nextChild = set2;
			set1->childListEnd = set2;
		}

		++set1->rank; // update rank
		setid = set1->index;
	}

	// Since two sets have fused into one, there is now one less set so update the set count.
	--dssets->num_sets;

	return setid;

}
