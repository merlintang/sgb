
#ifndef LINKLISTM_H
#define LINKLISTM_H

#define hashvalueLenth 5000

#define hashP 5381
#define hashA 7399
#define hashB 19

#include "postgres.h"

typedef struct AggGroupCenter
{
	int x;
	int y;
	int z;
}AggGroupCenter;

/*modifed by Mingjie*/
typedef struct tuple
{
	int numAttr; //real att number
	int values[5];//modified this later, we assume the highest dimension is 5
	struct tuple *next;
}tuple;

/*modifed by Mingjie*/
typedef struct tmpGroup
{
	tuple *tuples_headPtr;
	tuple *endPtr;
	int *pivot1;
	int *pivot2;
	struct tmpGroups *next;
	int member_len;
	AggGroupCenter *gcenter;
	bool isexisit;
	tuple *hashtable;
}tmpGroup;

extern void
delete_tmpGroup(tmpGroup *groupHead);

extern void
delete_tuples(tuple *tuplesHead);

extern
tuple *
append_data_IntoTupleList(tuple *tuplesHead, int* values, int len);

extern
void
updatePivotByTuple_tmpGroup(
		tmpGroup * entry,
		int *inputslotdata,
		int numAttr,
		int EPS);

extern void
print_groups(tmpGroup *groupHead, int numatt);

extern void
print_tuples(tuple *tuplesHead);

extern void
delete_oneGroup_nohash(tmpGroup *GroupsHeader);

extern
tmpGroup * append_Group_End(
		tmpGroup *GroupsHeader,
		int numAtrr,
		int x,
		int y,
		bool isexsit);

extern bool
Judge_and_Join_GLIST(
		tmpGroup *tupleIngroup1, /*temp groups*/
		tmpGroup *tupleIngroup2,  /*exsit groups*/
		int eps
		);

extern void
putNewGroupsIntoMaxclique(
		tmpGroup * newgroup,
		tmpGroup * maxcliqueptr,
		int eps);

extern int
containinGroup(
		tmpGroup *g1,/*smaller ones*/
		tmpGroup *g2 /*bigger group*/
		);

extern tmpGroup * constructGroup(
		int numAtrr,
		int centerX,
		int centerY,
		bool isexisit
		);

extern
tmpGroup *
constructGroup_nohash(
		int numAtrr,
		int centerX,
		int centerY,
		bool isexisit
		);
extern
int hashfunction(int *tuple,int numatt);

extern
bool contain_hash(tmpGroup *group,int *tuple, int numatt);

extern
bool put_hash(tmpGroup *group,int *tuple, int numatt);


#endif   /* linkListM.h */
