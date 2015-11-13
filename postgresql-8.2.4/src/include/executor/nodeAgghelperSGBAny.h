/*
 * nodeAgghelperSGBAny.h
 *
 *  Created on: Aug 31, 2013
 *      Author: merlin
 */

#ifndef NODEAGGHELPERSGBANY_H_
#define NODEAGGHELPERSGBANY_H_

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
#include "executor/nodeAggHelperSGBAll.h"

extern bool
judgeJoinGroup_Any(
		int numAttributes,
		TupleTableSlot *inputTuple,
		int EPS,
		int distanceMetric,
		AggHashEntry entry,
		TupleTableSlot *tmpslot,
		MemoryContext evalContext);


#endif /* NODEAGGHELPERSGBANY_H_ */
