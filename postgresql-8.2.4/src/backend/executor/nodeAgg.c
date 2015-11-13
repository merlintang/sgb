/*-------------------------------------------------------------------------
 *
deploy to the mc12

 *FOR SGB-All
 * * Method 3:
 * L-infinity:recatngle+rtree
 * L-2: rectangle+convexhull+rtree
 *
 *FOR SGB-ANY
 * method2
 * Rtree
 *
 * nodeAgg.c
 *	  Routines to handle aggregate nodes.
 *
 *	  ExecAgg evaluates each aggregate in the following steps:
 *
 *		 transvalue = initcond
 *		 foreach input_tuple do
 *			transvalue = transfunc(transvalue, input_value(s))
 *		 result = finalfunc(transvalue)
 *
 *	  If a finalfunc is not supplied then the result is just the ending
 *	  value of transvalue.
 *
 *	  If transfunc is marked "strict" in pg_proc and initcond is NULL,
 *	  then the first non-NULL input_value is assigned directly to transvalue,
 *	  and transfunc isn't applied until the second non-NULL input_value.
 *	  The agg's first input type and transtype must be the same in this case!
 *
 *	  If transfunc is marked "strict" then NULL input_values are skipped,
 *	  keeping the previous transvalue.	If transfunc is not strict then it
 *	  is called for every input tuple and must deal with NULL initcond
 *	  or NULL input_values for itself.
 *
 *	  If finalfunc is marked "strict" then it is not called when the
 *	  ending transvalue is NULL, instead a NULL result is created
 *	  automatically (this is just the usual handling of strict functions,
 *	  of course).  A non-strict finalfunc can make its own choice of
 *	  what to return for a NULL ending transvalue.
 *IMPORTANT
 *	  We compute aggregate input expressions and run the transition functions
 *	  in a temporary econtext (aggstate->tmpcontext).  This is reset at
 *	  least once per input tuple, so when the transvalue datatype is
 *	  pass-by-reference, we have to be careful to copy it into a longer-lived
 *	  memory context, and free the prior value to avoid memory leakage.
 *	  We store transvalues in the memory context aggstate->aggcontext,
 *	  which is also used for the hashtable structures in AGG_HASHED mode.
 *	  The node's regular econtext (aggstate->csstate.cstate.cs_ExprContext)
 *	  is used to run finalize functions and compute the output tuple;
 *	  this context can be reset once per output tuple.
 *
 *	  Beginning in PostgreSQL 8.1, the executor's AggState node is passed as
 *	  the fmgr "context" value in all transfunc and finalfunc calls.  It is
 *	  not really intended that the transition functions will look into the
 *	  AggState node, but they can use code like
 *			if (fcinfo->context && IsA(fcinfo->context, AggState))
 *	  to verify that they are being called by nodeAgg.c and not as ordinary
 *	  SQL functions.  The main reason a transition function might want to know
 *	  that is that it can avoid palloc'ing a fixed-size pass-by-ref transition
 *	  value on every call: it can instead just scribble on and return its left
 *	  input.  Ordinarily it is completely forbidden for functions to modify
 *	  pass-by-ref inputs, but in the aggregate case we know the left input is
 *	  either the initial transition value or a previous function result, and
 *	  in either case its value need not be preserved.  See int8inc() for an
 *	  example.	Notice that advance_transition_function() is coded to avoid a
 *	  data copy step when the previous transition value pointer is returned.
 *
 *
 * Portions Copyright (c) 1996-2006, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/executor/nodeAgg.c,v 1.146.2.1 2007/02/02 00:07:28 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */

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

/*
 * AggStatePerAggData - per-aggregate working state for the Agg scan
 */
typedef struct AggStatePerAggData {
	/*
	 * These values are set up during ExecInitAgg() and do not change
	 * thereafter:
	 */

	/* Links to Aggref expr and state nodes this working state is for */
	AggrefExprState *aggrefstate;
	Aggref *aggref;

	/* number of input arguments for aggregate */
	int numArguments;

	/* Oids of transfer functions */
	Oid transfn_oid;
	Oid finalfn_oid; /* may be InvalidOid */

	/*
	 * fmgr lookup data for transfer functions --- only valid when
	 * corresponding oid is not InvalidOid.  Note in particular that fn_strict
	 * flags are kept here.
	 */
	FmgrInfo transfn;
	FmgrInfo finalfn;

	/*
	 * Type of input data and Oid of sort operator to use for it; only
	 * set/used when aggregate has DISTINCT flag.  (These are not used
	 * directly by nodeAgg, but must be passed to the Tuplesort object.)
	 */
	Oid inputType;
	Oid sortOperator;

	/*
	 * fmgr lookup data for input type's equality operator --- only set/used
	 * when aggregate has DISTINCT flag.
	 */
	FmgrInfo equalfn;

	/*
	 * initial value from pg_aggregate entry
	 */
	Datum initValue;
	bool initValueIsNull;

	/*
	 * We need the len and byval info for the agg's input, result, and
	 * transition data types in order to know how to copy/delete values.
	 */
	int16 inputtypeLen, resulttypeLen, transtypeLen;
	bool inputtypeByVal, resulttypeByVal, transtypeByVal;

	/*
	 * These values are working state that is initialized at the start of an
	 * input tuple group and updated for each input tuple.
	 *
	 * For a simple (non DISTINCT) aggregate, we just feed the input values
	 * straight to the transition function.  If it's DISTINCT, we pass the
	 * input values into a Tuplesort object; then at completion of the input
	 * tuple group, we scan the sorted values, eliminate duplicates, and run
	 * the transition function on the rest.
	 */

	Tuplesortstate *sortstate; /* sort object, if a DISTINCT agg */
} AggStatePerAggData;

/*****************************************************/

static void initialize_aggregates(AggState *aggstate, AggStatePerAgg peragg,
		AggStatePerGroup pergroup);
static void advance_transition_function(AggState *aggstate,
		AggStatePerAgg peraggstate, AggStatePerGroup pergroupstate,
		FunctionCallInfoData *fcinfo);
static void advance_aggregates(AggState *aggstate, AggStatePerGroup pergroup);
static void process_sorted_aggregate(AggState *aggstate,
		AggStatePerAgg peraggstate, AggStatePerGroup pergroupstate);
static void finalize_aggregate(AggState *aggstate, AggStatePerAgg peraggstate,
		AggStatePerGroup pergroupstate, Datum *resultVal, bool *resultIsNull);
static Bitmapset *find_unaggregated_cols(AggState *aggstate);
static bool find_unaggregated_cols_walker(Node *node, Bitmapset **colnos);
static void build_hash_table(AggState *aggstate);
static AggHashEntry lookup_hash_entry(AggState *aggstate,
		TupleTableSlot *inputslot);


static void
sgb_newgroup_helper(int storeplace, AggState *aggstate ,int metrichose);;

static TupleTableSlot *agg_retrieve_direct(AggState *aggstate);
static TupleTableSlot *agg_retrieve_direct_DIST(AggState *aggstate); /* CHANGED BY RUBY*/
static TupleTableSlot *agg_retrieve_direct_around(AggState *aggstate); /*CHANGED BY YASIN */

static void agg_fill_hash_table(AggState *aggstate);
static void agg_fill_hash_table_SGBTOAll(AggState *aggstate);/*CHANGED BY MINGJIE*/
static void agg_fill_hash_table_SGBTOANY(AggState *aggstate);
static void agg_fill_hash_table_around(AggState *aggstate);/*CHANGED BY YASIN */
static void agg_fill_hash_table_around_MaxR(AggState *aggstate);/*CHANGED BY YASIN */
static void agg_fill_hash_table_around_MaxS(AggState *aggstate);/*CHANGED BY YASIN */
static void agg_fill_hash_table_unsupervised(AggState *aggstate);/*CHANGED BY YASIN */

/*CHANGED BY MINGJIE*/
static void agg_fill_hash_table_overlap_random_l1(AggState *aggstate,
		TupleTableSlot *inputslot);
static void agg_fill_hash_table_overlap_eliminated_l1(AggState *aggstate,
		TupleTableSlot *inputslot);
static void
agg_fill_hash_table_overlap_newgroup_l1(AggState *aggstate,
		TupleTableSlot *inputslot);

/*CHANGED BY MINGJIE*/
static void agg_fill_hash_table_overlap_random_l2(AggState *aggstate,
		TupleTableSlot *inputslot);
static void agg_fill_hash_table_overlap_eliminated_l2(AggState *aggstate,
		TupleTableSlot *inputslot);

static void
agg_fill_hash_table_overlap_newgroup_l2(AggState *aggstate,
		TupleTableSlot *inputslot);

static
void sgb_newgroup_iteration_l1(AggState *aggstate, TupleTableSlot *inputslot,
		int step);

static
void sgb_newgroup_iteration_l2(AggState *aggstate, TupleTableSlot *inputslot,
		int step);

static void
agg_fill_hash_table_SGBANY(AggState *aggstate,
		TupleTableSlot *inputslot);

static void IterateSets(DisJointSets *dssets,
		AggState* aggstate,
		TupleTableSlot *inputslot ) ;

static
void DFS_oneSet(element * parent,
		AggHashEntry entry, AggState* aggstate,
		TupleTableSlot *inputslot, int numAttributes,bool firstone );


static void enable_chain(AggState *aggstate);/*CHANGED BY YASIN */
static void reset_chain(AggState *aggstate);/*CHANGED BY YASIN */

static TupleTableSlot *agg_retrieve_hash_table(AggState *aggstate);
static TupleTableSlot *agg_retrieve_hash_table_around_MaxS(AggState *aggstate);/*CHANGED BY YASIN */
static Datum GetAggInitVal(Datum textInitVal, Oid transtype);

/*
 static void
 updatePivotByTuple(
 AggHashEntry entry,
 int *inputslot,
 int numAttr,
 int eps);
 */

static void printGroup(
		Tuplestorestate *tuplestorestate,
		TupleTableSlot *tmpslot
		)
{

	bool        forward;
	bool        eof_tuplestore;
	forward=true;
		/**
		 * scan from the begin
		 */
	if(tuplestorestate==NULL)
	{
		return;
	}

	tuplestore_rescan(tuplestorestate);

		 eof_tuplestore = (tuplestorestate == NULL) ||
		       tuplestore_ateof(tuplestorestate);
		 printf("group !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		   while(!eof_tuplestore)
			   {
				    //TupleTableSlot *slot;

					if (tuplestore_gettupleslot(tuplestorestate, forward, tmpslot))
					{
						//check whether this slot inside the bound of inputslot
						//printf("compare with slot \n");
						print_slot(tmpslot);
					}
					else
					{
						eof_tuplestore=true;
					}
			   }
		   printf("group ***************************************\n");
}

/*******************************************************************/

/*
 * Initialize all aggregates for a new group of input values.
 *
 * When called, CurrentMemoryContext should be the per-query context.
 */
static void initialize_aggregates(AggState *aggstate, AggStatePerAgg peragg,
		AggStatePerGroup pergroup) {
	int aggno;

	for (aggno = 0; aggno < aggstate->numaggs; aggno++) {
		AggStatePerAgg peraggstate = &peragg[aggno];
		AggStatePerGroup pergroupstate = &pergroup[aggno];
		Aggref *aggref = peraggstate->aggref;

		/*
		 * Start a fresh sort operation for each DISTINCT aggregate.
		 */
		if (aggref->aggdistinct) {
			/*
			 * In case of rescan, maybe there could be an uncompleted sort
			 * operation?  Clean it up if so.
			 */
			if (peraggstate->sortstate)
				tuplesort_end(peraggstate->sortstate);

			peraggstate->sortstate = tuplesort_begin_datum(
					peraggstate->inputType, peraggstate->sortOperator, work_mem,
					false );
		}

		/*
		 * If we are reinitializing after a group boundary, we have to free
		 * any prior transValue to avoid memory leakage.  We must check not
		 * only the isnull flag but whether the pointer is NULL; since
		 * pergroupstate is initialized with palloc0, the initial condition
		 * has isnull = 0 and null pointer.
		 */
		if (!peraggstate->transtypeByVal && !pergroupstate->transValueIsNull
				&& DatumGetPointer(pergroupstate->transValue) != NULL )
			pfree(DatumGetPointer(pergroupstate->transValue) );

		/*
		 * (Re)set transValue to the initial value.
		 *
		 * Note that when the initial value is pass-by-ref, we must copy it
		 * (into the aggcontext) since we will pfree the transValue later.
		 */
		if (peraggstate->initValueIsNull)
			pergroupstate->transValue = peraggstate->initValue;
		else {
			MemoryContext oldContext;

			oldContext = MemoryContextSwitchTo(aggstate->aggcontext);
			pergroupstate->transValue = datumCopy(peraggstate->initValue,
					peraggstate->transtypeByVal, peraggstate->transtypeLen);
			MemoryContextSwitchTo(oldContext);
		}
		pergroupstate->transValueIsNull = peraggstate->initValueIsNull;

		/*
		 * If the initial value for the transition state doesn't exist in the
		 * pg_aggregate table then we will let the first non-NULL value
		 * returned from the outer procNode become the initial value. (This is
		 * useful for aggregates like max() and min().) The noTransValue flag
		 * signals that we still need to do this.
		 */
		pergroupstate->noTransValue = peraggstate->initValueIsNull;
	}
}

/*
 * Given new input value(s), advance the transition function of an aggregate.
 *
 * The new values (and null flags) have been preloaded into argument positions
 * 1 and up in fcinfo, so that we needn't copy them again to pass to the
 * transition function.  No other fields of fcinfo are assumed valid.
 *
 * It doesn't matter which memory context this is called in.
 */
static void advance_transition_function(AggState *aggstate,
		AggStatePerAgg peraggstate, AggStatePerGroup pergroupstate,
		FunctionCallInfoData *fcinfo) {
	int numArguments = peraggstate->numArguments;
	MemoryContext oldContext;
	Datum newVal;
	int i;

	if (peraggstate->transfn.fn_strict) {
		/*
		 * For a strict transfn, nothing happens when there's a NULL input; we
		 * just keep the prior transValue.
		 */
		for (i = 1; i <= numArguments; i++) {
			if (fcinfo->argnull[i])
				return;
		}
		if (pergroupstate->noTransValue) {
			/*
			 * transValue has not been initialized. This is the first non-NULL
			 * input value. We use it as the initial value for transValue. (We
			 * already checked that the agg's input type is binary-compatible
			 * with its transtype, so straight copy here is OK.)
			 *
			 * We must copy the datum into aggcontext if it is pass-by-ref. We
			 * do not need to pfree the old transValue, since it's NULL.
			 */
			oldContext = MemoryContextSwitchTo(aggstate->aggcontext);
			pergroupstate->transValue = datumCopy(fcinfo->arg[1],
					peraggstate->transtypeByVal, peraggstate->transtypeLen);
			pergroupstate->transValueIsNull = false;
			pergroupstate->noTransValue = false;
			MemoryContextSwitchTo(oldContext);
			return;
		}
		if (pergroupstate->transValueIsNull) {
			/*
			 * Don't call a strict function with NULL inputs.  Note it is
			 * possible to get here despite the above tests, if the transfn is
			 * strict *and* returned a NULL on a prior cycle. If that happens
			 * we will propagate the NULL all the way to the end.
			 */
			return;
		}
	}

	/* We run the transition functions in per-input-tuple memory context */
	oldContext = MemoryContextSwitchTo(
			aggstate->tmpcontext->ecxt_per_tuple_memory);

	/*
	 * OK to call the transition function
	 */
	InitFunctionCallInfoData(*fcinfo, &(peraggstate->transfn), numArguments + 1,
			(void * ) aggstate, NULL);
	fcinfo->arg[0] = pergroupstate->transValue;
	fcinfo->argnull[0] = pergroupstate->transValueIsNull;

	newVal = FunctionCallInvoke(fcinfo);

	/*
	 * If pass-by-ref datatype, must copy the new value into aggcontext and
	 * pfree the prior transValue.	But if transfn returned a pointer to its
	 * first input, we don't need to do anything.
	 */
	if (!peraggstate->transtypeByVal
			&& DatumGetPointer(newVal)
					!= DatumGetPointer(pergroupstate->transValue) ) {
		if (!fcinfo->isnull) {
			MemoryContextSwitchTo(aggstate->aggcontext);
			newVal = datumCopy(newVal, peraggstate->transtypeByVal,
					peraggstate->transtypeLen);
		}
		if (!pergroupstate->transValueIsNull)
			pfree(DatumGetPointer(pergroupstate->transValue) );
	}

	pergroupstate->transValue = newVal;
	pergroupstate->transValueIsNull = fcinfo->isnull;

	MemoryContextSwitchTo(oldContext);
}

/*
 * Advance all the aggregates for one input tuple.	The input tuple
 * has been stored in tmpcontext->ecxt_scantuple, so that it is accessible
 * to ExecEvalExpr.  pergroup is the array of per-group structs to use
 * (this might be in a hashtable entry).
 *
 * When called, CurrentMemoryContext should be the per-query context.
 */
static void advance_aggregates(AggState *aggstate, AggStatePerGroup pergroup) {
	ExprContext *econtext = aggstate->tmpcontext;
	int aggno;

	for (aggno = 0; aggno < aggstate->numaggs; aggno++) {
		AggStatePerAgg peraggstate = &aggstate->peragg[aggno];
		AggStatePerGroup pergroupstate = &pergroup[aggno];
		AggrefExprState *aggrefstate = peraggstate->aggrefstate;
		Aggref *aggref = peraggstate->aggref;
		FunctionCallInfoData fcinfo;
		int i;
		ListCell *arg;
		MemoryContext oldContext;

		/* Switch memory context just once for all args */
		oldContext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

		/* Evaluate inputs and save in fcinfo */
		/* We start from 1, since the 0th arg will be the transition value */
		i = 1;
		foreach(arg, aggrefstate->args)
		{
			ExprState *argstate = (ExprState *) lfirst(arg);

			fcinfo.arg[i] = ExecEvalExpr(argstate, econtext,
					fcinfo.argnull + i, NULL);
			i++;
		}

		/* Switch back */
		MemoryContextSwitchTo(oldContext);

		if (aggref->aggdistinct) {
			/* in DISTINCT mode, we may ignore nulls */
			/* XXX we assume there is only one input column */
			if (fcinfo.argnull[1])
				continue;
			tuplesort_putdatum(peraggstate->sortstate, fcinfo.arg[1],
					fcinfo.argnull[1]);
		} else {
			advance_transition_function(aggstate, peraggstate, pergroupstate,
					&fcinfo);
		}
	}
}

/*
 * Run the transition function for a DISTINCT aggregate.  This is called
 * after we have completed entering all the input values into the sort
 * object.	We complete the sort, read out the values in sorted order,
 * and run the transition function on each non-duplicate value.
 *
 * When called, CurrentMemoryContext should be the per-query context.
 */
static void process_sorted_aggregate(AggState *aggstate,
		AggStatePerAgg peraggstate, AggStatePerGroup pergroupstate) {
	Datum oldVal = (Datum) 0;
	bool haveOldVal = false;
	MemoryContext workcontext = aggstate->tmpcontext->ecxt_per_tuple_memory;
	MemoryContext oldContext;
	Datum *newVal;
	bool *isNull;
	FunctionCallInfoData fcinfo;

	tuplesort_performsort(peraggstate->sortstate);

	newVal = fcinfo.arg + 1;
	isNull = fcinfo.argnull + 1;

	/*
	 * Note: if input type is pass-by-ref, the datums returned by the sort are
	 * freshly palloc'd in the per-query context, so we must be careful to
	 * pfree them when they are no longer needed.
	 */

	while (tuplesort_getdatum(peraggstate->sortstate, true, newVal, isNull)) {
		/*
		 * DISTINCT always suppresses nulls, per SQL spec, regardless of the
		 * transition function's strictness.
		 */
		if (*isNull)
			continue;

		/*
		 * Clear and select the working context for evaluation of the equality
		 * function and transition function.
		 */
		MemoryContextReset(workcontext);
		oldContext = MemoryContextSwitchTo(workcontext);

		if (haveOldVal && DatumGetBool(FunctionCall2(&peraggstate->equalfn,
						oldVal, *newVal)) ) {
			/* equal to prior, so forget this one */
			if (!peraggstate->inputtypeByVal)
				pfree(DatumGetPointer(*newVal) );
		} else {
			advance_transition_function(aggstate, peraggstate, pergroupstate,
					&fcinfo);
			/* forget the old value, if any */
			if (haveOldVal && !peraggstate->inputtypeByVal)
				pfree(DatumGetPointer(oldVal) );
			/* and remember the new one for subsequent equality checks */
			oldVal = *newVal;
			haveOldVal = true;
		}

		MemoryContextSwitchTo(oldContext);
	}

	if (haveOldVal && !peraggstate->inputtypeByVal)
		pfree(DatumGetPointer(oldVal) );

	tuplesort_end(peraggstate->sortstate);
	peraggstate->sortstate = NULL;
}

/*
 * Compute the final value of one aggregate.
 *
 * The finalfunction will be run, and the result delivered, in the
 * output-tuple context; caller's CurrentMemoryContext does not matter.
 */
static void finalize_aggregate(AggState *aggstate, AggStatePerAgg peraggstate,
		AggStatePerGroup pergroupstate, Datum *resultVal, bool *resultIsNull) {
	MemoryContext oldContext;

	oldContext = MemoryContextSwitchTo(
			aggstate->ss.ps.ps_ExprContext->ecxt_per_tuple_memory);

	/*
	 * Apply the agg's finalfn if one is provided, else return transValue.
	 */
	if (OidIsValid(peraggstate->finalfn_oid) ) {
		FunctionCallInfoData fcinfo;

		InitFunctionCallInfoData(fcinfo, &(peraggstate->finalfn), 1,
				(void * ) aggstate, NULL);
		fcinfo.arg[0] = pergroupstate->transValue;
		fcinfo.argnull[0] = pergroupstate->transValueIsNull;
		if (fcinfo.flinfo->fn_strict && pergroupstate->transValueIsNull) {
			/* don't call a strict function with NULL inputs */
			*resultVal = (Datum) 0;
			*resultIsNull = true;
		} else {
			*resultVal = FunctionCallInvoke(&fcinfo);
			*resultIsNull = fcinfo.isnull;
		}
	} else {
		*resultVal = pergroupstate->transValue;
		*resultIsNull = pergroupstate->transValueIsNull;
	}

	/*
	 * If result is pass-by-ref, make sure it is in the right context.
	 */
	if (!peraggstate->resulttypeByVal && !*resultIsNull
			&& !MemoryContextContains(CurrentMemoryContext,
					DatumGetPointer(*resultVal) ))
		*resultVal = datumCopy(*resultVal, peraggstate->resulttypeByVal,
				peraggstate->resulttypeLen);

	MemoryContextSwitchTo(oldContext);
}

/*
 * find_unaggregated_cols
 *	  Construct a bitmapset of the column numbers of un-aggregated Vars
 *	  appearing in our targetlist and qual (HAVING clause)
 */
static Bitmapset *
find_unaggregated_cols(AggState *aggstate) {
	Agg *node = (Agg *) aggstate->ss.ps.plan;
	Bitmapset *colnos;

	colnos = NULL;
	(void) find_unaggregated_cols_walker((Node *) node->plan.targetlist,
			&colnos);
	(void) find_unaggregated_cols_walker((Node *) node->plan.qual, &colnos);
	return colnos;
}

static bool find_unaggregated_cols_walker(Node *node, Bitmapset **colnos) {
	if (node == NULL )
		return false ;
	if (IsA(node, Var)) {
		Var *var = (Var *) node;

		/* setrefs.c should have set the varno to 0 */
		Assert(var->varno == 0);
		Assert(var->varlevelsup == 0);
		*colnos = bms_add_member(*colnos, var->varattno);
		return false ;
	}
	if (IsA(node, Aggref)) /* do not descend into aggregate exprs */
		return false ;
	return expression_tree_walker(node, find_unaggregated_cols_walker,
			(void *) colnos);
}

/*
 * Initialize the hash table to empty.
 *
 * The hash table always lives in the aggcontext memory context.
 */
static void build_hash_table(AggState *aggstate) {
	Agg *node = (Agg *) aggstate->ss.ps.plan;
	MemoryContext tmpmem = aggstate->tmpcontext->ecxt_per_tuple_memory;
	Size entrysize;
	Bitmapset *colnos;
	List *collist;
	int i;

	Assert(node->aggstrategy == AGG_HASHED);
	Assert(node->numGroups > 0);

	/*SIZE OF EACH ENTRY OF THE HASH TABLE*/
	entrysize = sizeof(AggHashEntryData) + /*This includes the first AggStatePerGroupData*/
	(aggstate->numaggs - 1) * sizeof(AggStatePerGroupData); /*The remaining ones*/

	aggstate->hashtable = BuildTupleHashTable(node->numCols,/*Number of GB cols*/
	node->grpColIdx,/*Indices of GB cols in targetList*/
	aggstate->eqfunctions, aggstate->hashfunctions, node->numGroups, entrysize,
			aggstate->aggcontext, /*context for the hash table*/
			tmpmem);

	/*
	 * Create a list of the tuple columns that actually need to be stored in
	 * hashtable entries.  The incoming tuples from the child plan node will
	 * contain grouping columns, other columns referenced in our targetlist
	 * and qual, columns used to compute the aggregate functions, and perhaps
	 * just junk columns we don't use at all.  Only columns of the first two
	 * types need to be stored in the hashtable, and getting rid of the others
	 * can make the table entries significantly smaller.  To avoid messing up
	 * Var numbering, we keep the same tuple descriptor for hashtable entries
	 * as the incoming tuples have, but set unwanted columns to NULL in the
	 * tuples that go into the table.
	 *
	 * To eliminate duplicates, we build a bitmapset of the needed columns,
	 * then convert it to an integer list (cheaper to scan at runtime). The
	 * list is in decreasing order so that the first entry is the largest;
	 * lookup_hash_entry depends on this to use slot_getsomeattrs correctly.
	 *
	 * Note: at present, searching the tlist/qual is not really necessary
	 * since the parser should disallow any unaggregated references to
	 * ungrouped columns.  However, the search will be needed when we add
	 * support for SQL99 semantics that allow use of "functionally dependent"
	 * columns that haven't been explicitly grouped by.
	 */

	/* Find Vars that will be needed in tlist and qual */
	colnos = find_unaggregated_cols(aggstate);
	/* Add in all the grouping columns */
	for (i = 0; i < node->numCols; i++)
		colnos = bms_add_member(colnos, node->grpColIdx[i]);
	/* Convert to list, using lcons so largest element ends up first */
	collist = NIL;
	while ((i = bms_first_member(colnos)) >= 0)
		collist = lcons_int(i, collist);
	aggstate->hash_needed = collist;
}

/*
 * Estimate per-hash-table-entry overhead for the planner.
 *
 * Note that the estimate does not include space for pass-by-reference
 * transition data values, nor for the representative tuple of each group.
 */
Size hash_agg_entry_size(int numAggs) {
	Size entrysize;

	/* This must match build_hash_table */
	entrysize = sizeof(AggHashEntryData)
			+ (numAggs - 1) * sizeof(AggStatePerGroupData);
	entrysize = MAXALIGN(entrysize);
	/* Account for hashtable overhead (assuming fill factor = 1) */
	entrysize += 3 * sizeof(void *);
	return entrysize;
}

/*
 * Find or create a hashtable entry for the tuple group containing the
 * given tuple.
 *
 * When called, CurrentMemoryContext should be the per-query context.
 */
static AggHashEntry lookup_hash_entry(AggState *aggstate,
		TupleTableSlot *inputslot) {
	TupleTableSlot *hashslot = aggstate->hashslot;
	Agg		   *node = (Agg *) aggstate->ss.ps.plan;
	ListCell   *l;
	AggHashEntry entry;
	bool		isnew;

	int numAttributes=node->numCols;

	/* if first time through, initialize hashslot by cloning input slot */
	if (hashslot->tts_tupleDescriptor == NULL)
	{
		ExecSetSlotDescriptor(hashslot, inputslot->tts_tupleDescriptor);
		/* Make sure all unused columns are NULLs */
		ExecStoreAllNullTuple(hashslot);
	}

	/* transfer just the needed columns into hashslot */
	slot_getsomeattrs(inputslot, linitial_int(aggstate->hash_needed));
	foreach(l, aggstate->hash_needed)
	{
		int			varNumber = lfirst_int(l) - 1;

		hashslot->tts_values[varNumber] = inputslot->tts_values[varNumber];
		hashslot->tts_isnull[varNumber] = inputslot->tts_isnull[varNumber];
	}

	/* find or create the hashtable entry using the filtered tuple */

	entry = (AggHashEntry) LookupTupleHashEntry(aggstate->hashtable,
												hashslot,
												&isnew);
	//print_slot(inputslot);
	 //printf("**********look up hash function for input tuple  results is: %d \n", isnew );

	if (isnew)
	{
		/*CHANGED BY YASIN*/
		entry->flagEnabled=0;/*Initialize flag for each new entry in the hash table*/
		//entry->tuplestorestate=;
		/*CHANGED BY MINGJIE*/
		entry->ismodified=false;
		convexhull *convex;
		convex = (convexhull *) entry->convexhull;
		if (convex == NULL ) {
			entry->convexhull = (void *) palloc(sizeof(convexhull));
			((convexhull *) entry->convexhull)->numofconvex = 0;
		}

		memset(	entry->pivot1Slot, 0 , sizeof(int)*5);
		memset(	entry->pivot2Slot, 0 , sizeof(int)*5);

		if(node->overlap==2)
			{
				entry->tupleInmemolist=(void *)tuplestore_begin_heap(true, false, work_mem);
				entry->tuplestorestate=( tmpGroup *)constructGroup(numAttributes,rand()%10000,rand()%10000,false); /*two D-array*/;
			}else
			{
				Tuplestorestate *tuplestorestate;
				tuplestorestate = tuplestore_begin_heap(true, false, work_mem);
				entry->tuplestorestate = (void *) tuplestorestate;
			}

		/* initialize aggregates for new tuple group */
		initialize_aggregates(aggstate, aggstate->peragg, entry->pergroup);
		int locationhash=aggstate->indexhasharray;

		memcpy(&(aggstate->hasharray[locationhash].entry), &(entry), sizeof(AggHashEntry));
		memset(	aggstate->hasharray[locationhash].inputslots, 0 , sizeof(int)*5);
		memset(	aggstate->hasharray[locationhash].pivot1, 0 , sizeof(int)*5);
		memset(	aggstate->hasharray[locationhash].pivot2, 0 , sizeof(int)*5);
		copyinputslottoDes(inputslot,aggstate->hasharray[locationhash].inputslots,numAttributes);
		aggstate->hasharray[locationhash].numAttr=numAttributes;
		//aggstate->indexhasharray++;
	}

	return entry;
}

/*
 * ExecAgg -
 *
 *	  ExecAgg receives tuples from its outer subplan and aggregates over
 *	  the appropriate attribute for each aggregate function use (Aggref
 *	  node) appearing in the targetlist or qual of the node.  The number
 *	  of tuples to aggregate over depends on whether grouped or plain
 *	  aggregation is selected.	In grouped aggregation, we produce a result
 *	  row for each group; in plain aggregation there's a single result row
 *	  for the whole query.	In either case, the value of each aggregate is
 *	  stored in the expression context to be used when ExecProject evaluates
 *	  the result tuple.
 */

/*MAIN CONFIGURATION NOTES (See also optimizer/plan/planner.c)*/
/* YASIN:
 * 1. SGB_A-MD Base:            agg_fill_hash_table_around(our version)             and agg_retrieve_hash_table (original)
 * 2. SGB_A-MD + MaxRadius:     agg_fill_hash_table_around_MaxR (our version)       and agg_retrieve_hash_table (original)
 * 3. SGB_A-MD + MaxSeparation: agg_fill_hash_table_around_MaxS (our version)       and agg_retrieve_hash_table_MaxS (our version)
 *
 * 4. SGB_U-MD + {MaxDiam,MaxSep} : agg_fill_hash_table_unsupervised (our version) and agg_retrieve_hash_table (our version)
 * */
/*
 * MINGJIE
 * 1. SGB_All:                 agg_fill_hash_table_DIST(our version) and agg_retrieve_hash_table (our version) later
 * 2. SGB-All:
 * */

TupleTableSlot *
ExecAgg(AggState *node) {
	//printf("!!!!!!!!!!!!!!!!!!!!!%d \n", ((Agg *) node->ss.ps.plan)->overlap);
	if (node->agg_done)
		return NULL ;
	if (((Agg *) node->ss.ps.plan)->aggstrategy == AGG_HASHED)/*HASH AGGREGATION*/
	{/*CHANGED BY YASIN*/
		if (((Plan *) node->ss.ps.plan)->righttree == NULL ) { /*NO SGB_A. COULD BE SGB_U or Regular GB*/
			if ((((Agg *) node->ss.ps.plan)->max_group_diam != -1)
					|| (((Agg *) node->ss.ps.plan)->max_elem_sep != -1)) {/*SGB_Unsup-MD*/
				if (!node->table_filled)
					agg_fill_hash_table_unsupervised(node);
				return agg_retrieve_hash_table(node);
			} else {/*Regular GB (without SGB)*/

				//printf("node agg SGBANY 845 %d \n",((Agg *) node->ss.ps.plan)->distanceany);
				//printf("node agg metric 846 %d \n",((Agg *) node->ss.ps.plan)->metric);
				if (((Agg *) node->ss.ps.plan)->distanceall != -1) {
					if (!node->table_filled)
						agg_fill_hash_table_SGBTOAll(node);
					//need to build new hash table function
					return agg_retrieve_hash_table(node);
				}else if (((Agg *) node->ss.ps.plan)->distanceany != -1) {
					if (!node->table_filled){
						//agg_sgb_any
						agg_fill_hash_table_SGBTOANY(node);
					}
					//need to build new hash table function
					return agg_retrieve_hash_table(node);
				}
				else if (!node->table_filled)
					agg_fill_hash_table(node);
				return agg_retrieve_hash_table(node);
			}
		} else {/*SGB_AROUND*/
			if ((((Agg *) node->ss.ps.plan)->max_group_diam != -1)
					&& (((Agg *) node->ss.ps.plan)->max_elem_sep != -1)) {/*SGB_A-MD + MaxRadius + MaxSeparation*/
				elog(
						ERROR, "SGB_Around with both max_group_diam AND max_elem_sep is not currently supported");
			} else if (((Agg *) node->ss.ps.plan)->max_group_diam != -1) {/*SGB_Around-MD + MaxRadius*/
				if (!node->table_filled)
					agg_fill_hash_table_around_MaxR(node);
				return agg_retrieve_hash_table(node);
			} else if (((Agg *) node->ss.ps.plan)->max_elem_sep != -1) { /*SGB_Around-MD + MaxSeparation*/
				if (!node->table_filled)
					agg_fill_hash_table_around_MaxS(node);
				return agg_retrieve_hash_table_around_MaxS(node);
			} else {/*SGB_Around-MD Base*/
				if (!node->table_filled)
					agg_fill_hash_table_around(node);
				return agg_retrieve_hash_table(node);
			}
		}
	} else /*SORT AGGREGATION or AGG_PLAN*//*CHANGED BY YASIN*//*Supports only SGB-Around*/
	{
		//printf("%d\n",((Agg *) node->ss.ps.plan)->distanceall );
		if (((Agg *) node->ss.ps.plan)->distanceall != -1) /*CHANGED BY RUBY*/
		{
			//return agg_retrieve_direct_DIST(node);
			return agg_retrieve_direct(node);
		} else if (((Plan *) node->ss.ps.plan)->righttree == NULL ) /*DOES NOT USE AROUND*/
		{

			return agg_retrieve_direct(node);
		} else /*USE AROUND*/
		{

			return agg_retrieve_direct_around(node);
		}
	}
}

static void printatt(unsigned attributeId, Form_pg_attribute attributeP,
		char *value) {
	printf(
			"\t%2d: %s%s%s%s\t(typeid = %u, len = %d, typmod = %d, byval = %c)\n",
			attributeId, NameStr(attributeP->attname),
			value != NULL ? " = \"" : "", value != NULL ? value : "",
			value != NULL ? "\"" : "", (unsigned int) (attributeP->atttypid),
			attributeP->attlen, attributeP->atttypmod,
			attributeP->attbyval ? 't' : 'f');
}


/*
 * ExecAgg for distance
 * CHANGED BY RUBY
 * */
static TupleTableSlot *
agg_retrieve_direct_DIST(AggState *aggstate) {
	Agg *node = (Agg *) aggstate->ss.ps.plan;
	PlanState *outerPlan;
	ExprContext *econtext;
	ExprContext *tmpcontext;
	ProjectionInfo *projInfo;
	Datum *aggvalues;
	bool *aggnulls;
	AggStatePerAgg peragg;
	AggStatePerGroup pergroup;
	TupleTableSlot *outerslot;
	TupleTableSlot *firstSlot;
	TupleTableSlot *s;
	TupleTable *testT;
	TupleDesc tupDesc;
	TupleDesc typeinfo;
	Oid typoutput;
	bool typisvarlena;
	AggStatePerGroup pergroupTest;

	GroupData gdata;

	gdata = (GroupTupleData*) palloc0(sizeof(GroupData));

	char *value;
	int aggno;
	int numAttributes; /*Num attributes in outer plan*/
	int distanceall;
	Datum distanceallDatum;

	distanceall = node->distanceall;
	distanceallDatum = distanceall;
	distanceallDatum = Int32GetDatum(distanceallDatum);

	numAttributes = list_length(aggstate->ss.ps.plan->lefttree->targetlist);/*# attrs in outer plan*/

	testT = (TupleTable) palloc(10 * sizeof(TupleTableData));
	int j;
	for (j = 0; j < 10; j++) {
		testT[j] = ExecCreateTupleTable(10);
	}
	/*
	 * get state info from node
	 */
	outerPlan = outerPlanState(aggstate);
	/* econtext is the per-output-tuple expression context */
	econtext = aggstate->ss.ps.ps_ExprContext;
	aggvalues = econtext->ecxt_aggvalues;
	aggnulls = econtext->ecxt_aggnulls;
	/* tmpcontext is the per-input-tuple expression context */
	tmpcontext = aggstate->tmpcontext; /*YAS IDENTIFY WHEN THIS VALUE WAS INITIALIZED!*/
	projInfo = aggstate->ss.ps.ps_ProjInfo;
	peragg = aggstate->peragg;
	pergroup = aggstate->pergroup;

	pergroupTest =
			(AggStatePerGroup) palloc0(sizeof(AggStatePerGroupData) * aggstate->numaggs);
	pergroupTest->noTransValue = false;
	pergroupTest->transValue = (Datum) 0;
	pergroupTest->transValueIsNull = false;

	firstSlot = aggstate->ss.ss_ScanTupleSlot;

	/*
	 * We loop retrieving groups until we find one matching
	 * aggstate->ss.ps.qual
	 */
	while (!aggstate->agg_done) {
		/*
		 * If we don't already have the first tuple of the NEW group, fetch it
		 * from the outer plan.
		 */
		if (aggstate->grp_firstTuple == NULL ) {

			outerslot = ExecProcNode(outerPlan); /*YAS HERE IT GETS A TUPLE FROM THE LEFT SUB-PLAN*/

			if (!TupIsNull(outerslot)) {
				/*
				 * Make a copy of the first input tuple; we will use this for
				 * comparisons (in group mode) and for projection.
				 */
				aggstate->grp_firstTuple = ExecCopySlotTuple(outerslot);
			} else {
				/* outer plan produced no tuples at all */
				aggstate->agg_done = true;
				/* If we are grouping, we should produce no tuples too */
				if (node->aggstrategy != AGG_PLAIN)
					return NULL ;
			}
		}

		/*
		 * Clear the per-output-tuple context for each group
		 */
		ResetExprContext(econtext);

		/*
		 * Initialize working state for a new input tuple group
		 * YAS THIS INITIALIZES STRUCTURES TO PROCESS A NEW GROUP
		 */

		//initialize_aggregates(aggstate, peragg, pergroupTest);
		initialize_aggregates(aggstate, peragg, testT[0]->pergroup);
		initialize_aggregates(aggstate, peragg, pergroup);

		if (aggstate->grp_firstTuple != NULL ) {
			/*
			 * Store the copied first input tuple in the tuple table slot
			 * reserved for it.  The tuple will be deleted when it is cleared
			 * from the slot.
			 */
			(aggstate->grp_firstTuple, firstSlot, InvalidBuffer, true );
			//TupleTableSlot *s = ExecAllocTableSlot(testT[0]);
			//TupleTableSlot *s = ExecAllocTableSlot(gdata->entry);
			gdata = gdata->next;

			tupDesc = outerslot->tts_tupleDescriptor;

			ExecSetSlotDescriptor(s, tupDesc);

			ExecStoreTuple(firstSlot->tts_tuple, s, InvalidBuffer, true );
			DecrTupleDescRefCount(tupDesc);

			aggstate->grp_firstTuple = NULL; /* don't keep two pointers */

			/* set up for first advance_aggregates call */
			tmpcontext->ecxt_scantuple = firstSlot;

			/*
			 * Process each outer-plan tuple, and then fetch the next one,
			 * until we exhaust the outer plan or cross a group boundary.
			 */
			for (;;) {

				advance_aggregates(aggstate, pergroup); /*YAS THE FIRST TIME THIS PROCESS THE FIRST TUPLE THAT WAS READ BEFOER THIS LOOP*/
				//advance_aggregates(aggstate, pergroupTest);

				//advance_aggregates(aggstate, testT[0]->pergroup);

				//typeinfo = outerslot->tts_tupleDescriptor;

				//getTypeOutputInfo(typeinfo->attrs[0]->atttypid,
				//						  &typoutput, &typisvarlena);

				//value = OidOutputFunctionCall(typoutput, pergroup->transValue);
				//float *t = OidOutputFunctionCall_Test(pergroup->transValue);
				//pfree(t);
				//printf("%f \n", OidOutputFunctionCall_Test(pergroup->transValue));
				//printf(" %s \n", value);
				//printatt((unsigned) 1, typeinfo->attrs[0], value);

				//pfree(value);

				/* Reset per-input-tuple context after each tuple */
				ResetExprContext(tmpcontext);

				outerslot = ExecProcNode(outerPlan);

				if (!TupIsNull(outerslot)) {
					gdata->next = (GroupTupleData*) palloc0(sizeof(GroupData));

					//TupleTableSlot *s = ExecAllocTableSlot(testT[0]);
					//TupleTableSlot *s = ExecAllocTableSlot(gdata->entry);
					gdata = gdata->next;

					tupDesc = outerslot->tts_tupleDescriptor;

					ExecSetSlotDescriptor(s, tupDesc);

					ExecStoreTuple(ExecCopySlotTuple(outerslot), s,
							InvalidBuffer, true );

					DecrTupleDescRefCount(tupDesc);
				}

				/*if (!TupIsNull(outerslot))
				 {
				 printf("%d \n", nodeTag(outerPlan));
				 debugtup(outerslot, outerslot);
				 }*/

				if (TupIsNull(outerslot)) {
					/* no more outer-plan tuples available */
					aggstate->agg_done = true;
					break;
				}
				/* set up for next advance_aggregates call */
				tmpcontext->ecxt_scantuple = outerslot;

				/*
				 * If we are grouping, check whether we've crossed a group
				 * boundary.
				 */
				if (node->aggstrategy == AGG_SORTED) {
					if (!execTuplesMatch(firstSlot, outerslot, node->numCols,
							node->grpColIdx, aggstate->eqfunctions,
							tmpcontext->ecxt_per_tuple_memory)) {
						/*
						 * Save the first input tuple of the next group.
						 */
						aggstate->grp_firstTuple = ExecCopySlotTuple(outerslot);
						break;
					}
				}
			}
		}
		/*FINISHED PROCESSING A GROUP*/
		/*
		 * Done scanning input tuple group. Finalize each aggregate
		 * calculation, and stash results in the per-output-tuple context.
		 */
		for (aggno = 0; aggno < aggstate->numaggs; aggno++) {
			AggStatePerAgg peraggstate = &peragg[aggno];
			AggStatePerGroup pergroupstate = &pergroup[aggno];

			if (peraggstate->aggref->aggdistinct)
				process_sorted_aggregate(aggstate, peraggstate, pergroupstate);

			finalize_aggregate(aggstate, peraggstate, pergroupstate,
					&aggvalues[aggno], &aggnulls[aggno]);
		}

		/*
		 * Use the representative input tuple for any references to
		 * non-aggregated input columns in the qual and tlist.	(If we are not
		 * grouping, and there are no input rows at all, we will come here
		 * with an empty firstSlot ... but if not grouping, there can't be any
		 * references to non-aggregated input columns, so no problem.)
		 */
		econtext->ecxt_scantuple = firstSlot;

		/*
		 * Check the qual (HAVING clause); if the group does not match, ignore
		 * it and loop back to try to process another group.
		 */
		if (ExecQual(aggstate->ss.ps.qual, econtext, false )) {

			typeinfo = outerslot->tts_tupleDescriptor;

			getTypeOutputInfo(typeinfo->attrs[0]->atttypid, &typoutput,
					&typisvarlena);

			value = OidOutputFunctionCall(typoutput,
					testT[0]->pergroup->transValue);

			printf(" %s \n", value);

			pfree(value);

			//printf("%d \n", testTable->next);

			/*int i = 0;
			 for (i=0; i<testT[0]->next; i++)
			 {
			 debugtup(&testT[0]->array[i], NULL);
			 }*/

			/*
			 * Form and return a projection tuple using the aggregate results
			 * and the representative input tuple.	Note we do not support
			 * aggregates returning sets ...
			 */

			return ExecProject(projInfo, NULL ); /*YAS HERE IT RETURNS A RESULT TUPLE*/
		}
	}

	/* No more groups */
	return NULL ;
}

/*
 * ExecAgg for non-hashed case
 */
static TupleTableSlot *
agg_retrieve_direct(AggState *aggstate) {
	Agg *node = (Agg *) aggstate->ss.ps.plan;
	PlanState *outerPlan;
	ExprContext *econtext;
	ExprContext *tmpcontext;
	ProjectionInfo *projInfo;
	Datum *aggvalues;
	bool *aggnulls;
	AggStatePerAgg peragg;
	AggStatePerGroup pergroup;
	TupleTableSlot *outerslot;
	TupleTableSlot *firstSlot;
	int aggno;

	/*
	 * get state info from node
	 */
	outerPlan = outerPlanState(aggstate);
	/* econtext is the per-output-tuple expression context */
	econtext = aggstate->ss.ps.ps_ExprContext;
	aggvalues = econtext->ecxt_aggvalues;
	aggnulls = econtext->ecxt_aggnulls;
	/* tmpcontext is the per-input-tuple expression context */
	tmpcontext = aggstate->tmpcontext; /*YAS IDENTIFY WHEN THIS VALUE WAS INITIALIZED!*/
	projInfo = aggstate->ss.ps.ps_ProjInfo;
	peragg = aggstate->peragg;
	pergroup = aggstate->pergroup;
	firstSlot = aggstate->ss.ss_ScanTupleSlot;

	/*
	 * We loop retrieving groups until we find one matching
	 * aggstate->ss.ps.qual
	 */
	while (!aggstate->agg_done) {
		/*
		 * If we don't already have the first tuple of the NEW group, fetch it
		 * from the outer plan.
		 */
		if (aggstate->grp_firstTuple == NULL ) {

			outerslot = ExecProcNode(outerPlan); /*YAS HERE IT GETS A TUPLE FROM THE LEFT SUB-PLAN*/

			if (!TupIsNull(outerslot)) {
				/*
				 * Make a copy of the first input tuple; we will use this for
				 * comparisons (in group mode) and for projection.
				 */
				aggstate->grp_firstTuple = ExecCopySlotTuple(outerslot);
			} else {
				/* outer plan produced no tuples at all */
				aggstate->agg_done = true;
				/* If we are grouping, we should produce no tuples too */
				if (node->aggstrategy != AGG_PLAIN)
					return NULL ;
			}
		}

		/*
		 * Clear the per-output-tuple context for each group
		 */
		ResetExprContext(econtext);

		/*
		 * Initialize working state for a new input tuple group
		 * YAS THIS INITIALIZES STRUCTURES TO PROCESS A NEW GROUP
		 */
		initialize_aggregates(aggstate, peragg, pergroup);

		if (aggstate->grp_firstTuple != NULL ) {
			/*
			 * Store the copied first input tuple in the tuple table slot
			 * reserved for it.  The tuple will be deleted when it is cleared
			 * from the slot.
			 */
			ExecStoreTuple(aggstate->grp_firstTuple, firstSlot, InvalidBuffer,
					true );
			aggstate->grp_firstTuple = NULL; /* don't keep two pointers */

			/* set up for first advance_aggregates call */
			tmpcontext->ecxt_scantuple = firstSlot;

			/*
			 * Process each outer-plan tuple, and then fetch the next one,
			 * until we exhaust the outer plan or cross a group boundary.
			 */
			for (;;) {
				advance_aggregates(aggstate, pergroup); /*YAS THE FIRST TIME THIS PROCESS THE FIRST TUPLE THAT WAS READ BEFOER THIS LOOP*/

				/* Reset per-input-tuple context after each tuple */
				ResetExprContext(tmpcontext);

				outerslot = ExecProcNode(outerPlan);

				if (TupIsNull(outerslot)) {
					/* no more outer-plan tuples available */
					aggstate->agg_done = true;
					break;
				}
				/* set up for next advance_aggregates call */
				tmpcontext->ecxt_scantuple = outerslot;

				/*
				 * If we are grouping, check whether we've crossed a group
				 * boundary.
				 */
				if (node->aggstrategy == AGG_SORTED) {
					if (!execTuplesMatch(firstSlot, outerslot, node->numCols,
							node->grpColIdx, aggstate->eqfunctions,
							tmpcontext->ecxt_per_tuple_memory)) {
						/*
						 * Save the first input tuple of the next group.
						 */
						aggstate->grp_firstTuple = ExecCopySlotTuple(outerslot);
						break;
					}
				}
			}
		}
		/*FINISHED PROCESSING A GROUP*/
		/*
		 * Done scanning input tuple group. Finalize each aggregate
		 * calculation, and stash results in the per-output-tuple context.
		 */
		for (aggno = 0; aggno < aggstate->numaggs; aggno++) {
			AggStatePerAgg peraggstate = &peragg[aggno];
			AggStatePerGroup pergroupstate = &pergroup[aggno];

			if (peraggstate->aggref->aggdistinct)
				process_sorted_aggregate(aggstate, peraggstate, pergroupstate);

			finalize_aggregate(aggstate, peraggstate, pergroupstate,
					&aggvalues[aggno], &aggnulls[aggno]);
		}

		/*
		 * Use the representative input tuple for any references to
		 * non-aggregated input columns in the qual and tlist.	(If we are not
		 * grouping, and there are no input rows at all, we will come here
		 * with an empty firstSlot ... but if not grouping, there can't be any
		 * references to non-aggregated input columns, so no problem.)
		 */
		econtext->ecxt_scantuple = firstSlot;

		/*
		 * Check the qual (HAVING clause); if the group does not match, ignore
		 * it and loop back to try to process another group.
		 */
		if (ExecQual(aggstate->ss.ps.qual, econtext, false )) {
			/*
			 * Form and return a projection tuple using the aggregate results
			 * and the representative input tuple.	Note we do not support
			 * aggregates returning sets ...
			 */
			return ExecProject(projInfo, NULL ); /*YAS HERE IT RETURNS A RESULT TUPLE*/
		}
	}

	/* No more groups */
	return NULL ;
}

/* CHANGED BY YASIN
 * ExecAgg for non-hashed case WHEN USING GROUP AROUND
 */
static TupleTableSlot *
agg_retrieve_direct_around(AggState *aggstate) {
	Agg *node = (Agg *) aggstate->ss.ps.plan;
	PlanState *outerPlan;
	ExprContext *econtext;
	ExprContext *tmpcontext;
	ProjectionInfo *projInfo;
	Datum *aggvalues;
	bool *aggnulls;
	AggStatePerAgg peragg;
	AggStatePerGroup pergroup;
	TupleTableSlot *outerslot;
	TupleTableSlot *firstSlot;
	int aggno;

	/*CHANGED BY YASIN*/
	PlanState *innerPlan;
	TupleTableSlot *innerslot; /*pointer to inner slot*/
	TupleTableSlot *pivot1Slot; /*pointer to pivot1 slot*/
	TupleTableSlot *pivot2Slot; /*pointer to pivot2 slot*/

	/*
	 * get state info from node
	 */
	outerPlan = outerPlanState(aggstate);
	innerPlan = innerPlanState(aggstate);/*CHANGED BY YASIN*//*It is null if GROUP AROUND is not used*/

	/* econtext is the per-output-tuple expression context */
	econtext = aggstate->ss.ps.ps_ExprContext;
	aggvalues = econtext->ecxt_aggvalues;
	aggnulls = econtext->ecxt_aggnulls;
	/* tmpcontext is the per-input-tuple expression context */
	tmpcontext = aggstate->tmpcontext; /*Initialized in execInitAgg*/
	projInfo = aggstate->ss.ps.ps_ProjInfo;
	peragg = aggstate->peragg;
	pergroup = aggstate->pergroup;
	firstSlot = aggstate->ss.ss_ScanTupleSlot; /*aggstate->ss.ss_ScanTupleSlot already points to the tuple table maintained in the master state*/

	pivot1Slot = aggstate->pivot1Slot; /*CHANGED BY YASIN*/
	pivot2Slot = aggstate->pivot2Slot; /*CHANGED BY YASIN*/

	/*
	 * We loop retrieving groups until we find one matching
	 * aggstate->ss.ps.qual
	 */
	while (!aggstate->agg_done)/*1 loop for each group*/
	{
		/*
		 * If we don't already have the first tuple of the NEW group, fetch it
		 * from the outer plan.
		 */
		if (aggstate->grp_firstTuple == NULL ) {
			outerslot = ExecProcNode(outerPlan); /*GETS A TUPLE FROM THE LEFT SUB-PLAN*/
			/*CHANGED BY YASIN ONLY FOR DEBUG
			 printf("Tuple de tabla base:\n");
			 print_slot(outerslot);*/

			if (!TupIsNull(outerslot)) {
				/*
				 * Make a copy of the first input tuple; we will use this for
				 * comparisons (in group mode) and for projection.
				 */
				aggstate->grp_firstTuple = ExecCopySlotTuple(outerslot);
			} else {
				/* outer plan produced no tuples at all */
				aggstate->agg_done = true;
				/* If we are grouping, we should produce no tuples too */
				if (node->aggstrategy != AGG_PLAIN)
					return NULL ;
			}
		}

		/*CHANGED BY YASIN*//*Loads Pivot1 and Pivot2*/
		if (aggstate->grp_Pivot1HeapTuple == NULL ) /*This happens at the very beginning of all the groups*/
		{
			innerslot = ExecProcNode(innerPlan); /*GETS A TUPLE FROM THE RIGHT SUB-PLAN*/
			/*CHANGED BY YASIN ONLY FOR DEBUG
			 printf("Tuple de tabla around 1:\n");
			 print_slot(innerslot);*/

			if (!TupIsNull(innerslot)) {
				/* Initialize pivot slots by cloning input slot (innerslot) */
				if (pivot1Slot->tts_tupleDescriptor == NULL ) {/*if true pivot2Slot->tts_tupleDescriptor is expected to be also NULL*/
					ExecSetSlotDescriptor(pivot1Slot,
							innerslot->tts_tupleDescriptor);
					ExecSetSlotDescriptor(pivot2Slot,
							innerslot->tts_tupleDescriptor);
				}

				/*Pivot 1*/
				aggstate->grp_Pivot1HeapTuple = ExecCopySlotTuple(innerslot); /*make an independent copy*/
				/*make pivot1Slot point to aggstate->grp_Pivot1HeapTuple*/
				ExecStoreTuple(aggstate->grp_Pivot1HeapTuple, pivot1Slot,
						InvalidBuffer, true );

				/*Pivot 2*/
				innerslot = ExecProcNode(innerPlan); /*GETS A TUPLE FROM THE RIGHT SUB-PLAN*/
				/*CHANGED BY YASIN ONLY FOR DEBUG
				 printf("Tuple de tabla around 2:\n");
				 print_slot(innerslot);*/

				if (!TupIsNull(innerslot)) {
					aggstate->grp_Pivot2HeapTuple = ExecCopySlotTuple(
							innerslot);/*make an independent copy*/
					/*make pivot2Slot point to aggstate->grp_Pivot2HeapTuple*/
					ExecStoreTuple(aggstate->grp_Pivot2HeapTuple, pivot2Slot,
							InvalidBuffer, true );
				}/*Obs: if Pivot2 does not exist, grp_Pivot2HeapTuple is NULL*/
			} else /*Zero tuples in right subplan*/
			{
				/* inner plan produced no tuples at all */
				aggstate->agg_done = true;
				/* If we are grouping, we should produce no tuples too */
				if (node->aggstrategy != AGG_PLAIN) /*This is true in case og Group Around*/
					return NULL ;
			}
		}/*Else: Pivots should be already updated. Pivots are always updated at the end of previous cicle*/

		/*
		 * Clear the per-output-tuple context for each group
		 */
		ResetExprContext(econtext);

		/*
		 * Initialize working state for a new input tuple group
		 * THIS INITIALIZES STRUCTURES TO PROCESS A NEW GROUP
		 */
		initialize_aggregates(aggstate, peragg, pergroup);

		if (aggstate->grp_firstTuple != NULL ) /*This should be always true for Group Around*/
		{
			/*
			 * Store the copied first input tuple in the tuple table slot
			 * reserved for it.  The tuple will be deleted when it is cleared
			 * from the slot.
			 */
			ExecStoreTuple(aggstate->grp_firstTuple, firstSlot, InvalidBuffer,
					true );
			aggstate->grp_firstTuple = NULL; /* don't keep two pointers */

			/* set up for first advance_aggregates call */
			tmpcontext->ecxt_scantuple = firstSlot;

			/*
			 * Process each OUTER-PLAN tuple, and then fetch the next one,
			 * until we exhaust the outer plan or cross a group boundary.
			 */
			for (;;) {
				advance_aggregates(aggstate, pergroup); /*THE FIRST TIME THIS PROCESSES THE FIRST TUPLE THAT WAS READ BEFOER THIS LOOP*/

				/* Reset per-input-tuple context after each tuple */
				ResetExprContext(tmpcontext);

				outerslot = ExecProcNode(outerPlan);
				/*CHANGED BY YASIN ONLY FOR DEBUG
				 printf("Tuple de tabla base (Bucle):\n");
				 print_slot(outerslot);*/

				if (TupIsNull(outerslot)) {
					/* no more outer-plan tuples available */
					aggstate->agg_done = true;
					break;
				}
				/* set up for next advance_aggregates call */
				tmpcontext->ecxt_scantuple = outerslot;

				/*
				 * If we are grouping, check whether we've crossed a group
				 * boundary.
				 */
				if (node->aggstrategy == AGG_SORTED) {
					/*CHANGED BY YASIN ONLY FOR DEBUG
					 printf ("Esto se ve desde NodeAgg\n");
					 print_slot(pivot1Slot);
					 print_slot(pivot2Slot);
					 print_slot(firstSlot);
					 print_slot(outerslot);*/

					/*CHANGED BY YASIN*/
					if (!execTuplesMatchAround(pivot1Slot, pivot2Slot, /*Pivot slots*/
					outerslot,/*Current tuple slot*/
					node->numCols, node->grpColIdx, aggstate->eqfunctions, /* = */
					aggstate->ltfunctions, /* < */
					aggstate->minusfunctions, /* - */
					tmpcontext->ecxt_per_tuple_memory)) {/*Obs: Enters here only if both pivots are not Null */
						/*Save the first input tuple (from base table) of the next group*/
						aggstate->grp_firstTuple = ExecCopySlotTuple(outerslot);
						/*We FIND now the correct Pivots for the next group*/
						for (;;) {
							/*Advance Pivot 1 = Make pivot1Slot.tuple point to aggstate->grp_Pivot2HeapTuple*/
							ExecStoreTuple(aggstate->grp_Pivot2HeapTuple,
									pivot1Slot, InvalidBuffer, true );/*This also frees the previous content*/
							aggstate->grp_Pivot1HeapTuple =
									aggstate->grp_Pivot2HeapTuple;

							/*Get Pivot 2*/
							aggstate->grp_Pivot2HeapTuple = NULL;
							innerslot = ExecProcNode(innerPlan); /*GETS A TUPLE FROM THE RIGHT SUB-PLAN*/
							pivot2Slot->tts_shouldFree = false; /*To avoid pfree of new pivot1*/

							/*CHANGED BY YASIN ONLY FOR DEBUG
							 printf("Tuple de tabla around (Bucle):\n");
							 print_slot(innerslot);*/

							if (!TupIsNull(innerslot)) /*There is second Pivot*/
							{
								aggstate->grp_Pivot2HeapTuple =
										ExecCopySlotTuple(innerslot);/*make an independent copy*/
								/*make pivot2Slot point to aggstate->grp_Pivot2HeapTuple*/
								ExecStoreTuple(aggstate->grp_Pivot2HeapTuple,
										pivot2Slot, InvalidBuffer, true );

								/*Check that current tuple BELONGS to group represented by the Pivots*/
								if (execTuplesMatchAround(pivot1Slot,
										pivot2Slot, /*Pivot slots*/
										outerslot,/*Current tuple slot*/
										node->numCols, node->grpColIdx,
										aggstate->eqfunctions, /* = */
										aggstate->ltfunctions, /* < */
										aggstate->minusfunctions, /* - */
										tmpcontext->ecxt_per_tuple_memory))
									break;/*We found the right pivots and the PivotXHeapTuple attributes have the right value*/
							} else
								break; /*There is not more tuples for pivots. Second Pivot will be NULL*/
						}
						break; /*Ready for next group*/
					}

				}
			}
		}
		/*FINISHED PROCESSING A GROUP*/
		/*
		 * Done scanning input tuple group. Finalize each aggregate
		 * calculation, and stash results in the per-output-tuple context.
		 */
		for (aggno = 0; aggno < aggstate->numaggs; aggno++) {
			AggStatePerAgg peraggstate = &peragg[aggno];
			AggStatePerGroup pergroupstate = &pergroup[aggno];

			if (peraggstate->aggref->aggdistinct)
				process_sorted_aggregate(aggstate, peraggstate, pergroupstate);

			finalize_aggregate(aggstate, peraggstate, pergroupstate,
					&aggvalues[aggno], &aggnulls[aggno]);
		}

		/*
		 * Use the representative input tuple for any references to
		 * non-aggregated input columns in the qual and tlist.	(If we are not
		 * grouping, and there are no input rows at all, we will come here
		 * with an empty firstSlot ... but if not grouping, there can't be any
		 * references to non-aggregated input columns, so no problem.)
		 */
		econtext->ecxt_scantuple = firstSlot;

		/*
		 * Check the qual (HAVING clause); if the group does not match, ignore
		 * it and loop back to try to process another group.
		 */
		if (ExecQual(aggstate->ss.ps.qual, econtext, false )) {
			/*
			 * Form and return a projection tuple using the aggregate results
			 * and the representative input tuple.	Note we do not support
			 * aggregates returning sets ...
			 */
			return ExecProject(projInfo, NULL ); /*HERE IT RETURNS A RESULT TUPLE*/
		}
	}

	/* No more groups */
	return NULL ;
}

/*
 * ExecAgg for hashed case: phase 1, read input and build hash table
 */
/*Changed by Mingjie
 *phase 1: find the groups for the tuples can join, update the aggregate on that group
 *phase 2: build new group for the tuples can not join, but have overlaps,update the group
 * */
static void agg_fill_hash_table_SGBTOAll(AggState *aggstate) {

	Agg *node = (Agg *) aggstate->ss.ps.plan;
	int distanceMetric=node->metric;

	PlanState *outerPlan;
	ExprContext *tmpcontext;
	TupleTableSlot *outerslot;

	int numAtt=node->numCols;

	int overlap;
	//get overlap here

	/*get the overlap type from here*/
	overlap = node->overlap; /*duplicated: any=overlap=1, formnew=2, eliminated=overlap=3, duplicate=4*/
	//distanceMetric = 1;/*(l2)distanceMetric=0, (l1)distanceMetric=1, (linfinity)distanceMetric=2*/

	/*
	 * get state info from node
	 */
	outerPlan = outerPlanState(aggstate);
	/* tmpcontext is the per-input-tuple expression context */
	tmpcontext = aggstate->tmpcontext;

	bool firstone = true;
	int num = 0;

	int attchose=0;

	/*
	 * Process each outer-plan tuple, and then fetch the next one, until we
	 * exhaust the outer plan.
	 */
	for (;;) {
		outerslot = ExecProcNode(outerPlan);/*outerslot POINTS TO CURRENT TUPLE*/
		if (TupIsNull(outerslot))
			break;
		/* set up for advance_aggregates call */
		tmpcontext->ecxt_scantuple = outerslot;

		if (firstone) {
			if (aggstate->modifiedSlot->tts_tupleDescriptor == NULL ) {
				ExecSetSlotDescriptor(aggstate->modifiedSlot,
						outerslot->tts_tupleDescriptor);
			}

			if (aggstate->pivot1Slot->tts_tupleDescriptor == NULL )
				ExecSetSlotDescriptor(aggstate->pivot1Slot,
						outerslot->tts_tupleDescriptor);

			ExecCopySlot(aggstate->pivot1Slot, outerslot);
			ExecCopySlot(aggstate->modifiedSlot, outerslot);
		}

		if(distanceMetric==1||numAtt==1)
		{
			//printf("Lone distance \n");
			attchose=1;
				if (overlap == 1) {
					agg_fill_hash_table_overlap_random_l1(aggstate, outerslot);
				} else if (overlap == 2) {
					agg_fill_hash_table_overlap_newgroup_l1(aggstate, outerslot);
				} else if (overlap == 3) {
					agg_fill_hash_table_overlap_eliminated_l1(aggstate, outerslot);
				}

		}else if ((distanceMetric==2||distanceMetric==-1)&&numAtt>=2)
		{
			//printf("LTWO distance \n");
			attchose=2;
			if (overlap == 1) {
					agg_fill_hash_table_overlap_random_l2(aggstate, outerslot);
				} else if (overlap == 2) {
					agg_fill_hash_table_overlap_newgroup_l2(aggstate, outerslot);
				} else if (overlap == 3) {
					agg_fill_hash_table_overlap_eliminated_l2(aggstate, outerslot);
				}

		}

		//advance_aggregates(aggstate, entry->pergroup); /*YAS THIS SHOULD USE THE ORIGINAL TUPLE*/

		/* Reset per-input-tuple context after each tuple */
		ResetExprContext(tmpcontext);
		firstone = false;
		aggstate->numbertuples++;
	}

	/*****************************************************/
	if (node->overlap == 2 && aggstate->numbertuples > 2) {

		sgb_newgroup_helper(0, aggstate,attchose);

	}					//if

	/*****************************************************/
	/*****************************************************/
	aggstate->table_filled = true;
	/* Initialize to walk the hash table */
	ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);
}

/*
 * ExecAgg for hashed case: phase 1, read input and build hash table
 */
/*Changed by Mingjie
 *phase 1: find the groups for the tuples can join, update the aggregate on that group
 *phase 2: build new group for the tuples can not join, but have overlaps,update the group
 * */
static void agg_fill_hash_table_SGBTOANY(AggState *aggstate) {
	Agg *node = (Agg *) aggstate->ss.ps.plan;

	PlanState *outerPlan;
	ExprContext *tmpcontext;
	TupleTableSlot *outerslot;

	int distanceMetric=node->metric;

	int overlap;
	//get overlap here

	/*get the overlap type from here*/
	overlap = node->overlap; /*duplicated: any=overlap=1, formnew=2, eliminated=overlap=3, duplicate=4*/
	//distanceMetric = 1;/*(l2)distanceMetric=0, (l1)distanceMetric=1, (linfinity)distanceMetric=2*/

	/*
	 * get state info from node
	 */
	outerPlan = outerPlanState(aggstate);
	/* tmpcontext is the per-input-tuple expression context */
	tmpcontext = aggstate->tmpcontext;

	bool firstone = true;
	int num = 0;
	/*
	 * Process each outer-plan tuple, and then fetch the next one, until we
	 * exhaust the outer plan.
	 */

	for (;;) {
		outerslot = ExecProcNode(outerPlan);/*outerslot POINTS TO CURRENT TUPLE*/
		if (TupIsNull(outerslot))
			break;

		if (firstone) {
			if (aggstate->modifiedSlot->tts_tupleDescriptor == NULL ) {
				ExecSetSlotDescriptor(aggstate->modifiedSlot,
						outerslot->tts_tupleDescriptor);
			}

			ExecCopySlot(aggstate->modifiedSlot, outerslot);

		if (aggstate->pivot1Slot->tts_tupleDescriptor == NULL ) {
						ExecSetSlotDescriptor(aggstate->pivot1Slot,
								outerslot->tts_tupleDescriptor);
					}

			ExecCopySlot(aggstate->pivot1Slot, outerslot);

			firstone=false;
			}

		/* set up for advance_aggregates call */
		tmpcontext->ecxt_scantuple = outerslot;
		agg_fill_hash_table_SGBANY(aggstate,outerslot);
		//advance_aggregates(aggstate, entry->pergroup); /*YAS THIS SHOULD USE THE ORIGINAL TUPLE*/
		/* Reset per-input-tuple context after each tuple */
		ResetExprContext(tmpcontext);
		aggstate->numbertuples++;
	}

	/*****************************************************/
	/*****************************************************/
	aggstate->table_filled = true;

	IterateSets(aggstate->dsjset,aggstate,aggstate->pivot1Slot);
	/* Initialize to walk the hash table */
	ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);
}
/*
 * ExecAgg for hashed case: phase 1, read input and build hash table
 */
static void agg_fill_hash_table(AggState *aggstate) {
	PlanState *outerPlan;
	ExprContext *tmpcontext;
	AggHashEntry entry;
	TupleTableSlot *outerslot;

	/*
	 * get state info from node
	 */
	outerPlan = outerPlanState(aggstate);
	/* tmpcontext is the per-input-tuple expression context */
	tmpcontext = aggstate->tmpcontext;

	/*
	 * Process each outer-plan tuple, and then fetch the next one, until we
	 * exhaust the outer plan.
	 */
	for (;;) {
		outerslot = ExecProcNode(outerPlan);/*outerslot POINTS TO CURRENT TUPLE*/
		if (TupIsNull(outerslot))
			break;
		/* set up for advance_aggregates call */
		tmpcontext->ecxt_scantuple = outerslot;

		/* Find or build hashtable entry for this tuple's group */
		entry = lookup_hash_entry(aggstate, outerslot);/*YAS THIS SHOULD USE THE MODIFIED TUPLE*/

		/* Advance the aggregates */
		advance_aggregates(aggstate, entry->pergroup); /*YAS THIS SHOULD USE THE ORIGINAL TUPLE*/

		/* Reset per-input-tuple context after each tuple */
		ResetExprContext(tmpcontext);
	}

	aggstate->table_filled = true;
	/* Initialize to walk the hash table */
	ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);
}

/*CHANGED BY YASIN*/
/*
 * ExecAgg for hashed case: phase 1, read input and build hash table
 * This function supports HASH GROUPING WITH N COLS
 * It does not support max-radius nor max-separation
 */
static void agg_fill_hash_table_around(AggState *aggstate) {
	PlanState *outerPlan;
	ExprContext *tmpcontext;
	AggHashEntry entry;
	TupleTableSlot *outerslot;

	/*CHANGED BY YASIN*/
	Agg *node = (Agg *) aggstate->ss.ps.plan;
	AttrNumber aroundOuterAtt; /*Index of the around attribute in left plan. Starts at 1*/
	AttrNumber aroundInnerAtt; /*Index of the around attribute in right plan. Starts at 1*/
	AttrNumber *aroundAttPtr;
	int indexInGroupBy; /*Index in group-by list (starts at 0) of col that uses around */
	bool dummyIsNull;
	int numAttributes; /*Num attributes in outer plan*/
	Datum centerDatum;
	Datum *values;
	bool *nulls;
	bool *replace;
	PlanState *innerPlan;
	TupleTableSlot *innerslot; /*pointer to inner slot*/
	TupleTableSlot *pivot1Slot; /*pointer to pivot1 slot*/
	TupleTableSlot *pivot2Slot; /*pointer to pivot2 slot*/

	aroundAttPtr = node->grpColIdx;
	indexInGroupBy = 0;/*We assume that col in group by that uses around is the first one*/
	aroundOuterAtt = aroundAttPtr[indexInGroupBy]; /*Indices in node->grpColIdx start at 1*/
	aroundInnerAtt = 1; /*we use the first col in the right plan as the around tuples*/
	numAttributes = list_length(aggstate->ss.ps.plan->lefttree->targetlist);/*# attrs in outer plan*/

	/*printf ("numAttributes in outer plan: %d \n", numAttributes);
	 printf ("Index of the around attribute in left plan: %d \n", aroundOuterAtt);
	 printf ("Index of the around attribute in right plan: %d \n", aroundInnerAtt);*/

	values = (Datum *) palloc(numAttributes * sizeof(Datum)); /*array*/
	nulls = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
	replace = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
	memset(values, 0, numAttributes * sizeof(Datum));
	memset(nulls, false, numAttributes * sizeof(bool));
	memset(replace, false, numAttributes * sizeof(bool));

	outerPlan = outerPlanState(aggstate);/*get state*/
	innerPlan = innerPlanState(aggstate);/*CHANGED BY YASIN*//*Inse we are procesing around we expect a non-null value*/
	tmpcontext = aggstate->tmpcontext;/* tmpcontext is the per-input-tuple expression context */

	pivot1Slot = aggstate->pivot1Slot; /*CHANGED BY YASIN*/
	pivot2Slot = aggstate->pivot2Slot; /*CHANGED BY YASIN*/

	/*Loads Pivot1 and Pivot2*//*CHANGED BY YASIN*/
	innerslot = ExecProcNode(innerPlan); /*GETS A TUPLE FROM THE RIGHT SUB-PLAN*/
	/*printf("Tuple de tabla around 1:\n");
	 print_slot(innerslot);*/

	if (!TupIsNull(innerslot)) {
		/* Initialize pivot slots by cloning input slot (innerslot) */
		if (pivot1Slot->tts_tupleDescriptor == NULL ) {/*if true pivot2Slot->tts_tupleDescriptor is expected to be also NULL*/
			ExecSetSlotDescriptor(pivot1Slot, innerslot->tts_tupleDescriptor);
			ExecSetSlotDescriptor(pivot2Slot, innerslot->tts_tupleDescriptor);
		}

		/*Pivot 1*/
		aggstate->grp_Pivot1HeapTuple = ExecCopySlotTuple(innerslot); /*make an independent copy*/
		/*make pivot1Slot point to aggstate->grp_Pivot1HeapTuple*/
		ExecStoreTuple(aggstate->grp_Pivot1HeapTuple, pivot1Slot, InvalidBuffer,
				true );

		/*Pivot 2*/
		innerslot = ExecProcNode(innerPlan); /*GETS A TUPLE FROM THE RIGHT SUB-PLAN*/
		/*CHANGED BY YASIN ONLY FOR DEBUG*/
		/*printf("Tuple de tabla around 2:\n");
		 print_slot(innerslot);*/

		if (!TupIsNull(innerslot)) {
			aggstate->grp_Pivot2HeapTuple = ExecCopySlotTuple(innerslot);/*make an independent copy*/
			/*make pivot2Slot point to aggstate->grp_Pivot2HeapTuple*/
			ExecStoreTuple(aggstate->grp_Pivot2HeapTuple, pivot2Slot,
					InvalidBuffer, true );
		}/*Obs: if Pivot2 does not exist, grp_Pivot2HeapTuple is NULL*/
	} else /*Zero tuples in right subplan*/
	{ /*Finalize*/
		pfree(values);/*free space used by arrays*/
		pfree(nulls);
		pfree(replace);
		aggstate->table_filled = true;
		/* Initialize to walk the hash table */
		ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);
		return;
	}

	/*
	 * Process each outer-plan tuple, and then fetch the next one, until we
	 * exhaust the outer plan.
	 */
	for (;;) {
		HeapTuple newHeapTuple;/*CHANGED BY YASIN*/

		outerslot = ExecProcNode(outerPlan);/*outerslot POINTS TO CURRENT TUPLE*/
		if (TupIsNull(outerslot))
			break;
		/* set up for advance_aggregates call */
		tmpcontext->ecxt_scantuple = outerslot; /*advance_aggregates should used riginal tuple*/

		/*CHECK IF PIVOTS ARE CORRECT (contain current outer tuple)*/
		/*printf ("Esto se ve desde NodeAgg\n");
		 print_slot(pivot1Slot);
		 print_slot(pivot2Slot);
		 print_slot(outerslot);*/
		if (!execTuplesMatchAroundHash(pivot1Slot, pivot2Slot, /*Pivot slots*/
		outerslot,/*Current tuple slot*/
		aroundOuterAtt, /*Index of the around attribute in left plan. Starts at 1*/
		indexInGroupBy, aggstate->eqfunctions, /* = */
		aggstate->ltfunctions, /* < */
		aggstate->minusfunctions, /* - */
		tmpcontext->ecxt_per_tuple_memory)) {/*Obs: Enters here only if both pivots are not Null */
			/*We FIND the correct Pivots that contain current outer tuple*/
			for (;;) {
				/*Advance Pivot 1 = Make pivot1Slot.tuple point to aggstate->grp_Pivot2HeapTuple*/
				ExecStoreTuple(aggstate->grp_Pivot2HeapTuple, pivot1Slot,
						InvalidBuffer, true );/*This also frees the previous content*/
				aggstate->grp_Pivot1HeapTuple = aggstate->grp_Pivot2HeapTuple;

				/*Get Pivot 2*/
				aggstate->grp_Pivot2HeapTuple = NULL;
				innerslot = ExecProcNode(innerPlan); /*GETS A TUPLE FROM THE RIGHT SUB-PLAN*/
				pivot2Slot->tts_shouldFree = false; /*To avoid pfree of new pivot1*/

				/*CHANGED BY YASIN ONLY FOR DEBUG*/
				/*printf("Tuple de tabla around (Bucle):\n");
				 print_slot(innerslot);*/

				if (!TupIsNull(innerslot)) /*There is second Pivot*/
				{
					aggstate->grp_Pivot2HeapTuple = ExecCopySlotTuple(
							innerslot);/*make an independent copy*/
					/*make pivot2Slot point to aggstate->grp_Pivot2HeapTuple*/
					ExecStoreTuple(aggstate->grp_Pivot2HeapTuple, pivot2Slot,
							InvalidBuffer, true );

					/*Check that current tuple BELONGS to group represented by the Pivots*/
					if (execTuplesMatchAroundHash(pivot1Slot, pivot2Slot, /*Pivot slots*/
					outerslot,/*Current tuple slot*/
					aroundOuterAtt, /*Index of the around attribute in left plan. Starts at 1*/
					indexInGroupBy, aggstate->eqfunctions, /* = */
					aggstate->ltfunctions, /* < */
					aggstate->minusfunctions, /* - */
					tmpcontext->ecxt_per_tuple_memory))
						break;/*We found the right pivots and the PivotXHeapTuple attributes have the right value*/
				} else
					break; /*There is not more tuples for pivots. Second Pivot will be NULL*/
			}
		}

		/*BUILD MODIFIED VERSION OF THE TUPLE BEING PROCESSED USING CENTER VALUE. Center value is taken from Pivot 1*/
		centerDatum = slot_getattr(pivot1Slot, aroundInnerAtt, &dummyIsNull);/*dummyIsNull should be always false*/
		replace[aroundOuterAtt - 1] = true;
		values[aroundOuterAtt - 1] = centerDatum;

		/*Create an independent new HeapTuple with the modified version*/
		newHeapTuple = heap_modify_tuple(outerslot->tts_tuple,
				outerslot->tts_tupleDescriptor, values, nulls, replace);
		/* Initialize modifiedSlot by cloning outerslot */
		if (aggstate->modifiedSlot->tts_tupleDescriptor == NULL )
			ExecSetSlotDescriptor(aggstate->modifiedSlot,
					outerslot->tts_tupleDescriptor);
		/*Make aggstate->modifiedSlot.tuple point to new HeapTuple*/
		ExecStoreTuple(newHeapTuple, aggstate->modifiedSlot, InvalidBuffer,
				true );/*This will free memory of previous tuple too*/
		/*printf("Tuple de tabla base ORIGINAL:\n");print_slot(outerslot);
		 printf("Tuple de tabla base UPDATED:\n");print_slot(aggstate->modifiedSlot);*/

		/* Find or build hashtable entry for this tuple's group*/
		entry = lookup_hash_entry(aggstate, aggstate->modifiedSlot);/*THIS USES THE MODIFIED TUPLE*/

		/* Advance the aggregates */
		advance_aggregates(aggstate, entry->pergroup); /*THIS USES THE ORIGINAL TUPLE*/

		/* Reset per-input-tuple context after each tuple */
		ResetExprContext(tmpcontext);
	}

	/*free space used by arrays*/
	pfree(values);
	pfree(nulls);
	pfree(replace);

	aggstate->table_filled = true;
	/* Initialize to walk the hash table */
	ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);
}

/*CHANGED BY YASIN*/
/*
 * ExecAgg for hashed case: phase 1, read input and build hash table
 * This function supports HASH GROUPING WITH N COLS and max-separation
 */
static void agg_fill_hash_table_around_MaxS(AggState *aggstate) {
	PlanState *outerPlan;
	ExprContext *tmpcontext;
	AggHashEntry entry;
	TupleTableSlot *outerslot;

	/*CHANGED BY YASIN*/
	Agg *node = (Agg *) aggstate->ss.ps.plan;
	AttrNumber aroundOuterAtt; /*Index of the around attribute in left plan. Starts at 1*/
	AttrNumber *aroundAttPtr;
	int indexInGroupBy; /*Index in group-by list (starts at 0) of col that uses around */
	int numAttributes; /*Num attributes in outer plan*/
	Datum *values;
	bool *nulls;
	bool *replace;
	PlanState *innerPlan;
	TupleTableSlot *innerslot; /*pointer to inner slot*/
	TupleTableSlot *pivot1Slot; /*pointer to pivot1 slot*/
	TupleTableSlot *pivot2Slot; /*pointer to pivot2 slot*/
	int maxSeparationInt; /*integer value of this parameter*/
	Datum maxSeparationDatum; /*the datum version of maxSeparationInt*/
	Datum basePositionDatum; /*the datum that contains the value of the last element already processed in the current group*/
	FmgrInfo *ltfunctions;
	FmgrInfo *minusfunctions;
	AttrNumber attPivots; /*Index of the around attribute in right plan. Starts at 1*/
	Datum attrPivot1, attrCurrent;
	bool ignoreUntilNextGroup;
	bool isNull1, isNull3;
	bool curItemConnectedWithCenter;/*True when current base tuple is connected with center element*/
	bool curChainConnectedWithCenter;/*True in chain is connected with center element*/
	bool firstTuple; /*True if element being analized is the first tuple of base table*/
	bool testMatch;

	maxSeparationInt = node->max_elem_sep; /*max_elem_sep comes from the SQL statement*/
	maxSeparationDatum = Int32GetDatum(maxSeparationInt);
	aroundAttPtr = node->grpColIdx;
	indexInGroupBy = 0;/*We assume that col in group by that uses around is the first one*/
	aroundOuterAtt = aroundAttPtr[indexInGroupBy]; /*Indices in node->grpColIdx start at 1*/
	numAttributes = list_length(aggstate->ss.ps.plan->lefttree->targetlist);/*# attrs in outer plan*/
	ignoreUntilNextGroup = false;
	curChainConnectedWithCenter = false;
	firstTuple = true;
	basePositionDatum = 0;/*we will change this value when we process 1st tuple*/

	/*printf ("numAttributes in outer plan: %d \n", numAttributes);
	 printf ("Index of the around attribute in left plan: %d \n", aroundOuterAtt);
	 printf ("Index of the around attribute in right plan: %d \n", aroundInnerAtt);*/

	values = (Datum *) palloc(numAttributes * sizeof(Datum)); /*array*/
	nulls = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
	replace = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
	memset(values, 0, numAttributes * sizeof(Datum));
	memset(nulls, false, numAttributes * sizeof(bool));
	memset(replace, false, numAttributes * sizeof(bool));

	outerPlan = outerPlanState(aggstate);/*get state*/
	innerPlan = innerPlanState(aggstate);/*CHANGED BY YASIN*//*Since we are procesing around we expect a non-null value*/
	tmpcontext = aggstate->tmpcontext; /*tmpcontext is the per-input-tuple expression context */
	pivot1Slot = aggstate->pivot1Slot; /*CHANGED BY YASIN*/
	pivot2Slot = aggstate->pivot2Slot; /*CHANGED BY YASIN*/

	/*Loads Pivot1 and Pivot2*//*CHANGED BY YASIN*/
	innerslot = ExecProcNode(innerPlan); /*GETS A TUPLE FROM THE RIGHT SUB-PLAN*/
	/*printf("Tuple de tabla around 1:\n");
	 print_slot(innerslot);*/
	if (!TupIsNull(innerslot)) {
		/* Initialize pivot slots by cloning input slot (innerslot) */
		if (pivot1Slot->tts_tupleDescriptor == NULL ) {/*if true pivot2Slot->tts_tupleDescriptor is expected to be also NULL*/
			ExecSetSlotDescriptor(pivot1Slot, innerslot->tts_tupleDescriptor);
			ExecSetSlotDescriptor(pivot2Slot, innerslot->tts_tupleDescriptor);
		}

		/*Pivot 1*/
		aggstate->grp_Pivot1HeapTuple = ExecCopySlotTuple(innerslot); /*make an independent copy*/
		/*make pivot1Slot point to aggstate->grp_Pivot1HeapTuple*/
		ExecStoreTuple(aggstate->grp_Pivot1HeapTuple, pivot1Slot, InvalidBuffer,
				true );

		/*Pivot 2*/
		innerslot = ExecProcNode(innerPlan); /*GETS A TUPLE FROM THE RIGHT SUB-PLAN*/
		/*CHANGED BY YASIN ONLY FOR DEBUG*/
		/*printf("Tuple de tabla around 2:\n");
		 print_slot(innerslot);*/

		if (!TupIsNull(innerslot)) {
			aggstate->grp_Pivot2HeapTuple = ExecCopySlotTuple(innerslot);/*make an independent copy*/
			/*make pivot2Slot point to aggstate->grp_Pivot2HeapTuple*/
			ExecStoreTuple(aggstate->grp_Pivot2HeapTuple, pivot2Slot,
					InvalidBuffer, true );
		}/*Obs: if Pivot2 does not exist, grp_Pivot2HeapTuple is NULL*/
	} else /*Zero tuples in right subplan*/
	{ /*Finalize*/
		pfree(values);/*free space used by arrays*/
		pfree(nulls);
		pfree(replace);
		aggstate->table_filled = true;
		/* Initialize to walk the hash table */
		ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);
		return;
	}

	/*
	 * Process each outer-plan tuple, and then fetch the next one, until we
	 * exhaust the outer plan.
	 */
	attPivots = (int16) 1;/*We always use the first col of the right(inner) plan. Starts at 1*/
	ltfunctions = aggstate->ltfunctions;
	minusfunctions = aggstate->minusfunctions;
	for (;;) {
		HeapTuple newHeapTuple;/*CHANGED BY YASIN*/
		outerslot = ExecProcNode(outerPlan);/*outerslot POINTS TO CURRENT TUPLE*/

		if (TupIsNull(outerslot)) {
			if (curChainConnectedWithCenter)/*Finish processing of last chain if needed*/
				enable_chain(aggstate);
			break;
		} else {/*We have a base tuple to process*/
			/* set up for advance_aggregates call */
			tmpcontext->ecxt_scantuple = outerslot; /*advance_aggregates should used riginal tuple*/

			testMatch = execTuplesMatchAroundHash(pivot1Slot, pivot2Slot,
					outerslot, aroundOuterAtt, indexInGroupBy,
					aggstate->eqfunctions, aggstate->ltfunctions,
					aggstate->minusfunctions,
					tmpcontext->ecxt_per_tuple_memory);
			if (!testMatch) {/*If current tuple does not belong to current pivots*/
				/*Finish processing of last chain if needed*/
				if (curChainConnectedWithCenter)
					enable_chain(aggstate);

				/*Update pivots. Enters here only if both pivots are not Null*/
				for (;;) {
					/*Advance Pivot 1 = Make pivot1Slot.tuple point to aggstate->grp_Pivot2HeapTuple*/
					ExecStoreTuple(aggstate->grp_Pivot2HeapTuple, pivot1Slot,
							InvalidBuffer, true );/*This also frees the previous content*/
					aggstate->grp_Pivot1HeapTuple =
							aggstate->grp_Pivot2HeapTuple;

					/*Get Pivot 2*/
					aggstate->grp_Pivot2HeapTuple = NULL;
					innerslot = ExecProcNode(innerPlan); /*GETS A TUPLE FROM THE RIGHT SUB-PLAN*/
					pivot2Slot->tts_shouldFree = false; /*To avoid pfree of new pivot1*/

					/*printf("Tuple de tabla around (Bucle):\n");
					 print_slot(innerslot);*/

					if (!TupIsNull(innerslot)) /*There is second Pivot*/
					{
						aggstate->grp_Pivot2HeapTuple = ExecCopySlotTuple(
								innerslot);/*make an independent copy*/
						/*make pivot2Slot point to aggstate->grp_Pivot2HeapTuple*/
						ExecStoreTuple(aggstate->grp_Pivot2HeapTuple,
								pivot2Slot, InvalidBuffer, true );

						/*Check that current tuple BELONGS to group represented by the Pivots*/
						if (execTuplesMatchAroundHash(pivot1Slot, pivot2Slot,
								outerslot, aroundOuterAtt, indexInGroupBy,
								aggstate->eqfunctions, aggstate->ltfunctions,
								aggstate->minusfunctions,
								tmpcontext->ecxt_per_tuple_memory))
							break;/*We found the right pivots and the PivotXHeapTuple attributes have the right value*/
					} else
						break; /*There is not more tuples for pivots. Second Pivot will be NULL*/
				}

				/*
				 * Process current tuple
				 */
				attrPivot1 = slot_getattr(pivot1Slot, attPivots, &isNull1);
				attrCurrent = slot_getattr(outerslot, aroundOuterAtt, &isNull3);
				/*BUILD MODIFIED VERSION OF THE TUPLE BEING PROCESSED USING CENTER VALUE. Center value is taken from Pivot 1*/
				replace[aroundOuterAtt - 1] = true;
				values[aroundOuterAtt - 1] = attrPivot1;
				/*Create an independent new HeapTuple with the modified version*/
				newHeapTuple = heap_modify_tuple(outerslot->tts_tuple,
						outerslot->tts_tupleDescriptor, values, nulls, replace);
				/* Initialize modifiedSlot by cloning outerslot */
				if (aggstate->modifiedSlot->tts_tupleDescriptor == NULL )
					ExecSetSlotDescriptor(aggstate->modifiedSlot,
							outerslot->tts_tupleDescriptor);
				/*Make aggstate->modifiedSlot.tuple point to new HeapTuple*/
				ExecStoreTuple(newHeapTuple, aggstate->modifiedSlot,
						InvalidBuffer, true );/*This will free memory of previous tuple too*/
				/*printf("Tuple de tabla base ORIGINAL:\n");print_slot(outerslot);
				 printf("Tuple de tabla base UPDATED:\n");print_slot(aggstate->modifiedSlot);*/
				/* Find or build hashtable entry for this tuple's group*/
				entry = lookup_hash_entry(aggstate, aggstate->modifiedSlot);/*THIS USES THE MODIFIED TUPLE*/
				/* Advance the aggregates */
				advance_aggregates(aggstate, entry->pergroup); /*THIS USES THE ORIGINAL TUPLE*/
				/* Reset per-input-tuple context after each tuple */
				ResetExprContext(tmpcontext);

				/*
				 * Update flags and varialbles
				 */
				basePositionDatum = attrCurrent;/*save value of processed element to use in next round*/
				entry->flagEnabled = 1;/*Being processed flag*/
				ignoreUntilNextGroup = false;
				/*Test if current element is connected with current center*/
				if (DatumGetBool( FunctionCall2(&ltfunctions[indexInGroupBy],attrCurrent,attrPivot1)) ) {/*P<P1*/
					/*(P1-P)>max-separation. Equivalent to max-separation<(P1-P)*/
					if (DatumGetBool( FunctionCall2(&ltfunctions[indexInGroupBy],maxSeparationDatum,
									FunctionCall2(&minusfunctions[indexInGroupBy],attrPivot1,attrCurrent))) )
						curItemConnectedWithCenter = false;
					else
						/*(P1-P)<=max-separation*/
						curItemConnectedWithCenter = true;
				} else {/*P1<=P*/
					/*(P-P1)>max-separation. Equivalent to max-separation<(P-P1)*/
					if (DatumGetBool( FunctionCall2(&ltfunctions[indexInGroupBy],maxSeparationDatum,
									FunctionCall2(&minusfunctions[indexInGroupBy],attrCurrent,attrPivot1))) )
						curItemConnectedWithCenter = false;
					else
						/*(P-P1)<=max-separation*/
						curItemConnectedWithCenter = true;
				}
				curChainConnectedWithCenter = curItemConnectedWithCenter;
			} else {/*Current tuple belongs to current pivot*/
				if (firstTuple) {/*1st Tuple*/
					/*
					 * Process current tuple
					 */
					attrPivot1 = slot_getattr(pivot1Slot, attPivots, &isNull1);
					attrCurrent = slot_getattr(outerslot, aroundOuterAtt,
							&isNull3);
					/*BUILD MODIFIED VERSION OF THE TUPLE BEING PROCESSED USING CENTER VALUE. Center value is taken from Pivot 1*/
					replace[aroundOuterAtt - 1] = true;
					values[aroundOuterAtt - 1] = attrPivot1;
					/*Create an independent new HeapTuple with the modified version*/
					newHeapTuple = heap_modify_tuple(outerslot->tts_tuple,
							outerslot->tts_tupleDescriptor, values, nulls,
							replace);
					/* Initialize modifiedSlot by cloning outerslot */
					if (aggstate->modifiedSlot->tts_tupleDescriptor == NULL )
						ExecSetSlotDescriptor(aggstate->modifiedSlot,
								outerslot->tts_tupleDescriptor);
					/*Make aggstate->modifiedSlot.tuple point to new HeapTuple*/
					ExecStoreTuple(newHeapTuple, aggstate->modifiedSlot,
							InvalidBuffer, true );/*This will free memory of previous tuple too*/
					/*printf("Tuple de tabla base ORIGINAL:\n");print_slot(outerslot);
					 printf("Tuple de tabla base UPDATED:\n");print_slot(aggstate->modifiedSlot);*/
					/* Find or build hashtable entry for this tuple's group*/
					entry = lookup_hash_entry(aggstate, aggstate->modifiedSlot);/*THIS USES THE MODIFIED TUPLE*/
					/* Advance the aggregates */
					advance_aggregates(aggstate, entry->pergroup); /*THIS USES THE ORIGINAL TUPLE*/
					/* Reset per-input-tuple context after each tuple */
					ResetExprContext(tmpcontext);

					/*
					 * Update flags and varialbles
					 */
					basePositionDatum = attrCurrent;/*save value of processed element to use in next round*/
					entry->flagEnabled = 1;/*Being processed flag*/
					/*Test if current element is connected with current center*/
					if (DatumGetBool( FunctionCall2(&ltfunctions[indexInGroupBy],attrCurrent,attrPivot1)) ) {/*P<P1*/
						/*(P1-P)>max-separation. Equivalent to max-separation<(P1-P)*/
						if (DatumGetBool( FunctionCall2(&ltfunctions[indexInGroupBy],maxSeparationDatum,
										FunctionCall2(&minusfunctions[indexInGroupBy],attrPivot1,attrCurrent))) )
							curItemConnectedWithCenter = false;
						else
							/*(P1-P)<=max-separation*/
							curItemConnectedWithCenter = true;
					} else {/*P1<=P*/
						/*(P-P1)>max-separation. Equivalent to max-separation<(P-P1)*/
						if (DatumGetBool( FunctionCall2(&ltfunctions[indexInGroupBy],maxSeparationDatum,
										FunctionCall2(&minusfunctions[indexInGroupBy],attrCurrent,attrPivot1))) )
							curItemConnectedWithCenter = false;
						else
							/*(P-P1)<=max-separation*/
							curItemConnectedWithCenter = true;
					}
					curChainConnectedWithCenter = curItemConnectedWithCenter;
					firstTuple = false;
				} else {/*Not first tuple*/
					if (!ignoreUntilNextGroup) {/*We should process current base tuple*/

						attrPivot1 = slot_getattr(pivot1Slot, attPivots,
								&isNull1);
						attrCurrent = slot_getattr(outerslot, aroundOuterAtt,
								&isNull3);

						/*Test if current element is connected with current center*/
						if (DatumGetBool( FunctionCall2(&ltfunctions[indexInGroupBy],attrCurrent,attrPivot1)) ) {/*P<P1*/
							/*(P1-P)>max-separation. Equivalent to max-separation<(P1-P)*/
							if (DatumGetBool( FunctionCall2(&ltfunctions[indexInGroupBy],maxSeparationDatum,
											FunctionCall2(&minusfunctions[indexInGroupBy],attrPivot1,attrCurrent))) )
								curItemConnectedWithCenter = false;
							else
								/*(P1-P)<=max-separation*/
								curItemConnectedWithCenter = true;
						} else {/*P1<=P*/
							/*(P-P1)>max-separation. Equivalent to max-separation<(P-P1)*/
							if (DatumGetBool( FunctionCall2(&ltfunctions[indexInGroupBy],maxSeparationDatum,
											FunctionCall2(&minusfunctions[indexInGroupBy],attrCurrent,attrPivot1))) )
								curItemConnectedWithCenter = false;
							else
								/*(P-P1)<=max-separation*/
								curItemConnectedWithCenter = true;
						}

						/*There is cut between 2 elements.If P-basepos > max-separation*/
						if (DatumGetBool(FunctionCall2(&ltfunctions[indexInGroupBy],maxSeparationDatum,
										FunctionCall2(&minusfunctions[indexInGroupBy],attrCurrent,basePositionDatum))) ) {
							if (curItemConnectedWithCenter) {/*current element is connected with center*/
								if (curChainConnectedWithCenter) {/*chain is connected. There is not real cut, elements are joined with pivot*/
									/*
									 * Process current tuple
									 */
									/*BUILD MODIFIED VERSION OF THE TUPLE BEING PROCESSED USING CENTER VALUE. Center value is taken from Pivot 1*/
									replace[aroundOuterAtt - 1] = true;
									values[aroundOuterAtt - 1] = attrPivot1;
									/*Create an independent new HeapTuple with the modified version*/
									newHeapTuple = heap_modify_tuple(
											outerslot->tts_tuple,
											outerslot->tts_tupleDescriptor,
											values, nulls, replace);
									/* Initialize modifiedSlot by cloning outerslot */
									if (aggstate->modifiedSlot->tts_tupleDescriptor
											== NULL )
										ExecSetSlotDescriptor(
												aggstate->modifiedSlot,
												outerslot->tts_tupleDescriptor);
									/*Make aggstate->modifiedSlot.tuple point to new HeapTuple*/
									ExecStoreTuple(newHeapTuple,
											aggstate->modifiedSlot,
											InvalidBuffer, true );/*This will free memory of previous tuple too*/
									/*printf("Tuple de tabla base ORIGINAL:\n");print_slot(outerslot);
									 printf("Tuple de tabla base UPDATED:\n");print_slot(aggstate->modifiedSlot);*/
									/* Find or build hashtable entry for this tuple's group*/
									entry = lookup_hash_entry(aggstate,
											aggstate->modifiedSlot);/*THIS USES THE MODIFIED TUPLE*/
									/* Advance the aggregates */
									advance_aggregates(aggstate,
											entry->pergroup); /*THIS USES THE ORIGINAL TUPLE*/
									/* Reset per-input-tuple context after each tuple */
									ResetExprContext(tmpcontext);

									/*
									 * Update flags and varialbles
									 */
									basePositionDatum = attrCurrent;/*save value of processed element to use in next round*/
									entry->flagEnabled = 1;/*Being processed flag*/
								} else {/*chain is NOT connected*/
									/*Reset chain*/
									reset_chain(aggstate);

									/*
									 * Process current tuple
									 */
									/*BUILD MODIFIED VERSION OF THE TUPLE BEING PROCESSED USING CENTER VALUE. Center value is taken from Pivot 1*/
									replace[aroundOuterAtt - 1] = true;
									values[aroundOuterAtt - 1] = attrPivot1;
									/*Create an independent new HeapTuple with the modified version*/
									newHeapTuple = heap_modify_tuple(
											outerslot->tts_tuple,
											outerslot->tts_tupleDescriptor,
											values, nulls, replace);
									/* Initialize modifiedSlot by cloning outerslot */
									if (aggstate->modifiedSlot->tts_tupleDescriptor
											== NULL )
										ExecSetSlotDescriptor(
												aggstate->modifiedSlot,
												outerslot->tts_tupleDescriptor);
									/*Make aggstate->modifiedSlot.tuple point to new HeapTuple*/
									ExecStoreTuple(newHeapTuple,
											aggstate->modifiedSlot,
											InvalidBuffer, true );/*This will free memory of previous tuple too*/
									/*printf("Tuple de tabla base ORIGINAL:\n");print_slot(outerslot);
									 printf("Tuple de tabla base UPDATED:\n");print_slot(aggstate->modifiedSlot);*/
									/* Find or build hashtable entry for this tuple's group*/
									entry = lookup_hash_entry(aggstate,
											aggstate->modifiedSlot);/*THIS USES THE MODIFIED TUPLE*/
									/* Advance the aggregates */
									advance_aggregates(aggstate,
											entry->pergroup); /*THIS USES THE ORIGINAL TUPLE*/
									/* Reset per-input-tuple context after each tuple */
									ResetExprContext(tmpcontext);

									/*
									 * Update flags and varialbles
									 */
									basePositionDatum = attrCurrent;/*save value of processed element to use in next round*/
									entry->flagEnabled = 1;/*Being processed flag*/
									curChainConnectedWithCenter = true;
								}
							} else {/*current element is NOT connected with center*/
								if (curChainConnectedWithCenter) {/*chain is connected*/
									/*Enable chain*/
									enable_chain(aggstate);
									/*do not process current base tuple*/

									/*
									 * Update flags and varialbles
									 */
									curChainConnectedWithCenter = false;
									ignoreUntilNextGroup = true;
								} else {/*chain is NOT connected*/
									/*Reset chain*/
									reset_chain(aggstate);
									/*do not process current base tuple*/
								}
							}
						} else {/*No cut between 2 elements*/
							/*
							 * Process current tuple
							 */
							/*BUILD MODIFIED VERSION OF THE TUPLE BEING PROCESSED USING CENTER VALUE. Center value is taken from Pivot 1*/
							replace[aroundOuterAtt - 1] = true;
							values[aroundOuterAtt - 1] = attrPivot1;
							/*Create an independent new HeapTuple with the modified version*/
							newHeapTuple = heap_modify_tuple(
									outerslot->tts_tuple,
									outerslot->tts_tupleDescriptor, values,
									nulls, replace);
							/* Initialize modifiedSlot by cloning outerslot */
							if (aggstate->modifiedSlot->tts_tupleDescriptor
									== NULL )
								ExecSetSlotDescriptor(aggstate->modifiedSlot,
										outerslot->tts_tupleDescriptor);
							/*Make aggstate->modifiedSlot.tuple point to new HeapTuple*/
							ExecStoreTuple(newHeapTuple, aggstate->modifiedSlot,
									InvalidBuffer, true );/*This will free memory of previous tuple too*/
							/*printf("Tuple de tabla base ORIGINAL:\n");print_slot(outerslot);
							 printf("Tuple de tabla base UPDATED:\n");print_slot(aggstate->modifiedSlot);*/
							/* Find or build hashtable entry for this tuple's group*/
							entry = lookup_hash_entry(aggstate,
									aggstate->modifiedSlot);/*THIS USES THE MODIFIED TUPLE*/
							/* Advance the aggregates */
							advance_aggregates(aggstate, entry->pergroup); /*THIS USES THE ORIGINAL TUPLE*/
							/* Reset per-input-tuple context after each tuple */
							ResetExprContext(tmpcontext);

							/*
							 * Update flags and varialbles
							 */
							basePositionDatum = attrCurrent;/*save value of processed element to use in next round*/
							entry->flagEnabled = 1;/*Being processed flag*/
							if (curItemConnectedWithCenter)/*Current tuple is connected with center*/
								curChainConnectedWithCenter = true;
						}
					}
				}
			}
		}
	}/*end of main for*/

	/*free space used by arrays*/
	pfree(values);
	pfree(nulls);
	pfree(replace);

	aggstate->table_filled = true;
	/* Initialize to walk the hash table */
	ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);
}

/*CHANGED BY YASIN*/
/*
 * ExecAgg for hashed case: phase 1, read input and build hash table
 * This function supports HASH SGB_U GROUPING WITH N COLS and any combination of max-elem-sep and max-group-diam
 */
static void agg_fill_hash_table_unsupervised(AggState *aggstate) {
	PlanState *outerPlan;
	ExprContext *tmpcontext;
	AggHashEntry entry;
	TupleTableSlot *outerslot;

	/*CHANGED BY YASIN*/
	Agg *node = (Agg *) aggstate->ss.ps.plan;
	AttrNumber SGBOuterAtt; /*Index of the around attribute in left plan. Starts at 1*/
	AttrNumber *SGBAttPtr;
	int indexInGroupBy; /*Index in group-by list (starts at 0) of col that uses around */
	int numAttributes; /*Num attributes in outer plan*/
	Datum *values;
	bool *nulls;
	bool *replace;
	int maxSeparationInt; /*integer value of this parameter*/
	int maxDiameterInt; /*integer value of this parameter*/
	Datum maxSeparationDatum; /*the datum version of maxSeparationInt*/
	Datum maxDiameterDatum; /*the datum version of maxDiameterInt*/
	Datum basePositionDatum; /*datum that contains the value of the last element already processed in the current group*/
	Datum firstGrpElementDatum; /*datum of the SGA of the first tuple of the current group*/

	FmgrInfo *ltfunctions;
	FmgrInfo *minusfunctions;
	Datum attrCurrent;
	bool isNull1, isNull3;
	bool firstTuple; /*True if element being analized is the first tuple of base table*/
	bool cutFound;

	maxSeparationInt = node->max_elem_sep; /*max_elem_sep comes from the SQL statement*/
	maxDiameterInt = node->max_group_diam; /*max_group_diam comes from the SQL statement*/
	maxSeparationDatum = Int32GetDatum(maxSeparationInt);
	maxDiameterDatum = Int32GetDatum(maxDiameterInt);

	SGBAttPtr = node->grpColIdx;
	indexInGroupBy = 0;/*We assume that col in group by that uses around is the first one*/
	SGBOuterAtt = SGBAttPtr[indexInGroupBy]; /*Indices in node->grpColIdx start at 1*/
	numAttributes = list_length(aggstate->ss.ps.plan->lefttree->targetlist);/*# attrs in outer plan*/
	firstTuple = true;
	basePositionDatum = 0;/*we will change this value when we process 1st tuple*/
	firstGrpElementDatum = 0;

	/*printf ("numAttributes in outer plan: %d \n", numAttributes);
	 printf ("Index of the around attribute in left plan: %d \n", aroundOuterAtt);
	 printf ("Index of the around attribute in right plan: %d \n", aroundInnerAtt);*/

	values = (Datum *) palloc(numAttributes * sizeof(Datum)); /*array*/
	nulls = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
	replace = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
	memset(values, 0, numAttributes * sizeof(Datum));
	memset(nulls, false, numAttributes * sizeof(bool));
	memset(replace, false, numAttributes * sizeof(bool));

	outerPlan = outerPlanState(aggstate);/*get state*/
	tmpcontext = aggstate->tmpcontext; /*tmpcontext is the per-input-tuple expression context */

	/*
	 * Process each outer-plan tuple, and then fetch the next one, until we
	 * exhaust the outer plan.
	 */
	ltfunctions = aggstate->ltfunctions;
	minusfunctions = aggstate->minusfunctions;
	for (;;) {
		HeapTuple newHeapTuple;/*CHANGED BY YASIN*/
		outerslot = ExecProcNode(outerPlan);/*outerslot POINTS TO CURRENT TUPLE*/

		if (TupIsNull(outerslot)) {
			break;
		} else {/*We have a base tuple to process*/

			/* set up for advance_aggregates call */
			tmpcontext->ecxt_scantuple = outerslot; /*advance_aggregates should used riginal tuple*/

			if (firstTuple) {/*1st Tuple*/
				attrCurrent = slot_getattr(outerslot, SGBOuterAtt, &isNull3);
				basePositionDatum = attrCurrent;/*save value of processed element to use in next round*/
				firstGrpElementDatum = attrCurrent; /*attrCurrent is also first element of current group*/
				/*
				 * Process current tuple
				 */
				/*BUILD MODIFIED VERSION OF THE TUPLE BEING PROCESSED USING CENTER VALUE. Center value is taken from Pivot 1*/
				replace[SGBOuterAtt - 1] = true;
				values[SGBOuterAtt - 1] = firstGrpElementDatum;
				/*Create an independent new HeapTuple with the modified version*/
				newHeapTuple = heap_modify_tuple(outerslot->tts_tuple,
						outerslot->tts_tupleDescriptor, values, nulls, replace);
				/* Initialize modifiedSlot by cloning outerslot */
				if (aggstate->modifiedSlot->tts_tupleDescriptor == NULL )
					ExecSetSlotDescriptor(aggstate->modifiedSlot,
							outerslot->tts_tupleDescriptor);
				/*Make aggstate->modifiedSlot.tuple point to new HeapTuple*/
				ExecStoreTuple(newHeapTuple, aggstate->modifiedSlot,
						InvalidBuffer, true );/*This will free memory of previous tuple too*/
				/*printf("Tuple de tabla base ORIGINAL:\n");print_slot(outerslot);printf("Tuple de tabla base UPDATED:\n");print_slot(aggstate->modifiedSlot);*/
				/* Find or build hashtable entry for this tuple's group*/
				entry = lookup_hash_entry(aggstate, aggstate->modifiedSlot);/*THIS USES THE MODIFIED TUPLE*/
				/* Advance the aggregates */
				advance_aggregates(aggstate, entry->pergroup); /*THIS USES THE ORIGINAL TUPLE*/
				/* Reset per-input-tuple context after each tuple */
				ResetExprContext(tmpcontext);

				firstTuple = false;
			} else { /*not 1st Tuple*/
				attrCurrent = slot_getattr(outerslot, SGBOuterAtt, &isNull3);
				if ((maxDiameterInt != -1) && (maxSeparationInt != -1)) {//max-group-diam and max-elem-sep
					cutFound =
							(DatumGetBool(FunctionCall2(&ltfunctions[indexInGroupBy],maxDiameterDatum,
											FunctionCall2(&minusfunctions[indexInGroupBy],attrCurrent,firstGrpElementDatum)))
									|| DatumGetBool(FunctionCall2(&ltfunctions[indexInGroupBy],maxSeparationDatum,
													FunctionCall2(&minusfunctions[indexInGroupBy],attrCurrent,basePositionDatum))) );
				} else if (maxDiameterInt != -1) {//max-group-diam (max-group-diam < P-firstOfGroup)
					cutFound =
							DatumGetBool(FunctionCall2(&ltfunctions[indexInGroupBy],maxDiameterDatum,
											FunctionCall2(&minusfunctions[indexInGroupBy],attrCurrent,firstGrpElementDatum)));
				} else {			//max-elem-sep (max-elem-sep < P-basepos)
					cutFound =
							DatumGetBool(FunctionCall2(&ltfunctions[indexInGroupBy],maxSeparationDatum,
											FunctionCall2(&minusfunctions[indexInGroupBy],attrCurrent,basePositionDatum)));
				}

				/*There is cut between 2 elements.*/
				if (cutFound) {
					basePositionDatum = attrCurrent;/*save value of processed element to use in next round*/
					firstGrpElementDatum = attrCurrent; /*this attrCurrent is also first element of (new) current group*/
					/*
					 * Process current tuple
					 */
					/*BUILD MODIFIED VERSION OF THE TUPLE BEING PROCESSED USING CENTER VALUE. Center value is taken from Pivot 1*/
					replace[SGBOuterAtt - 1] = true;
					values[SGBOuterAtt - 1] = firstGrpElementDatum;
					/*Create an independent new HeapTuple with the modified version*/
					newHeapTuple = heap_modify_tuple(outerslot->tts_tuple,
							outerslot->tts_tupleDescriptor, values, nulls,
							replace);
					/* Initialize modifiedSlot by cloning outerslot */
					if (aggstate->modifiedSlot->tts_tupleDescriptor == NULL )
						ExecSetSlotDescriptor(aggstate->modifiedSlot,
								outerslot->tts_tupleDescriptor);
					/*Make aggstate->modifiedSlot.tuple point to new HeapTuple*/
					ExecStoreTuple(newHeapTuple, aggstate->modifiedSlot,
							InvalidBuffer, true );/*This will free memory of previous tuple too*/
					/*printf("Tuple de tabla base ORIGINAL:\n");print_slot(outerslot);printf("Tuple de tabla base UPDATED:\n");print_slot(aggstate->modifiedSlot);*/
					/* Find or build hashtable entry for this tuple's group*/
					entry = lookup_hash_entry(aggstate, aggstate->modifiedSlot);/*THIS USES THE MODIFIED TUPLE*/
					/* Advance the aggregates */
					advance_aggregates(aggstate, entry->pergroup); /*THIS USES THE ORIGINAL TUPLE*/
					/* Reset per-input-tuple context after each tuple */
					ResetExprContext(tmpcontext);
				} else {/*No cut between 2 elements*/
					basePositionDatum = attrCurrent;/*save value of processed element to use in next round*/
					/*
					 * Process current tuple
					 */
					/*BUILD MODIFIED VERSION OF THE TUPLE BEING PROCESSED USING CENTER VALUE. Center value is taken from Pivot 1*/
					replace[SGBOuterAtt - 1] = true;
					values[SGBOuterAtt - 1] = firstGrpElementDatum;
					/*Create an independent new HeapTuple with the modified version*/
					newHeapTuple = heap_modify_tuple(outerslot->tts_tuple,
							outerslot->tts_tupleDescriptor, values, nulls,
							replace);
					/* Initialize modifiedSlot by cloning outerslot */
					if (aggstate->modifiedSlot->tts_tupleDescriptor == NULL )
						ExecSetSlotDescriptor(aggstate->modifiedSlot,
								outerslot->tts_tupleDescriptor);
					/*Make aggstate->modifiedSlot.tuple point to new HeapTuple*/
					ExecStoreTuple(newHeapTuple, aggstate->modifiedSlot,
							InvalidBuffer, true );/*This will free memory of previous tuple too*/
					/*printf("Tuple de tabla base ORIGINAL:\n");print_slot(outerslot);printf("Tuple de tabla base UPDATED:\n");print_slot(aggstate->modifiedSlot);*/
					/* Find or build hashtable entry for this tuple's group*/
					entry = lookup_hash_entry(aggstate, aggstate->modifiedSlot);/*THIS USES THE MODIFIED TUPLE*/
					/* Advance the aggregates */
					advance_aggregates(aggstate, entry->pergroup); /*THIS USES THE ORIGINAL TUPLE*/
					/* Reset per-input-tuple context after each tuple */
					ResetExprContext(tmpcontext);
				}
			}
		}
	}/*end of main for*/

	/*free space used by arrays*/
	pfree(values);
	pfree(nulls);
	pfree(replace);

	aggstate->table_filled = true;
	/* Initialize to walk the hash table */
	ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);
}

/*CHANGED BY YASIN*/
/*
 * ExecAgg for hashed case: phase 1, read input and build hash table
 * This function supports HASH GROUPING WITH N COLS and max-radius
 * It does not support max-separation
 */
static void agg_fill_hash_table_around_MaxR(AggState *aggstate) {
	PlanState *outerPlan;
	ExprContext *tmpcontext;
	AggHashEntry entry;
	TupleTableSlot *outerslot;

	/*CHANGED BY YASIN*/
	Agg *node = (Agg *) aggstate->ss.ps.plan;
	AttrNumber aroundOuterAtt; /*Index of the around attribute in left plan. Starts at 1*/
	AttrNumber aroundInnerAtt; /*Index of the around attribute in right plan. Starts at 1*/
	AttrNumber *aroundAttPtr;
	int indexInGroupBy; /*Index in group-by list (starts at 0) of col that uses around */
	bool dummyIsNull;
	int numAttributes; /*Num attributes in outer plan*/
	Datum centerDatum;
	Datum *values;
	bool *nulls;
	bool *replace;
	PlanState *innerPlan;
	TupleTableSlot *innerslot; /*pointer to inner slot*/
	TupleTableSlot *pivot1Slot; /*pointer to pivot1 slot*/
	TupleTableSlot *pivot2Slot; /*pointer to pivot2 slot*/
	int maxRadiusInt; /*integer value of this parameter*/
	Datum maxRadiusDatum; /*the datum version of maxRadiusInt*/
	bool ignore;/*Indicates if the current tuple of base tuple should be ignored(does not belong to any group)*/
	bool stop;/*True when current tuple and following ones should not be processed (do not belong to any group)*/
	bool resultMatch;

	maxRadiusInt = (node->max_group_diam) / 2; /*max_group_diam was obtained from SQL statement*/
	maxRadiusDatum = Int32GetDatum(maxRadiusInt);
	aroundAttPtr = node->grpColIdx;
	indexInGroupBy = 0;/*We assume that col in group by that uses around is the first one*/
	aroundOuterAtt = aroundAttPtr[indexInGroupBy]; /*Indices in node->grpColIdx start at 1*/
	aroundInnerAtt = 1; /*we use the first col in the right plan as the around tuples*/
	numAttributes = list_length(aggstate->ss.ps.plan->lefttree->targetlist);/*# attrs in outer plan*/

	/*printf ("numAttributes in outer plan: %d \n", numAttributes);
	 printf ("Index of the around attribute in left plan: %d \n", aroundOuterAtt);
	 printf ("Index of the around attribute in right plan: %d \n", aroundInnerAtt);*/

	values = (Datum *) palloc(numAttributes * sizeof(Datum)); /*array*/
	nulls = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
	replace = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
	memset(values, 0, numAttributes * sizeof(Datum));
	memset(nulls, false, numAttributes * sizeof(bool));
	memset(replace, false, numAttributes * sizeof(bool));

	outerPlan = outerPlanState(aggstate);/*get state*/
	innerPlan = innerPlanState(aggstate);/*CHANGED BY YASIN*//*Inse we are procesing around we expect a non-null value*/
	tmpcontext = aggstate->tmpcontext;/* tmpcontext is the per-input-tuple expression context */

	pivot1Slot = aggstate->pivot1Slot; /*CHANGED BY YASIN*/
	pivot2Slot = aggstate->pivot2Slot; /*CHANGED BY YASIN*/

	/*Loads Pivot1 and Pivot2*//*CHANGED BY YASIN*/
	innerslot = ExecProcNode(innerPlan); /*GETS A TUPLE FROM THE RIGHT SUB-PLAN*/
	/*printf("Tuple de tabla around (A):\n");
	 print_slot(innerslot);*/

	if (!TupIsNull(innerslot)) {
		/* Initialize pivot slots by cloning input slot (innerslot) */
		if (pivot1Slot->tts_tupleDescriptor == NULL ) {/*if true pivot2Slot->tts_tupleDescriptor is expected to be also NULL*/
			ExecSetSlotDescriptor(pivot1Slot, innerslot->tts_tupleDescriptor);
			ExecSetSlotDescriptor(pivot2Slot, innerslot->tts_tupleDescriptor);
		}

		/*Pivot 1*/
		aggstate->grp_Pivot1HeapTuple = ExecCopySlotTuple(innerslot); /*make an independent copy*/
		/*make pivot1Slot.heap point to aggstate->grp_Pivot1HeapTuple*/
		ExecStoreTuple(aggstate->grp_Pivot1HeapTuple, pivot1Slot, InvalidBuffer,
				true );

		/*Pivot 2*/
		innerslot = ExecProcNode(innerPlan); /*GETS A TUPLE FROM THE RIGHT SUB-PLAN*/
		/*CHANGED BY YASIN ONLY FOR DEBUG*/
		/*printf("Tuple de tabla around (B):\n");
		 print_slot(innerslot);*/

		if (!TupIsNull(innerslot)) {
			aggstate->grp_Pivot2HeapTuple = ExecCopySlotTuple(innerslot);/*make an independent copy*/
			/*make pivot2Slot.heap point to aggstate->grp_Pivot2HeapTuple*/
			ExecStoreTuple(aggstate->grp_Pivot2HeapTuple, pivot2Slot,
					InvalidBuffer, true );
		}/*Obs: if Pivot2 does not exist, grp_Pivot2HeapTuple is NULL. pivot2Slot is always not null*/
	} else /*Zero tuples in right subplan*/
	{ /*Finalize*/
		pfree(values);/*free space used by arrays*/
		pfree(nulls);
		pfree(replace);
		aggstate->table_filled = true;
		/* Initialize to walk the hash table */
		ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);
		return;
	}

	/*Process each outer-plan tuple, and then fetch the next one, until we exhaust the outer plan.*/
	for (;;) {
		HeapTuple newHeapTuple;/*CHANGED BY YASIN*/
		outerslot = ExecProcNode(outerPlan);/*outerslot POINTS TO CURRENT leftchild TUPLE*/
		if (TupIsNull(outerslot))
			break;
		/* set up for advance_aggregates call */
		tmpcontext->ecxt_scantuple = outerslot; /*advance_aggregates should used riginal tuple*/

		/*CHECK IF PIVOTS ARE CORRECT (contain current outer tuple)*/
		/*printf ("Esto se ve desde main function for phase1\n");
		 print_slot(pivot1Slot);	print_slot(pivot2Slot); print_slot(outerslot);*/

		/*execTuplesMatchAroundHash_MaxR returns true if cur tuple belongs to current group*/
		if (!execTuplesMatchAroundHash_MaxR(pivot1Slot, pivot2Slot, /*Pivot slots*/
		outerslot, aroundOuterAtt, indexInGroupBy, aggstate->eqfunctions,
				aggstate->ltfunctions, aggstate->minusfunctions,
				tmpcontext->ecxt_per_tuple_memory, maxRadiusDatum, &ignore,
				&stop)) {/*Obs: enters here with 1 or 2 pivots*/

			if ((pivot2Slot->tts_shouldFree) && (!ignore))/*there are 2 pivots and ignore=false*/
				/*We FIND the correct Pivots that contain current outer tuple*/
				for (;;) /*FOR finishes when (1)[P1,P2] or [P1] contains current tuple T or(2) when T should be ignored and is before segment of T or (3)when stop=T*/
				{
					/*Advance Pivot 1 = Make pivot1Slot.tuple point to aggstate->grp_Pivot2HeapTuple*/
					ExecStoreTuple(aggstate->grp_Pivot2HeapTuple, pivot1Slot,
							InvalidBuffer, true );/*This also frees the previous content*/
					aggstate->grp_Pivot1HeapTuple =
							aggstate->grp_Pivot2HeapTuple;

					/*Get Pivot 2*/
					aggstate->grp_Pivot2HeapTuple = NULL;
					innerslot = ExecProcNode(innerPlan); /*GETS A TUPLE FROM THE RIGHT SUB-PLAN*/
					pivot2Slot->tts_shouldFree = false; /*To avoid pfree of new pivot1*/
					/*printf("Tuple de tabla around (Bucle):\n");
					 print_slot(innerslot);*/
					if (!TupIsNull(innerslot)) /*There is second Pivot*/
					{
						aggstate->grp_Pivot2HeapTuple = ExecCopySlotTuple(
								innerslot);/*make an independent copy*/
						/*make pivot2Slot point to aggstate->grp_Pivot2HeapTuple*/
						ExecStoreTuple(aggstate->grp_Pivot2HeapTuple,
								pivot2Slot, InvalidBuffer, true );
					}

					/*Check that current tuple BELONGS to group represented by the Pivots*/
					resultMatch = execTuplesMatchAroundHash_MaxR(pivot1Slot,
							pivot2Slot, /*Pivot slots*/
							outerslot, aroundOuterAtt, indexInGroupBy,
							aggstate->eqfunctions, aggstate->ltfunctions,
							aggstate->minusfunctions,
							tmpcontext->ecxt_per_tuple_memory, maxRadiusDatum,
							&ignore, &stop);
					if (stop || ((pivot2Slot->tts_shouldFree == false )))
						break;/*if stop or there is only 1 pivot, we finish*/
					/*There are 2 pivots*/
					if (((!resultMatch) && (ignore))
							|| ((resultMatch) && (!ignore)))
						break;
					/*else if if resultMatch==true && ignore=false we continue in the for loop*/
				}/*for*/
		}
		if (stop)
			break; /*we finish outter for*/
		if (!ignore) {/*only if current tuple belongs to current group (determined by current pivots)*/

			/*BUILD MODIFIED VERSION OF THE TUPLE BEING PROCESSED USING CENTER VALUE. Center value is taken from Pivot 1*/
			centerDatum = slot_getattr(pivot1Slot, aroundInnerAtt,
					&dummyIsNull);/*dummyIsNull should be always false*/
			replace[aroundOuterAtt - 1] = true;
			values[aroundOuterAtt - 1] = centerDatum;

			/*Create an independent new HeapTuple with the modified version*/
			newHeapTuple = heap_modify_tuple(outerslot->tts_tuple,
					outerslot->tts_tupleDescriptor, values, nulls, replace);
			/* Initialize modifiedSlot by cloning outerslot */
			if (aggstate->modifiedSlot->tts_tupleDescriptor == NULL )
				ExecSetSlotDescriptor(aggstate->modifiedSlot,
						outerslot->tts_tupleDescriptor);
			/*Make aggstate->modifiedSlot.tuple point to new HeapTuple*/
			ExecStoreTuple(newHeapTuple, aggstate->modifiedSlot, InvalidBuffer,
					true );/*This will free memory of previous tuple too*/
			/*printf("Tuple de tabla base ORIGINAL:\n");print_slot(outerslot);
			 printf("Tuple de tabla base MODIFIED:\n");print_slot(aggstate->modifiedSlot);*/

			/* Find or build hashtable entry for this tuple's group*/
			entry = lookup_hash_entry(aggstate, aggstate->modifiedSlot);/*THIS USES THE MODIFIED TUPLE*/

			/* Advance the aggregates */
			advance_aggregates(aggstate, entry->pergroup); /*THIS USES THE ORIGINAL TUPLE*/

			/* Reset per-input-tuple context after each tuple */
			ResetExprContext(tmpcontext);
		}
	}

	/*free space used by arrays*/
	pfree(values);
	pfree(nulls);
	pfree(replace);

	aggstate->table_filled = true;
	/* Initialize to walk the hash table */
	ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);
}


/*
 * ExecAgg for hashed case: phase 2, retrieving groups from hash table
 */
static TupleTableSlot *
agg_retrieve_hash_table(AggState *aggstate) {
	ExprContext *econtext;
	ProjectionInfo *projInfo;
	Datum *aggvalues;
	bool *aggnulls;
	AggStatePerAgg peragg;
	AggStatePerGroup pergroup;
	AggHashEntry entry;
	TupleTableSlot *firstSlot;
	int aggno;
	Agg *node = (Agg *) aggstate->ss.ps.plan;

	/*
	 * get state info from node
	 */
	/* econtext is the per-output-tuple expression context */
	econtext = aggstate->ss.ps.ps_ExprContext;
	aggvalues = econtext->ecxt_aggvalues;
	aggnulls = econtext->ecxt_aggnulls;
	projInfo = aggstate->ss.ps.ps_ProjInfo;
	peragg = aggstate->peragg;
	firstSlot = aggstate->ss.ss_ScanTupleSlot;

	/*
	 * We loop retrieving groups until we find one satisfying
	 * aggstate->ss.ps.qual
	 */
	while (!aggstate->agg_done) {
		/*
		 * Find the next entry in the hash table
		 */
		entry = (AggHashEntry) ScanTupleHashTable(&aggstate->hashiter);
		if (entry == NULL ) {
			/* No more entries in hashtable, so done */
			aggstate->agg_done = TRUE;
			return NULL ;
		}

		/*modifed by Mingjie*/
		if (entry->flagEnabled == 0 && node->overlap == 2) {
			//entry->flagEnabled=0;
			continue;
		}

		/*
		 * Clear the per-output-tuple context for each group
		 */
		ResetExprContext(econtext);

		/*
		 * Store the copied first input tuple in the tuple table slot reserved
		 * for it, so that it can be used in ExecProject.
		 */
		ExecStoreMinimalTuple(entry->shared.firstTuple, firstSlot, false );

		pergroup = entry->pergroup;

		/*
		 * Finalize each aggregate calculation, and stash results in the
		 * per-output-tuple context.
		 */
		for (aggno = 0; aggno < aggstate->numaggs; aggno++) {
			AggStatePerAgg peraggstate = &peragg[aggno];
			AggStatePerGroup pergroupstate = &pergroup[aggno];

			Assert(!peraggstate->aggref->aggdistinct);
			finalize_aggregate(aggstate, peraggstate, pergroupstate,
					&aggvalues[aggno], &aggnulls[aggno]);
		}

		/*
		 * Use the representative input tuple for any references to
		 * non-aggregated input columns in the qual and tlist.
		 */
		econtext->ecxt_scantuple = firstSlot;

		/*
		 * Check the qual (HAVING clause); if the group does not match, ignore
		 * it and loop back to try to process another group.
		 */
		if (ExecQual(aggstate->ss.ps.qual, econtext, false )) {
			/*
			 * Form and return a projection tuple using the aggregate results
			 * and the representative input tuple.	Note we do not support
			 * aggregates returning sets ...
			 */
			return ExecProject(projInfo, NULL );
		}
	}

	/* No more groups */
	return NULL ;
}

/*
 * ExecAgg for hashed case: phase 2, retrieving groups from hash table
 * This version supports maximun-separation. It will only consider the entries with flagEnabled=2
 */
static TupleTableSlot *
agg_retrieve_hash_table_around_MaxS(AggState *aggstate) {
	ExprContext *econtext;
	ProjectionInfo *projInfo;
	Datum *aggvalues;
	bool *aggnulls;
	AggStatePerAgg peragg;
	AggStatePerGroup pergroup;
	AggHashEntry entry;
	TupleTableSlot *firstSlot;
	int aggno;

	/*
	 * get state info from node
	 */
	/* econtext is the per-output-tuple expression context */
	econtext = aggstate->ss.ps.ps_ExprContext;
	aggvalues = econtext->ecxt_aggvalues;
	aggnulls = econtext->ecxt_aggnulls;
	projInfo = aggstate->ss.ps.ps_ProjInfo;
	peragg = aggstate->peragg;
	firstSlot = aggstate->ss.ss_ScanTupleSlot;

	/*
	 * We loop retrieving groups until we find one satisfying
	 * aggstate->ss.ps.qual
	 */
	while (!aggstate->agg_done) {
		/*
		 * Find the next entry in the hash table
		 */
		entry = (AggHashEntry) ScanTupleHashTable(&aggstate->hashiter);
		if (entry == NULL ) {
			/* No more entries in hashtable, so done */
			aggstate->agg_done = TRUE;
			return NULL ;
		}

		/*CHANGED BY YASIN*/
		if (entry->flagEnabled != 2) /*We skip non-enabled entries*/
			continue;

		/*
		 * Clear the per-output-tuple context for each group
		 */
		ResetExprContext(econtext);

		/*
		 * Store the copied first input tuple in the tuple table slot reserved
		 * for it, so that it can be used in ExecProject.
		 */
		ExecStoreMinimalTuple(entry->shared.firstTuple, firstSlot, false );

		pergroup = entry->pergroup;

		/*
		 * Finalize each aggregate calculation, and stash results in the
		 * per-output-tuple context.
		 */
		for (aggno = 0; aggno < aggstate->numaggs; aggno++) {
			AggStatePerAgg peraggstate = &peragg[aggno];
			AggStatePerGroup pergroupstate = &pergroup[aggno];

			Assert(!peraggstate->aggref->aggdistinct);
			finalize_aggregate(aggstate, peraggstate, pergroupstate,
					&aggvalues[aggno], &aggnulls[aggno]);
		}

		/*
		 * Use the representative input tuple for any references to
		 * non-aggregated input columns in the qual and tlist.
		 */
		econtext->ecxt_scantuple = firstSlot;

		/*
		 * Check the qual (HAVING clause); if the group does not match, ignore
		 * it and loop back to try to process another group.
		 */
		if (ExecQual(aggstate->ss.ps.qual, econtext, false )) {
			/*
			 * Form and return a projection tuple using the aggregate results
			 * and the representative input tuple.	Note we do not support
			 * aggregates returning sets ...
			 */
			return ExecProject(projInfo, NULL );
		}
	}

	/* No more groups */
	return NULL ;
}

/*CHANGED BY YASIN*/
static void enable_chain(AggState *aggstate) {
	AggHashEntry entry;
	bool finishProcessing;
	finishProcessing = false;

	/* Initialize to walk the hash table */
	ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);

	while (!finishProcessing) {
		/*Find the next entry in the hash table*/
		entry = (AggHashEntry) ScanTupleHashTable(&aggstate->hashiter);
		if (entry == NULL )/* No more entries in hashtable, so done */
			finishProcessing = true;
		else if (entry->flagEnabled == 1)
			entry->flagEnabled = 2;
	}
}

/*CHANGED BY YASIN*/
static void reset_chain(AggState *aggstate) {
	AggHashEntry entry;
	bool finishProcessing;
	finishProcessing = false;

	/* Initialize to walk the hash table */
	ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);

	while (!finishProcessing) {
		/*Find the next entry in the hash table*/
		entry = (AggHashEntry) ScanTupleHashTable(&aggstate->hashiter);
		if (entry == NULL )/* No more entries in hashtable, so done */
			finishProcessing = true;
		else if (entry->flagEnabled == 1) {
			initialize_aggregates(aggstate, aggstate->peragg, entry->pergroup);/*re-start counters*/
			entry->flagEnabled = 0;
		}
	}
}

/* -----------------
 * ExecInitAgg
 *
 *	Creates the run-time information for the agg node produced by the
 *	planner and initializes its outer subtree
 * -----------------
 */
AggState *
ExecInitAgg(Agg *node, EState *estate, int eflags) {


	AggState   *aggstate;
	AggStatePerAgg peragg;
	Plan	   *outerPlan;
	ExprContext *econtext;
	int			numaggs,
				aggno;
	ListCell   *l;

	/*CHANGED BY YASIN*/
	Plan	   *innerPlan;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	/*
	 * create state structure
	 */
	aggstate = makeNode(AggState);
	aggstate->ss.ps.plan = (Plan *) node;
	aggstate->ss.ps.state = estate;

	aggstate->aggs = NIL;
	aggstate->numaggs = 0;
	aggstate->eqfunctions = NULL;
	aggstate->hashfunctions = NULL;
	aggstate->peragg = NULL;
	aggstate->agg_done = false;
	aggstate->pergroup = NULL;
	aggstate->grp_firstTuple = NULL;
	aggstate->hashtable = NULL;
	aggstate->grp_Pivot1HeapTuple = NULL; /*CHANGED BY YASIN*/
	aggstate->grp_Pivot2HeapTuple = NULL; /*CHANGED BY YASIN*/
	aggstate->ltfunctions = NULL; /*CHANGED BY YASIN*/
	aggstate->minusfunctions = NULL; /*CHANGED BY YASIN*/
	aggstate->indexhasharray=0;
	/*CHANGED BY YASIN*/
	if (eflags & EXEC_FLAG_INAROUND) aggstate->ss.ps.ps_InAround = true;


	/*
	 * Create expression contexts.	We need two, one for per-input-tuple
	 * processing and one for per-output-tuple processing.	We cheat a little
	 * by using ExecAssignExprContext() to build both.
	 */
	ExecAssignExprContext(estate, &aggstate->ss.ps);
	aggstate->tmpcontext = aggstate->ss.ps.ps_ExprContext;
	ExecAssignExprContext(estate, &aggstate->ss.ps);

	//ini random number
	srand((int)time(0)); /*Changed by Mingjie*/

	/*
	 * We also need a long-lived memory context for holding hashtable data
	 * structures and transition values.  NOTE: the details of what is stored
	 * in aggcontext and what is stored in the regular per-query memory
	 * context are driven by a simple decision: we want to reset the
	 * aggcontext in ExecReScanAgg to recover no-longer-wanted space.
	 */
	aggstate->aggcontext =
		AllocSetContextCreate(CurrentMemoryContext,
							  "AggContext",
							  ALLOCSET_DEFAULT_MINSIZE,
							  ALLOCSET_DEFAULT_INITSIZE,
							  ALLOCSET_DEFAULT_MAXSIZE);

/*CHANGED BY YASIN*/
/*Initially was 3. 4 and 5 are used for pivots, 6 is used for updated tuple while hash grouping*/
#define AGG_NSLOTS 6

	/*
	 * tuple table initialization
	 */
	ExecInitScanTupleSlot(estate, &aggstate->ss);
	ExecInitResultTupleSlot(estate, &aggstate->ss.ps);
	aggstate->hashslot = ExecInitExtraTupleSlot(estate);

	aggstate->pivot1Slot = ExecInitExtraTupleSlot(estate);/*CHANGED BY YASIN*/
	aggstate->pivot2Slot = ExecInitExtraTupleSlot(estate);/*CHANGED BY YASIN*/
	aggstate->modifiedSlot = ExecInitExtraTupleSlot(estate);/*CHANGED BY YASIN*/

	if(node->distanceall!=0)
	{
	aggstate->rtreeroot=RTreeCreate(MySearchCallback);

	aggstate->hasharray=palloc(sizeof(AggHashTupleM)*sizeofgroup); /*changed by Mingjie*/

	memset(aggstate->hasharray,NULL,sizeof(AggHashTupleM)*sizeofgroup);

	 initilListBegin();
	}

	//if this overlap semantics is duplicate changed my Mingjie
	if(node->overlap==2)
	{
		aggstate->overlaptuplestorestate1= (void *)tuplestore_begin_heap(true, false, work_mem);
		aggstate->overlaptuplestorestate2= (void *)tuplestore_begin_heap(true, false, work_mem);
		aggstate->numbertuples=0;
	}

	//this is for the RTREE and Disjoint set
	if(node->distanceany!=0)
	{
		aggstate->rtreeroot=RTreeCreate(MySearchCallback);

		aggstate->dsjset=palloc(sizeof(DisJointSets));
		aggstate->dsjset->capacity = 3000000;
		aggstate->dsjset->num_elements = 0;
		aggstate->dsjset->num_sets = 0;
		aggstate->dsjset->m_nodes = palloc(sizeof(element) * aggstate->dsjset->capacity);
		aggstate->indexhasharray=0;
		initilListBegin();
	}

	/*
	 * initialize child expressions
	 *
	 * Note: ExecInitExpr finds Aggrefs for us, and also checks that no aggs
	 * contain other agg calls in their arguments.	This would make no sense
	 * under SQL semantics anyway (and it's forbidden by the spec). Because
	 * that is true, we don't need to worry about evaluating the aggs in any
	 * particular order.
	 */
	aggstate->ss.ps.targetlist = (List *)
		ExecInitExpr((Expr *) node->plan.targetlist,
					 (PlanState *) aggstate);
	aggstate->ss.ps.qual = (List *)
		ExecInitExpr((Expr *) node->plan.qual,
					 (PlanState *) aggstate);

	/*
	 * initialize child nodes
	 *
	 * If we are doing a hashed aggregation then the child plan does not need
	 * to handle REWIND efficiently; see ExecReScanAgg. CHECK THIS FOR OUR CASE WHEN WE SUPPORT HASHING GROUPING
	 */
	if (node->aggstrategy == AGG_HASHED)
		eflags &= ~EXEC_FLAG_REWIND;

	outerPlan = outerPlan(node);
	outerPlanState(aggstate) = ExecInitNode(outerPlan, estate, eflags);

	/*CHANGED BY YASIN*/
	innerPlan = innerPlan(node);
	if (innerPlan != NULL){/*there is inner plan if we are using SGB_AROUND*/
		innerPlanState(aggstate) = ExecInitNode(innerPlan, estate, (eflags | EXEC_FLAG_INAROUND));
	}
	else
		innerPlanState(aggstate) = NULL;


	/*
	 * initialize source tuple type.
	 */
	ExecAssignScanTypeFromOuterPlan(&aggstate->ss);

	/*
	 * Initialize result tuple type and projection info.
	 */
	ExecAssignResultTypeFromTL(&aggstate->ss.ps);
	ExecAssignProjectionInfo(&aggstate->ss.ps, NULL);

	/*
	 * get the count of aggregates in targetlist and quals
	 */
	numaggs = aggstate->numaggs;
	Assert(numaggs == list_length(aggstate->aggs));
	if (numaggs <= 0)
	{
		/*
		 * This is not an error condition: we might be using the Agg node just
		 * to do hash-based grouping.  Even in the regular case,
		 * constant-expression simplification could optimize away all of the
		 * Aggrefs in the targetlist and qual.	So keep going, but force local
		 * copy of numaggs positive so that palloc()s below don't choke.
		 */
		numaggs = 1;
	}

	/*
	 * If we are grouping, precompute fmgr lookup data for inner loop. We need
	 * both equality and hashing functions to do it by hashing, but only
	 * equality if not hashing.
	 */
	if (node->numCols > 0)
	{
		if (node->aggstrategy == AGG_HASHED){/*HASH BASED*/
			/*CHANGED BY YASIN*/
			if ((innerPlan == NULL) && (node->max_group_diam == -1) && (node->max_elem_sep == -1))/*Regular Group. This is the original code*/
				execTuplesHashPrepare(ExecGetScanType(&aggstate->ss),
									  node->numCols,
									  node->grpColIdx,
									  &aggstate->eqfunctions,
									  &aggstate->hashfunctions);
			else /*we are using SGB*/
				execTuplesHashPrepareSGB(ExecGetScanType(&aggstate->ss),
									  node->numCols,
									  node->grpColIdx,
									  &aggstate->eqfunctions,
									  &aggstate->hashfunctions,
									  &aggstate->ltfunctions,
									  &aggstate->minusfunctions);
		}
		else{/*SORT BASED. Supports only SGB_Around*/
			/*CHANGED BY YASIN*/
			if (innerPlan == NULL)/*Regular Group. This is the original code*/
				aggstate->eqfunctions =/*This gets equality functions*/
					execTuplesMatchPrepare(ExecGetScanType(&aggstate->ss),
									   node->numCols,
									   node->grpColIdx);
			else /*we are using SGB_AROUND*/
				execTuplesMatchAroundPrepare(ExecGetScanType(&aggstate->ss),/*This gets '<' and '-' functions*/
									  node->numCols,
									  node->grpColIdx,
									  &aggstate->eqfunctions,
									  &aggstate->ltfunctions,
									  &aggstate->minusfunctions);

		}
	}

	/*
	 * Set up aggregate-result storage in the output expr context, and also
	 * allocate my private per-agg working storage
	 */
	econtext = aggstate->ss.ps.ps_ExprContext;
	econtext->ecxt_aggvalues = (Datum *) palloc0(sizeof(Datum) * numaggs);
	econtext->ecxt_aggnulls = (bool *) palloc0(sizeof(bool) * numaggs);

	peragg = (AggStatePerAgg) palloc0(sizeof(AggStatePerAggData) * numaggs);
	aggstate->peragg = peragg;

	if (node->aggstrategy == AGG_HASHED)
	{
		build_hash_table(aggstate);
		aggstate->table_filled = false;
	}
	else
	{
		AggStatePerGroup pergroup;

		pergroup = (AggStatePerGroup) palloc0(sizeof(AggStatePerGroupData) * numaggs);
		aggstate->pergroup = pergroup;
	}

	/*
	 * Perform lookups of aggregate function info, and initialize the
	 * unchanging fields of the per-agg data.  We also detect duplicate
	 * aggregates (for example, "SELECT sum(x) ... HAVING sum(x) > 0"). When
	 * duplicates are detected, we only make an AggStatePerAgg struct for the
	 * first one.  The clones are simply pointed at the same result entry by
	 * giving them duplicate aggno values.
	 */
	aggno = -1;
	foreach(l, aggstate->aggs)
	{
		AggrefExprState *aggrefstate = (AggrefExprState *) lfirst(l);
		Aggref	   *aggref = (Aggref *) aggrefstate->xprstate.expr;
		AggStatePerAgg peraggstate;
		Oid			inputTypes[FUNC_MAX_ARGS];
		int			numArguments;
		HeapTuple	aggTuple;
		Form_pg_aggregate aggform;
		Oid			aggtranstype;
		AclResult	aclresult;
		Oid			transfn_oid,
					finalfn_oid;
		Expr	   *transfnexpr,
				   *finalfnexpr;
		Datum		textInitVal;
		int			i;
		ListCell   *lc;

		/* Planner should have assigned aggregate to correct level */
		Assert(aggref->agglevelsup == 0);

		/* Look for a previous duplicate aggregate */
		for (i = 0; i <= aggno; i++)
		{
			if (equal(aggref, peragg[i].aggref) &&
				!contain_volatile_functions((Node *) aggref))
				break;
		}
		if (i <= aggno)
		{
			/* Found a match to an existing entry, so just mark it */
			aggrefstate->aggno = i;
			continue;
		}

		/* Nope, so assign a new PerAgg record */
		peraggstate = &peragg[++aggno];

		/* Mark Aggref state node with assigned index in the result array */
		aggrefstate->aggno = aggno;

		/* Fill in the peraggstate data */
		peraggstate->aggrefstate = aggrefstate;
		peraggstate->aggref = aggref;
		numArguments = list_length(aggref->args);
		peraggstate->numArguments = numArguments;

		/*
		 * Get actual datatypes of the inputs.	These could be different from
		 * the agg's declared input types, when the agg accepts ANY, ANYARRAY
		 * or ANYELEMENT.
		 */
		i = 0;
		foreach(lc, aggref->args)
		{
			inputTypes[i++] = exprType((Node *) lfirst(lc));
		}

		aggTuple = SearchSysCache(AGGFNOID,
								  ObjectIdGetDatum(aggref->aggfnoid),
								  0, 0, 0);
		if (!HeapTupleIsValid(aggTuple))
			elog(ERROR, "cache lookup failed for aggregate %u",
				 aggref->aggfnoid);
		aggform = (Form_pg_aggregate) GETSTRUCT(aggTuple);

		/* Check permission to call aggregate function */
		aclresult = pg_proc_aclcheck(aggref->aggfnoid, GetUserId(),
									 ACL_EXECUTE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_PROC,
						   get_func_name(aggref->aggfnoid));

		peraggstate->transfn_oid = transfn_oid = aggform->aggtransfn;
		peraggstate->finalfn_oid = finalfn_oid = aggform->aggfinalfn;

		/* Check that aggregate owner has permission to call component fns */
		{
			HeapTuple	procTuple;
			Oid			aggOwner;

			procTuple = SearchSysCache(PROCOID,
									   ObjectIdGetDatum(aggref->aggfnoid),
									   0, 0, 0);
			if (!HeapTupleIsValid(procTuple))
				elog(ERROR, "cache lookup failed for function %u",
					 aggref->aggfnoid);
			aggOwner = ((Form_pg_proc) GETSTRUCT(procTuple))->proowner;
			ReleaseSysCache(procTuple);

			aclresult = pg_proc_aclcheck(transfn_oid, aggOwner,
										 ACL_EXECUTE);
			if (aclresult != ACLCHECK_OK)
				aclcheck_error(aclresult, ACL_KIND_PROC,
							   get_func_name(transfn_oid));
			if (OidIsValid(finalfn_oid))
			{
				aclresult = pg_proc_aclcheck(finalfn_oid, aggOwner,
											 ACL_EXECUTE);
				if (aclresult != ACLCHECK_OK)
					aclcheck_error(aclresult, ACL_KIND_PROC,
								   get_func_name(finalfn_oid));
			}
		}

		/* resolve actual type of transition state, if polymorphic */
		aggtranstype = aggform->aggtranstype;
		if (aggtranstype == ANYARRAYOID || aggtranstype == ANYELEMENTOID)
		{
			/* have to fetch the agg's declared input types... */
			Oid		   *declaredArgTypes;
			int			agg_nargs;

			(void) get_func_signature(aggref->aggfnoid,
									  &declaredArgTypes, &agg_nargs);
			Assert(agg_nargs == numArguments);
			aggtranstype = enforce_generic_type_consistency(inputTypes,
															declaredArgTypes,
															agg_nargs,
															aggtranstype);
			pfree(declaredArgTypes);
		}

		/* build expression trees using actual argument & result types */
		build_aggregate_fnexprs(inputTypes,
								numArguments,
								aggtranstype,
								aggref->aggtype,
								transfn_oid,
								finalfn_oid,
								&transfnexpr,
								&finalfnexpr);

		fmgr_info(transfn_oid, &peraggstate->transfn);
		peraggstate->transfn.fn_expr = (Node *) transfnexpr;

		if (OidIsValid(finalfn_oid))
		{
			fmgr_info(finalfn_oid, &peraggstate->finalfn);
			peraggstate->finalfn.fn_expr = (Node *) finalfnexpr;
		}

		get_typlenbyval(aggref->aggtype,
						&peraggstate->resulttypeLen,
						&peraggstate->resulttypeByVal);
		get_typlenbyval(aggtranstype,
						&peraggstate->transtypeLen,
						&peraggstate->transtypeByVal);

		/*
		 * initval is potentially null, so don't try to access it as a struct
		 * field. Must do it the hard way with SysCacheGetAttr.
		 */
		textInitVal = SysCacheGetAttr(AGGFNOID, aggTuple,
									  Anum_pg_aggregate_agginitval,
									  &peraggstate->initValueIsNull);

		if (peraggstate->initValueIsNull)
			peraggstate->initValue = (Datum) 0;
		else
			peraggstate->initValue = GetAggInitVal(textInitVal,
												   aggtranstype);

		/*
		 * If the transfn is strict and the initval is NULL, make sure input
		 * type and transtype are the same (or at least binary-compatible), so
		 * that it's OK to use the first input value as the initial
		 * transValue.	This should have been checked at agg definition time,
		 * but just in case...
		 */
		if (peraggstate->transfn.fn_strict && peraggstate->initValueIsNull)
		{
			if (numArguments < 1 ||
				!IsBinaryCoercible(inputTypes[0], aggtranstype))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_FUNCTION_DEFINITION),
						 errmsg("aggregate %u needs to have compatible input type and transition type",
								aggref->aggfnoid)));
		}

		if (aggref->aggdistinct)
		{
			Oid			eq_function;

			/* We don't implement DISTINCT aggs in the HASHED case */
			Assert(node->aggstrategy != AGG_HASHED);

			/*
			 * We don't currently implement DISTINCT aggs for aggs having more
			 * than one argument.  This isn't required for anything in the SQL
			 * spec, but really it ought to be implemented for
			 * feature-completeness.  FIXME someday.
			 */
			if (numArguments != 1)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("DISTINCT is supported only for single-argument aggregates")));

			peraggstate->inputType = inputTypes[0];
			get_typlenbyval(inputTypes[0],
							&peraggstate->inputtypeLen,
							&peraggstate->inputtypeByVal);

			eq_function = equality_oper_funcid(inputTypes[0]);
			fmgr_info(eq_function, &(peraggstate->equalfn));
			peraggstate->sortOperator = ordering_oper_opid(inputTypes[0]);
			peraggstate->sortstate = NULL;
		}

		ReleaseSysCache(aggTuple);
	}

	/* Update numaggs to match number of unique aggregates found */
	aggstate->numaggs = aggno + 1;

	return aggstate;

}

static Datum GetAggInitVal(Datum textInitVal, Oid transtype) {
	Oid typinput, typioparam;
	char *strInitVal;
	Datum initVal;

	getTypeInputInfo(transtype, &typinput, &typioparam);
	strInitVal = DatumGetCString(DirectFunctionCall1(textout, textInitVal));
	initVal = OidInputFunctionCall(typinput, strInitVal, typioparam, -1);
	pfree(strInitVal);
	return initVal;
}

int ExecCountSlotsAgg(Agg *node) {
	return ExecCountSlotsNode(outerPlan(node) )
			+ ExecCountSlotsNode(innerPlan(node) ) + AGG_NSLOTS;
}

void ExecEndAgg(AggState *node) {

	PlanState  *outerPlan;
		int			aggno;
		PlanState  *innerPlan;/*CHANGED BY YASIN*/
		bool		Pivot2HeapTupleIsNull;/*CHANGED BY YASIN*/

		if (node->grp_Pivot2HeapTuple == NULL) Pivot2HeapTupleIsNull=true;/*CHANGED BY YASIN*/
		else Pivot2HeapTupleIsNull=false;/*CHANGED BY YASIN*/

		/* Make sure we have closed any open tuplesorts */
		for (aggno = 0; aggno < node->numaggs; aggno++)
		{
			AggStatePerAgg peraggstate = &node->peragg[aggno];

			if (peraggstate->sortstate)
				tuplesort_end(peraggstate->sortstate);
		}

		//printf("end operation \n");

		/*
		 * Free both the expr contexts.
		 */
		ExecFreeExprContext(&node->ss.ps);
		node->ss.ps.ps_ExprContext = node->tmpcontext;
		ExecFreeExprContext(&node->ss.ps);

		/* clean up tuple table */
		ExecClearTuple(node->ss.ss_ScanTupleSlot);

		/*CHANGED BY YASIN*/
		ExecClearTuple(node->pivot1Slot);/*set tuple to null and tts_shouldFree = false*/
		if (Pivot2HeapTupleIsNull)/*In this case we should not free the tuple, because it was already done in the previous line*/
			(node->pivot2Slot)->tts_shouldFree = false;/*This should be already false, but just in case*/
		ExecClearTuple(node->pivot2Slot);/*set tuple to null and frees previous content if tts_shouldFree = true*/
		ExecClearTuple(node->modifiedSlot);/*set tuple to null and tts_shouldFree = false*/

		pfree(node->hasharray);			/*free the hash array by Mingjie*/
		RTreeDestroy (node->rtreeroot); /*free the r tree by Mingjie*/

		MemoryContextDelete(node->aggcontext);

		/*Clean Children*/
		outerPlan = outerPlanState(node);
		ExecEndNode(outerPlan);

		/*CHANGED BY YASIN*/
		innerPlan = innerPlanState(node);
		if ( ( (Plan *) node->ss.ps.plan)->righttree != NULL) /*USE SGB-AROUND*/
			ExecEndNode(innerPlan);

}


/*
 * update the group pivot value by the inputslot
 * */
/*
 static void
 updatePivotByTuple(
 AggHashEntry entry,
 int *inputslotdata,
 int numAttr,
 int EPS)
 {
 int i=0;

 for(; i<numAttr; i++)
 {
 if(abs(entry->pivot1Slot[i]-entry->pivot2Slot[i])<=EPS)
 {
 continue;
 }

 if(inputslotdata[i]-EPS>entry->pivot1Slot[i])
 {
 entry->pivot1Slot[i]=inputslotdata[i]-EPS;
 }

 if(inputslotdata[i]+EPS<entry->pivot2Slot[i])
 {
 entry->pivot2Slot[i]=inputslotdata[i]+EPS;
 }

 }

 }*/


void ExecReScanAgg(AggState *node, ExprContext *exprCtxt) {
	ExprContext *econtext = node->ss.ps.ps_ExprContext;
	int aggno;

	node->agg_done = false;

	if (((Agg *) node->ss.ps.plan)->aggstrategy == AGG_HASHED) {
		/*
		 * In the hashed case, if we haven't yet built the hash table then we
		 * can just return; nothing done yet, so nothing to undo. If subnode's
		 * chgParam is not NULL then it will be re-scanned by ExecProcNode,
		 * else no reason to re-scan it at all.
		 */
		if (!node->table_filled)
			return;

		/*
		 * If we do have the hash table and the subplan does not have any
		 * parameter changes, then we can just rescan the existing hash table;
		 * no need to build it again.
		 */
		if (((PlanState *) node)->lefttree->chgParam == NULL ) {
			ResetTupleHashIterator(node->hashtable, &node->hashiter);
			return;
		}
	}

	/* Make sure we have closed any open tuplesorts */
	for (aggno = 0; aggno < node->numaggs; aggno++) {
		AggStatePerAgg peraggstate = &node->peragg[aggno];

		if (peraggstate->sortstate)
			tuplesort_end(peraggstate->sortstate);
		peraggstate->sortstate = NULL;
	}

	/* Release first tuple of group, if we have made a copy */
	if (node->grp_firstTuple != NULL ) {
		heap_freetuple(node->grp_firstTuple);
		node->grp_firstTuple = NULL;
	}

	/* Forget current agg values */
	MemSet(econtext->ecxt_aggvalues, 0, sizeof(Datum) * node->numaggs);
	MemSet(econtext->ecxt_aggnulls, 0, sizeof(bool) * node->numaggs);

	/* Release all temp storage */
	MemoryContextReset(node->aggcontext);

	if (((Agg *) node->ss.ps.plan)->aggstrategy == AGG_HASHED) {
		/* Rebuild an empty hash table */
		build_hash_table(node);
		node->table_filled = false;
	} else {
		/*
		 * Reset the per-group state (in particular, mark transvalues null)
		 */
		MemSet(node->pergroup, 0, sizeof(AggStatePerGroupData) * node->numaggs);
	}

	/*
	 * if chgParam of subnode is not null then plan will be re-scanned by
	 * first ExecProcNode.
	 */
	if (((PlanState *) node)->lefttree->chgParam == NULL )
		ExecReScan(((PlanState *) node)->lefttree, exprCtxt);
}

/*
 * aggregate_dummy - dummy execution routine for aggregate functions
 *
 * This function is listed as the implementation (prosrc field) of pg_proc
 * entries for aggregate functions.  Its only purpose is to throw an error
 * if someone mistakenly executes such a function in the normal way.
 *
 * Perhaps someday we could assign real meaning to the prosrc field of
 * an aggregate?
 */
Datum aggregate_dummy(PG_FUNCTION_ARGS) {
	elog(ERROR, "aggregate function %u called as normal function",
	fcinfo->flinfo->fn_oid);
	return (Datum) 0; /* keep compiler quiet */
}


/**
 * if isnew is true, insert this tuple into the hash table
 * else
 * if isnew is false, do not insert this tuple into the hashtable
 * then if find this tuple, return true, or else
 */
static bool lookup_hash_entry_group(
				  AggState *aggstate,
				  TupleTableSlot *inputslot,
				  bool isnew
				  )
{
	TupleTableSlot *hashslot = aggstate->hashslot;
	Agg		   *node = (Agg *) aggstate->ss.ps.plan;
	ListCell   *l;
	AggHashEntry entry;
	//bool		isnew=false;
	//bool foundResult;

	int numAttributes=node->numCols;

	/* if first time through, initialize hashslot by cloning input slot */
	if (hashslot->tts_tupleDescriptor == NULL)
	{
		ExecSetSlotDescriptor(hashslot, inputslot->tts_tupleDescriptor);
		/* Make sure all unused columns are NULLs */
		ExecStoreAllNullTuple(hashslot);
	}

	/* transfer just the needed columns into hashslot */
	slot_getsomeattrs(inputslot, linitial_int(aggstate->hash_needed));
	foreach(l, aggstate->hash_needed)
	{
		int			varNumber = lfirst_int(l) - 1;

		hashslot->tts_values[varNumber] = inputslot->tts_values[varNumber];
		hashslot->tts_isnull[varNumber] = inputslot->tts_isnull[varNumber];
	}

	/* find or create the hashtable entry using the filtered tuple */

	entry = (AggHashEntry) LookupTupleHashEntry(aggstate->hashtable,
												hashslot,
												&isnew);

	if(entry==NULL&&isnew==NULL)
	{
		return true;
	}

	return isnew;

}

/*CHANGED BY MINGJIE*/
static
void agg_fill_hash_table_overlap_random_l2(
		AggState *aggstate,
		TupleTableSlot *inputslot) {

	Agg		   *node = (Agg *) aggstate->ss.ps.plan;

			/*EPS in the SGB-All */
			int distanceall;
			distanceall = node->distanceall;

			Tuplestorestate *tuplestorestate;
			ExprContext *tmpcontext;

			tmpcontext = aggstate->tmpcontext; /*tmpcontext is the per-input-tuple expression context */
						/* set up for advance_aggregates call */
			tmpcontext->ecxt_scantuple = inputslot; /*advance_aggregates should used riginal tuple*/

			//Tuplestorestate Arraytuplestorestate[100];

			int numAttributes=node->numCols; //we assume the group attribute are the first two column

			Datum		*values = (Datum *) palloc(numAttributes * sizeof(Datum)); /*array*/
			bool * nulls = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
			bool * replace = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
			memset(values, 0, numAttributes * sizeof(Datum));
			memset(nulls, false, numAttributes * sizeof(bool));
			memset(replace, false, numAttributes * sizeof(bool));

			//printf("begin to process this tuple \n");
			//print_slot(inputslot);

			int distanceMetric;

			distanceMetric=2;/*(l2)distanceMetric=0, (l1)distanceMetric=1, (linfinity)distanceMetric=2*/

			//tmpGroup *maxcliquePTR=constructGroup(numAttributes,rand()%100000,rand()%100000,false); /*two D-array*/;

			initilList();

			bool overlapisrandom=true;

			RTREEMBR searchMBR=
					getMBRinputSlot(inputslot,numAttributes);

			bool localcontain=true;
			iscontain_M=localcontain;

			int	 nhits =0;

			nhits=RTreeSearch(aggstate->rtreeroot,
							&searchMBR,
							MySearchCallback,
							localcontain,
							overlapisrandom);

		//printf("hit points %d \n", nhits);

				//if this tuple can join in one group.
			int numcontain=0;

			 if(nhits>0)
						{

						int i=0;
						for(;i<index_contain&&numcontain<1;i++)
						{
							//printf("Step1: !!!!!!!!!!!!!!!!this tuple can join this group %d, %d \n", i, index_contain);

							int index_hashID=containArray[i];

							AggHashEntry ContainEntry=aggstate->hasharray[index_hashID-1].entry;

							bool emptyentry=false;

							if(ContainEntry==NULL)
									{

									getTableSlot(aggstate->hasharray[index_hashID-1].inputslots, numAttributes,inputslot,aggstate->modifiedSlot);
									bool isnew=false;
									isnew=lookup_hash_entry_group(aggstate, aggstate->modifiedSlot,isnew);

									if(isnew==false)
									{
										ContainEntry=lookup_hash_entry(aggstate,aggstate->modifiedSlot);
									}else
									{
										continue;
										printf("error here to find the hash table \n");
									}

									emptyentry=true;
									//continue;
									}
							//printf("Step1: hashID is %d, \n", index_hashID);

							int tmp_pivot1[5];
							int tmp_pivot2[5];

							memcpy(tmp_pivot1,ContainEntry->pivot1Slot, sizeof(int)*numAttributes );
							memcpy(tmp_pivot2,ContainEntry->pivot2Slot, sizeof(int)*numAttributes );

							//printf("p1 %d, p1 %d \n", tmp_pivot1[0],tmp_pivot1[1]);

							bool contain=false;
							bool overlap;
							if(judgeJoinGroup(
								numAttributes,
								tmp_pivot1,
								tmp_pivot2,
								inputslot,
								distanceall,
								distanceMetric,
								&overlap,
								tmpcontext->ecxt_per_tuple_memory)
								)
							{
								contain=true;
								//

							}else
							{
								continue;
							}

							int judgeresult = 2;

							if(contain==true)
							{
								tuple *inputTuple_;
								inputTuple_ = (tuple *) palloc(sizeof(tuple));
								inputTuple_->numAttr = 2;
								copyfromTupleTableToArray(inputTuple_->values, inputTuple_->values,
										inputslot, numAttributes);

								convexhull * convex = (convexhull *) ContainEntry->convexhull;

								judgeresult = insideConvexAndInsideBound(convex, inputTuple_,
										distanceall);

								if (judgeresult == 1) {
									unionConvex(convex, inputTuple_, distanceall);
								} else {
									pfree(inputTuple_);
								}

							}

							if(judgeresult<=1)
							{
								//printf("can join \n");

								numcontain++;
								RTREEMBR deleteMBR=
								getMBRHashEntry(ContainEntry,numAttributes);

								int result=RTreeDelete(aggstate->rtreeroot,
								&deleteMBR,
								(void*)(index_hashID)
								);

								memcpy(ContainEntry->pivot1Slot,tmp_pivot1, sizeof(int)*numAttributes );
								memcpy(ContainEntry->pivot2Slot,tmp_pivot2, sizeof(int)*numAttributes );

								//printf("after update \n");
								Tuplestorestate *tuplestorestateHash;
								tuplestorestateHash=(Tuplestorestate *)ContainEntry->tuplestorestate;

								if (tuplestorestateHash)
									tuplestore_puttupleslot(tuplestorestateHash, inputslot);

								memcpy(&(aggstate->hasharray[index_hashID-1].entry), &(ContainEntry), sizeof(AggHashEntry));

								advance_aggregates(aggstate, ContainEntry->pergroup); /*YAS THIS SHOULD USE THE ORIGINAL TUPLE*/

								/*********update the tree**************/

								RTREEMBR newMBR=getMBRHashEntry(ContainEntry,numAttributes);

								RTreeInsert(aggstate->rtreeroot, &newMBR,   /* the mbr being inserted */
								  (void*)(index_hashID),  /* i+1 is mbr ID. ID MUST NEVER BE ZERO */
								  0     /* always zero which means to add from the root */
								 );


							}

						}//for loop

			 }

			 if(numcontain==0)
					{
				 			AggHashEntry entry = lookup_hash_entry(aggstate, inputslot);/*YAS THIS SHOULD USE THE MODIFIED TUPLE*/

				 			if(entry->flagEnabled!=0)
				 			{
				 				return;
				 			}

							//AggHashEntry entry = lookup_hash_entry(aggstate, inputslot);/*YAS THIS SHOULD USE THE MODIFIED TUPLE*/
							entry->flagEnabled=1;
							Tuplestorestate *tuplestorestate;
							tuplestorestate=(Tuplestorestate *)entry->tuplestorestate;
													//put the tuple into the tuplestorestate
							if (tuplestorestate)
								tuplestore_puttupleslot(tuplestorestate, inputslot);

							tuple *inputTuple_;
							inputTuple_ = (tuple *) palloc(sizeof(tuple));
							inputTuple_->numAttr = 2;
							copyfromTupleTableToArray(inputTuple_->values, inputTuple_->values,
									inputslot, numAttributes);

							convexhull * convex = (convexhull *) entry->convexhull;
							unionConvex(convex, inputTuple_, distanceall);

							copyfromTupleTableToArray(entry->pivot1Slot,entry->pivot2Slot,inputslot,numAttributes);
							updateInputSlotbyEPS(entry->pivot1Slot,entry->pivot2Slot, numAttributes, distanceall);
							/***********************************************/

							RTREEMBR insertMBR=getMBRHashEntry(entry,numAttributes);

							int result=RTreeInsert(aggstate->rtreeroot, &insertMBR,   /* the mbr being inserted */
									  (void*)(aggstate->indexhasharray+1),  /* i+1 is mbr ID. ID MUST NEVER BE ZERO */
									  0     /* always zero which means to add from the root */
									 );

							aggstate->indexhasharray++;
							initialize_aggregates(aggstate, aggstate->peragg, entry->pergroup);
							/***********************************************/
							/* Advance the aggregates */
							advance_aggregates(aggstate, entry->pergroup); /*YAS THIS SHOULD USE THE ORIGINAL TUPLE*/

							ResetExprContext(tmpcontext);

				}

				//do not need to consider the overlap problem here.
				/* Initialize to walk the hash table */

				/*bool finishProcessing = false;
				 ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);
				 while (!finishProcessing)
				 {

				AggHashEntry  entry = (AggHashEntry) ScanTupleHashTable(&aggstate->hashiter);

				 if (entry == NULL)
				 {
				 finishProcessing = true;
				 }else if(entry->flagEnabled == 1)
				 {

					printGroup((Tuplestorestate *)entry->tuplestorestate, aggstate->modifiedSlot);
				    //print_groups((tmpGroup *)entry->tuplestorestate,2);
				 	printf("convex hull is \n");
				 	print_tuples(((convexhull *) entry->convexhull)->convexheader);
				 }

				 }

				printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!end to operation for this tuple \n");*/

				pfree(values);
				pfree(replace);
				pfree(nulls);

}

/*CHANGED BY MINGJIE*/
static
void agg_fill_hash_table_overlap_random_l1(
		AggState *aggstate,
		TupleTableSlot *inputslot)
{
	Agg		   *node = (Agg *) aggstate->ss.ps.plan;

			/*EPS in the SGB-All */
			int distanceall;
			distanceall = node->distanceall;

			Tuplestorestate *tuplestorestate;
			ExprContext *tmpcontext;

			tmpcontext = aggstate->tmpcontext; /*tmpcontext is the per-input-tuple expression context */
						/* set up for advance_aggregates call */
			tmpcontext->ecxt_scantuple = inputslot; /*advance_aggregates should used riginal tuple*/

			//Tuplestorestate Arraytuplestorestate[100];

			int numAttributes=node->numCols; //we assume the group attribute are the first two column

			Datum		*values = (Datum *) palloc(numAttributes * sizeof(Datum)); /*array*/
				bool * nulls = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
				bool * replace = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
				memset(values, 0, numAttributes * sizeof(Datum));
				memset(nulls, false, numAttributes * sizeof(bool));
				memset(replace, false, numAttributes * sizeof(bool));

			int distanceMetric;

			distanceMetric=1;/*(l2)distanceMetric=0, (l1)distanceMetric=1, (linfinity)distanceMetric=2*/

			//tmpGroup *maxcliquePTR=constructGroup(numAttributes,rand()%100000,rand()%100000,false); /*two D-array*/;

			initilList();

			//printf("begin to process this tuple \n");
			//print_slot(inputslot);

			RTREEMBR searchMBR=
					getMBRinputSlot(inputslot,numAttributes);

			bool localcontain=true;
			iscontain_M=localcontain;

			int	 nhits =0;
			bool isoverlaprandom=true;

				nhits=RTreeSearch(aggstate->rtreeroot,
							&searchMBR,
							MySearchCallback,
							localcontain,
							isoverlaprandom);

				//if this tuple can join in one group.
				int numcontain=0;
				AggHashEntry Tmp_entry;
			    int index_hashID_tmp;

				if(nhits>0)
						{

						int i=0;
						for(;i<index_contain&&numcontain<1;i++)
						{
							//printf("Step1: !!!!!!!!!!!!!!!!this tuple can join this group %d, %d \n", i, index_contain);

							int index_hashID=containArray[i];

							AggHashEntry ContainEntry=aggstate->hasharray[index_hashID-1].entry;

							bool emptyentry=false;

							if(ContainEntry==NULL)
									{
									//printf("original error here 1 %d \n", index_hashID);
									getTableSlot(aggstate->hasharray[index_hashID-1].inputslots, numAttributes,inputslot,aggstate->modifiedSlot);
									bool isnew=false;
									isnew=lookup_hash_entry_group(aggstate, aggstate->modifiedSlot,isnew);

									if(isnew==false)
									{
										ContainEntry=lookup_hash_entry(aggstate,aggstate->modifiedSlot);
									}else
									{
										continue;
										printf("error here to find the hash table \n");
									}

									emptyentry=true;
									//continue;
									}
							//printf("Step1: hashID is %d, \n", index_hashID);

							int tmp_pivot1[5];
							int tmp_pivot2[5];


							memcpy(tmp_pivot1,ContainEntry->pivot1Slot, sizeof(int)*numAttributes );
							memcpy(tmp_pivot2,ContainEntry->pivot2Slot, sizeof(int)*numAttributes );

							//printf("p1 %d, p1 %d \n", tmp_pivot1[0],tmp_pivot1[1]);

							bool overlap;

							if(judgeJoinGroup(
								numAttributes,
								tmp_pivot1,
								tmp_pivot2,
								inputslot,
								distanceall,
								distanceMetric,
								&overlap,
								tmpcontext->ecxt_per_tuple_memory)
								)
							{
								//printf("can join \n");
								numcontain++;
								RTREEMBR deleteMBR=
								getMBRHashEntry(ContainEntry,numAttributes);

								int result=RTreeDelete(aggstate->rtreeroot,
								&deleteMBR,
								(void*)(index_hashID)
								);

								memcpy(ContainEntry->pivot1Slot,tmp_pivot1, sizeof(int)*numAttributes );
								memcpy(ContainEntry->pivot2Slot,tmp_pivot2, sizeof(int)*numAttributes );

								Tuplestorestate *tuplestorestateHash;
								tuplestorestateHash=(Tuplestorestate *)ContainEntry->tuplestorestate;

								if(tuplestorestateHash)
									tuplestore_puttupleslot(tuplestorestateHash, inputslot);

								advance_aggregates(aggstate, ContainEntry->pergroup); /*YAS THIS SHOULD USE THE ORIGINAL TUPLE*/

							}else
							{
								continue;
							}

							memcpy(&(aggstate->hasharray[index_hashID-1].entry), &(ContainEntry), sizeof(AggHashEntry));
							/*********update the tree**************/
							RTREEMBR newMBR=
							getMBRHashEntry(ContainEntry,numAttributes);

							RTreeInsert(aggstate->rtreeroot, &newMBR,   /* the mbr being inserted */
							  (void*)(index_hashID),  /* i+1 is mbr ID. ID MUST NEVER BE ZERO */
							  0     /* always zero which means to add from the root */
							 );

						}//for loop

			 }

			if(numcontain==0)
					{

					AggHashEntry entry = lookup_hash_entry(aggstate, inputslot);/*YAS THIS SHOULD USE THE MODIFIED TUPLE*/

					if(entry->flagEnabled!=0)
					{
						return;
					}

					//AggHashEntry entry = lookup_hash_entry(aggstate, inputslot);/*YAS THIS SHOULD USE THE MODIFIED TUPLE*/
					entry->flagEnabled=1;
					Tuplestorestate *tuplestorestate;
					tuplestorestate=(Tuplestorestate *)entry->tuplestorestate;
											//put the tuple into the tuplestorestate
					if (tuplestorestate)
						tuplestore_puttupleslot(tuplestorestate, inputslot);

					copyfromTupleTableToArray(entry->pivot1Slot,entry->pivot2Slot,inputslot,numAttributes);
					updateInputSlotbyEPS(entry->pivot1Slot,entry->pivot2Slot, numAttributes, distanceall);
					/***********************************************/

					RTREEMBR insertMBR=getMBRHashEntry(entry,numAttributes);

					int result=RTreeInsert(aggstate->rtreeroot, &insertMBR,   /* the mbr being inserted */
							  (void*)(aggstate->indexhasharray+1),  /* i+1 is mbr ID. ID MUST NEVER BE ZERO */
							  0     /* always zero which means to add from the root */
							 );

					aggstate->indexhasharray++;
					initialize_aggregates(aggstate, aggstate->peragg, entry->pergroup);
					/***********************************************/
					/* Advance the aggregates */
					advance_aggregates(aggstate, entry->pergroup); /*YAS THIS SHOULD USE THE ORIGINAL TUPLE*/

					ResetExprContext(tmpcontext);

				}

				//do not need to consider the overlap problem here.

				pfree(values);
				pfree(replace);
				pfree(nulls);


}

/*CHANGED BY MINGJIE*/
static
void agg_fill_hash_table_overlap_eliminated_l2(AggState *aggstate,
		TupleTableSlot *inputslot) {

		Agg		   *node = (Agg *) aggstate->ss.ps.plan;
		/*EPS in the SGB-All */
		int distanceall;
		distanceall = node->distanceall;

		Tuplestorestate *tuplestorestate;
		ExprContext *tmpcontext;

		tmpcontext = aggstate->tmpcontext; /*tmpcontext is the per-input-tuple expression context */
					/* set up for advance_aggregates call */
		tmpcontext->ecxt_scantuple = inputslot; /*advance_aggregates should used riginal tuple*/

		int numAttributes=node->numCols; //we assume the group attribute are the first two column

		Datum		*values = (Datum *) palloc(numAttributes * sizeof(Datum)); /*array*/
		bool * nulls = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
		bool * replace = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
		memset(values, 0, numAttributes * sizeof(Datum));
		memset(nulls, false, numAttributes * sizeof(bool));
		memset(replace, false, numAttributes * sizeof(bool));
		int distanceMetric;

		distanceMetric=2;/*(l2)distanceMetric=0, (l1)distanceMetric=1, (linfinity)distanceMetric=2*/

		initilList();
		RTREEMBR searchMBR=
				getMBRinputSlot(inputslot,numAttributes);

		bool localcontain=true;
		iscontain_M=localcontain;
		bool isoverlaprandom=false;

		int	 nhits =0;
		nhits=RTreeSearch(aggstate->rtreeroot,
						&searchMBR,
						MySearchCallback,
						localcontain,
						isoverlaprandom);

	 //printf("number hits is %d \n", nhits);

	  int numcontain=0;

	  AggHashEntry Tmp_entry;
	  int index_hashID_tmp;

	  if(nhits>0)
			{

			int i=0;

			for(;i<index_contain&&numcontain<=2;i++)
			{

				//printf("Step1: !!!!!!!!!!!!!!!!this tuple can join this group %d, %d \n", i, index_contain);

				int index_hashID=containArray[i];

				AggHashEntry ContainEntry=aggstate->hasharray[index_hashID-1].entry;

				bool emptyentry=false;

				if(ContainEntry==NULL)
						{
						//printf("original error here 1 %d \n", index_hashID);
						getTableSlot(aggstate->hasharray[index_hashID-1].inputslots, numAttributes,inputslot,aggstate->modifiedSlot);
						bool isnew=false;
						isnew=lookup_hash_entry_group(aggstate, aggstate->modifiedSlot,isnew);

						if(isnew==false)
						{
							ContainEntry=lookup_hash_entry(aggstate,aggstate->modifiedSlot);
						}else
						{
							printf("error here to find the hash table \n");
							continue;
						}

						emptyentry=true;
						//continue;
						}
				//printf("Step1: hashID is %d, \n", index_hashID);

				int tmp_pivot1[5];
				int tmp_pivot2[5];

				memcpy(tmp_pivot1,ContainEntry->pivot1Slot, sizeof(int)*numAttributes );
				memcpy(tmp_pivot2,ContainEntry->pivot2Slot, sizeof(int)*numAttributes );

				//printf("p1 %d, p1 %d \n", tmp_pivot1[0],tmp_pivot1[1]);

				bool overlap;
				bool contain=false;

				if(judgeJoinGroup(
					numAttributes,
					tmp_pivot1,
					tmp_pivot2,
					inputslot,
					distanceall,
					distanceMetric,
					&overlap,
					tmpcontext->ecxt_per_tuple_memory)
					)
				{
					//printf("can join \n");
					contain=true;
				}else
				{
					continue;
				}

				int judgeresult=2;

				if(contain==true)
				{
					tuple *inputTuple_;
					inputTuple_ = (tuple *) palloc(sizeof(tuple));
					inputTuple_->numAttr = 2;
					copyfromTupleTableToArray(inputTuple_->values, inputTuple_->values,
							inputslot, numAttributes);

					convexhull * convex = (convexhull *) ContainEntry->convexhull;

					judgeresult = insideConvexAndInsideBound(convex, inputTuple_,distanceall);

					pfree(inputTuple_);

					if(judgeresult>1)
					{
						continue;

					}	else if(judgeresult<=1)
					{
						numcontain++;

						RTREEMBR deleteMBR=
						getMBRHashEntry(ContainEntry,numAttributes);

						int result=RTreeDelete(aggstate->rtreeroot,
						&deleteMBR,
						(void*)(index_hashID)
						);

						memcpy(ContainEntry->pivot1Slot,tmp_pivot1, sizeof(int)*numAttributes );
						memcpy(ContainEntry->pivot2Slot,tmp_pivot2, sizeof(int)*numAttributes );

						if(numcontain==1)
						{
							Tmp_entry=ContainEntry;
							index_hashID_tmp=index_hashID;
						}

						memcpy(&(aggstate->hasharray[index_hashID-1].entry), &(ContainEntry), sizeof(AggHashEntry));

						RTREEMBR newMBR=getMBRHashEntry(ContainEntry,numAttributes);

						RTreeInsert(aggstate->rtreeroot, &newMBR,   /* the mbr being inserted */
										  (void*)(index_hashID),  /* i+1 is mbr ID. ID MUST NEVER BE ZERO */
										  0     /* always zero which means to add from the root */
										 );

					}

				}else
				{
					continue;
				}

			}//for loop


			}

       if(numcontain==1){

	   	   	AggHashEntry ContainEntry=Tmp_entry;

	   	   	tuple *inputTuple_;
			inputTuple_ = (tuple *) palloc(sizeof(tuple));
			inputTuple_->numAttr = 2;
			copyfromTupleTableToArray(inputTuple_->values, inputTuple_->values,
					inputslot, numAttributes);

			convexhull * convex = (convexhull *) ContainEntry->convexhull;
			int judgeresult = insideConvexAndInsideBound(convex, inputTuple_,distanceall);

			if(judgeresult==1)
				unionConvex(convex, inputTuple_, distanceall);

			Tuplestorestate *tuplestorestateHash;
									tuplestorestateHash=(Tuplestorestate *)ContainEntry->tuplestorestate;

				if (tuplestorestateHash)
							   tuplestore_puttupleslot(tuplestorestateHash, inputslot);

				advance_aggregates(aggstate, ContainEntry->pergroup); /*YAS THIS SHOULD USE THE ORIGINAL TUPLE*/

				memcpy(&(aggstate->hasharray[index_hashID_tmp-1].entry), &(ContainEntry), sizeof(AggHashEntry));

		}else if(numcontain==0)
			{

			  if(numAttributes==1)
				{
					values[0] = (rand()%50000000)+aggstate->numbertuples+rand()%20000000;
					replace[0]=true;
				}else if (numAttributes==2)
				{
					values[0] = (rand()%50000000)+aggstate->numbertuples;
					replace[0]=true;
					values[1] =  rand()%10000;
					replace[1]=true;
				}
				//printf("4135 \n");
				HeapTuple	newHeapTuple;/*CHANGED BY Mingjie*/

				//newHeapTuple=heap_form_tuple(inputslot->tts_tupleDescriptor, values,nulls);

				newHeapTuple = heap_modify_tuple(inputslot->tts_tuple, inputslot->tts_tupleDescriptor, values, nulls, replace);

				ExecStoreTuple(newHeapTuple,aggstate->modifiedSlot,InvalidBuffer,true);/*This will free memory of previous tuple too*/

				AggHashEntry entry = lookup_hash_entry(aggstate, aggstate->modifiedSlot);/*YAS THIS SHOULD USE THE MODIFIED TUPLE*/

				if(entry->flagEnabled!=0)
				{
					return;
				}

				entry->flagEnabled=1;
				Tuplestorestate *tuplestorestate;
				tuplestorestate=(Tuplestorestate *)entry->tuplestorestate;
										//put the tuple into the tuplestorestate
				if (tuplestorestate)
					tuplestore_puttupleslot(tuplestorestate, inputslot);

				tuple *inputTuple_;
				inputTuple_ = (tuple *) palloc(sizeof(tuple));
				inputTuple_->numAttr = 2;
				copyfromTupleTableToArray(inputTuple_->values, inputTuple_->values,
						inputslot, numAttributes);

				convexhull * convex = (convexhull *) entry->convexhull;
				unionConvex(convex, inputTuple_, distanceall);

				copyfromTupleTableToArray(entry->pivot1Slot,entry->pivot2Slot,inputslot,numAttributes);
				updateInputSlotbyEPS(entry->pivot1Slot,entry->pivot2Slot, numAttributes, distanceall);
				/***********************************************/
				RTREEMBR insertMBR=getMBRHashEntry(entry,numAttributes);

				int result=RTreeInsert(aggstate->rtreeroot, &insertMBR,   /* the mbr being inserted */
						  (void*)(aggstate->indexhasharray+1),  /* i+1 is mbr ID. ID MUST NEVER BE ZERO */
						  0     /* always zero which means to add from the root */
						 );
				aggstate->indexhasharray++;
				initialize_aggregates(aggstate, aggstate->peragg, entry->pergroup);
				/***********************************************/
				/* Advance the aggregates */
				advance_aggregates(aggstate, entry->pergroup); /*YAS THIS SHOULD USE THE ORIGINAL TUPLE*/

				ResetExprContext(tmpcontext);

			}

}

/*CHANGED BY MINGJIE*/
static
void agg_fill_hash_table_overlap_eliminated_l1(AggState *aggstate,
		TupleTableSlot *inputslot)
{

		Agg		   *node = (Agg *) aggstate->ss.ps.plan;

		/*EPS in the SGB-All */
		int distanceall;
		distanceall = node->distanceall;

		Tuplestorestate *tuplestorestate;
		ExprContext *tmpcontext;

		tmpcontext = aggstate->tmpcontext; /*tmpcontext is the per-input-tuple expression context */
					/* set up for advance_aggregates call */
		tmpcontext->ecxt_scantuple = inputslot; /*advance_aggregates should used riginal tuple*/


		int numAttributes=node->numCols; //we assume the group attribute are the first two column

		int distanceMetric;

		distanceMetric=1;/*(l2)distanceMetric=0, (l1)distanceMetric=1, (linfinity)distanceMetric=2*/

		initilList();
		RTREEMBR searchMBR=
				getMBRinputSlot(inputslot,numAttributes);

		bool localcontain=true;
		iscontain_M=localcontain;

		bool isoverlaprandom=false;

		int	 nhits =0;
		nhits=RTreeSearch(aggstate->rtreeroot,
						&searchMBR,
						MySearchCallback,
						localcontain,
						isoverlaprandom);

		int numcontain=0;

		  AggHashEntry Tmp_entry;
		  int index_hashID_tmp;

			if(nhits>0)
				{
					int i=0;

					for(;i<index_contain&&numcontain<=2;i++)
					{
						//printf("Step1: !!!!!!!!!!!!!!!!this tuple can join this group %d, %d \n", i, index_contain);

						int index_hashID=containArray[i];

						AggHashEntry ContainEntry=aggstate->hasharray[index_hashID-1].entry;

						bool emptyentry=false;

						if(ContainEntry==NULL)
								{
								//printf("original error here 1 %d \n", index_hashID);
								getTableSlot(aggstate->hasharray[index_hashID-1].inputslots, numAttributes,inputslot,aggstate->modifiedSlot);
								bool isnew=false;
								isnew=lookup_hash_entry_group(aggstate, aggstate->modifiedSlot,isnew);

								if(isnew==false)
								{
									ContainEntry=lookup_hash_entry(aggstate,aggstate->modifiedSlot);
								}else
								{
									printf("error here to find the hash table \n");
									continue;
								}

								emptyentry=true;
								//continue;
								}
						//printf("Step1: hashID is %d, \n", index_hashID);

						int tmp_pivot1[5];
						int tmp_pivot2[5];


						memcpy(tmp_pivot1,ContainEntry->pivot1Slot, sizeof(int)*numAttributes );
						memcpy(tmp_pivot2,ContainEntry->pivot2Slot, sizeof(int)*numAttributes );

						//printf("p1 %d, p1 %d \n", tmp_pivot1[0],tmp_pivot1[1]);

						bool overlap;

						if(judgeJoinGroup(
							numAttributes,
							tmp_pivot1,
							tmp_pivot2,
							inputslot,
							distanceall,
							distanceMetric,
							&overlap,
							tmpcontext->ecxt_per_tuple_memory)
							)
						{
							//printf("can join \n");
							numcontain++;
							RTREEMBR deleteMBR=
							getMBRHashEntry(ContainEntry,numAttributes);

							int result=RTreeDelete(aggstate->rtreeroot,
							&deleteMBR,
							(void*)(index_hashID)
							);

							memcpy(ContainEntry->pivot1Slot,tmp_pivot1, sizeof(int)*numAttributes );
							memcpy(ContainEntry->pivot2Slot,tmp_pivot2, sizeof(int)*numAttributes );

							if(numcontain==1)
							{
								Tmp_entry=ContainEntry;
								index_hashID_tmp=index_hashID;
							}

							memcpy(&(aggstate->hasharray[index_hashID-1].entry), &(ContainEntry), sizeof(AggHashEntry));

							RTREEMBR newMBR=
							getMBRHashEntry(ContainEntry,numAttributes);

							RTreeInsert(aggstate->rtreeroot, &newMBR,   /* the mbr being inserted */
							  (void*)(index_hashID),  /* i+1 is mbr ID. ID MUST NEVER BE ZERO */
							  0     /* always zero which means to add from the root */
							 );

						}else
						{
							continue;
						}

						//printf("after update \n")


					}//for loop

				}

			if(numcontain==1)
			{
				AggHashEntry ContainEntry=Tmp_entry;
				Tuplestorestate *tuplestorestateHash;
				tuplestorestateHash=(Tuplestorestate *)ContainEntry->tuplestorestate;

				if (tuplestorestateHash)
					tuplestore_puttupleslot(tuplestorestateHash, inputslot);

				advance_aggregates(aggstate, ContainEntry->pergroup); /*YAS THIS SHOULD USE THE ORIGINAL TUPLE*/

				memcpy(&(aggstate->hasharray[index_hashID_tmp-1].entry), &(ContainEntry), sizeof(AggHashEntry));
			}


			if(numcontain==0)
				{
						AggHashEntry entry = lookup_hash_entry(aggstate, inputslot);/*YAS THIS SHOULD USE THE MODIFIED TUPLE*/

						if(	entry->flagEnabled!=0)
							return;

						entry->flagEnabled=1;
						Tuplestorestate *tuplestorestate;
						tuplestorestate=(Tuplestorestate *)entry->tuplestorestate;
												//put the tuple into the tuplestorestate
						if (tuplestorestate)
							tuplestore_puttupleslot(tuplestorestate, inputslot);

						copyfromTupleTableToArray(entry->pivot1Slot,entry->pivot2Slot,inputslot,numAttributes);
						updateInputSlotbyEPS(entry->pivot1Slot,entry->pivot2Slot, numAttributes, distanceall);
						/***********************************************/
						RTREEMBR insertMBR=getMBRHashEntry(entry,numAttributes);

						int result=RTreeInsert(aggstate->rtreeroot, &insertMBR,   /* the mbr being inserted */
								  (void*)(aggstate->indexhasharray+1),  /* i+1 is mbr ID. ID MUST NEVER BE ZERO */
								  0     /* always zero which means to add from the root */
								 );
						aggstate->indexhasharray++;
						initialize_aggregates(aggstate, aggstate->peragg, entry->pergroup);
						/***********************************************/
						/* Advance the aggregates */
						advance_aggregates(aggstate, entry->pergroup); /*YAS THIS SHOULD USE THE ORIGINAL TUPLE*/

						ResetExprContext(tmpcontext);

					}


					/* Initialize to walk the hash table */

					/* finishProcessing = false;
					 ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);
					 while (!finishProcessing)
					 {

					 entry = (AggHashEntry) ScanTupleHashTable(&aggstate->hashiter);

					 if (entry == NULL)
					 {
					 finishProcessing = true;
					 }else if(entry->flagEnabled == 1)
					 {

						 printGroup((Tuplestorestate *)entry->tuplestorestate, aggstate->modifiedSlot);
					     //print_groups((tmpGroup *)entry->tuplestorestate,2);
					 	//printf("convex hull is \n");
					 	//print_tuples(((convexhull *) entry->convexhull)->convexheader);
					 }

					 }

					printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!end to operation for this tuple \n");*/
}

/*CHANGED BY MINGJIE*/
static
void agg_fill_hash_table_overlap_newgroup_l2(AggState *aggstate,
		TupleTableSlot *inputslot) {

	Agg		   *node = (Agg *) aggstate->ss.ps.plan;
			ExprContext *tmpcontext;
			tmpcontext = aggstate->tmpcontext; /*tmpcontext is the per-input-tuple expression context */
												/* set up for advance_aggregates call */
			tmpcontext->ecxt_scantuple = inputslot; /*advance_aggregates should used riginal tuple*/

			/*EPS in the SGB-All */
			int distanceall;
			Datum distanceallDatum;
			distanceall = node->distanceall;
			distanceallDatum = Int32GetDatum(distanceallDatum);

			int distanceMetric;
			distanceMetric=2;/*(l2)distanceMetric=0, (l1)distanceMetric=1, (linfinity)distanceMetric=2*/

			Datum		attrPivot1,attrPivot2,attrCurrent;
			bool		isNull1,isNull2,isNull3;
			FmgrInfo *ltfunctions;
			FmgrInfo *minusfunctions;

			int numAttributes=node->numCols; //we assume the group attribute are the first two column

			Datum		*values = (Datum *) palloc(numAttributes * sizeof(Datum)); /*array*/
			bool * nulls = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
			bool * replace = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
			memset(values, 0, numAttributes * sizeof(Datum));
			memset(nulls, false, numAttributes * sizeof(bool));
			memset(replace, false, numAttributes * sizeof(bool));

			if (aggstate->modifiedSlot->tts_tupleDescriptor == NULL)
				{
					ExecSetSlotDescriptor(aggstate->modifiedSlot,inputslot->tts_tupleDescriptor);
				}

			//printf("begin to process this tuple \n");
			//print_slot(inputslot);

			tmpGroup *maxcliquePTR=NULL;

			//=constructGroup(numAttributes,rand()%100000,rand()%100000,false); /*two D-array*/;

			initilList();
			RTREEMBR searchMBR=
					getMBRinputSlot(inputslot,numAttributes);

			bool localcontain=true;
			bool overlaprandom=false;

			iscontain_M=localcontain;
			int	 nhits =0;
			nhits=RTreeSearch(aggstate->rtreeroot,
							&searchMBR,
							MySearchCallback,
							localcontain,
							overlaprandom);

			AggHashEntry tmpEntry;
			int tmpHashID=0;

			AggHashEntry updateEntry;
			int updateHashID=0;

			//printf("number of hits in contain is %d \n", nhits);

			//for the group in the
			int numcontain=0;

			if(nhits>0)
			{
				int i=0;
				//printf("Step1: !!!!!!!!!!!!!!! hits is %d, and index contain is %d \n", nhits, index_contain);

				for(;i<index_contain;i++)
					{
						int index_hashID=containArray[i];

						AggHashEntry ContainEntry=aggstate->hasharray[index_hashID-1].entry;
						bool emptyentry=false;

						if(ContainEntry==NULL)
							{
							//printf("original error here 1 %d \n", index_hashID);
							getTableSlot(aggstate->hasharray[index_hashID-1].inputslots, numAttributes,inputslot,aggstate->modifiedSlot);
							bool isnew=false;
							isnew=lookup_hash_entry_group(aggstate, aggstate->modifiedSlot,isnew);

							if(isnew==false)
							{
								ContainEntry=lookup_hash_entry(aggstate,aggstate->modifiedSlot);
							}else
							{
								//printf("error here to find the hash table \n");
								continue;
							}

							emptyentry=true;
							//continue;
							}

					bool overlap;

					int tmp_pivot1[5];
					int tmp_pivot2[5];

					memcpy(tmp_pivot1,ContainEntry->pivot1Slot, sizeof(int)*numAttributes );
					memcpy(tmp_pivot2,ContainEntry->pivot2Slot, sizeof(int)*numAttributes );

					bool contain=false;

					if(judgeJoinGroup(
						numAttributes,
						tmp_pivot1,
						tmp_pivot2,
						inputslot,
						distanceall,
						distanceMetric,
						&overlap,
						tmpcontext->ecxt_per_tuple_memory)
						)
					{
						contain=true;
						//printf("can update and pivot1 %d, %d \n",ContainEntry->pivot1Slot[0], ContainEntry->pivot1Slot[1]);
					}else
					{
						continue;
					}

					int judgeresult = 2;

					if(contain==true)
					{
						tuple *inputTuple_;
						inputTuple_ = (tuple *) palloc(sizeof(tuple));
						inputTuple_->numAttr = 2;
						copyfromTupleTableToArray(inputTuple_->values, inputTuple_->values,inputslot, numAttributes);
						convexhull * convex = (convexhull *) ContainEntry->convexhull;

						if(convex->convexheader!=NULL)
							judgeresult = insideConvexAndInsideBound(convex, inputTuple_,distanceall);

						if (judgeresult == 1&&numcontain<=1) {
							unionConvex(convex, inputTuple_, distanceall);
						} else {
							pfree(inputTuple_);
							//continue;
						}

						//printf("convex hull is \n");
						//print_tuples(((convexhull *) ContainEntry->convexhull)->convexheader);
						//printf("judge result for the overlap %d \n", judgeresult);

					}

					if(judgeresult<=1)
					{
						numcontain++;

						RTREEMBR deleteMBR=
						getMBRHashEntry(ContainEntry,numAttributes);

						int result=RTreeDelete(aggstate->rtreeroot,&deleteMBR,(void*)(index_hashID)
						);

						memcpy(ContainEntry->pivot1Slot,tmp_pivot1, sizeof(int)*numAttributes );
						memcpy(ContainEntry->pivot2Slot,tmp_pivot2, sizeof(int)*numAttributes );
						ContainEntry->ismodified=true;

						if(i==0)
						{
							updateEntry=ContainEntry;
							updateHashID=index_hashID;
						}

						if(i==1)
						{
						Tuplestorestate *tuplestorestate;
						tuplestorestate=(Tuplestorestate *)aggstate->overlaptuplestorestate1;
						if(tuplestorestate)
							tuplestore_puttupleslot(tuplestorestate, inputslot);
						}

						memcpy(&(aggstate->hasharray[index_hashID-1].entry), &(ContainEntry), sizeof(AggHashEntry));

					}

				}

			}

			/*******************************************************************************************/
			RTREEMBR searchMBR2=
					getMBRinputSlotBigger(inputslot,numAttributes, distanceall);

			localcontain=false;
			iscontain_M=localcontain;
			int nhits2=0;

			nhits2 =RTreeSearch(
							aggstate->rtreeroot,
							&searchMBR2,
							MySearchCallback,
							localcontain,
							overlaprandom);

			int tmpindex=0;

			//printf("number of overlap is %d \n", nhits2);

			if(nhits2>0)
			{
				int i=0;
				bool putdoor=true;

				for(;i<index_overlap;i++)
				{
					int index_hashID=overlapArray[i];
					AggHashEntry ContainEntry=aggstate->hasharray[index_hashID-1].entry;

					//iterate the tuplestorestate
					if(ContainEntry==NULL)
					{
						getTableSlot(aggstate->hasharray[index_hashID-1].inputslots, numAttributes,inputslot,aggstate->modifiedSlot);

						bool isnew=false;
						isnew=lookup_hash_entry_group(aggstate, aggstate->modifiedSlot,isnew);

						if(isnew==false)
						{
							ContainEntry=lookup_hash_entry(aggstate,aggstate->modifiedSlot);
						}else
						{
							//printf("error here to find the hash table \n");
							continue;
						}

					}

					if(ContainEntry->flagEnabled==0||ContainEntry->flagEnabled==3)
					{
						continue;
					}

					int judgeresult=2;

					tuple *inputTuple_;
					inputTuple_ = (tuple *) palloc(sizeof(tuple));
					inputTuple_->numAttr = 2;
					copyfromTupleTableToArray(inputTuple_->values, inputTuple_->values,
							inputslot, numAttributes);

					convexhull * convex = (convexhull *) ContainEntry->convexhull;

					if(convex->convexheader!=NULL)
						judgeresult = insideConvexAndInsideBound(convex, inputTuple_,distanceall);

					pfree(inputTuple_);

					//printf("convex hull is \n");
					//print_tuples(((convexhull *) ContainEntry->convexhull)->convexheader);
					//printf("judge result for the overlap %d \n", judgeresult);

					if(judgeresult!=3)
					{
						continue;
					}

					bool iisempty=true;

					bool entryNULL=false;

					if(tmpindex<1)
					{
						//printf("Step2:!!!!!!!!!!!!!!!!begin to have overlap first group \n");
						//tmpGroup *tmpTupleStore;

						maxcliquePTR =findIntersection_putIntoMemoList_Convex(
						inputslot,
						aggstate->modifiedSlot,
						aggstate->pivot1Slot,
						(tmpGroup *)ContainEntry->tuplestorestate,
						(Tuplestorestate *)ContainEntry->tupleInmemolist,
						(Tuplestorestate *)aggstate->overlaptuplestorestate1,
						&iisempty,
						&entryNULL,
						distanceall,
						distanceMetric,
						numAttributes
						);

						if(maxcliquePTR==NULL||maxcliquePTR->member_len<=0)
						{
							maxcliquePTR=NULL;
							delete_oneGroup_nohash(maxcliquePTR);
						}else
						{
							tmpindex++;
						}

						//printf("find intersetion done \n");

					}else
					{
						if(tmpindex==1&&putdoor==true)
						{
							Tuplestorestate *tuplestorestate;
							tuplestorestate=(Tuplestorestate *)aggstate->overlaptuplestorestate1;
							if(tuplestorestate){
								tuplestore_puttupleslot(tuplestorestate, inputslot);
							}
							putdoor=false;
						}

						findIntersection_putIntoMemoList_noreturn
						(
							inputslot,
							aggstate->modifiedSlot,
							aggstate->pivot1Slot,
							(tmpGroup *)ContainEntry->tuplestorestate,
							(Tuplestorestate *)ContainEntry->tupleInmemolist,
							(Tuplestorestate *)aggstate->overlaptuplestorestate1,
							&iisempty,
							&entryNULL,
							distanceall,
							distanceMetric,
							numAttributes
						);

						if(iisempty==false)
						{
							//printf("find intersection done !!!! \n");
							tmpindex++;
						}

						//delete this tuple from the tree
					}

					 if(entryNULL==true)
					  {

						ContainEntry->flagEnabled=3;
						delete_tmpGroup((tmpGroup *)ContainEntry->tuplestorestate);
					  }
					 //printf("one iteration \n");

					memcpy(&(aggstate->hasharray[index_hashID-1].entry), &(ContainEntry), sizeof(AggHashEntry));

				}//for
			}
			/*********************************/

			//numcontain=0 ->no intersection with the exist group and tmpindex==0->can not join in any groups
		    if(numcontain==0&&tmpindex==0)
				{

				//printf("step3: build new group here \n");
				/*************************************************************/
				if(numAttributes==1)
				{
					values[0] = (rand()%50000000)+aggstate->numbertuples+rand()%20000000;
					replace[0]=true;
				}else if (numAttributes==2)
				{
					values[0] = (rand()%5000000)+aggstate->numbertuples;
					replace[0]=true;
					values[1] =  rand()%100000;
					replace[1]=true;
				}
				//printf("4135 \n");
				HeapTuple	newHeapTuple;/*CHANGED BY Mingjie*/
				newHeapTuple = heap_modify_tuple(inputslot->tts_tuple, inputslot->tts_tupleDescriptor, values, nulls, replace);
				ExecStoreTuple(newHeapTuple,aggstate->modifiedSlot,InvalidBuffer,true);/*This will free memory of previous tuple too*/
				AggHashEntry entry = lookup_hash_entry(aggstate, aggstate->modifiedSlot);/*YAS THIS SHOULD USE THE MODIFIED TUPLE*/

				if(entry->flagEnabled!=0)
					{
						return;
					}

				entry->flagEnabled=1;

				tmpGroup *tuplestorestatePTR;
				tuplestorestatePTR=(tmpGroup * )entry->tuplestorestate;

				if (tuplestorestatePTR)
				{
					tmpGroup_puttupleslot(tuplestorestatePTR, inputslot,numAttributes, true);
				};

				tuple *inputTuple_;
				inputTuple_ = (tuple *) palloc(sizeof(tuple));
				inputTuple_->numAttr = 2;
				copyfromTupleTableToArray(inputTuple_->values, inputTuple_->values,inputslot, numAttributes);
				convexhull * convex = (convexhull *) entry->convexhull;
				unionConvex(convex, inputTuple_, distanceall);

				copyfromTupleTableToArray(entry->pivot1Slot,entry->pivot2Slot,inputslot,numAttributes);
				updateInputSlotbyEPS(entry->pivot1Slot,entry->pivot2Slot, numAttributes, distanceall);
				/***********************************************/
				RTREEMBR insertMBR=getMBRHashEntry(entry,numAttributes);

				int result=RTreeInsert(
						  aggstate->rtreeroot,
						  &insertMBR,   /* the mbr being inserted */
						  (void*)(aggstate->indexhasharray+1),  /* i+1 is mbr ID. ID MUST NEVER BE ZERO */
						  0     /* always zero which means to add from the root */
						 );

				aggstate->indexhasharray++;
				ResetExprContext(tmpcontext);

				//can join in one group and no intersection with other group
				}else if(tmpindex==0&&numcontain==1)
				{

					if(updateEntry!=NULL&&updateEntry->flagEnabled == 1)
					{
						tmpGroup *tuplestorestatePTR;

						tuplestorestatePTR=(tmpGroup * )updateEntry->tuplestorestate;

						if (tuplestorestatePTR)
						{
							tmpGroup_puttupleslot(tuplestorestatePTR, inputslot,numAttributes, false);
						}

						memcpy(&(aggstate->hasharray[tmpHashID-1].entry), &(updateEntry), sizeof(AggHashEntry));
					}

				//numcontain=0 -> can not join in any exsit group, tmpindex==1 -> have overlap with one group
				}else
			if(tmpindex==1&&numcontain==0)
				{
					//need to build new groups and those groups have intersection with already group
					//build group in the candidatesIngroup
					tmpGroup *ptr;
					ptr=maxcliquePTR;

					int num_build=0;

					while(ptr!=NULL&&num_build<1)
					{
						if(ptr->isexisit==false)
						{
							if(numAttributes==1)
							{
								values[0] = (rand()%50000000)+aggstate->numbertuples+rand()%1000000;
								replace[0]=true;
							}else if (numAttributes==2)
							{
								values[0] = (rand()%50000000)+aggstate->numbertuples;
								replace[0]=true;
								values[1] =  rand()%10000;
								replace[1]=true;
							}

						HeapTuple	newHeapTuple;/*CHANGED BY Mingjie*/

						newHeapTuple = heap_modify_tuple(inputslot->tts_tuple, inputslot->tts_tupleDescriptor, values, nulls, replace);
						/* Initialize modifiedSlot by cloning outerslot */
						if (aggstate->modifiedSlot->tts_tupleDescriptor == NULL)
							ExecSetSlotDescriptor(aggstate->modifiedSlot, inputslot->tts_tupleDescriptor);

						ExecStoreTuple(newHeapTuple,aggstate->modifiedSlot,InvalidBuffer,true);/*This will free memory of previous tuple too*/
						//update the center poin;
						//print_slot(aggstate->modifiedSlot);

						if(ptr->member_len>0)
						{
							AggHashEntry entry=lookup_hash_entry(aggstate,aggstate->modifiedSlot);

							if(entry->flagEnabled!=0)
							{
								return;
							}

							//entry = lookup_hash_entry(aggstate, aggstate->modifiedSlot);/*YAS THIS SHOULD USE THE MODIFIED TUPLE*/
							entry->flagEnabled=1;

							tmpGroup *tuplestorestatePTR;
							tuplestorestatePTR=(tmpGroup * )entry->tuplestorestate;


							tuple *inputTuple_;
							convexhull * convex = (convexhull *) entry->convexhull;
							int tmpnumbindex=0;
							tuple *tmptuplegroupptr;
							tmptuplegroupptr=ptr->tuples_headPtr;

							while(tmpnumbindex<ptr->member_len&&tmptuplegroupptr!=NULL)
							{
								int judgeresult = 2;

								inputTuple_ = (tuple *) palloc(sizeof(tuple));
								inputTuple_->numAttr = 2;
								memcpy(inputTuple_->values,tmptuplegroupptr->values,sizeof(int)*numAttributes );

								judgeresult = insideConvexAndInsideBound(convex, inputTuple_,distanceall);

								if (judgeresult == 1) {
									unionConvex(convex, inputTuple_, distanceall);
								}else
								{
									pfree(inputTuple_);
								}

								tmptuplegroupptr=tmptuplegroupptr->next;
								tmpnumbindex++;
							}

							if (tuplestorestatePTR)
							{
								tmpGroup_puttupleslot(tuplestorestatePTR, inputslot,numAttributes, true);
							}

							int hashid;

							memcpy(entry->pivot1Slot, ptr->pivot1,sizeof(int)*numAttributes);
							memcpy(entry->pivot2Slot, ptr->pivot2,sizeof(int)*numAttributes);

							RTREEMBR insertMBR=getMBRHashEntry(entry,numAttributes);

							int result=RTreeInsert(aggstate->rtreeroot, &insertMBR,   /* the mbr being inserted */
									  (void*)(aggstate->indexhasharray+1),  /* i+1 is mbr ID. ID MUST NEVER BE ZERO */
									  0     /* always zero which means to add from the root */
									 );
							hashid=aggstate->indexhasharray+1;
							//printf("insert the MBR with id %d and insert result is %d \n", aggstate->indexhasharray+1, result);
							aggstate->indexhasharray++;

							memcpy(&(aggstate->hasharray[hashid-1].entry), &(entry), sizeof(AggHashEntry));
							//aggstate->hasharray[hashid-1].entry=entry;

							}//if


						}//if this is new group

						ptr=ptr->next;
					}

				// printf("Step4 : ##################&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& finish build new overlap group \n");

				}

			//insert the group back to the rtree
			if(numcontain>0)
			{
				//printf("insert the old group back to the rTree \n");
				int i=0;
				for(;i<index_contain;i++)
				{
					int index_hashID=containArray[i];
					AggHashEntry ContainEntry=aggstate->hasharray[index_hashID-1].entry;

					if(ContainEntry==NULL)
						{
						getTableSlot(aggstate->hasharray[index_hashID-1].inputslots,numAttributes,inputslot,aggstate->modifiedSlot);
						ContainEntry=lookup_hash_entry(aggstate, aggstate->modifiedSlot);
						}

						if(ContainEntry->ismodified==true)
						{
							RTREEMBR newMBR=
							getMBRHashEntry(ContainEntry,numAttributes);

							RTreeInsert(aggstate->rtreeroot, &newMBR,   /* the mbr being inserted */
							  (void*)(index_hashID),  /* i+1 is mbr ID. ID MUST NEVER BE ZERO */
							  0     /* always zero which means to add from the root */
							 );

							ContainEntry->ismodified=false;

						}else
						{
							continue;
						}

				}

			}
					/**************************************************************************/

				/* Initialize to walk the hash table */

				/*bool finishProcessing = false;
				ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);
				while (!finishProcessing)
						{

					AggHashEntry entry = (AggHashEntry) ScanTupleHashTable(&aggstate->hashiter);

					if (entry == NULL)
					{
						finishProcessing = true;
					}else if(entry->flagEnabled == 1)
					{
						//printGroup((Tuplestorestate *)entry->tuplestorestate, aggstate->modifiedSlot);
						print_groups((tmpGroup *)entry->tuplestorestate, numAttributes);

					 	printf("convex hull is \n");
						print_tuples(((convexhull *) entry->convexhull)->convexheader);
					}

				}

				printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!end to operation for this tuple \n");*/

				if(maxcliquePTR!=NULL)
					delete_oneGroup_nohash(maxcliquePTR);

				pfree(values);
				pfree(replace);
				pfree(nulls);
				ResetExprContext(tmpcontext);

}

static void
agg_fill_hash_table_overlap_newgroup_l1(AggState *aggstate,
		TupleTableSlot *inputslot) {

	Agg		   *node = (Agg *) aggstate->ss.ps.plan;
			ExprContext *tmpcontext;
			tmpcontext = aggstate->tmpcontext; /*tmpcontext is the per-input-tuple expression context */
												/* set up for advance_aggregates call */
			tmpcontext->ecxt_scantuple = inputslot; /*advance_aggregates should used riginal tuple*/

			/*EPS in the SGB-All */
			int distanceall;
			Datum distanceallDatum;
			distanceall = node->distanceall;

			int distanceMetric;
			distanceMetric=node->metric;/*(l2)distanceMetric=0, (l1)distanceMetric=1, (linfinity)distanceMetric=2*/


			int numAttributes=node->numCols; //we assume the group attribute are the first two column

			Datum		*values = (Datum *) palloc(numAttributes * sizeof(Datum)); /*array*/
			bool * nulls = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
			bool * replace = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
			memset(values, 0, numAttributes * sizeof(Datum));
			memset(nulls, false, numAttributes * sizeof(bool));
			memset(replace, false, numAttributes * sizeof(bool));

			if (aggstate->modifiedSlot->tts_tupleDescriptor == NULL)
				{
					ExecSetSlotDescriptor(aggstate->modifiedSlot,inputslot->tts_tupleDescriptor);
				}

			//printf("begin to process this tuple \n");
			//print_slot(inputslot);

			tmpGroup *maxcliquePTR=NULL;

			//clock_t start = clock();

			initilList();
			RTREEMBR searchMBR=
					getMBRinputSlot(inputslot,numAttributes);

			bool localcontain=true;
			iscontain_M=localcontain;
			int	 nhits =0;
			bool isoverlaprandom=false;

			//printf("Begin search tree \n");
			nhits=RTreeSearch(aggstate->rtreeroot,
							&searchMBR,
							MySearchCallback,
							localcontain,
							isoverlaprandom);
			//printf("Begin search tree \n");
			AggHashEntry tmpEntry;
			int tmpHashID=0;

			AggHashEntry updateEntry;
			int updateHashID=0;

			//printf("number of hits in contain is %d and index contain %d \n", nhits, index_contain);

			//for the group in the

			int numcontain=0;

			if(nhits>0)
			{
				int i=0;
				for(;i<index_contain;i++)
					{

					//printf("Step1: !!!!!!!!!!!!!!!!this tuple can join this group %d, %d \n", i, index_contain);

					int index_hashID=containArray[i];

					AggHashEntry ContainEntry=aggstate->hasharray[index_hashID-1].entry;

					if(ContainEntry==NULL)
							{
							//printf("original error here 1 %d \n", index_hashID);
							getTableSlot(aggstate->hasharray[index_hashID-1].inputslots, numAttributes,inputslot,aggstate->modifiedSlot);
							bool isnew=false;
							isnew=lookup_hash_entry_group(aggstate, aggstate->modifiedSlot,isnew);

							if(isnew==false)
							{
								ContainEntry=lookup_hash_entry(aggstate,aggstate->modifiedSlot);
							}else
							{
								//printf("error here to find the hash table \n");
								continue;
							}

							//continue;
							}

					bool overlap;

					int tmp_pivot1[5];
					int tmp_pivot2[5];

					memcpy(tmp_pivot1,ContainEntry->pivot1Slot, sizeof(int)*numAttributes );
					memcpy(tmp_pivot2,ContainEntry->pivot2Slot, sizeof(int)*numAttributes );

					if(judgeJoinGroup(
						numAttributes,
						tmp_pivot1,
						tmp_pivot2,
						inputslot,
						distanceall,
						distanceMetric,
						&overlap,
						tmpcontext->ecxt_per_tuple_memory)
						)
					{

						//printf("this tuple can join this group \n");

						RTREEMBR deleteMBR=
						getMBRHashEntry(ContainEntry,numAttributes);

						int result=RTreeDelete(aggstate->rtreeroot,
						&deleteMBR,
						(void*)(index_hashID)
						);

						numcontain++;
						memcpy(ContainEntry->pivot1Slot,tmp_pivot1, sizeof(int)*numAttributes );
						memcpy(ContainEntry->pivot2Slot,tmp_pivot2, sizeof(int)*numAttributes );

						ContainEntry->ismodified=true;
						//printf("can update and pivot1 %d, %d \n",ContainEntry->pivot1Slot[0], ContainEntry->pivot1Slot[1]);

					}else
					{
						continue;
					}

					if(i==0)
					{
						updateEntry=ContainEntry;
						updateHashID=index_hashID;
					}

					if(i==1)
					{
					Tuplestorestate *tuplestorestate;
					tuplestorestate=(Tuplestorestate *)aggstate->overlaptuplestorestate1;

					if(tuplestorestate)
						tuplestore_puttupleslot(tuplestorestate, inputslot);
					}

					memcpy(&(aggstate->hasharray[index_hashID-1].entry), &(ContainEntry), sizeof(AggHashEntry));

					}

			}

			/*******************************************************************************************/
			RTREEMBR searchMBR2=
					getMBRinputSlotBigger(inputslot,numAttributes, distanceall);

			localcontain=false;
			iscontain_M=localcontain;
			int nhits2=0;
			nhits2 =RTreeSearch(
							aggstate->rtreeroot,
							&searchMBR2,
							MySearchCallback,
							localcontain,
							isoverlaprandom);

			int tmpindex=0;

			//printf("number of overlap is %d and overlap number %d \n", nhits2, index_overlap);

			if(nhits2>0)
			{
				int i=0;
				bool putdoor=true;

				for(;i<index_overlap;i++)
				{
					int index_hashID=overlapArray[i];
					AggHashEntry ContainEntry=aggstate->hasharray[index_hashID-1].entry;

					//printf("Step2.1: hashID is %d, and size of hashtable is %d\n", index_hashID, aggstate->indexhasharray);
					//iterate the tuplestorestate
					if(ContainEntry==NULL)
					{
						getTableSlot(aggstate->hasharray[index_hashID-1].inputslots, numAttributes,inputslot,aggstate->modifiedSlot);

						bool isnew=false;
						isnew=lookup_hash_entry_group(aggstate, aggstate->modifiedSlot,isnew);

						if(isnew==false)
						{
							ContainEntry=lookup_hash_entry(aggstate,aggstate->modifiedSlot);
						}else
						{
							//printf("error here to find the hash table \n");
							continue;
						}

					}

					if(ContainEntry->flagEnabled==0||ContainEntry->flagEnabled==3)
					{
						continue;
					}

					bool iisempty=true;

					bool entryNULL=false;

					if(tmpindex<1)
					{
						//printf("Step2:!!!!!!!!!!!!!!!!begin to have overlap first group \n");
						//tmpGroup *tmpTupleStore;

						maxcliquePTR=findIntersection_putIntoMemoList(
						inputslot,
						aggstate->modifiedSlot,
						aggstate->pivot1Slot,
						(tmpGroup *)ContainEntry->tuplestorestate,
						(Tuplestorestate *)ContainEntry->tupleInmemolist,
						(Tuplestorestate *)aggstate->overlaptuplestorestate1,
						&iisempty,
						&entryNULL,
						distanceall,
						distanceMetric,
						numAttributes
						);

						if(maxcliquePTR==NULL||maxcliquePTR->member_len<=0)
						{
							maxcliquePTR=NULL;
							delete_oneGroup_nohash(maxcliquePTR);
						}else
						{
							//printf("find intersetion one \n");
							tmpindex++;
						}

					}else
					{
						if(i==1&&putdoor==true)
						{
							Tuplestorestate *tuplestorestate;
							tuplestorestate=(Tuplestorestate *)aggstate->overlaptuplestorestate1;
							if(tuplestorestate){
								tuplestore_puttupleslot(tuplestorestate, inputslot);
							}
							putdoor=false;
						}

						tmpGroup *tuplestorestate1=(tmpGroup *)ContainEntry->tuplestorestate;

						//printf("findIntersection_putIntoMemoList_noreturn\n");

						findIntersection_putIntoMemoList_noreturn
						(
							inputslot,
							aggstate->modifiedSlot,
							aggstate->pivot1Slot,
							(tmpGroup *)ContainEntry->tuplestorestate,
							(Tuplestorestate *)ContainEntry->tupleInmemolist,
							(Tuplestorestate *)aggstate->overlaptuplestorestate1,
							&iisempty,
							&entryNULL,
							distanceall,
							distanceMetric,
							numAttributes
						);

						if(iisempty==false)
						{
							//printf("find intersection done !!!! \n");
							tmpindex++;
						}

						//delete this tuple from the tree
					}

					 if(entryNULL==true)
					  {

						ContainEntry->flagEnabled=3;
						delete_tmpGroup((tmpGroup *)ContainEntry->tuplestorestate);

					}

					 //printf("one iteration \n");

					memcpy(&(aggstate->hasharray[index_hashID-1].entry), &(ContainEntry), sizeof(AggHashEntry));

				}//for
			}
			/*********************************/

			//nhits=0 ->no intersection with the exist group and tmpindex==0->can not join in any groups
		    if(numcontain==0&&tmpindex==0)
				{
				//printf("step3: build new group here \n");
				/*************************************************************/

				if(numAttributes==1)
				{
					values[0] = (rand()%50000000)+aggstate->numbertuples+rand()%20000000;
					replace[0]=true;
				}else if (numAttributes==2)
				{
					values[0] = (rand()%50000000)+aggstate->numbertuples;
					replace[0]=true;
					values[1] =  rand()%1000000;
					replace[1]=true;
				}
				//printf("4135 \n");
				HeapTuple	newHeapTuple;/*CHANGED BY Mingjie*/

				//newHeapTuple=heap_form_tuple(inputslot->tts_tupleDescriptor, values,nulls);

				newHeapTuple = heap_modify_tuple(inputslot->tts_tuple, inputslot->tts_tupleDescriptor, values, nulls, replace);

				ExecStoreTuple(newHeapTuple,aggstate->modifiedSlot,InvalidBuffer,true);/*This will free memory of previous tuple too*/

				AggHashEntry entry = lookup_hash_entry(aggstate, aggstate->modifiedSlot);/*YAS THIS SHOULD USE THE MODIFIED TUPLE*/

				if(entry->flagEnabled!=0)
					{
						return;
					}

				entry->flagEnabled=1;

				tmpGroup *tuplestorestatePTR;
				tuplestorestatePTR=(tmpGroup * )entry->tuplestorestate;

				if (tuplestorestatePTR)
				{
					tmpGroup_puttupleslot(tuplestorestatePTR, inputslot,numAttributes, true);
				};

				copyfromTupleTableToArray(entry->pivot1Slot,entry->pivot2Slot,inputslot,numAttributes);
				updateInputSlotbyEPS(entry->pivot1Slot,entry->pivot2Slot, numAttributes, distanceall);
				/***********************************************/
				RTREEMBR insertMBR=getMBRHashEntry(entry,numAttributes);

				int result=RTreeInsert(aggstate->rtreeroot, &insertMBR,   /* the mbr being inserted */
						  (void*)(aggstate->indexhasharray+1),  /* i+1 is mbr ID. ID MUST NEVER BE ZERO */
						  0     /* always zero which means to add from the root */
						 );

				aggstate->indexhasharray++;
				ResetExprContext(tmpcontext);

				//can join in one group and no intersection with other group
				}else
			  if(tmpindex==0&&numcontain==1)
				{

					if(updateEntry!=NULL&&updateEntry->flagEnabled == 1)
					{
						//printf("for overlap is 0 and can join only one group, this tuple will join the exisit group \n");
						tmpGroup *tuplestorestatePTR;

						tuplestorestatePTR=(tmpGroup * )updateEntry->tuplestorestate;

						if (tuplestorestatePTR)
						{
							tmpGroup_puttupleslot(tuplestorestatePTR, inputslot,numAttributes, false);
						}

						memcpy(&(aggstate->hasharray[tmpHashID-1].entry), &(updateEntry), sizeof(AggHashEntry));
					}

				//nhtis=0 -> can not join in any exsit group, tmpindex==1 -> have overlap with one group
				}
			  else if(tmpindex==1&&numcontain==0)
				{
					//need to build new groups and those groups have intersection with already group
					//build group in the candidatesIngroup
					tmpGroup *ptr;
					ptr=maxcliquePTR;

					int num_build=0;

					while(ptr!=NULL&&num_build<1)
					{
						if(ptr->isexisit==false)
						{

							if(numAttributes==1)
							{
								values[0] = (rand()%50000000)+aggstate->numbertuples+rand()%1000000;
								replace[0]=true;
							}else if (numAttributes==2)
							{
								values[0] = (rand()%50000000)+aggstate->numbertuples;
								replace[0]=true;
								values[1] =  rand()%1000000;
								replace[1]=true;
							}

						HeapTuple	newHeapTuple;/*CHANGED BY Mingjie*/

						newHeapTuple = heap_modify_tuple(inputslot->tts_tuple, inputslot->tts_tupleDescriptor, values, nulls, replace);
						/* Initialize modifiedSlot by cloning outerslot */
						if (aggstate->modifiedSlot->tts_tupleDescriptor == NULL)
							ExecSetSlotDescriptor(aggstate->modifiedSlot, inputslot->tts_tupleDescriptor);

						ExecStoreTuple(newHeapTuple,aggstate->modifiedSlot,InvalidBuffer,true);/*This will free memory of previous tuple too*/
						//update the center poin;
						//print_slot(aggstate->modifiedSlot);

						if(ptr->member_len>0)
						{

							//printf("step 4, to build new group which can not join in any and the overlap is 1 \n");

							AggHashEntry entry=lookup_hash_entry(aggstate,aggstate->modifiedSlot);

							if(entry->flagEnabled!=0)
										{
											return;
										}

							//entry = lookup_hash_entry(aggstate, aggstate->modifiedSlot);/*YAS THIS SHOULD USE THE MODIFIED TUPLE*/
							entry->flagEnabled=1;

							tmpGroup *tuplestorestatePTR;
							tuplestorestatePTR=(tmpGroup * )entry->tuplestorestate;

							if (tuplestorestatePTR)
							{
								tmpGroup_puttupleslot(tuplestorestatePTR, inputslot,numAttributes, true);
							}

							int j=0;
							tuple *tupleptr=ptr->tuples_headPtr;
							int hashid;
							memcpy(entry->pivot1Slot, ptr->pivot1,sizeof(int)*numAttributes);
							memcpy(entry->pivot2Slot, ptr->pivot2,sizeof(int)*numAttributes);

							//printf("pivot x %d and y %d \n",entry->pivot1Slot[0],entry->pivot2Slot[0]);

							//insert this rectangle into the Rtree
							RTREEMBR insertMBR=getMBRHashEntry(entry,numAttributes);
							int result=RTreeInsert(aggstate->rtreeroot, &insertMBR,   /* the mbr being inserted */
									  (void*)(aggstate->indexhasharray+1),  /* i+1 is mbr ID. ID MUST NEVER BE ZERO */
									  0     /* always zero which means to add from the root */
									 );
							hashid=aggstate->indexhasharray+1;
							//printf("insert the MBR with id %d and insert result is %d \n", aggstate->indexhasharray+1, result);
							aggstate->indexhasharray++;

							memcpy(&(aggstate->hasharray[hashid-1].entry), &(entry), sizeof(AggHashEntry));
							//aggstate->hasharray[hashid-1].entry=entry;

							}//if


						}//if this is new group

						ptr=ptr->next;
					}

				//printf("Step4 : ##################&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& finish build new overlap group \n");

				}

			//insert the group back to the rtree
			if(numcontain>0)
			{
				//printf("insert the old group back to the rTree \n");

				int i=0;
				for(;i<index_contain;i++)
				{
					int index_hashID=containArray[i];
					AggHashEntry ContainEntry=aggstate->hasharray[index_hashID-1].entry;

					if(ContainEntry==NULL)
						{

						getTableSlot(aggstate->hasharray[index_hashID-1].inputslots,numAttributes,inputslot,aggstate->modifiedSlot);
						ContainEntry=lookup_hash_entry(aggstate, aggstate->modifiedSlot);
						}

						bool overlap;
						if(ContainEntry->ismodified==true)
						{

							//printf("pivot x %d and y %d \n",ContainEntry->pivot1Slot[0],ContainEntry->pivot2Slot[0]);
							RTREEMBR newMBR=
								getMBRHashEntry(ContainEntry,numAttributes);

							//printf("update the index bound at the final step here %d \n", index_hashID);

							RTreeInsert(aggstate->rtreeroot, &newMBR,   /* the mbr being inserted */
								  (void*)(index_hashID),  /* i+1 is mbr ID. ID MUST NEVER BE ZERO */
								  0     /* always zero which means to add from the root */
								 );

							ContainEntry->ismodified=false;

						}else
						{
							continue;
						}

				}

			}
					/**************************************************************************/

				/* Initialize to walk the hash table */

				/*bool finishProcessing = false;
				ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);
				while (!finishProcessing)
						{

					AggHashEntry entry = (AggHashEntry) ScanTupleHashTable(&aggstate->hashiter);

					if (entry == NULL)
					{
						finishProcessing = true;
					}else if(entry->flagEnabled == 1)
					{
						//printGroup((Tuplestorestate *)entry->tuplestorestate, aggstate->modifiedSlot);
						print_groups((tmpGroup *)entry->tuplestorestate, numAttributes);
					}

				}*/

				//printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!end to operation for this tuple \n");

				//printf("!!!!!!!!!!!!!!!!!!!!!! bounds is %d \n", countBound);



				if(maxcliquePTR!=NULL)
					delete_oneGroup_nohash(maxcliquePTR);

				pfree(values);
				pfree(replace);
				pfree(nulls);
				ResetExprContext(tmpcontext);

}


static void sgb_newgroup_aggfunction(AggState *aggstate) {
	Agg *node = (Agg *) aggstate->ss.ps.plan;

	AggHashEntry entry;
	bool finishProcessing;
	finishProcessing = false;
	/* Initialize to walk the hash table */
	ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);

	//advanced agg for every hashentry
	while (!finishProcessing) {
		/*Find the next entry in the hash table*/
		entry = (AggHashEntry) ScanTupleHashTable(&aggstate->hashiter);

		if (entry == NULL )/* No more entries in hashtable, so done */
		{

			finishProcessing = true;

		} else if (entry->flagEnabled == 1) {
			tmpGroup *tmpstorestage;
			tmpstorestage = (tmpGroup *) entry->tuplestorestate;

			//printf("final results is \n");
			//print_groups(tmpstorestage);

			tuple *headerPTR = tmpstorestage->tuples_headPtr;
			ExprContext *tmpcontext;
			tmpcontext = aggstate->tmpcontext; /*tmpcontext is the per-input-tuple expression context */

			int j = 0;

			while (headerPTR != NULL ) {
				//printf("iteration time %d \n", j);
				puttuple_TupleTableSlot(headerPTR, aggstate->modifiedSlot,
						aggstate->pivot1Slot, node->numCols);

				tmpcontext->ecxt_scantuple = aggstate->modifiedSlot; /*advance_aggregates should used riginal tuple*/

				if (j == 0) {
					initialize_aggregates(aggstate, aggstate->peragg,
							entry->pergroup);
				}

				advance_aggregates(aggstate, entry->pergroup);
				headerPTR = headerPTR->next;
				j++;
			}

			entry->flagEnabled = 2;
			delete_tmpGroup(tmpstorestage);

		} else if (entry->flagEnabled == 3) {
			//remove this one from the final results
			entry->flagEnabled = 0;
		}

	}//while finish proceessing

}

static
void sgb_newgroup_iteration_l1(AggState *aggstate, TupleTableSlot *inputslot,
		int step)
{
		Tuplestorestate *globalmemolist;

		if (step == 1) {
			globalmemolist = (Tuplestorestate *) aggstate->overlaptuplestorestate1;
		} else if (step == 0) {
			globalmemolist = (Tuplestorestate *) aggstate->overlaptuplestorestate2;
		}

	Agg		   *node = (Agg *) aggstate->ss.ps.plan;
				ExprContext *tmpcontext;
				tmpcontext = aggstate->tmpcontext; /*tmpcontext is the per-input-tuple expression context */
													/* set up for advance_aggregates call */
				//inputslot->canuse=true;
				tmpcontext->ecxt_scantuple = inputslot; /*advance_aggregates should used riginal tuple*/

				/*EPS in the SGB-All */
				int distanceall;
				Datum distanceallDatum;
				distanceall = node->distanceall;
				distanceallDatum = Int32GetDatum(distanceallDatum);

				Datum		attrPivot1,attrPivot2,attrCurrent;
				bool		isNull1,isNull2,isNull3;
				FmgrInfo *ltfunctions;
				FmgrInfo *minusfunctions;

				int numAttributes=node->numCols; //we assume the group attribute are the first two column

				Datum		*values = (Datum *) palloc(numAttributes * sizeof(Datum)); /*array*/
				bool * nulls = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
				bool * replace = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
				memset(values, 0, numAttributes * sizeof(Datum));
				memset(nulls, false, numAttributes * sizeof(bool));
				memset(replace, false, numAttributes * sizeof(bool));


				//assum the max groups that tuple can join

				//printf("begin to process this tuple \n");
				//print_slot(inputslot);

				tmpGroup *maxcliquePTR=constructGroup(numAttributes,rand()%10000,rand()%10000,false); /*two D-array*/;

				int distanceMetric;
				distanceMetric=1;/*(l2)distanceMetric=0, (l1)distanceMetric=1, (linfinity)distanceMetric=2*/


					AggHashEntry entry;
					AggHashEntry entryTmp=NULL;
					bool finishProcessing;
					finishProcessing = false;

					/* Initialize to walk the hash table */
					ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);

					bool contain;
					contain=true;

					int tmpindex=0;
					int tmpindex2=0; //use this to indicate whether there is intersection
					bool putdoor=true;
					/**************************************************************************/
						while (!finishProcessing)
						{
							/*Find the next entry in the hash table*/
							entry = (AggHashEntry) ScanTupleHashTable(&aggstate->hashiter);
							bool overlap;
							overlap=false;

							if (entry == NULL)/* No more entries in hashtable, so done */
								{

								finishProcessing = true;

								}else if(entry->flagEnabled == 1||entry->flagEnabled == 3)
								{
								//this is old groups
								/*judge whether this tuple can join this group*/

									if(judgeJoinGroup(
												numAttributes,
												entry->pivot1Slot,
												entry->pivot2Slot,
												inputslot,
												distanceall,
												distanceMetric,
												&overlap,
												tmpcontext->ecxt_per_tuple_memory
												)
											 )
											{
												contain=true;
											}else
											{
												contain=false;
											}

								/*if can join, update the agg function*/
								if(contain)
								{
									//printf("Step1: !!!!!!!!!!!!this tuple can join this group\n");

									if(tmpindex==0)
									{
										entryTmp=entry;

									}else if(tmpindex==1)
									{

										Tuplestorestate *tuplestorestate;

										tuplestorestate=(Tuplestorestate *)entry->tupleInmemolist;
										if(tuplestorestate)
												tuplestore_puttupleslot(tuplestorestate, inputslot);

										//tuplestorestate=(Tuplestorestate *)aggstate->overlaptuplestorestate1;
										//if(tuplestorestate)
											//tuplestore_puttupleslot(tuplestorestate, inputslot);

										if (globalmemolist)
												tuplestore_puttupleslot(globalmemolist, inputslot);
									}

									tmpindex++;

								}else //can not join this group
								{
									if(entry->flagEnabled == 3)
									{
										continue;
									}

									//if there is overlap between new tuple and the exist group
									if(overlap)
									{
										//Tuplestorestate *tuplestorestate;
										//tuplestorestate=(Tuplestorestate *)aggstate->overlaptuplestorestate1;

										//iterate the tuplestorestate
										//printf("Step2.1 : ##################&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& find intersection \n");
										if(tmpindex2==0)
										{
											//printf("Step2:!!!!!!!!!!!!!!!!begin to have overlap first group \n");
											tmpGroup *tmpTupleStore;
											bool iisempty=true;
											bool entryNULL=false;

											//tuplestorestate=(Tuplestorestate *)entry->tuplestorestate;
											tmpTupleStore=findIntersection_putIntoMemoList(
											inputslot,
											aggstate->modifiedSlot,
											aggstate->pivot1Slot,
											(tmpGroup *)entry->tuplestorestate,
											(Tuplestorestate *)entry->tupleInmemolist,
											globalmemolist,
											&iisempty,
											&entryNULL,
											distanceall,
											distanceMetric,
											numAttributes
											);

											if (entryNULL == true ) {
											entry->flagEnabled = 3;
											delete_tmpGroup((tmpGroup *) entry->tuplestorestate);
											//printf("delete this group \n");
											}

											if(tmpTupleStore->member_len<=0)
											{
												delete_tmpGroup(tmpTupleStore);
												continue;
											}

											if(Judge_and_Join_GLIST(tmpTupleStore,maxcliquePTR,distanceall)==false)
											{
												delete_tmpGroup(tmpTupleStore);

												if(maxcliquePTR->member_len>0)
												{
													tmpindex2++;
												}
											}

										}else if(tmpindex2>=1)
										{
											if(tmpindex2>aggstate->numbertuples)
											{
												break;
											}

											if(tmpindex2==1&&putdoor==true)
												{
														if (globalmemolist) {
																tuplestore_puttupleslot(globalmemolist,inputslot);
															}

													putdoor=false;
												}

											//printf("Step2.1:!!!!!!!!!!!!!!!!findIntersection_putIntoMemoList_noreturn  \n");
											//update the tuple store

											bool iisempty=true;

											bool entryNULL=false;

											tmpGroup *tuplestorestate1=(tmpGroup *)entry->tuplestorestate;

											findIntersection_putIntoMemoList_noreturn
											(
												inputslot,
												aggstate->modifiedSlot,
												aggstate->pivot1Slot,
												(tmpGroup *)entry->tuplestorestate,
												(Tuplestorestate *)entry->tupleInmemolist,
												globalmemolist,
												&iisempty,
												&entryNULL,
												distanceall,
												distanceMetric,
												numAttributes
											);

											if(iisempty==false)
											{
												tmpindex2++;
												//printf("intersection number is not null \n");
											}

											if(entryNULL==true)
											{
												//printf("this group died \n");
												entry->flagEnabled=3;
												delete_tmpGroup((tmpGroup *)entry->tuplestorestate);
											}

										}

									   //print_groups(tmpTupleStore);
									}//overlap

								}//handle can no join this group
								//countBound++;
							}
						}//while hash map

					/***************************************************************/
					//case 1
				    //can no join in old groups and no intersection with any old groups
						if(tmpindex==0&&tmpindex2==0)
					{

						 if(numAttributes==1)
						{
							values[0] = (rand()%70000000)+aggstate->numbertuples+rand()%20000000;
							replace[0]=true;
						}else if (numAttributes==2)
						{
							values[0] = (rand()%50000000)+aggstate->numbertuples;
							replace[0]=true;
							values[1] =  rand()%10000;
							replace[1]=true;
						}

						 HeapTuple	newHeapTuple;/*CHANGED BY Mingjie*/
						 newHeapTuple = heap_modify_tuple(inputslot->tts_tuple, inputslot->tts_tupleDescriptor, values, nulls, replace);

						 ExecStoreTuple(newHeapTuple,aggstate->modifiedSlot,InvalidBuffer,true);/*This will free memory of previous tuple too*/

						 AggHashEntry entry = lookup_hash_entry(aggstate, aggstate->modifiedSlot);/*YAS THIS SHOULD USE THE MODIFIED TUPLE*/

					if(entry->flagEnabled!=0)
						{
							return;
						}

						entry->flagEnabled=1;
						//Tuplestorestate *tuplestorestate;

						tmpGroup *tuplestorestatePTR;
						tuplestorestatePTR=(tmpGroup * )entry->tuplestorestate;
						if (tuplestorestatePTR)
						{
							tmpGroup_puttupleslot(tuplestorestatePTR, inputslot,numAttributes, true);
						}

						//copy value to pivot
						copyfromTupleTableToArray(entry->pivot1Slot,entry->pivot2Slot,inputslot,numAttributes);
						//copyfromTupleTableToArray(pivot1Slot1,inputslot,numAttributes);
						updateInputSlotbyEPS(entry->pivot1Slot,entry->pivot2Slot, numAttributes, distanceall);
						ResetExprContext(tmpcontext);
						//
					}else if(tmpindex==1&&tmpindex2==0)
					{
						Tuplestorestate *tuplestorestate;
						if(entryTmp!=NULL&&entryTmp->flagEnabled == 1)
						{
							//printf("this tuple will join the exisit group \n");
							tmpGroup *tuplestorestatePTR;

							tuplestorestatePTR=(tmpGroup * )entryTmp->tuplestorestate;

							if (tuplestorestatePTR)
							{
								tmpGroup_puttupleslot(tuplestorestatePTR, inputslot,numAttributes, false);
							}

						}

					}else if(tmpindex2==1&&tmpindex==0)
					//only one overlap group and build the new group for this tuple, and its bounds is intersection with the one group
					{
						tmpGroup *ptr;
						ptr=maxcliquePTR;
						bool firstone=true;

						while(ptr!=NULL&&firstone)
								{

								if(ptr->isexisit==false)
									{

									if(numAttributes==1)
								{
									values[0] = (rand()%70000000)+aggstate->numbertuples+rand()%20000000;
									replace[0]=true;
									}else if (numAttributes==2)
								{
									values[0] = (rand()%50000000)+aggstate->numbertuples;
									replace[0]=true;
									values[1] =  rand()%10000;
									replace[1]=true;
								}

									HeapTuple	newHeapTuple;/*CHANGED BY Mingjie*/
									newHeapTuple = heap_modify_tuple(inputslot->tts_tuple, inputslot->tts_tupleDescriptor, values, nulls, replace);

									if (aggstate->modifiedSlot->tts_tupleDescriptor == NULL)
										ExecSetSlotDescriptor(aggstate->modifiedSlot, inputslot->tts_tupleDescriptor);

									ExecStoreTuple(newHeapTuple,aggstate->modifiedSlot,InvalidBuffer,true);/*This will free memory of previous tuple too*/

									if(ptr->member_len>0)
									{
										entry = lookup_hash_entry(aggstate, aggstate->modifiedSlot);/*YAS THIS SHOULD USE THE MODIFIED TUPLE*/
										if(entry->flagEnabled!=0)
										{
											return;
										}

										entry->flagEnabled=1;

										tmpGroup *tuplestorestatePTR;
										tuplestorestatePTR=(tmpGroup * )entry->tuplestorestate;
										if (tuplestorestatePTR)
										{
											tmpGroup_puttupleslot(tuplestorestatePTR, inputslot,numAttributes, true);
										}

										memcpy(entry->pivot1Slot, ptr->pivot1,sizeof(int)*numAttributes);
										memcpy(entry->pivot2Slot, ptr->pivot2,sizeof(int)*numAttributes);
										firstone=false;
									}//if
								}//if this is new group
								ptr=ptr->next;

							}

					}//if

			/* Initialize to walk the hash table */

			 /*finishProcessing = false;
			 ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);
			 while (!finishProcessing)
			 {

			 entry = (AggHashEntry) ScanTupleHashTable(&aggstate->hashiter);

			 if (entry == NULL)
			 {
			 finishProcessing = true;
			 }else if(entry->flagEnabled == 1)
			 {
			 //printGroup((Tuplestorestate *)entry->tuplestorestate, aggstate->modifiedSlot);
				 print_groups((tmpGroup *)entry->tuplestorestate,2);

			 	 //printf("convex hull is \n");
			 	 //print_tuples(((convexhull *) entry->convexhull)->convexheader);
			 }

			 }

			printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!end to operation for this tuple \n");*/

					delete_tmpGroup(maxcliquePTR);
					pfree(values);
					pfree(replace);
					pfree(nulls);
					ResetExprContext(tmpcontext);

}


static void sgb_newgroup_iteration_l2(AggState *aggstate, TupleTableSlot *inputslot,
		int step) {

	Tuplestorestate *globalmemolist;

	if (step == 1) {
		globalmemolist = (Tuplestorestate *) aggstate->overlaptuplestorestate1;
	} else if (step == 0) {
		globalmemolist = (Tuplestorestate *) aggstate->overlaptuplestorestate2;
	}

	Agg *node = (Agg *) aggstate->ss.ps.plan;
		ExprContext *tmpcontext;
		tmpcontext = aggstate->tmpcontext; /*tmpcontext is the per-input-tuple expression context */
		/* set up for advance_aggregates call */
		tmpcontext->ecxt_scantuple = inputslot; /*advance_aggregates should used riginal tuple*/

		/*EPS in the SGB-All */
		int distanceall;
		Datum distanceallDatum;
		distanceall = node->distanceall;
		distanceallDatum = Int32GetDatum(distanceallDatum);

		Datum attrPivot1, attrPivot2, attrCurrent;
		bool isNull1, isNull2, isNull3;
		FmgrInfo *ltfunctions;
		FmgrInfo *minusfunctions;

		int numAttributes = node->numCols; //we assume the group attribute are the first two column

		Datum *values = (Datum *) palloc(numAttributes * sizeof(Datum)); /*array*/
		bool * nulls = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
		bool * replace = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
		memset(values, 0, numAttributes * sizeof(Datum));
		memset(nulls, false, numAttributes * sizeof(bool));
		memset(replace, false, numAttributes * sizeof(bool));
		//printf("begin to process this tuple \n");
		//print_slot(inputslot);

		tmpGroup *maxcliquePTR = constructGroup(numAttributes, rand() % 10000,rand() % 10000, false ); /*two D-array*/

		int distanceMetric;
		distanceMetric = 2;/*(l2)distanceMetric=0, (l1)distanceMetric=1, (linfinity)distanceMetric=2*/

		/*CHANGED BY Mingjie*/
		/*find all the hash entry(group) that were in that group bounds*/
		/*************************************************************************/
		AggHashEntry entry;
		AggHashEntry entryTmp = NULL;
		bool finishProcessing;
		finishProcessing = false;

		/* Initialize to walk the hash table */
		ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);

		bool contain;
		contain = true;

		int tmpindex = 0;
		int tmpindex2 = 0; //use this to indicate whether there is intersection
		bool putdoor = true;
		/**************************************************************************/
		while (!finishProcessing) {
			/*Find the next entry in the hash table*/
			entry = (AggHashEntry) ScanTupleHashTable(&aggstate->hashiter);
			bool overlap;
			overlap = false;

			if (entry == NULL )/* No more entries in hashtable, so done */
			{
				finishProcessing = true;

			} else if (entry->flagEnabled == 1 || entry->flagEnabled == 3) {

				//this is old groups
				/*judge whether this tuple can join this group*/

				if (judgeJoinGroup(numAttributes,
						entry->pivot1Slot,
						entry->pivot2Slot,
						inputslot, distanceall, distanceMetric,
						&overlap,
						tmpcontext->ecxt_per_tuple_memory))
				{
					contain = true;
				} else {
					contain = false;
				}

				int judgeresult = 2;

				if(contain==true)
				{
					tuple *inputTuple_;
					inputTuple_ = (tuple *) palloc(sizeof(tuple));
					inputTuple_->numAttr = 2;
					copyfromTupleTableToArray(inputTuple_->values, inputTuple_->values,
							inputslot, numAttributes);

					convexhull * convex = (convexhull *) entry->convexhull;
					judgeresult = insideConvexAndInsideBound(convex, inputTuple_,
							distanceall);

					//printf("judge result is %d \n", judgeresult);

					if (judgeresult == 1) {
						unionConvex(convex, inputTuple_, distanceall);

					} else {
						pfree(inputTuple_);
					}

				}

				/*if can join, update the agg function*/
				if (judgeresult<=1) {

					 //printf("Step1: !!!!!!!!!!!!this tuple can join this group\n");

					if (tmpindex == 0) {
						entryTmp = entry;

					} else if (tmpindex == 1) {

						//printf("Step1.2: !!!!!!!!!!!!this tuple can join more than one group\n");

						Tuplestorestate *tuplestorestate;

						tuplestorestate =
								(Tuplestorestate *) entry->tupleInmemolist;
						if (tuplestorestate)
							tuplestore_puttupleslot(tuplestorestate, inputslot);

						if (globalmemolist)
							tuplestore_puttupleslot(globalmemolist, inputslot);
					}
					tmpindex++;

				} else //can not join this group
				{
					if (entry->flagEnabled == 3) {
						continue;
					}

					if(overlap)
					{
						tuple *inputTuple_;
						inputTuple_ = (tuple *) palloc(sizeof(tuple));
						inputTuple_->numAttr = 2;
						copyfromTupleTableToArray(inputTuple_->values, inputTuple_->values,
								inputslot, numAttributes);

						convexhull * convex = (convexhull *) entry->convexhull;
						judgeresult = insideConvexAndInsideBound(convex, inputTuple_,
								distanceall);

						//printf("judge result need to be three or two %d \n", judgeresult);

						pfree(inputTuple_);
					}

					//if there is overlap between new tuple and the exist group
					if (judgeresult==3) {

						//printf("Step2:!!!!!!!!!!!!!!!!begin to have overlap \n");

						if (tmpindex2 == 0) {
							tmpGroup *tmpTupleStore;
							bool iisempty = true;
							bool entryNULL = false;

							//tuplestorestate=(Tuplestorestate *)entry->tuplestorestate;
							tmpTupleStore =findIntersection_putIntoMemoList(inputslot,
											aggstate->modifiedSlot,
											aggstate->pivot1Slot,
											(tmpGroup *) entry->tuplestorestate,
											(Tuplestorestate *) entry->tupleInmemolist,
											globalmemolist,
											&iisempty,
											&entryNULL,
											distanceall,
											distanceMetric,
											numAttributes);

							if (entryNULL == true ) {
								entry->flagEnabled = 3;
								delete_tmpGroup((tmpGroup *) entry->tuplestorestate);
								//printf("delete this group \n");
								}

							if (tmpTupleStore->member_len <= 0) {

								delete_tmpGroup(tmpTupleStore);
								continue;
							}

							if (Judge_and_Join_GLIST(tmpTupleStore, maxcliquePTR,distanceall) == false ) {

								delete_tmpGroup(tmpTupleStore);

								if (maxcliquePTR->member_len > 0) {
									tmpindex2++;
								}
							}


						} else if (tmpindex2 >= 1) {

							if (tmpindex2 > aggstate->numbertuples) {
								break;
							}

							if (tmpindex2 == 1 && putdoor == true ) {

								if (globalmemolist) {
									//printf("put tuple into door here 3820 \n");
									tuplestore_puttupleslot(globalmemolist,
											inputslot);
								}
								putdoor = false;
							}

							bool iisempty = true;

							bool entryNULL = false;

							tmpGroup *tuplestorestate1 =
									(tmpGroup *) entry->tuplestorestate;

							findIntersection_putIntoMemoList_noreturn(
									inputslot,
									aggstate->modifiedSlot, aggstate->pivot1Slot,
									(tmpGroup *) entry->tuplestorestate,
									(Tuplestorestate *) entry->tupleInmemolist,
									globalmemolist,
									&iisempty,
									&entryNULL,
									distanceall,
									distanceMetric, numAttributes);

							if (iisempty == false ) {
								tmpindex2++;
								//printf("intersection number is not null \n");
							}

							if (entryNULL == true ) {
								//printf("this group died \n");
								entry->flagEnabled = 3;
								delete_tmpGroup((tmpGroup *) entry->tuplestorestate);
							}

						}

						//print_groups(tmpTupleStore);
					}									//overlap

				}									//handle can no join this group
													//countBound++;
			}
		}						//while hash map

		/***************************************************************/
		//case 1
		//can no join in old groups and no intersection with any old groups
		if (tmpindex == 0 && tmpindex2 == 0) {

			if (numAttributes == 1) {
				values[0] = (rand() % 70000000) + aggstate->numbertuples+ rand() % 20000000;
				replace[0] = true;
			} else if (numAttributes == 2) {
				values[0] = (rand() % 50000000) + aggstate->numbertuples;
				replace[0] = true;
				values[1] = rand() % 10000;
				replace[1] = true;
			}

			HeapTuple newHeapTuple;/*CHANGED BY Mingjie*/
			newHeapTuple = heap_modify_tuple(inputslot->tts_tuple,inputslot->tts_tupleDescriptor, values, nulls, replace);

			ExecStoreTuple(newHeapTuple, aggstate->modifiedSlot, InvalidBuffer,true );/*This will free memory of previous tuple too*/

			AggHashEntry entry = lookup_hash_entry(aggstate, aggstate->modifiedSlot);/*YAS THIS SHOULD USE THE MODIFIED TUPLE*/

			if (entry->flagEnabled != 0) {
				return;
			}

			entry->flagEnabled = 1;
			//Tuplestorestate *tuplestorestate;

			tuple *inputTuple_;
			inputTuple_ = (tuple *) palloc(sizeof(tuple));
			inputTuple_->numAttr = 2;
			copyfromTupleTableToArray(inputTuple_->values, inputTuple_->values,inputslot, numAttributes);

			convexhull * convex = (convexhull *) entry->convexhull;

			unionConvex(convex, inputTuple_, distanceall);

			tmpGroup *tuplestorestatePTR;
			tuplestorestatePTR = (tmpGroup *) entry->tuplestorestate;
			if (tuplestorestatePTR) {
				tmpGroup_puttupleslot(tuplestorestatePTR, inputslot, numAttributes,
						true );
			}

			copyfromTupleTableToArray(entry->pivot1Slot, entry->pivot2Slot,inputslot, numAttributes);

			updateInputSlotbyEPS(entry->pivot1Slot, entry->pivot2Slot,numAttributes, distanceall);

			ResetExprContext(tmpcontext);
			//
		} else if (tmpindex == 1 && tmpindex2 == 0) {
			Tuplestorestate *tuplestorestate;
			if (entryTmp != NULL && entryTmp->flagEnabled == 1) {
				//printf("this tuple will join the exisit group \n");
				tmpGroup *tuplestorestatePTR;

				tuplestorestatePTR = (tmpGroup *) entryTmp->tuplestorestate;

				if (tuplestorestatePTR) {
					tmpGroup_puttupleslot(tuplestorestatePTR, inputslot,
							numAttributes, false );
				}

			}

		} else if (tmpindex2 == 1 && tmpindex == 0)
		//only one overlap group and build the new group for this tuple, and its bounds is intersection with the one group
				{
				tmpGroup *ptr;
				ptr = maxcliquePTR;
				bool firstone = true;

				while (ptr != NULL && firstone) {

				if (ptr->isexisit == false ) {

					if (numAttributes == 1) {
						values[0] = (rand() % 70000000) + aggstate->numbertuples+ rand() % 20000000;
						replace[0] = true;
					} else if (numAttributes == 2) {
						values[0] = (rand() % 50000000) + aggstate->numbertuples;
						replace[0] = true;
						values[1] = rand() % 10000;
						replace[1] = true;
					}

					HeapTuple newHeapTuple;/*CHANGED BY Mingjie*/
					newHeapTuple = heap_modify_tuple(inputslot->tts_tuple,
							inputslot->tts_tupleDescriptor, values, nulls, replace);

					if (aggstate->modifiedSlot->tts_tupleDescriptor == NULL )
						ExecSetSlotDescriptor(aggstate->modifiedSlot,
								inputslot->tts_tupleDescriptor);

					ExecStoreTuple(newHeapTuple, aggstate->modifiedSlot,
							InvalidBuffer, true );/*This will free memory of previous tuple too*/

					//update the center poin;
					//print_slot(aggstate->modifiedSlot);

					//printf("Step4: build new overlap group\n");

					if (ptr->member_len > 0) {

						entry = lookup_hash_entry(aggstate, aggstate->modifiedSlot);/*YAS THIS SHOULD USE THE MODIFIED TUPLE*/

						if (entry->flagEnabled != 0) {
							return;
						}

						entry->flagEnabled = 1;

						tmpGroup *tuplestorestatePTR;
						tuplestorestatePTR = (tmpGroup *) entry->tuplestorestate;

						tuple *inputTuple_;
						convexhull * convex = (convexhull *) entry->convexhull;

						int tmpnumbindex=0;
						tuple *tmptuplegroupptr;
						tmptuplegroupptr=ptr->tuples_headPtr;

						while(tmpnumbindex<ptr->member_len&&tmptuplegroupptr!=NULL)
						{
							int judgeresult = 2;

							inputTuple_ = (tuple *) palloc(sizeof(tuple));
							inputTuple_->numAttr = 2;
							memcpy(inputTuple_->values,tmptuplegroupptr->values,sizeof(int)*numAttributes );

							judgeresult = insideConvexAndInsideBound(convex, inputTuple_,distanceall);

							if (judgeresult == 1) {
								unionConvex(convex, inputTuple_, distanceall);
							}else
							{
								pfree(inputTuple_);
							}
							//unionConvex(convex, tmpgroupptr, distanceall);
							tmptuplegroupptr=tmptuplegroupptr->next;
							tmpnumbindex++;
						}

						if (tuplestorestatePTR) {
							tmpGroup_puttupleslot(tuplestorestatePTR, inputslot,
													numAttributes, true );
						}

						memcpy(entry->pivot1Slot, ptr->pivot1,
								sizeof(int) * numAttributes);
						memcpy(entry->pivot2Slot, ptr->pivot2,
								sizeof(int) * numAttributes);
						//initialize_aggregates(aggstate, aggstate->peragg, entry->pergroup);
						firstone = false;
					}							//if
				}							//if this is new group
				ptr = ptr->next;

			}

		}

		/* Initialize to walk the hash table */

		 /*finishProcessing = false;
		 ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);
		 while (!finishProcessing)
		 {

		 entry = (AggHashEntry) ScanTupleHashTable(&aggstate->hashiter);

		 if (entry == NULL)
		 {
		 finishProcessing = true;
		 }else if(entry->flagEnabled == 1)
		 {
		 //printGroup((Tuplestorestate *)entry->tuplestorestate, aggstate->modifiedSlot);
		 print_groups((tmpGroup *)entry->tuplestorestate,2);

		 	 printf("convex hull is \n");
		 	 print_tuples(((convexhull *) entry->convexhull)->convexheader);
		 }

		 }

		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!end to operation for this tuple \n");*/
		delete_tmpGroup(maxcliquePTR);
		pfree(values);
		pfree(replace);
		pfree(nulls);
		ResetExprContext(tmpcontext);

}

static
void sgb_newgroup_helper(int storeplace, AggState *aggstate, int metrichose) {
	//go through the hashtable

	bool iteration = true;
	TupleTableSlot *tmpslot = aggstate->pivot2Slot;

	if (aggstate->pivot2Slot->tts_tupleDescriptor == NULL )
		ExecSetSlotDescriptor(aggstate->pivot2Slot,
				aggstate->pivot1Slot->tts_tupleDescriptor);

	while (iteration) {

		sgb_newgroup_aggfunction(aggstate);

		Tuplestorestate *tuplestorestate;

		if (storeplace == 0) {
			tuplestorestate =
					(Tuplestorestate *) aggstate->overlaptuplestorestate1;
		} else if (storeplace == 1) {
			tuplestorestate =
					(Tuplestorestate *) aggstate->overlaptuplestorestate2;
		}

		bool forward;
		bool eof_tuplestore;
		forward = true;

		if (tuplestorestate == NULL ) {
			iteration = false;
			break;
		}

		if (storeplace == 1) {
			iteration = false;
			break;
		}

		//printGroup(tuplestorestate, aggstate->modifiedSlot);

		tuplestore_rescan(tuplestorestate);

		eof_tuplestore = (tuplestorestate == NULL )
				|| tuplestore_ateof(tuplestorestate);

		while (!eof_tuplestore) {
			//TupleTableSlot *slot;
			if (tuplestore_gettupleslot(tuplestorestate, forward, tmpslot)) {
				//print_slot(tmpslot);

				if(metrichose==1)
				{
					//sgb_newgroup_iteration_l1(aggstate,tmpslot, storeplace);
				}else if(metrichose==2)
				{
					//sgb_newgroup_iteration_l2(aggstate,tmpslot, storeplace);
				}
				//sgb_newgroup_hash(aggstate, tmpslot, storeplace);
			} else {
				eof_tuplestore = true;
			}
		}

		storeplace++;

		//storeplace=storeplace%2;

		//tuplesort_end(tuplestorestate);
	}

}


static void
agg_fill_hash_table_SGBANY(AggState *aggstate,
		TupleTableSlot *inputslot)
{
	Agg *node = (Agg *) aggstate->ss.ps.plan;

	/*EPS in the SGB-All */
	int distanceall;
	distanceall = node->distanceany;

	ExprContext *tmpcontext;
	int numAttributes = node->numCols; //we assume the group attribute are the first two column

	tmpcontext = aggstate->tmpcontext; /*tmpcontext is the per-input-tuple expression context */
	/* set up for advance_aggregates call */
	tmpcontext->ecxt_scantuple = inputslot; /*advance_aggregates should used riginal tuple*/

	int distanceMetric;
	distanceMetric = node->metric;/*(l2)distanceMetric=0, (l1)distanceMetric=1, (linfinity)distanceMetric=2*/

	if (distanceMetric == -1) {
		distanceMetric = 2;
	}

	initilList();

	//printf("input tuple is \n");
	//print_slot(inputslot);

	//this one only for the L1 distance
	RTREEMBR searchMBR2 =
			getMBRinputSlotBigger(
					inputslot,
					numAttributes,
					distanceall);

	int nhits = 0;

	bool localcontain=true;
	iscontain_M=localcontain;

	//get all the tuples which was contained by this MRB2
	nhits = RTreeSearch_Contain(
			aggstate->rtreeroot,
			&searchMBR2,
			MySearchCallback);

	//printf("nhtis is %d \n", nhits);

	int tupleid=aggstate->indexhasharray;

	//insert this tuple into the current ds-joint-set
	tuple *p1;
	p1 = palloc(sizeof(tuple));
	p1->numAttr = 2;
	copyinputslottoDes( inputslot,p1->values,numAttributes);
	//from tupleslot to the tuple

	element *newptr;
	InsertElement(p1, tupleid, aggstate->dsjset, newptr);

	/***************************************************************/
	if(nhits>0)
	{
		//for all the search results, do the dsjoint set operation
		int i=0;
		int newsetID=tupleid;

		//printf("input data tuple id is  %d \n", newsetID);

		for(;i<index_contain;i++)
		{

			int insiderange_tupleid=containArray[i];

			//printf("insiderange_tupleid %d \n", insiderange_tupleid);

			int tupleid_inDSset=insiderange_tupleid-1;

			//if this is the l2 distance, we need to double check this again.
			if(distanceMetric==2)
			{
				double le2sitance=L2distance(p1,aggstate->dsjset->m_nodes[tupleid_inDSset].data);

				int le2sitance1=(int)le2sitance;

				if(le2sitance1>distanceall)
				   continue;
			}

			int setID1=FindSet(tupleid_inDSset, aggstate->dsjset);

			//printf("set id1 is %d \n", setID1);

			if(setID1!=newsetID)
			{
				newsetID=unionSet(setID1, newsetID, aggstate->dsjset);
			}

		}

	}
	else if(nhits == 0) {
		//no union-find,insert this tuple into the dsjoint set as a new set and insert this tuple in the Rtree

	}

	RTREEMBR insertMBR=
						getMBRinputSlot(inputslot,numAttributes);

	int result=RTreeInsert(
				aggstate->rtreeroot,
				&insertMBR,
				(void*)(aggstate->indexhasharray+1),
				0
		 );

	//printf("insert result is %d \n", result);

	 aggstate->indexhasharray++;

	/* Initialize to walk the hash table */

	/*finishProcessing = false;
	 ResetTupleHashIterator(aggstate->hashtable, &aggstate->hashiter);
	 while (!finishProcessing)
	 {

	 entry = (AggHashEntry) ScanTupleHashTable(&aggstate->hashiter);

	 if (entry == NULL)
	 {
	 finishProcessing = true;
	 }else if(entry->flagEnabled == 1)
	 {

	 printGroup((Tuplestorestate *)entry->tuplestorestate, aggstate->modifiedSlot);
	 }

	 }*/

	 //printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!end to operation for this tuple \n");

	ResetExprContext(tmpcontext);


}

static void IterateSets(DisJointSets *dssets,
		AggState* aggstate,
		TupleTableSlot *inputslot ) {

	Agg		   *node = (Agg *) aggstate->ss.ps.plan;

	int numAttributes = node->numCols; //we assume the group attribute are the first two column

	Datum *values = (Datum *) palloc(numAttributes * sizeof(Datum)); /*array*/
	bool * nulls = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
	bool * replace = (bool *) palloc(numAttributes * sizeof(bool)); /*array*/
	memset(values, 0, numAttributes * sizeof(Datum));
	memset(nulls, false, numAttributes * sizeof(bool));
	memset(replace, false, numAttributes * sizeof(bool));

	int i = 0;
	while (i < dssets->num_elements) {
		//printf("set id is %d \n", FindSet(i,dssets));

		element *ptr = &(dssets->m_nodes[i]);

		if (ptr->parent == NULL ) {

			//printf("one group  !!!!!!!!!!!!!!! \n");

			//AggHashEntry entry;

			if (numAttributes == 1) {
				values[0] = (rand() % 70000000)
						+ rand() % 20000000;
				replace[0] = true;
			} else if (numAttributes == 2) {
				values[0] = (rand() % 50000000);
				replace[0] = true;
				values[1] = rand() % 10000;
				replace[1] = true;
			}

			HeapTuple newHeapTuple;/*CHANGED BY Mingjie*/

			//printf("!!!!!!! modifed tuple \n");
			//print_slot(inputslot);

			newHeapTuple = heap_modify_tuple(inputslot->tts_tuple,
					inputslot->tts_tupleDescriptor, values, nulls, replace);

			ExecStoreTuple(newHeapTuple, aggstate->modifiedSlot, InvalidBuffer,
					true );/*This will free memory of previous tuple too*/

			//print_slot(aggstate->modifiedSlot);

			AggHashEntry entry = lookup_hash_entry(aggstate, aggstate->modifiedSlot);/*YAS THIS SHOULD USE THE MODIFIED TUPLE*/

			//print_slot(aggstate->modifiedSlot);

			if (entry->flagEnabled != 0) {
				return;
			}

			entry->flagEnabled = 1;
			//printf("entry falge enabled  %d \n",entry->flagEnabled );

			DFS_oneSet(ptr, entry, aggstate,inputslot,numAttributes, true);

			//printf("xxxxxxxxxxxxxxxxxxxxxxx \n" );
		}

		i++;
	}

	pfree(values);
	pfree(replace);
	pfree(nulls);

}


static
void DFS_oneSet(element * parent, AggHashEntry entry, AggState* aggstate, TupleTableSlot *inputslot, int numAttributes , bool firstone) {

	//printf("!!!!!!!!!!!!!!!DFS_oneSet !!!!!!!! \n");

	if (parent->visited == 0) {

		ExprContext *tmpcontext;

		tmpcontext = aggstate->tmpcontext; /*tmpcontext is the per-input-tuple expression context */
											/* set up for advance_aggregates call */

		puttuple_TupleTableSlot(parent->data,aggstate->modifiedSlot,inputslot,numAttributes);

		//printf("tuple here is \n");
		//print_slot(aggstate->modifiedSlot);

		parent->visited=1;

		tmpcontext->ecxt_scantuple = aggstate->modifiedSlot; /*advance_aggregates should used riginal tuple*/

		if(firstone)
		{
			initialize_aggregates(aggstate, aggstate->peragg, entry->pergroup);
		}

		/* Advance the aggregates */
		advance_aggregates(aggstate, entry->pergroup); /*YAS THIS SHOULD USE THE ORIGINAL TUPLE*/

		element *begin = parent->childListHead;

		while (begin != NULL && begin->visited != 1) {

			DFS_oneSet(begin, entry, aggstate,inputslot,numAttributes,false);
			//DFS_oneSet(begin);
			//begin->visited = 1;
			begin = begin->nextChild;
		}

	} else {
		return;
	}

}

