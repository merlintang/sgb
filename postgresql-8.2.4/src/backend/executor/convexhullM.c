/*
 * convexhullM.c
 *
 *  Created on: Aug 14, 2013
 *      Author: merlin
 */

#include "postgres.h"
#include "executor/convexhullM.h"
#include <math.h>
/**
 *if return is >0, ob left turn to oa.
 *else <0, ob right tun to oa.
 */
int crossproduct(tuple *o, tuple* a, tuple *b) {
	return (a->values[0] - o->values[0]) * (b->values[1] - o->values[1])
			- (a->values[1] - o->values[1]) * (b->values[0] - o->values[0]);
}

double L2distance(tuple *o, tuple *to) {
	double distance = 0;

	distance = (o->values[0] - to->values[0]) * (o->values[0] - to->values[0])
			+ (o->values[1] - to->values[1]) * (o->values[1] - to->values[1]);

	return sqrt(distance);
}

/**
 * delete the tuple among begin and end.
 */
int delete_from_to_connect(convexhull *convex, tuple * begin, tuple *end, tuple *newtuple) {

	tuple *current, *next;

	current = begin->next;
	int i = 0;

	while (current != end&&i<convex->numofconvex) {

		next = current->next;

		if (current == convex->convexheader)
		{
			convex->convexheader = begin;
		}

		pfree(current);

		current = next;

		i++;
	}

	begin->next = newtuple;
	newtuple->next = end;
	return i;
}

/**
 * judge this t is a tangent point or not where o is the new point
 */
bool isTangentPoint(tuple *o, tuple *prevous, tuple *current, tuple *nextone) {

	int product1 = crossproduct(o, prevous, current);
	int product2 = crossproduct(o, nextone, current);

	int multiple = 1;

	if ((product2 > 0 && product1 > 0) || (product2 < 0 && product1 < 0)) {
		multiple = 1;
	} else {
		multiple = -1;
	}

	if (product1 == 0 || product2 == 0) {
		if (product1 == 0 && product2 == 0) {
			return false ;
		} else {
			return true ;
		}

	} else if (multiple > 0) {
		return true ;
	} else {
		return false ;
	}

}

double getDegree(tuple *o, tuple *prevous, tuple *current) {

	double ma_x = prevous->values[0] - o->values[0];
	double ma_y = prevous->values[1] - o->values[1];
	double mb_x = current->values[0] - o->values[0];
	double mb_y = current->values[0] - o->values[0];

	double v1 = (ma_x * mb_x) + (ma_y * mb_y);
	double ma_val = sqrt(ma_x * ma_x + ma_y * ma_y);
	double mb_val = sqrt(mb_x * mb_x + mb_y * mb_y);

	if (ma_val * mb_val == 0) {
		return 180;
	}

	double cosM = v1 / (ma_val * mb_val);

	double angleAMB = acos(cosM) * 180 / M_PI;

	return angleAMB;
}

bool isConvexPoint(tuple *o, tuple *prevous, tuple *current, tuple *nextone) {

	double ocn = getDegree(current, o, nextone);
	double pco = getDegree(current, prevous, o);
	double pcn = getDegree(current, prevous, nextone);

	if (ocn == 0) {
		int pco_ = (int) pco;
		int pcn_ = (int) pcn;

		if ((ocn + pco_) > pcn_) {
			// this is a convex point
			return true ;

		} else {
			//this is not convex point
			return false ;
		}
	}

	if ((ocn + pco) > pcn) {
		// this is a convex point
		return true ;

	} else {
		//this is not convex point
		return false ;
	}

}

/**
 * judge whether this new point inside the convex or not.
 * use the o(n) search method
 * if return ==0 this tuple inside the convex and this tupe can join this convex
 * if return ==1 this tuple outside of convex and it also can join this convex
 * if return ==2 this tuple outside of convex and it can not join this convex because the longest
 * if return ==3 this tuple outside of convex and the it has some overlap with the current group
 * distance is bigger than the eps
 */
int insideConvexAndInsideBound(convexhull *convex, tuple *newpoint, int eps) {

	if (convex == NULL ) {
		return 2;
	}

	if (convex->numofconvex == 0) {
		return 1;
	} else if (convex->numofconvex == 1) {

		double distance = L2distance(convex->convexheader, newpoint);

		if (distance <= eps) {
			return 1;
		} else {
			return 2;
		}

	}
	tuple *current = NULL;

	if (convex->convexheader != NULL )
		current = convex->convexheader->next;

	int count1 = 0;
	int count2 = 0;
	double longest = 0;

	int i = 0;
	bool overlap=false;
	while (current != NULL && i < convex->numofconvex) {

		tuple *next = current->next;

		if (next == NULL ) {
			break;
		}

		double distance = L2distance(newpoint, current);

		if(distance==0)
			{
		      return 0;
			}

		if (distance > longest) {

			if(longest<=eps)
			{
				overlap=true;
			}

			longest = distance;

			if (longest > eps&&overlap==true) {
				return 3;
			}
		}

		int value = crossproduct(newpoint, current, next);

		if (value > 0) {
			count1++;
		} else if (value < 0) {
			count2++;
		}

		current = current->next;
		i++;
	}

	if (0 == count1 || 0 == count2) {

		if (convex->numofconvex == 2)
			return 1;
		else
			return 0;

	} else //this tuple is in the outside area
	{
		if (longest < eps)
			return 1;
		else{

			if(overlap==true)
			{
				return 3;
			}else
			{
				return 2;
			}
		}
	}
}

/**
 *remove the no convex points when the new point is comming
 */
void unionConvex(convexhull *convex, tuple *newpoint, int eps) {

	if (convex == NULL ) {
		return;
	}

	if (convex->numofconvex == 0) {
		convex->convexheader = newpoint;
		convex->convexheader->next = NULL;
		convex->numofconvex++;
		return;

	} else if (convex->numofconvex == 1) {

		convex->convexheader->next = newpoint;
		newpoint->next = convex->convexheader;
		convex->numofconvex++;
		return;

	} else if (convex->numofconvex == 2) {
		tuple *tmp = convex->convexheader->next;
		tmp->next = newpoint;
		newpoint->next = convex->convexheader;
		convex->numofconvex++;
		return;

	} else {
		int i = 0;
		tuple *header = convex->convexheader;
		tuple *current = header->next;
		tuple *previous = header;
		tuple *beginpoint = current;
		tuple *tangleOne = NULL;
		tuple *tangleTwo = NULL;
		bool firstone = false;
		bool secondone = false;

		bool shouldchangeTangle = false;

		while (i <= convex->numofconvex) {

			tuple *nextone = current->next;

			//print_onetuple(current);

			if (nextone == NULL || previous == NULL )
				break;

			if (isTangentPoint(newpoint, previous, current, nextone)) {
				if (firstone == false ) {

					tangleOne = current;
					firstone = true;

				} else {

					if (current != tangleOne) {
						tangleTwo = current;
						secondone = true;
					}

				}
			} else if (firstone == true && secondone == false ) {
				if (isConvexPoint(newpoint, previous, current, nextone)) {
					shouldchangeTangle = true;
				}
			}

			previous = current;
			current = current->next;

			/*if(current==beginpoint)
			 break;*/

			i++;
		}

		int lostnumber = 0;
		if (tangleTwo != NULL && tangleOne != NULL ) {
			//int value=crossproduct(newpoint, tangleOne, tangleTwo);
			//printf("inner product value %d \n", value);

			//printf("tangle point one: ");
			//print_onetuple(tangleOne);

			//printf("tangle point two: ");
			//print_onetuple(tangleTwo);

			//printf("should change %d \n", shouldchangeTangle);
			if (shouldchangeTangle) {
				tuple *tmp = tangleTwo;
				tangleTwo = tangleOne;
				tangleOne = tmp;
			}

			lostnumber = delete_from_to_connect(convex, tangleOne, tangleTwo,newpoint);
			convex->numofconvex = convex->numofconvex - lostnumber + 1;

		} else {
			//printf("problem \n");
		}

	}

}

void print_onetuple(tuple *t) {
	tuple *ptr = t;
	//while(ptr!=NULL)
	//{
	int i = 0;
	for (; i < ptr->numAttr && i < 10; i++) {
		printf(" %d ", ptr->values[i]);
	}
	printf("\n");
	//ptr=ptr->next;
	//}
}

void print_convexhull(convexhull *convex) {
	int number = convex->numofconvex;
	tuple *current, *next;
	current = convex->convexheader;

	int i = 0;
	while (current != NULL && i < number) {
		next = current->next;
		print_onetuple(current);
		current = next;
		i++;
	}
}

void freeconvexhull(convexhull *convex) {
	int number = convex->numofconvex;
	tuple *current, *next;
	current = convex->convexheader;

	int i = 0;
	while (current != NULL && i < number) {
		next = current->next;
		pfree(current);
		current = next;
		i++;
	}
}
