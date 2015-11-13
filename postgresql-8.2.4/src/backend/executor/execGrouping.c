/*-------------------------------------------------------------------------
 *
 * execGrouping.c
 *	  executor utility routines for grouping, hashing, and aggregation
 *
 * Portions Copyright (c) 1996-2006, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/executor/execGrouping.c,v 1.21 2006/07/14 14:52:18 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "executor/executor.h"
#include "parser/parse_oper.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/syscache.h"


static TupleHashTable CurTupleHashTable = NULL;

static uint32 TupleHashTableHash(const void *key, Size keysize);
static int TupleHashTableMatch(const void *key1, const void *key2,
					Size keysize);


/*****************************************************************************
 *		Utility routines for grouping tuples together
 *****************************************************************************/

/*
 * execTuplesMatch
 *		Return true if two tuples match in all the indicated fields.
 *
 * This actually implements SQL's notion of "not distinct".  Two nulls
 * match, a null and a not-null don't match.
 *
 * slot1, slot2: the tuples to compare (must have same columns!)
 * numCols: the number of attributes to be examined
 * matchColIdx: array of attribute column numbers
 * eqFunctions: array of fmgr lookup info for the equality functions to use
 * evalContext: short-term memory context for executing the functions
 *
 * NB: evalContext is reset each time!
 */
bool
execTuplesMatch(TupleTableSlot *slot1,
				TupleTableSlot *slot2,
				int numCols,
				AttrNumber *matchColIdx,
				FmgrInfo *eqfunctions,
				MemoryContext evalContext)
{
	MemoryContext oldContext;
	bool		result;
	int			i;

	/* Reset and switch into the temp context. */
	MemoryContextReset(evalContext);
	oldContext = MemoryContextSwitchTo(evalContext);

	/*
	 * We cannot report a match without checking all the fields, but we can
	 * report a non-match as soon as we find unequal fields.  So, start
	 * comparing at the last field (least significant sort key). That's the
	 * most likely to be different if we are dealing with sorted input.
	 */
	result = true;

	for (i = numCols; --i >= 0;)
	{
		AttrNumber	att = matchColIdx[i];
		Datum		attr1,
					attr2;
		bool		isNull1,
					isNull2;

		attr1 = slot_getattr(slot1, att, &isNull1);

		attr2 = slot_getattr(slot2, att, &isNull2);

		if (isNull1 != isNull2)
		{
			result = false;		/* one null and one not; they aren't equal */
			break;
		}

		if (isNull1)
			continue;			/* both are null, treat as equal */

		/* Apply the type-specific equality function */

		if (!DatumGetBool(FunctionCall2(&eqfunctions[i],
										attr1, attr2)))
		{
			result = false;		/* they aren't equal */
			break;
		}
	}

	MemoryContextSwitchTo(oldContext);

	return result;
}


/*CHANGED BY YASIN*/
/*Output: True  = Current tuple belongs to current group
 *        False = Current tuple does not belong to current group. 
 * 				  It belong to any later group, no necesarily the next one!*/
bool
execTuplesMatchAround(TupleTableSlot *pivot1Slot,
				TupleTableSlot *pivot2Slot,
				TupleTableSlot *currentSlot,
				int numCols,
				AttrNumber *matchColIdx,
				FmgrInfo *eqfunctions,
				FmgrInfo *ltfunctions,
				FmgrInfo *minusfunctions,
				MemoryContext evalContext)
{
	MemoryContext oldContext;
	bool		result;
	int			i;

   /*CHANGED BY YASIN ONLY FOR DEBUG
	printf ("Esto se ve desde dentro de execTuplesMatchAround\n");
	print_slot(pivot1Slot);
	print_slot(pivot2Slot);
	print_slot(currentSlot);*/
		
	/* We could have the following case: pivot1slot.tuple and pivot1slot.tuple point to the same structure 
	 * This happens when pivot2Heap is null (we only have 1 pivot). We detect this case using tts_shouldFree
	 * that is set to false to avoid double pfree of the same tuple*/
	if (pivot2Slot->tts_shouldFree == false){ return true;}
	
	/* Reset and switch into the temp context. */
	MemoryContextReset(evalContext);
	oldContext = MemoryContextSwitchTo(evalContext);
	result = true;
		
	/*We support Group Around several columns: we take the values of the pivots from the unique select around*/
	for (i = 0; i< numCols; i++)
	{/*Each cicle processes a group around column*/
		Datum		attrPivot1,
					attrPivot2,
					attrCurrent;
		bool		isNull1,
					isNull2,
					isNull3;
		AttrNumber	att = matchColIdx[i];/*This is the attribute number in the current tuple.They always start at 1*/
		AttrNumber	attPivots = (int16) (i+1);/*This is the attribute number in the Pivots. They always start at 1*/	

		/*printf ("att: %d \n", att);	
		printf ("attPivots: %d \n", attPivots);*/	
					
	
		attrPivot1 = slot_getattr(pivot1Slot, attPivots, &isNull1);
		attrPivot2 = slot_getattr(pivot2Slot, attPivots, &isNull2);
		attrCurrent = slot_getattr(currentSlot, att, &isNull3);
		
		if ((isNull1) && (isNull2 && isNull3))
		{
			continue;			/* all are null, treat as equal */
		}
		
		if ( (isNull1 || isNull2) || isNull3)
		{
			result = false;		/* only one or two are null; they aren't equal */
			break;
		}
		/*The 3 attributes are NOT null*/

		/* If (Pivot1=Pivot2) return true. This only happens if there are more than one col*/
		if (numCols>1){
			if (DatumGetBool(FunctionCall2(&eqfunctions[i],attrPivot1, attrPivot2)))
			{
				continue;	/* The difference (in pivots) should be in one of the next cols*/
			}
		}
		/*If Current <= Pivot1 then True. Equivalent expression: !(Pivot1 < Current)   */
		if (!DatumGetBool(FunctionCall2(&ltfunctions[i],attrPivot1,attrCurrent)))
		{
			continue;	
		}
		/*If Current >= Pivot2 then False. Equivalent expression: !(Current < Pivot2)   */
		/*if (!DatumGetBool(FunctionCall2(&ltfunctions[i],attrCurrent,attrPivot2))){result = false;break;}*/
		
		/* return (Current - Pivot1) < (Pivot2 - Current). */ 		
		if (DatumGetBool( FunctionCall2(&ltfunctions[i],
									FunctionCall2(&minusfunctions[i],attrCurrent,attrPivot1),
									FunctionCall2(&minusfunctions[i],attrPivot2,attrCurrent))))
		{
			continue;	
		}
		else
		{
			result = false;
			break;
		}
	}
	MemoryContextSwitchTo(oldContext);

	/*CHANGED BY YASIN ONLY FOR DEBUG
	if (result)	printf("VERDADERO\n");
	else printf("FALSO\n");*/

	return result;
}



/*CHANGED BY YASIN*/
/*Output: True  = Current tuple.aroundAtt belongs to current group (delimited by P1 and P2)
 *        False = It does not belong to current group. 
 * 				  It belongs to any later group, no necesarily the next one!*/
bool
execTuplesMatchAroundHash(TupleTableSlot *pivot1Slot,
				TupleTableSlot *pivot2Slot,
				TupleTableSlot *currentSlot,
				AttrNumber aroundOuterAtt, 
				int indexInGroupBy,
				FmgrInfo *eqfunctions,
				FmgrInfo *ltfunctions,
				FmgrInfo *minusfunctions,
				MemoryContext evalContext)
{
	MemoryContext oldContext;
	bool		result;
	AttrNumber	attPivots;

	Datum		attrPivot1,attrPivot2,attrCurrent;
	bool		isNull1,isNull2,isNull3;
	
   /*CHANGED BY YASIN ONLY FOR DEBUG
	printf ("Esto se ve desde dentro de execTuplesMatchAround\n");
	print_slot(pivot1Slot);
	print_slot(pivot2Slot);
	print_slot(currentSlot);*/
		
	/* We could have the following case: pivot1slot.tuple and pivot1slot.tuple point to the same structure 
	 * This happens when pivot2Heap is null (we only have 1 pivot). We detect this case using tts_shouldFree
	 * that is set to false to avoid double pfree of the same tuple*/
	if (pivot2Slot->tts_shouldFree == false){ return true;}
	
	/* Reset and switch into the temp context. */
	MemoryContextReset(evalContext);
	oldContext = MemoryContextSwitchTo(evalContext);
	result = true;
	attPivots = (int16) 1;/*We always use the first col of the right(inner) plan. Starts at 1*/

	attrPivot1 = slot_getattr(pivot1Slot, attPivots, &isNull1);
	attrPivot2 = slot_getattr(pivot2Slot, attPivots, &isNull2);
	attrCurrent = slot_getattr(currentSlot, aroundOuterAtt, &isNull3);
	
	if (isNull1 && (isNull2 && isNull3))
	{	MemoryContextSwitchTo(oldContext);
		return true;			/* all are null, treat as equal */
	}
	if ( (isNull1 || isNull2) || isNull3)
	{	MemoryContextSwitchTo(oldContext);
		return false;		/* only one or two are null; they aren't equal */
	}
	/*The 3 attributes are NOT null*/

	/*If Current <= Pivot1 then True. Equivalent expression: !(Pivot1 < Current)   */
	if (!DatumGetBool(FunctionCall2(&ltfunctions[indexInGroupBy],attrPivot1,attrCurrent)))
	{	MemoryContextSwitchTo(oldContext);
		return true;	
	}

	/* return (Current - Pivot1) < (Pivot2 - Current). */ 		
	if (DatumGetBool( FunctionCall2(&ltfunctions[indexInGroupBy],/*We assume that col in group-by that uses around is the first one*/
								FunctionCall2(&minusfunctions[indexInGroupBy],attrCurrent,attrPivot1),
								FunctionCall2(&minusfunctions[indexInGroupBy],attrPivot2,attrCurrent))))
	{	MemoryContextSwitchTo(oldContext);
		return true;	
	}

	MemoryContextSwitchTo(oldContext);
	return false;
}

/*CHANGED BY YASIN*/
/* Implements execTuplesMatch with support of maxRadius around the center points
 * Output: True  = Current tuple.aroundAtt belongs to current group (derived from P1 and P2 or just P1 in case P2=NULL)
 *        False = It does not belong to current group. 
 * 				  It can be a tuple to be ignored and located before the limits of current group or
 * 			      It can belong to any later group, no necesarily the next one!
 * 		  ignore = True if tuple.aroundAtt is located before current groups limits or (after limits and only P1 is valid)
 * 		  stop   = True if tuple.aroundAtt is located to the right of limits and only P1 is valid*/
bool
execTuplesMatchAroundHash_MaxR(TupleTableSlot *pivot1Slot,
				TupleTableSlot *pivot2Slot,
				TupleTableSlot *currentSlot,
				AttrNumber aroundOuterAtt, 
				int indexInGroupBy,
				FmgrInfo *eqfunctions,
				FmgrInfo *ltfunctions,
				FmgrInfo *minusfunctions,
				MemoryContext evalContext,
				Datum maxRadiusDatum,
				bool *ignore,bool *stop)
{
	MemoryContext oldContext;
	bool		result;
	AttrNumber	attPivots;
	Datum		attrPivot1,attrPivot2,attrCurrent;
	bool		isNull1,isNull2,isNull3;
	
	/*printf ("Esto se ve desde dentro de execTuplesMatchAroundHash_MaxR\n");
	print_slot(pivot1Slot);print_slot(pivot2Slot);print_slot(currentSlot);*/
	
	/* Reset and switch into the temp context. */
	MemoryContextReset(evalContext);
	oldContext = MemoryContextSwitchTo(evalContext);
	
	attPivots = (int16) 1;/*We always use the first col of the right(inner) plan. Starts at 1*/

	attrPivot1 = slot_getattr(pivot1Slot, attPivots, &isNull1);
	attrCurrent = slot_getattr(currentSlot, aroundOuterAtt, &isNull3);

	if (pivot2Slot->tts_shouldFree == false){/*We have only P1*/
		if (isNull1 && isNull3)
		{	MemoryContextSwitchTo(oldContext);
			return true;			/* P1 and current tuple are null, treat as equal */
		}
		if (isNull1 || isNull3)
		{	MemoryContextSwitchTo(oldContext);
			return false;		/* only one is null; they aren't equal */
		}
		/*The 2 attributes are NOT null*/
		/*If Current < (Pivot1-maxRadius)*/
		if (DatumGetBool( FunctionCall2(&ltfunctions[indexInGroupBy],/*We assume that col in group-by that uses around is the first one*/
									attrCurrent,
									FunctionCall2(&minusfunctions[indexInGroupBy],attrPivot1,maxRadiusDatum))))
		{result = false; *ignore = true; *stop = false;}
		/*if Current < (P1+maxRadius) equivalent to Current-maxRadius < P1*/
		else if (DatumGetBool( FunctionCall2(&ltfunctions[indexInGroupBy],/*We assume that col in group-by that uses around is the first one*/
									FunctionCall2(&minusfunctions[indexInGroupBy],attrCurrent,maxRadiusDatum),
									attrPivot1)))
		{result = true; *ignore = false; *stop = false;}
		else /*Current >= P1+maxRadius*/
		{result = false; *ignore = true; *stop = true;}
	}
	else /*We have P1 and P2*/
	{
		attrPivot2 = slot_getattr(pivot2Slot, attPivots, &isNull2);
		if (isNull1 && (isNull2 && isNull3))
		{	MemoryContextSwitchTo(oldContext);
			return true;			/* all are null, treat as equal */
		}
		if ( (isNull1 || isNull2) || isNull3)
		{	MemoryContextSwitchTo(oldContext);
			return false;		/* only one or two are null; they aren't equal */
		}
		/*The 3 attributes are NOT null*/
		/*If 2*maxRadius > P2-P1 equivalent to ((P2-P1)-maxRadius) < maxRadius */
		if (DatumGetBool( FunctionCall2(&ltfunctions[indexInGroupBy],/*We assume that col in group-by that uses around is the first one*/
									FunctionCall2(&minusfunctions[indexInGroupBy],
										FunctionCall2(&minusfunctions[indexInGroupBy],attrPivot2,attrPivot1),
										maxRadiusDatum),
									maxRadiusDatum)))
		{/*limit is given by (P2-P1)/2 */
			/*if Current < (P1-maxRadius)*/
			if (DatumGetBool( FunctionCall2(&ltfunctions[indexInGroupBy],/*We assume that col in group-by that uses around is the first one*/
									attrCurrent,
									FunctionCall2(&minusfunctions[indexInGroupBy],attrPivot1,maxRadiusDatum))))
			{result = false; *ignore = true; *stop = false;}
			/*if Current < P1*/
			else if (DatumGetBool( FunctionCall2(&ltfunctions[indexInGroupBy],/*We assume that col in group-by that uses around is the first one*/
										attrCurrent,
										attrPivot1)))
			{result = true; *ignore = false; *stop = false;}
			/*return Current-P1 < (P2-current)*/
			else
			{
				result = DatumGetBool( FunctionCall2(&ltfunctions[indexInGroupBy],/*We assume that col in group-by that uses around is the first one*/
								FunctionCall2(&minusfunctions[indexInGroupBy],attrCurrent,attrPivot1),
								FunctionCall2(&minusfunctions[indexInGroupBy],attrPivot2,attrCurrent)));
				*ignore=false; *stop = false;
			}	
		}
		else /*limit is given by maxRadius*/
		{
			/*if Current < (P1-maxRadius)*/
			if (DatumGetBool( FunctionCall2(&ltfunctions[indexInGroupBy],/*We assume that col in group-by that uses around is the first one*/
									attrCurrent,
									FunctionCall2(&minusfunctions[indexInGroupBy],attrPivot1,maxRadiusDatum))))
			{result = false; *ignore = true; *stop = false;}
			/*if Current < P1+maxRadius. Equivalent to P-P1<maxRadius*/
			else if (DatumGetBool( FunctionCall2(&ltfunctions[indexInGroupBy],/*We assume that col in group-by that uses around is the first one*/
										FunctionCall2(&minusfunctions[indexInGroupBy],attrCurrent,attrPivot1),
										maxRadiusDatum)))
			{result = true; *ignore = false; *stop = false;}
			/*Current >=P1+maxRadius*/
			else
			{result = false; *ignore=false; *stop = false;}	
		}
	}

	MemoryContextSwitchTo(oldContext);
	/*if (result) printf ("TuplesMatch.result: TRUE \n");
	else printf ("TuplesMatch.result: FALSE\n");
	if (*ignore) printf ("TuplesMatch.ignore: TRUE \n");
	else printf ("TuplesMatch.ignore: FALSE\n");
	if (*stop) printf ("TuplesMatch.stop: TRUE \n");
	else printf ("TuplesMatch.stop: FALSE\n");*/
	
	return result;
}

/*
 * execTuplesUnequal
 *		Return true if two tuples are definitely unequal in the indicated
 *		fields.
 *
 * Nulls are neither equal nor unequal to anything else.  A true result
 * is obtained only if there are non-null fields that compare not-equal.
 *
 * Parameters are identical to execTuplesMatch.
 */
bool
execTuplesUnequal(TupleTableSlot *slot1,
				  TupleTableSlot *slot2,
				  int numCols,
				  AttrNumber *matchColIdx,
				  FmgrInfo *eqfunctions,
				  MemoryContext evalContext)
{
	MemoryContext oldContext;
	bool		result;
	int			i;

	/* Reset and switch into the temp context. */
	MemoryContextReset(evalContext);
	oldContext = MemoryContextSwitchTo(evalContext);

	/*
	 * We cannot report a match without checking all the fields, but we can
	 * report a non-match as soon as we find unequal fields.  So, start
	 * comparing at the last field (least significant sort key). That's the
	 * most likely to be different if we are dealing with sorted input.
	 */
	result = false;

	for (i = numCols; --i >= 0;)
	{
		AttrNumber	att = matchColIdx[i];
		Datum		attr1,
					attr2;
		bool		isNull1,
					isNull2;

		attr1 = slot_getattr(slot1, att, &isNull1);

		if (isNull1)
			continue;			/* can't prove anything here */

		attr2 = slot_getattr(slot2, att, &isNull2);

		if (isNull2)
			continue;			/* can't prove anything here */

		/* Apply the type-specific equality function */

		if (!DatumGetBool(FunctionCall2(&eqfunctions[i],
										attr1, attr2)))
		{
			result = true;		/* they are unequal */
			break;
		}
	}

	MemoryContextSwitchTo(oldContext);

	return result;
}


/*
 * execTuplesMatchPrepare
 *		Look up the equality functions needed for execTuplesMatch or
 *		execTuplesUnequal.
 *
 * The result is a palloc'd array.
 */
FmgrInfo *
execTuplesMatchPrepare(TupleDesc tupdesc,
					   int numCols,
					   AttrNumber *matchColIdx)
{
	FmgrInfo   *eqfunctions = (FmgrInfo *) palloc(numCols * sizeof(FmgrInfo));
	int			i;

	for (i = 0; i < numCols; i++)
	{
		AttrNumber	att = matchColIdx[i];
		Oid			typid = tupdesc->attrs[att - 1]->atttypid;
		Oid			eq_function;

		eq_function = equality_oper_funcid(typid);/*GIVEN A TYPE GET THE EQUALITY FUNCTION*/
		fmgr_info(eq_function, &eqfunctions[i]);
	}

	return eqfunctions;
}

/*CHANGED BY YASIN*/
void
execTuplesMatchAroundPrepare(TupleDesc tupdesc,
					  int numCols,
					  AttrNumber *matchColIdx,
					  FmgrInfo **eqfunctions,
					  FmgrInfo **ltfunctions,
					  FmgrInfo **minusfunctions)
{ 
	int			i;
	*eqfunctions = (FmgrInfo *) palloc(numCols * sizeof(FmgrInfo));
	*ltfunctions = (FmgrInfo *) palloc(numCols * sizeof(FmgrInfo));
	*minusfunctions = (FmgrInfo *) palloc(numCols * sizeof(FmgrInfo));

	for (i = 0; i < numCols; i++)
	{
		AttrNumber	att = matchColIdx[i];
		Oid			typid = tupdesc->attrs[att - 1]->atttypid;
		Operator	optup;
		Oid			eq_function;
		Oid			lt_function;
		Oid			minus_function;

		/*GIVEN A TYPE GET THE = FUNCTION*/ /*eq_function = equality_oper_funcid(typid);*/
		optup = equality_oper(typid, false);
		eq_function = oprfuncid(optup);
		ReleaseSysCache(optup);

		/*GIVEN A TYPE GET THE < FUNCTION*/ 
		optup = ordering_oper(typid, false);
		lt_function = oprfuncid(optup);
		ReleaseSysCache(optup);

		/*GIVEN A TYPE GET THE - FUNCTION*/ 
		optup = minus_oper(typid, false); /*minus_oper WAS ADDED BY YASIN*/
		minus_function = oprfuncid(optup); /*get the function from the operator tuple*/
		ReleaseSysCache(optup);

		fmgr_info(eq_function, &(*eqfunctions)[i]);
		fmgr_info(lt_function, &(*ltfunctions)[i]);
		fmgr_info(minus_function, &(*minusfunctions)[i]);
				
	}
}


/*
 * execTuplesHashPrepare
 *		Look up the equality and hashing functions needed for a TupleHashTable.
 *
 * This is similar to execTuplesMatchPrepare, but we also need to find the
 * hash functions associated with the equality operators.  *eqfunctions and
 * *hashfunctions receive the palloc'd result arrays.
 */
void
execTuplesHashPrepare(TupleDesc tupdesc,
					  int numCols,
					  AttrNumber *matchColIdx,
					  FmgrInfo **eqfunctions,
					  FmgrInfo **hashfunctions)
{
	int			i;

	*eqfunctions = (FmgrInfo *) palloc(numCols * sizeof(FmgrInfo));
	*hashfunctions = (FmgrInfo *) palloc(numCols * sizeof(FmgrInfo));

	for (i = 0; i < numCols; i++)
	{
		AttrNumber	att = matchColIdx[i];
		Oid			typid = tupdesc->attrs[att - 1]->atttypid;
		Operator	optup;
		Oid			eq_opr;
		Oid			eq_function;
		Oid			hash_function;

		optup = equality_oper(typid, false);
		eq_opr = oprid(optup);
		eq_function = oprfuncid(optup);
		ReleaseSysCache(optup);
		hash_function = get_op_hash_function(eq_opr);
		if (!OidIsValid(hash_function)) /* should not happen */
			elog(ERROR, "could not find hash function for hash operator %u",
				 eq_opr);
		fmgr_info(eq_function, &(*eqfunctions)[i]);
		fmgr_info(hash_function, &(*hashfunctions)[i]);
	}
}

/*CHANGED BY YASIN*/
/*
 * execTuplesHashPrepare
 *		Look up the equality and hashing functions needed for a TupleHashTable.
 *
 * This is similar to execTuplesMatchPrepare, but we also need to find the
 * hash functions associated with the equality operators.  *eqfunctions and
 * *hashfunctions receive the palloc'd result arrays.
 */
void
execTuplesHashPrepareSGB(TupleDesc tupdesc,
					  int numCols,
					  AttrNumber *matchColIdx,
					  FmgrInfo **eqfunctions,
					  FmgrInfo **hashfunctions,
					  FmgrInfo **ltfunctions,
					  FmgrInfo **minusfunctions)
{
	int			i;
	*eqfunctions = (FmgrInfo *) palloc(numCols * sizeof(FmgrInfo));
	*hashfunctions = (FmgrInfo *) palloc(numCols * sizeof(FmgrInfo));
	*ltfunctions = (FmgrInfo *) palloc(numCols * sizeof(FmgrInfo));
	*minusfunctions = (FmgrInfo *) palloc(numCols * sizeof(FmgrInfo));

	for (i = 0; i < numCols; i++)
	{
		AttrNumber	att = matchColIdx[i];
		Oid			typid = tupdesc->attrs[att - 1]->atttypid;
		Operator	optup;
		Oid			eq_opr;
		Oid			eq_function;
		Oid			hash_function;
		Oid			lt_function;
		Oid			minus_function;

		/*GIVEN A TYPE GET THE = FUNCTION*/ /*eq_function = equality_oper_funcid(typid);*/
		optup = equality_oper(typid, false);
		eq_opr = oprid(optup);
		eq_function = oprfuncid(optup);
		ReleaseSysCache(optup);
		
		/*GIVEN A TYPE GET THE HASH FUNCTION*/
		hash_function = get_op_hash_function(eq_opr);
		if (!OidIsValid(hash_function)) /* should not happen */
			elog(ERROR, "could not find hash function for hash operator %u",
				 eq_opr);
		
		/*GIVEN A TYPE GET THE < FUNCTION*/ 
		optup = ordering_oper(typid, false);
		lt_function = oprfuncid(optup);
		ReleaseSysCache(optup);

		/*GIVEN A TYPE GET THE - FUNCTION*/ 
		optup = minus_oper(typid, false); /*minus_oper WAS ADDED BY YASIN*/
		minus_function = oprfuncid(optup); /*get the function from the operator tuple*/
		ReleaseSysCache(optup);
				 
		fmgr_info(eq_function, &(*eqfunctions)[i]);
		fmgr_info(hash_function, &(*hashfunctions)[i]);
		fmgr_info(lt_function, &(*ltfunctions)[i]);
		fmgr_info(minus_function, &(*minusfunctions)[i]);
	}
}
		

/*****************************************************************************
 *		Utility routines for all-in-memory hash tables
 *
 * These routines build hash tables for grouping tuples together (eg, for
 * hash aggregation).  There is one entry for each not-distinct set of tuples
 * presented.
 *****************************************************************************/

/*
 * Construct an empty TupleHashTable
 *
 *	numCols, keyColIdx: identify the tuple fields to use as lookup key
 *	eqfunctions: equality comparison functions to use
 *	hashfunctions: datatype-specific hashing functions to use
 *	nbuckets: initial estimate of hashtable size
 *	entrysize: size of each entry (at least sizeof(TupleHashEntryData))
 *	tablecxt: memory context in which to store table and table entries
 *	tempcxt: short-lived context for evaluation hash and comparison functions
 *
 * The function arrays may be made with execTuplesHashPrepare().
 *
 * Note that keyColIdx, eqfunctions, and hashfunctions must be allocated in
 * storage that will live as long as the hashtable does.
 */
TupleHashTable
BuildTupleHashTable(int numCols, AttrNumber *keyColIdx,
					FmgrInfo *eqfunctions,
					FmgrInfo *hashfunctions,
					int nbuckets, Size entrysize,
					MemoryContext tablecxt, MemoryContext tempcxt)
{
	TupleHashTable hashtable;
	HASHCTL		hash_ctl;

	Assert(nbuckets > 0);
	Assert(entrysize >= sizeof(TupleHashEntryData));

	hashtable = (TupleHashTable) MemoryContextAlloc(tablecxt,
												 sizeof(TupleHashTableData));

	hashtable->numCols = numCols;
	hashtable->keyColIdx = keyColIdx;
	hashtable->eqfunctions = eqfunctions;
	hashtable->hashfunctions = hashfunctions;
	hashtable->tablecxt = tablecxt;
	hashtable->tempcxt = tempcxt;
	hashtable->entrysize = entrysize;
	hashtable->tableslot = NULL;	/* will be made on first lookup */
	hashtable->inputslot = NULL;

	MemSet(&hash_ctl, 0, sizeof(hash_ctl));
	hash_ctl.keysize = sizeof(TupleHashEntryData);
	hash_ctl.entrysize = entrysize;
	hash_ctl.hash = TupleHashTableHash;
	hash_ctl.match = TupleHashTableMatch;
	hash_ctl.hcxt = tablecxt;
	hashtable->hashtab = hash_create("TupleHashTable", (long) nbuckets,
									 &hash_ctl,
					HASH_ELEM | HASH_FUNCTION | HASH_COMPARE | HASH_CONTEXT);

	return hashtable;
}

/*CHANGED BY RUBY
 * no function was added
 * */
TupleHashEntry
LookupTupleHashEntry_DIST(AggState *state, TupleHashTable hashtable, TupleTableSlot *slot,
					 bool *isnew)
{
	TupleHashEntry entry;
	MemoryContext oldContext;
	TupleHashTable saveCurHT;
	TupleHashEntryData dummy;
	bool		found;

	/* If first time through, clone the input slot to make table slot */
	if (hashtable->tableslot == NULL)
	{
		TupleDesc	tupdesc;

		oldContext = MemoryContextSwitchTo(hashtable->tablecxt);

		/*
		 * We copy the input tuple descriptor just for safety --- we assume
		 * all input tuples will have equivalent descriptors.
		 */
		tupdesc = CreateTupleDescCopy(slot->tts_tupleDescriptor);
		hashtable->tableslot = MakeSingleTupleTableSlot(tupdesc);
		MemoryContextSwitchTo(oldContext);
	}

	/* Need to run the hash functions in short-lived context */
	oldContext = MemoryContextSwitchTo(hashtable->tempcxt);

	/*
	 * Set up data needed by hash and match functions
	 *
	 * We save and restore CurTupleHashTable just in case someone manages to
	 * invoke this code re-entrantly.
	 */
	hashtable->inputslot = slot;
	saveCurHT = CurTupleHashTable;
	CurTupleHashTable = hashtable;

	/* Search the hash table */
	dummy.firstTuple = NULL;	/* flag to reference inputslot */
	/*entry = (TupleHashEntry) hash_search(hashtable->hashtab,
										 &dummy,
										 isnew ? HASH_ENTER : HASH_FIND,
										 &found);
										 */
	Agg		   *node = (Agg *) state->ss.ps.plan;





	entry = (TupleHashEntry) hash_search_DIST(node->distanceall ,  hashtable->hashtab,
											 &dummy,
											 isnew ? HASH_ENTER : HASH_FIND,
											 &found);

	if (isnew)
	{
		if (found)
		{
			/* found pre-existing entry */
			*isnew = false;
		}
		else
		{
			/*
			 * created new entry
			 *
			 * Zero any caller-requested space in the entry.  (This zaps the
			 * "key data" dynahash.c copied into the new entry, but we don't
			 * care since we're about to overwrite it anyway.)
			 */
			MemSet(entry, 0, hashtable->entrysize);
			/* Copy the first tuple into the table context */
			MemoryContextSwitchTo(hashtable->tablecxt);
			entry->firstTuple = ExecCopySlotMinimalTuple(slot);


			*isnew = true;
		}
	}

	CurTupleHashTable = saveCurHT;

	MemoryContextSwitchTo(oldContext);

	return entry;
}


/*
 * Find or create a hashtable entry for the tuple group containing the
 * given tuple.
 *
 * If isnew is NULL, we do not create new entries; we return NULL if no
 * match is found.
 *
 * If isnew isn't NULL, then a new entry is created if no existing entry
 * matches.  On return, *isnew is true if the entry is newly created,
 * false if it existed already.  Any extra space in a new entry has been
 * zeroed.
 */
TupleHashEntry
LookupTupleHashEntry(TupleHashTable hashtable, TupleTableSlot *slot,
					 bool *isnew)
{
	TupleHashEntry entry;
	MemoryContext oldContext;
	TupleHashTable saveCurHT;
	TupleHashEntryData dummy;
	bool		found;

	/* If first time through, clone the input slot to make table slot */
	if (hashtable->tableslot == NULL)
	{
		TupleDesc	tupdesc;

		oldContext = MemoryContextSwitchTo(hashtable->tablecxt);

		/*
		 * We copy the input tuple descriptor just for safety --- we assume
		 * all input tuples will have equivalent descriptors.
		 */
		tupdesc = CreateTupleDescCopy(slot->tts_tupleDescriptor);
		hashtable->tableslot = MakeSingleTupleTableSlot(tupdesc);
		MemoryContextSwitchTo(oldContext);
	}

	/* Need to run the hash functions in short-lived context */
	oldContext = MemoryContextSwitchTo(hashtable->tempcxt);

	/*
	 * Set up data needed by hash and match functions
	 *
	 * We save and restore CurTupleHashTable just in case someone manages to
	 * invoke this code re-entrantly.
	 */
	hashtable->inputslot = slot;
	saveCurHT = CurTupleHashTable;
	CurTupleHashTable = hashtable;

	/* Search the hash table */
	dummy.firstTuple = NULL;	/* flag to reference inputslot */
	entry = (TupleHashEntry) hash_search(hashtable->hashtab,
										 &dummy,
										 isnew ? HASH_ENTER : HASH_FIND,
										 &found);

	if (isnew)
	{
		if (found)
		{
			/* found pre-existing entry */
			*isnew = false;
		}
		else
		{
			/*
			 * created new entry
			 *
			 * Zero any caller-requested space in the entry.  (This zaps the
			 * "key data" dynahash.c copied into the new entry, but we don't
			 * care since we're about to overwrite it anyway.)
			 */
			MemSet(entry, 0, hashtable->entrysize);
			/* Copy the first tuple into the table context */
			MemoryContextSwitchTo(hashtable->tablecxt);
			entry->firstTuple = ExecCopySlotMinimalTuple(slot);


			*isnew = true;
		}
	}

	CurTupleHashTable = saveCurHT;

	MemoryContextSwitchTo(oldContext);

	return entry;
}

/*
 * Compute the hash value for a tuple
 *
 * The passed-in key is a pointer to TupleHashEntryData.  In an actual hash
 * table entry, the firstTuple field points to a tuple (in MinimalTuple
 * format).  LookupTupleHashEntry sets up a dummy TupleHashEntryData with a
 * NULL firstTuple field --- that cues us to look at the inputslot instead.
 * This convention avoids the need to materialize virtual input tuples unless
 * they actually need to get copied into the table.
 *
 * CurTupleHashTable must be set before calling this, since dynahash.c
 * doesn't provide any API that would let us get at the hashtable otherwise.
 *
 * Also, the caller must select an appropriate memory context for running
 * the hash functions. (dynahash.c doesn't change CurrentMemoryContext.)
 */
static uint32
TupleHashTableHash(const void *key, Size keysize)
{
	MinimalTuple tuple = ((const TupleHashEntryData *) key)->firstTuple;
	TupleTableSlot *slot;
	TupleHashTable hashtable = CurTupleHashTable;
	int			numCols = hashtable->numCols;
	AttrNumber *keyColIdx = hashtable->keyColIdx;
	uint32		hashkey = 0;
	int			i;

	if (tuple == NULL)
	{
		/* Process the current input tuple for the table */
		slot = hashtable->inputslot;
	}
	else
	{
		/* Process a tuple already stored in the table */
		/* (this case never actually occurs in current dynahash.c code) */
		slot = hashtable->tableslot;
		ExecStoreMinimalTuple(tuple, slot, false);
	}

	//debugtup(slot, slot);
	for (i = 0; i < numCols; i++)
	{
		AttrNumber	att = keyColIdx[i];
		Datum		attr;
		bool		isNull;

		/* rotate hashkey left 1 bit at each step */
		hashkey = (hashkey << 1) | ((hashkey & 0x80000000) ? 1 : 0);

		attr = slot_getattr(slot, att, &isNull);

		if (!isNull)			/* treat nulls as having hash key 0 */
		{
			uint32		hkey;

			hkey = DatumGetUInt32(FunctionCall1(&hashtable->hashfunctions[i],
												attr));
			hashkey ^= hkey;
		}
	}

	return hashkey;
}

/*
 * See whether two tuples (presumably of the same hash value) match
 *
 * As above, the passed pointers are pointers to TupleHashEntryData.
 *
 * CurTupleHashTable must be set before calling this, since dynahash.c
 * doesn't provide any API that would let us get at the hashtable otherwise.
 *
 * Also, the caller must select an appropriate memory context for running
 * the compare functions.  (dynahash.c doesn't change CurrentMemoryContext.)
 */
static int
TupleHashTableMatch(const void *key1, const void *key2, Size keysize)
{
	MinimalTuple tuple1 = ((const TupleHashEntryData *) key1)->firstTuple;

#ifdef USE_ASSERT_CHECKING
	MinimalTuple tuple2 = ((const TupleHashEntryData *) key2)->firstTuple;
#endif
	TupleTableSlot *slot1;
	TupleTableSlot *slot2;
	TupleHashTable hashtable = CurTupleHashTable;

	/*
	 * We assume that dynahash.c will only ever call us with the first
	 * argument being an actual table entry, and the second argument being
	 * LookupTupleHashEntry's dummy TupleHashEntryData.  The other direction
	 * could be supported too, but is not currently used by dynahash.c.
	 */
	Assert(tuple1 != NULL);
	slot1 = hashtable->tableslot;
	ExecStoreMinimalTuple(tuple1, slot1, false);
	Assert(tuple2 == NULL);
	slot2 = hashtable->inputslot;

	if (execTuplesMatch(slot1,
						slot2,
						hashtable->numCols,
						hashtable->keyColIdx,
						hashtable->eqfunctions,
						hashtable->tempcxt))
		return 0;
	else
		return 1;
}
