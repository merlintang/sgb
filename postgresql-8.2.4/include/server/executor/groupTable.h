
#ifndef GROUPTABLE_H
#define GROUPTABLE_H

#include "executor/tuptable.h"

typedef struct GroupTupleData *GroupData;
typedef struct GroupTupleData
{
	struct GroupTupleData *next;
	TupleTableSlot entry;

}GroupTupleData;

#endif   /* GROUPTABLE_H */
