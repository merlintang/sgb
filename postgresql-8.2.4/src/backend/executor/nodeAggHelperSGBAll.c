/*
 * nodeAggHelperSGBAll.c
 *
 *  Created on: Aug 28, 2013
 *      Author: merlin
 *
 * this is working for the SGBAll
 * Method 2:
 * L-infinity:recatngle
 * L-2: rectangle+convexhull
 */

#include "executor/nodeAggHelperSGBAll.h"


static bool
InsideBound_tuple(tuple *slot, TupleTableSlot *inputslot, int distanceall,
		int distanceMetric, int numAttributes);

void copyfromTupleTableToArray(int *des, int*des2, TupleTableSlot *inputslot,
		int numAttributes);


static bool
InsideBound_tuple(tuple *slot, TupleTableSlot *inputslot, int distanceall,
		int distanceMetric, int numAttributes);

void copyfromTupleTableToArray(int *des, int*des2, TupleTableSlot *inputslot,
		int numAttributes);

static tmpGroup *
changeToTmpGroup(Tuplestorestate *tuplestorestate, TupleTableSlot *tmpSlot,
		int numAtt, int eps);

static tmpGroup *
findIntersection(TupleTableSlot *inputslot, TupleTableSlot *tmpslot,
		Tuplestorestate *tuplestorestate, int distanceall, int distanceMetric,
		int numAttributes);


/**
 * put the tuples value into the tuplestorestage
 */
static void tuplestorestate_puttuple(tuple *input, Tuplestorestate * tuplestore,
		TupleTableSlot *aggslot, TupleTableSlot *inputslot, int numAttributes) {

	Datum *values = (Datum *) palloc(numAttributes * sizeof(Datum)); /*array*/
	bool * nulls = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
	bool * replace = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
	memset(values, 0, numAttributes * sizeof(Datum));
	memset(nulls, false, numAttributes * sizeof(bool));
	memset(replace, false, numAttributes * sizeof(bool));

	int i = 0;
	for (; i < numAttributes; i++) {
		//printf(" %d ", tupleptr->values[i]);
		values[i] = input->values[i];
		replace[i] = true;
	}

	HeapTuple tmpnewHeapTuple;
	tmpnewHeapTuple = heap_modify_tuple(inputslot->tts_tuple,
			inputslot->tts_tupleDescriptor, values, nulls, replace);

	ExecStoreTuple(tmpnewHeapTuple, aggslot, InvalidBuffer, true );

	//print_slot(aggslot);

	if (tuplestore) {
		//printf("possible error here 1092 \n");
		tuplestore_puttupleslot(tuplestore, aggslot);
	}
	//printf("put tuple into the memo list done \n");

	pfree(values);
	pfree(nulls);
	pfree(replace);

}

/**
 * put the tuples value into the tupletableslot
 */
void puttuple_TupleTableSlot(tuple *input, TupleTableSlot *aggslot,
		TupleTableSlot * inputslot, int numAttributes) {

	Datum attrPivot1, attrPivot2, attrCurrent;
	bool isNull1, isNull2, isNull3;

	Datum *values = (Datum *) palloc(numAttributes * sizeof(Datum)); /*array*/
	bool * nulls = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
	bool * replace = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
	memset(values, 0, numAttributes * sizeof(Datum));
	memset(nulls, false, numAttributes * sizeof(bool));
	memset(replace, false, numAttributes * sizeof(bool));

	int i = 0;
	for (; i < numAttributes; i++) {
		//printf(" %d ", input->values[i]);
		values[i] = input->values[i];
		replace[i] = true;
	}

	//printf("get value %d , %d \n", values[0], values[1]);

	HeapTuple tmpnewHeapTuple;/*CHANGED BY Mingjie*/

	//print_slot(inputslot);
	//print_slot(aggslot);

	tmpnewHeapTuple = heap_modify_tuple(inputslot->tts_tuple,
			inputslot->tts_tupleDescriptor, values, nulls, replace);

	ExecStoreTuple(tmpnewHeapTuple, aggslot, InvalidBuffer, true );

	pfree(values);
	pfree(nulls);
	pfree(replace);

}

/**
 * put the inputslot into the des tmpgroup
 */
 void tmpGroup_puttupleslot(tmpGroup * tuplestorestatePTR,
		TupleTableSlot * inputslot, int numAttributes, bool isfirst) {
	int index;
	Datum attrPivot1;
	bool isNull1;
	/*********************************************************/
	//while(index<=numAttributes)
	int value[2];

	//printf("get value from here\n");

	for (index = 0; index < numAttributes; index++) {
		attrPivot1 = slot_getattr(inputslot, index + 1, &isNull1);
		//printf("!!!!!!!!!!!data: %d \n",DatumGetInt32(attrPivot1));
		value[index] = DatumGetInt32(attrPivot1);
	}

	if (isfirst == true ) {

		tuplestorestatePTR->tuples_headPtr = append_data_IntoTupleList(
				tuplestorestatePTR->tuples_headPtr, value, numAttributes);
		tuplestorestatePTR->endPtr = tuplestorestatePTR->tuples_headPtr;

	} else {
		if (tuplestorestatePTR->endPtr == NULL ) {
			tuplestorestatePTR->tuples_headPtr = append_data_IntoTupleList(
					tuplestorestatePTR->tuples_headPtr, value, numAttributes);
			tuplestorestatePTR->endPtr = tuplestorestatePTR->tuples_headPtr;

		} else {
			//printf("error to put tuple into the tuple endPtr is NULL %d, %d \n",tuplestorestatePTR->endPtr->values[0], tuplestorestatePTR->endPtr->values[1]);
			tuplestorestatePTR->endPtr = append_data_IntoTupleList(
					tuplestorestatePTR->tuples_headPtr, value, numAttributes);
		}

	}

	tuplestorestatePTR->member_len++;
}


/*CHANGED BY MINGJIE*/
/*update piovt bounds*/
void updateInputSlotbyEPS(int *pivotslot1, int *pivotslot2, int numAttributes,
		int EPS) {
	//int numAttributes=2; //we assume the group attribute are the first two column

	/*for loop to update the pivotslot*/
	/*update the pivot value 1*/

	if (numAttributes == 1) {
		pivotslot1[0] = pivotslot1[0] - EPS;
		pivotslot2[0] = pivotslot2[0] + EPS;

	} else if (numAttributes == 2) {
		pivotslot1[0] = pivotslot1[0] - EPS;
		pivotslot1[1] = pivotslot1[1] - EPS;
		//printf("right center: %d, %d", pivotslot1[0], pivotslot1[1]);

		/*update the pivot value 2*/
		pivotslot2[0] = pivotslot2[0] + EPS;
		pivotslot2[1] = pivotslot2[1] + EPS;
		//printf("left center: %d, %d", pivotslot2[0], pivotslot2[1]);
	}

}

/*CAHNGED BY MINGJIE
 * judge whether the new tuple can join this group bound.
 * if can join this group, update the pivot1 and pivot2 value by EPS
 * */
bool judgeJoinGroup(int numAttributes, int *pivotslot1, int *pivotslot2,
		TupleTableSlot *inputTuple, int EPS, int distanceMetric, bool *overlap,
		MemoryContext evalContext) {
	MemoryContext oldContext;
	bool join = true;

	Datum attrCurrent;
	bool isNull3;
	/* Reset and switch into the temp context. */
	MemoryContextReset(evalContext);
	oldContext = MemoryContextSwitchTo(evalContext);

	//printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! judge input is: \n ");
	//print_slot(inputTuple);
	//Tuplestorestate *tuplestorestate;

	//int distanceMetric;
	int index = 0;
	int leftc_x, leftc_y, rightc_x, rightc_y;
	leftc_x = 0;
	leftc_y = 0;
	rightc_x = 0;
	rightc_y = 0;

	*overlap = true;
	//printf("***************** \n");
	//we assume our data is two dimensions
	while (index < numAttributes) {

		attrCurrent = slot_getattr(inputTuple, index + 1, &isNull3);

		if (isNull3) {
			//error here
			MemoryContextSwitchTo(oldContext);
			return false ;
		}

		int x1, x2, x3;
		x1 = pivotslot1[index];
		x2 = pivotslot2[index];
		x3 = DatumGetInt32(attrCurrent);
		//printf("bound x1 %d, bound x2 %d, input x3 %d \n", x1, x2, x3);

		if (index == 0) {

			if (x3 < x1 || x3 > x2) {
				join = false;
				//judge overlap here
				//return join;
				if (x3 + EPS < x1 || x3 - EPS > x2) {
					*overlap = false;
				}

				leftc_x = x1;
				rightc_x = x2;

				//inside the bound
			} else {
				//this is minum bound
				if (abs(x1 - x2) <= EPS) {
					leftc_x = x1;
					rightc_x = x2;
				} else {
					if (x3 - EPS > x1) {
						leftc_x = x3 - EPS;
					} else {
						leftc_x = x1;
					}

					if (x3 + EPS < x2) {
						rightc_x = x3 + EPS;
					} else {
						rightc_x = x2;
					}
				}
			}

		} else if (index == 1) {
			if (x3 < x1 || x3 > x2) {
				join = false;
				//return join;
				if (x3 + EPS < x1 || x3 - EPS > x2) {
					*overlap = false;
					//printf("!!!!!!!!!!!!!!!!!!!!!!!!! wrong\n");
				}
				leftc_y = x1;
				rightc_y = x2;

			} else {

				//this is the min rectangle distance
				if (abs(x1 - x2) <= EPS) {
					leftc_y = x1;
					rightc_y = x2;
				} else {
					//update the bound
					if (x3 - EPS > x1) {
						leftc_y = x3 - EPS;
					} else {
						leftc_y = x1;
					}

					if (x3 + EPS < x2) {
						rightc_y = x3 + EPS;
					} else {
						rightc_y = x2;
					}
				}

			}					//else can join this group
		}

		index++;
	}//while


	//if this tuple can join this group
	if (join) {
		if (numAttributes == 1) {
			pivotslot1[0] = leftc_x;
			pivotslot2[0] = rightc_x;

		} else if (numAttributes == 2) {
			pivotslot1[0] = leftc_x;
			pivotslot1[1] = leftc_y;
			pivotslot2[0] = rightc_x;
			pivotslot2[1] = rightc_y;

		}
	}

	MemoryContextSwitchTo(oldContext);
	return join;

}

/**
 * (1) find the intersction and put the results into the memolist
 * (2)
 */
tmpGroup *
findIntersection_putIntoMemoList(
		TupleTableSlot *inputslot,
		TupleTableSlot *tmpslot,
		TupleTableSlot *bufferslot,
		tmpGroup *tuplestorestate,
		Tuplestorestate *memolist,
		Tuplestorestate *globalmemolist,
		bool *iisempty,
		bool *entryNULL,
		int distanceall,
		int distanceMetric,
		int numAttributes) {


	int index = 0;

	int rx = rand() % 10000;
	int ry = rand() % 10000;
	tmpGroup* tmp = constructGroup_nohash(numAttributes, rx, ry, false );

	int att_index = 0;
	Datum attrPivot1;
	bool isNull1;

	tuple *endPtr = tmp->tuples_headPtr;
	/****************************************************************************/

	tuple *tupleHeadptr = tuplestorestate->tuples_headPtr;
	tuple *tuplePreptr = tupleHeadptr;

	bool firsttupleptr = true;

	while (tupleHeadptr != NULL ) {

		bool candelete = false;


		if (InsideBound_tuple(tupleHeadptr, inputslot, distanceall,distanceMetric, numAttributes))
		{
			*iisempty = false;

			if (tuplestorestate->member_len == 1) {
				*entryNULL = true;
				//printf("*111 this group is null \n");
				break;
			}

			updatePivotByTuple_tmpGroup(tmp, tupleHeadptr->values,numAttributes, distanceall);
			tmp->member_len++;
			index++;

			/***********************/
			puttuple_TupleTableSlot(tupleHeadptr,bufferslot,inputslot, numAttributes);
			if (globalmemolist) {
							//printf("5357 \n");
				  tuplestore_puttupleslot(globalmemolist, bufferslot);
				}

			//delete this tuple from the linklist
			if (firsttupleptr) {

				tuplestorestate->tuples_headPtr = tupleHeadptr->next;

				if (tupleHeadptr->values[0]== tuplestorestate->endPtr->values[0]
						&& tupleHeadptr->values[1]== tuplestorestate->endPtr->values[1])
				{
					tuplestorestate->endPtr = tuplestorestate->tuples_headPtr;
					//printf("equal operation here 3 \n");
				}

				pfree(tupleHeadptr);
				tupleHeadptr=NULL;
				tupleHeadptr = tuplestorestate->tuples_headPtr;
			} else {

				tuplePreptr->next = tupleHeadptr->next;

				if (tupleHeadptr->values[0]== tuplestorestate->endPtr->values[0]
					&& tupleHeadptr->values[1]== tuplestorestate->endPtr->values[1]) {

					tuplestorestate->endPtr = tuplePreptr;
					//printf("equal operation here 4 \n");
				}

				pfree(tupleHeadptr);
				tupleHeadptr=NULL;
				tupleHeadptr = tuplePreptr->next;
				candelete = true;
			}

			tuplestorestate->member_len--;
		}

		firsttupleptr = false;

		if (candelete == false )
			tuplePreptr = tupleHeadptr;

		if (tupleHeadptr != NULL && candelete == false )
			tupleHeadptr = tupleHeadptr->next;

		//tupleHeadptr=tupleHeadptr->next;
	}

	/****************************************************************************/
	//no intersection points
	if (tmp->member_len == 0) {
		return tmp;
	}

	//have intersection, puts new tuple into the group
	int value[5];
	value[0] = 0;
	value[1] = 0;
	for (att_index = 0; att_index < numAttributes; att_index++) {
		attrPivot1 = slot_getattr(inputslot, att_index + 1, &isNull1);
		//printf("***index: %d \n", index);
		if (isNull1) {
			break;
		}
		value[att_index] = DatumGetInt32(attrPivot1);
	}

	updatePivotByTuple_tmpGroup(tmp, value, numAttributes, distanceall);
	tmp->member_len++;
	index++;

	return tmp;

}


/**
 * (1) find the intersection and put the results into the tmpGroup
 * (2) use the tuple in the tupel group to build convex hull
 */
tmpGroup *
findIntersection_putIntoMemoList_Convex(
		TupleTableSlot *inputslot,
		TupleTableSlot *tmpslot,
		TupleTableSlot *bufferslot,
		tmpGroup *tuplestorestate,
		Tuplestorestate *memolist,
		Tuplestorestate *globalmemolist,
		bool *iisempty,
		bool *entryNULL,
		int distanceall,
		int distanceMetric,
		int numAttributes) {

	bool forward;
		bool eof_tuplestore;
		int index = 0;
		forward = true;

		int rx = rand() % 10000;
		int ry = rand() % 10000;
		tmpGroup* tmp = constructGroup_nohash(numAttributes, rx, ry, false );

		int att_index = 0;
		Datum attrPivot1;
		bool isNull1;

		tuple *endPtr = tmp->tuples_headPtr;

		/********************************************************************************/
		tuple *tupleHeadptr = tuplestorestate->tuples_headPtr;
		tuple *tuplePreptr = tupleHeadptr;

		bool firsttupleptr = true;

		while (tupleHeadptr != NULL ) {

			bool candelete = false;

			if (InsideBound_tuple(tupleHeadptr, inputslot, distanceall,distanceMetric, numAttributes))
			{

				//printf("tuple is %d, %d \n",tupleHeadptr->values[0],tupleHeadptr->values[1] );

				*iisempty = false;

				if (tuplestorestate->member_len == 1) {
					*entryNULL = true;
					//printf("*111 this group is null \n");
					break;
				}

				if (index == 0) {
					tmp->tuples_headPtr = append_data_IntoTupleList(tmp->tuples_headPtr, tupleHeadptr->values,numAttributes);
					endPtr = tmp->tuples_headPtr;

				} else {
					endPtr = append_data_IntoTupleList(endPtr, tupleHeadptr->values,numAttributes);
				}

				updatePivotByTuple_tmpGroup(tmp, tupleHeadptr->values,numAttributes, distanceall);
				//put_hash(tmp, tupleHeadptr->values, numAttributes);
				tmp->member_len++;
				index++;

				/***********************/
				//printf("intersection operation put tuple into \n");
				tuplestorestate_puttuple(tupleHeadptr, memolist, bufferslot,inputslot, numAttributes);

				if (globalmemolist) {
					//printf("5357 \n");
					tuplestore_puttupleslot(globalmemolist, bufferslot);
				}
				/***********************/

				//delete this tuple from the linklist
				if (firsttupleptr) {

					tuplestorestate->tuples_headPtr = tupleHeadptr->next;

					if (tupleHeadptr->values[0]== tuplestorestate->endPtr->values[0]
							&& tupleHeadptr->values[1]== tuplestorestate->endPtr->values[1])
					{
						tuplestorestate->endPtr = tuplestorestate->tuples_headPtr;
						//printf("equal operation here 3 \n");
					}

					pfree(tupleHeadptr);

					tupleHeadptr = tuplestorestate->tuples_headPtr;
				} else {
					tuplePreptr->next = tupleHeadptr->next;

					if (tupleHeadptr->values[0]== tuplestorestate->endPtr->values[0]
						&& tupleHeadptr->values[1]== tuplestorestate->endPtr->values[1]) {

						tuplestorestate->endPtr = tuplePreptr;
						//printf("equal operation here 4 \n");
					}

					pfree(tupleHeadptr);
					tupleHeadptr = tuplePreptr->next;
					candelete = true;
				}

				//printf("intersection operation free tuple  \n");
				tuplestorestate->member_len--;
			}

			firsttupleptr = false;

			if (candelete == false )
				tuplePreptr = tupleHeadptr;

			if (tupleHeadptr != NULL && candelete == false )
				tupleHeadptr = tupleHeadptr->next;

			//tupleHeadptr=tupleHeadptr->next;
		}

		/****************************************************************************/

		//no intersection points
		if (tmp->member_len == 0) {
			return tmp;
		}

		//have intersection, puts new tuple into the group
		int value[2];
		value[0] = 0;
		value[1] = 0;
		for (att_index = 0; att_index < numAttributes; att_index++) {
			attrPivot1 = slot_getattr(inputslot, att_index + 1, &isNull1);
			//printf("***index: %d \n", index);
			if (isNull1) {
				break;
			}
			value[att_index] = DatumGetInt32(attrPivot1);
		}

		append_data_IntoTupleList(endPtr, value, numAttributes);
		updatePivotByTuple_tmpGroup(tmp, value, numAttributes, distanceall);
		//put_hash(tmp, value, numAttributes);
		tmp->member_len++;

		index++;

		//printf("*******************finish find the intersection with number is * %d \n", index);

		return tmp;


}

/**
 * put the intersetction tuples into the memo list of that group
 */
void findIntersection_putIntoMemoList_noreturn(TupleTableSlot *inputslot,
		TupleTableSlot *tmpslot, TupleTableSlot *bufferslot,
		tmpGroup *tuplestorestate, Tuplestorestate *memolist,
		Tuplestorestate *globalmemolist,
		bool *iisempty,
		bool *entryNULL,
		int distanceall,
		int distanceMetric,
		int numAttributes) {
	tuple *tupleHeadptr = tuplestorestate->tuples_headPtr;
	tuple *tuplePreptr = tupleHeadptr;
	bool firsttupleptr = true;

	while (tupleHeadptr != NULL ) {

		//printf("inter tuple is %d , %d \n", tupleHeadptr->values[0], tupleHeadptr->values[1]);

		if (InsideBound_tuple(tupleHeadptr, inputslot, distanceall,
				distanceMetric, numAttributes)) {

			//printf("inter tuple is %d , %d \n", tupleHeadptr->values[0], tupleHeadptr->values[1]);

			if (tuplestorestate->member_len == 1) {
				*entryNULL = true;
				*iisempty = false;
				break;
			}

			//store this value into the memolist
			//this operation is coming to make sure the order independent
			//tuplestorestate_puttuple(tupleHeadptr, memolist, bufferslot,inputslot, numAttributes);

			if (globalmemolist) {
				tuplestore_puttupleslot(globalmemolist, bufferslot);
			}

			if (firsttupleptr) {
				tuplestorestate->tuples_headPtr = tupleHeadptr->next;

				if (tupleHeadptr->values[0]
						== tuplestorestate->endPtr->values[0]
						&& tupleHeadptr->values[1]
								== tuplestorestate->endPtr->values[1]) {
					tuplestorestate->endPtr = tuplestorestate->tuples_headPtr;
					//printf("equal operation here 2 \n");
				}
				pfree(tupleHeadptr);
				tupleHeadptr = tuplestorestate->tuples_headPtr;
			} else {
				//printf("!!! \n");
				tuplePreptr->next = tupleHeadptr->next;

				if (tupleHeadptr->values[0]
						== tuplestorestate->endPtr->values[0]
						&& tupleHeadptr->values[1]
								== tuplestorestate->endPtr->values[1]) {
					tuplestorestate->endPtr = tuplePreptr;
					//printf("equal operation here 1 \n");
				}

				pfree(tupleHeadptr);
				tupleHeadptr = tuplePreptr->next;
			}
			tuplestorestate->member_len--;

			*iisempty = false;

		}

		firsttupleptr = false;
		tuplePreptr = tupleHeadptr;
		//printf("problem here intersection 22222 \n");
		if (tupleHeadptr != NULL )
			tupleHeadptr = tupleHeadptr->next;
		//printf("problem here intersection 11111 \n");
	}
}

/**
 * check the new tuple "inputslot" can lay inside the TupleTableSlot "slot" range
 */
bool InsideBound(
		TupleTableSlot *slot,
		TupleTableSlot *inputslot,
		int distanceall,
		int distanceMetric,
		int numAttributes) {
	bool inside = true;
	bool isNull1;

	int index = 0;
	int d1 = 0;
	int max_D = 0;
	//find the max distance between two tuples
	while (index < numAttributes) {
		Datum attrPivot1, attrCurrent;
		/*********************************************************/
		attrPivot1 = slot_getattr(inputslot, index + 1, &isNull1);
		int x_input = DatumGetInt32(attrPivot1);

		attrCurrent = slot_getattr(slot, index + 1, &isNull1);
		int x_slot = DatumGetInt32(attrCurrent);

		if (distanceMetric == 1) {

			d1 = abs(x_input - x_slot)+d1;

		}else if(distanceMetric == 2){

			d1=d1+(x_input - x_slot)*(x_input - x_slot);

		}else if(distanceMetric == 3)
		{
			d1=abs(x_input - x_slot);

			if (d1 >= max_D) {
						max_D = d1;
					}

		}


		index++;
	}

	/*********************************************************/
	/*this is L infinity distance*/
	if (distanceMetric == 3) {

		if (max_D <= distanceall) {
			return true ;
		} else {
			return false ;
		}

		/*L2 distance*/
	} else if (distanceMetric == 2)
	{
		//int sqrtresult=sqrt(d1);
		//printf("distance is %d, sqrt %d , distanceall %d \n",d1, sqrtresult, distanceall);

		if(sqrt(d1)<=distanceall)
		{
			return true ;
		}else {
			return false ;
		}

	 /*this is L1 distance*/
	}else if (distanceMetric == 1) {

		if (d1<= distanceall) {
			return true ;
		} else {
			return false ;
		}

	}

	return inside;
}

bool InsideBound_tuple(tuple *slot, TupleTableSlot *inputslot,
		int distanceall, int distanceMetric, int numAttributes) {
	bool inside = true;

	bool isNull1;
	int index = 0;
	int d1 = 0;
	int max_D = 0;
	//find the max distance between two tuples
	while (index < numAttributes) {
		Datum attrPivot1;
		/*********************************************************/
		attrPivot1 = slot_getattr(inputslot, index + 1, &isNull1);
		int x_input = DatumGetInt32(attrPivot1);

		//attrCurrent=slot_getattr(slot, index+1, &isNull1);
		int x_slot = slot->values[index];

		if (distanceMetric == 3) {

			d1 = abs(x_input - x_slot);

			if (d1 >= max_D) {
						max_D = d1;
					}

		}else if(distanceMetric == 2)
		{
			d1=d1+(x_input - x_slot)*(x_input - x_slot);

		}else if(distanceMetric == 1)
		{
			d1=d1+abs(x_input - x_slot);
		}



		index++;
	}

	/*********************************************************/
	/*this is L infinity distance*/
	if (distanceMetric == 3) {
		if (max_D <= distanceall) {
			return true ;
		} else {
			return false ;
		}

	} else if (distanceMetric ==2)/*L2 distance*/
	{
		/*to do list here*/
		if(sqrt(d1)<=distanceall)
			{
				return true ;
			}else {
				return false ;
			}

	}else if (distanceMetric == 1) {

		//printf("d1 %d \n", d1);
		//printf("distance all %d \n", distanceall);
		if (d1<= distanceall) {
			//printf("YES \n");
			return true ;
		} else {
			return false ;
		}

	}

	return inside;
}


/**
 *one problem
 */
tmpGroup *
changeToTmpGroup(Tuplestorestate *tuplestorestate, TupleTableSlot *tmpSlot,
		int numAttributes, int eps) {
	bool forward;
	bool eof_tuplestore;

	forward = true;

	int xc1 = rand() % 1000;
	int yc1 = rand() % 1000;
	//printf("build new group here\n");
	tmpGroup* tmp = constructGroup(numAttributes, xc1, yc1, true );
	//printf("build new group succ\n");

	tuple *endPtr = tmp->tuples_headPtr;

	tuplestore_rescan(tuplestorestate);

	eof_tuplestore = (tuplestorestate == NULL )
			|| tuplestore_ateof(tuplestorestate);

	int index = 0;
	//iterate the tuple store state list
	while (!eof_tuplestore) {
		//print_slot(tmpSlot);
		if (tuplestore_gettupleslot(tuplestorestate, forward, tmpSlot)) {
			//update the center point
			Datum attrPivot1;
			bool isNull1;
			//print_slot(tmpSlot);
			int att_index = 0;
			int value[2];
			for (; att_index < numAttributes; att_index++) {
				attrPivot1 = slot_getattr(tmpSlot, att_index + 1, &isNull1);
				if (isNull1) {
					break;
				}
				//printf("!!!!!!!!!!!data: %d \n",DatumGetInt32(attrPivot1));
				value[att_index] = DatumGetInt32(attrPivot1);
			}

			//printf("!!!!!!!!!!! input data into the group\n");
			if (index == 0) {
				tmp->tuples_headPtr = append_data_IntoTupleList(
						tmp->tuples_headPtr, value, numAttributes);
				endPtr = tmp->tuples_headPtr;
			} else {
				endPtr = append_data_IntoTupleList(endPtr, value,
						numAttributes);
			}

			put_hash(tmp, value, numAttributes);
			updatePivotByTuple_tmpGroup(tmp, value, numAttributes, eps);
			//printf("!!!!!!!!!!! update the bound finish\n");
			tmp->member_len++;
			//tmp->.numAttr=numAttributes;
			index++;

		} else {
			eof_tuplestore = true;
		}

	}					//while eof
	//tmp->member_len=index;
	return tmp;

}
/**
 *
 */
void copyfromTupleTableToArray(int *des, int *des2, TupleTableSlot *inputslot,
		int numAttributes)
{

	int index;
	Datum attrPivot1;
	bool isNull1;
	/*********************************************************/
	//while(index<=numAttributes)
	for (index = 0; index < numAttributes; index++) {
		attrPivot1 = slot_getattr(inputslot, index + 1, &isNull1);

		//printf("***index: %d \n", index);
		if (isNull1) {
			return;
		}

		//printf("!!!!!!!!!!!data: %d \n",DatumGetInt32(attrPivot1));
		des[index] = DatumGetInt32(attrPivot1);
		des2[index] = DatumGetInt32(attrPivot1);
	}

}


/**
 * get the MBR from the inputslot
 */
RTREEMBR getMBRinputSlot(TupleTableSlot *inputslot, int attr)
{
		int index;
		Datum		attrPivot1;
		bool isNull1;

		int x;
		int y;
		int z;
		/*********************************************************/
		//while(index<=numAttributes)
		for(index=0;index<attr;index++)
		{
			attrPivot1 = slot_getattr(inputslot, index+1, &isNull1);
			//printf("***index: %d \n", index);
			if(index==0)
				x=DatumGetInt32(attrPivot1);
			else if(index==1)
				y=DatumGetInt32(attrPivot1);
			else
				z=DatumGetInt32(attrPivot1);
		}

		RTREEMBR search_mbr1;

		if(attr==1)
		{
			RTREEMBR search_mbr={{x,x}};
			return search_mbr;

		}else if(attr==2)
		{
			RTREEMBR search_mbr={{x,y,x,y}};
			return search_mbr;
		}else if(attr==3)
		{
			RTREEMBR search_mbr={{x,y,z, x,y,z}};
			return search_mbr;
		}else
		{
			printf("we are not support higher dimension for R tree now \n");
			return search_mbr1;
		}

}

/**
 * get the MBR from the inputslot
 */
 RTREEMBR getMBRinputSlotBigger(TupleTableSlot *inputslot, int attr, int eps)
{
		int index;
		Datum		attrPivot1;
		bool isNull1;

		int x;
		int y;
		int z;

		RTREEMBR search_mbr1;

		/*********************************************************/
		//while(index<=numAttributes)
		for(index=0;index<attr;index++)
		{
			attrPivot1 = slot_getattr(inputslot, index+1, &isNull1);

			//printf("***index: %d \n", index);
			if(index==0)
				x=DatumGetInt32(attrPivot1);
			else if(index==1)
				y=DatumGetInt32(attrPivot1);
			else
				z=DatumGetInt32(attrPivot1);
		}

		if(attr==1)
		{
			RTREEMBR search_mbr={{x-eps,x+eps}};
			return search_mbr;

		}else if(attr==2)
		{
			RTREEMBR search_mbr ={
							{x-eps,y-eps,x+eps,y+eps}
					};
			return search_mbr;

		}else if(attr==3)
		{
			RTREEMBR search_mbr={
					{x-eps,y-eps,z-eps,x+eps,y+eps,z+eps}
								};
			return search_mbr;

		}else {
			printf("we are not support higher dimension for R tree now \n");
			return search_mbr1;
		}

}


 /**
  * get the MBR from the entry's pivot value
  */
 RTREEMBR getMBRHashEntry(AggHashEntry entry, int att)
 {
 	RTREEMBR search_mbr1;

 	if(att==2)
 	{
 		RTREEMBR search_mbr = {
 			 {entry->pivot1Slot[0], entry->pivot1Slot[1], entry->pivot2Slot[0], entry->pivot2Slot[1]}   /* search will find above mbrs that this one overlaps */
 			};

 		return search_mbr;

 	}else if(att==1)
 	{
 		RTREEMBR search_mbr = {
 				 {entry->pivot1Slot[0], entry->pivot2Slot[0]}   /* search will find above mbrs that this one overlaps */
 				};

 		return search_mbr;
 	}else
 		{
 			printf("we are not support higher dimension for R tree now \n");
 			return search_mbr1;
 		}


 }


  void getTableSlot(int *inputsdata, int numAttributes, TupleTableSlot * inputslot, TupleTableSlot *aggmodified)
 {
 	Datum		*values = (Datum *) palloc(numAttributes * sizeof(Datum)); /*array*/
 	bool * nulls = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
 	bool * replace = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
 	memset(values, 0, numAttributes * sizeof(Datum));
 	memset(nulls, false, numAttributes * sizeof(bool));
 	memset(replace, false, numAttributes * sizeof(bool));

 	if(numAttributes==1)
 		{
 			values[0] = inputsdata[0];
 			replace[0]=true;
 		}else if (numAttributes==2)
 		{
 			values[0] = inputsdata[0];
 			replace[0]=true;
 			values[1] = inputsdata[1];
 			replace[1]=true;
 		}

 	HeapTuple	newHeapTuple;/*CHANGED BY Mingjie*/

 	newHeapTuple = heap_modify_tuple(inputslot->tts_tuple, inputslot->tts_tupleDescriptor, values, nulls, replace);
 	/* Initialize modifiedSlot by cloning outerslot */
 	if (aggmodified->tts_tupleDescriptor == NULL)
 		ExecSetSlotDescriptor(aggmodified, inputslot->tts_tupleDescriptor);
 	/*Make aggstate->modifiedSlot.tuple point to new HeapTuple*/
 	ExecStoreTuple(newHeapTuple,aggmodified,InvalidBuffer,true);/*This will free memory of previous tuple too*/

 	pfree(values);
 	pfree(nulls);
 	pfree(replace);

 }

  void copyinputslottoDes(TupleTableSlot * inputslot, int *des, int numAttributes)
  {
  		int index;
  		Datum		attrPivot1;
  		bool isNull1;

  		/*********************************************************/
  		//while(index<=numAttributes)
  		for(index=0;index<numAttributes;index++)
  		{
  			attrPivot1 = slot_getattr(inputslot, index+1, &isNull1);

  			//printf("***index: %d \n", index);
  			if(isNull1)
  			{
  				return;
  			}

  			//printf("!!!!!!!!!!!data: %d \n",DatumGetInt32(attrPivot1));
  			des[index]=DatumGetInt32(attrPivot1);
  			//des2[index]=DatumGetInt32(attrPivot1);
  		}

  }


