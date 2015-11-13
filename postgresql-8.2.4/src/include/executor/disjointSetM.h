/*
 * disjointSetM.h
 *
 *  Created on: Sep 2, 2013
 *      Author: merlin
 */

#ifndef DISJOINTSETM_H_
#define DISJOINTSETM_H_

#include "postgres.h"
#include "executor/linkListM.h"

typedef struct element
{
	int rank; // This roughly represent the max height of the node in its subtree

	int index; // The index of the element the node represents

	int visited; //dfs for the dsets

	 tuple *data; //store the value inside thie element

	 struct element *parent; //this is for disjoint set problem
	 struct element *childListHead;//this is for disjoint set problem
	 struct element *childListEnd;
	 struct element *nextChild;

}element;

typedef struct DisJointSets
{
	element *m_nodes;
	int num_sets;
	int num_elements;
	int capacity;

}DisJointSets;

extern int FindSet(int tupleid1, DisJointSets *dssets);

extern  int unionSet(int setId1, int setId2, DisJointSets *dssets);

extern void FreeDJset(DisJointSets *dssets);

extern bool InsertElement(tuple *ptr, int index, DisJointSets *dsets, element *eptr);


#endif /* DISJOINTSETM_H_ */
