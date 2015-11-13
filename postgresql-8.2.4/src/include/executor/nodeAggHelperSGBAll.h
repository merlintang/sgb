/*
 * nodeAggHelperSGBAll.h
 *
 *  Created on: Aug 28, 2013
 *      Author: merlin
 */

#ifndef NODEAGGHELPER_H_
#define NODEAGGHELPER_H_

#include "executor/ruby.h"
#include "executor/linkListM.h"
#include "executor/rtreeM.h"
#include "executor/convexhullM.h"

#include "postgres.h"
#include "access/heapam.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "executor/nodeAgg.h"
#include "miscadmin.h"
#include "optimizer/clauses.h"
#include "parser/parse_agg.h"
#include "parser/parse_coerce.h"
#include "parser/parse_expr.h"
#include "parser/parse_oper.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#include "utils/tuplesort.h"
#include "utils/datum.h"

#include "executor/groupTable.h"

double rtreesearchContain;
double rtreesearchOverlap;
double rtreeUpdate;
double judgeContain;
double fetcthOverlap;


extern tmpGroup *
findIntersection_putIntoMemoList(TupleTableSlot *inputslot,
		TupleTableSlot *tmpslot, TupleTableSlot *bufferslot,
		tmpGroup *tuplestorestate,
		Tuplestorestate *memolist,
		Tuplestorestate *globalmemolist,
		bool *iisempty,
		bool *entryNULL,
		int distanceall,
		int distanceMetric,
		int numAttributes);

extern tmpGroup *
findIntersection_putIntoMemoList_Convex(TupleTableSlot *inputslot,
                TupleTableSlot *tmpslot, TupleTableSlot *bufferslot,
                tmpGroup *tuplestorestate,
                Tuplestorestate *memolist,
                Tuplestorestate *globalmemolist,
                bool *iisempty,
                bool *entryNULL,
                int distanceall,
                int distanceMetric,
                int numAttributes);

/**
 * put the intersetction tuples into the memo list of that group
 */
extern void findIntersection_putIntoMemoList_noreturn(TupleTableSlot *inputslot,
		TupleTableSlot *tmpslot, TupleTableSlot *bufferslot,
		tmpGroup *tuplestorestate, Tuplestorestate *memolist,
		Tuplestorestate *globalmemolist,
		bool *iisempty,
		bool *entryNULL,
		int distanceall,
		int distanceMetric,
		int numAttributes);

extern
void tmpGroup_puttupleslot(tmpGroup * tuplestorestatePTR,
		TupleTableSlot * inputslot, int numAttributes, bool isfirst);

extern
bool judgeJoinGroup(int numAttributes, int *pivotslot1, int *pivotslot2,
		TupleTableSlot *inputTuple, int EPS, int distanceMetric, bool *overlap,
		MemoryContext evalContext) ;

extern
void puttuple_TupleTableSlot(tuple *input, TupleTableSlot *aggslot,
		TupleTableSlot * inputslot, int numAttributes);

/********************for r tree*********************/
extern
RTREEMBR getMBRinputSlot(TupleTableSlot *inputslot, int attr);

extern
RTREEMBR getMBRinputSlotBigger(TupleTableSlot *inputslot, int attr, int eps);

extern
RTREEMBR getMBRHashEntry(AggHashEntry entry, int att);

extern
void getTableSlot(int *inputsdata, int numAttributes, TupleTableSlot * inputslot, TupleTableSlot *aggmodified);

extern
void copyinputslottoDes(TupleTableSlot * inputslot, int *des, int numAttributes);

extern
bool InsideBound(
		TupleTableSlot *slot,
		TupleTableSlot *inputslot,
		int distanceall,
		int distanceMetric,
		int numAttributes);

#endif /* NODEAGGHELPER_H_ */
