/*
 * nodeAgghelperSGBAny.c
 *
 *  Created on: Aug 31, 2013
 *      Author: merlin
 */

#include "executor/nodeAgghelperSGBAny.h"


bool judgeJoinGroup_Any(
		int numAttributes,
		TupleTableSlot *inputTuple,
		int EPS,
		int distanceMetric,
		AggHashEntry entry,
		TupleTableSlot *tmpslot,
		MemoryContext evalContext)
 {

	bool forward;
	bool eof_tuplestore;
	int index = 0;
	forward = true;

	Tuplestorestate *tuplestorestate;
	tuplestorestate=(Tuplestorestate *)entry->tuplestorestate;

	/********************************************************************************/
	tuplestore_rescan(tuplestorestate);
	eof_tuplestore = (tuplestorestate == NULL ) || tuplestore_ateof(tuplestorestate);
	//iterate the tuple store state list
	while (!eof_tuplestore) {
		//TupleTableSlot *slot;
		if (tuplestore_gettupleslot(tuplestorestate, forward, tmpslot)) {

			//check whether this slot inside the bound of inputslot
			if (InsideBound(tmpslot, inputTuple, EPS, distanceMetric, numAttributes)) {

				//printf("inside this group \n");
				return true;
			}

		} else {
			eof_tuplestore = true;
		}

	}					//while eof

	return false;
 }
