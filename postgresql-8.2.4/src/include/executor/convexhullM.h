/*
 * convexhullM.h
 *
 *  Created on: Aug 14, 2013
 *      Author: merlin
 */

#ifndef CONVEXHULLM_H_
#define CONVEXHULLM_H_

#include "executor/linkListM.h"
#include <math.h>

typedef struct convexhull
{
	tuple *convexheader;//tuple are storied as the clock turn order.
	int numofconvex;
}convexhull;

/**
 * delete the tuple from the begin to end,and link the begin to end.
 */
int delete_from_to_connect(convexhull *convex,tuple * begin, tuple *end, tuple *newtuple);

void freeconvexhull(convexhull *convex);

void print_onetuple(tuple *t);
/**
* get cross product
*/
double L2distance(tuple *o, tuple *to);

int  crossproduct(tuple *o, tuple* a, tuple *b);

bool isTangentPoint(tuple *o, tuple *prevous, tuple *current, tuple *nextone );

int insideConvexAndInsideBound(convexhull *convex, tuple *newpoint, int eps);

void unionConvex(convexhull *convex, tuple *newpoint, int eps);

void print_convexhull(convexhull *convex);

#endif /* CONVEXHULLM_H_ */
