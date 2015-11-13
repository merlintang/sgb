/*-------------------------------------------------------------------------
 *
 linkListM.c
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "executor/linkListM.h"

static
bool contain_tuple(int *value,tuple *t)
{
	tuple *g2Ptr=t;

	while(g2Ptr!=NULL)
	{
		if(g2Ptr->numAttr==0)
			return false;

		bool equal=true;
		int att=0;
		for(;att<g2Ptr->numAttr;att++)
		{
			//printf("A: %d, B: %d \n",t2.data[att],t.data[att]);
			if(g2Ptr->values[att]!=value[att])
			{
				equal=false;
				break;
			}
		}

		if(equal==true)
		{
			return equal;
		}

		g2Ptr=g2Ptr->next;
	}

	  return false;

	}


int hashfunction(int *tuple, int numAttr)
{

	int i=0;
	int value=0;
	for(;i<numAttr;i++)
	{
		value=value+(tuple[i]*hashA+hashB)%hashP;
	}

	value=value%hashvalueLenth;

	if(value<0)
		value=-value;

	return value;
}

bool contain_hash(tmpGroup *group,
		int *t,
		int numatt)
{

	int hashvalue=hashfunction(t,numatt);
		tuple *ptr=&(group->hashtable[hashvalue]);

		if(ptr==NULL)
		{
			return false;

		}else
		{
			//tuple *ptr=&(group->hashtable[hashvalue]);

			if(contain_tuple(t,ptr))
			{
				return true;
			}else
			{
				return false;
			}

		}

}

bool put_hash(tmpGroup *group, int *t, int numatt)
{
	if(contain_hash(group,t,numatt)==true)
		{
			return false;
		}

		int hashvalue=hashfunction(t, numatt);

		tuple *ptr=&(group->hashtable[hashvalue]);

		if(ptr->numAttr==0)
			{
				ptr->numAttr=numatt;
				int i=0;
				while(i<numatt)
				{
					ptr->values[i]=t[i];
					i++;
				}

			}else if(ptr!=NULL||ptr->numAttr!=0)
			{

				append_data_IntoTupleList(
									ptr,
									t,
									numatt);
			}

			return true;
}

 void
delete_tuples(tuple *tuplesHead)
{
	tuple *current, *next;
	current=tuplesHead;
	while(current!=NULL)
	{
		next=current->next;
		pfree(current);
		current=NULL;
		current=next;
	}
}

 void
print_tuples(tuple *tuplesHead)
{
	tuple *ptr=tuplesHead;
	while(ptr!=NULL)
	{
		int i=0;
		for(;i<ptr->numAttr&&i<10;i++)
		{
			printf(" %d ", ptr->values[i]);
		}
		printf("\n");
		ptr=ptr->next;

		if(ptr==tuplesHead)
		 break;
	}

}
/**************************************************************************/
 void
delete_tmpGroup(tmpGroup *groupHead)
{
	 if(groupHead==NULL||groupHead->member_len<1)
	 {
		 return;
	 }

	tmpGroup *current, *next;
	current=groupHead;

	 while(current!=NULL)
	    {
		 	if(current->gcenter!=NULL)
		 		pfree(current->gcenter);

		 	if(current->pivot1!=NULL)
		 	{
		 		pfree(current->pivot1);
		 	}

		 	if(current->pivot2!=NULL)
			{
				pfree(current->pivot2);
			}

			if(current->hashtable!=NULL&&current->member_len!=0)
			{
				tuple *dataPtr=current->tuples_headPtr;
				while(dataPtr!=NULL)
				{
					int hashvalue=hashfunction(dataPtr->values,dataPtr->numAttr);

					if(hashvalue>0)
					{
						delete_tuples(current->hashtable[hashvalue].next);
						current->hashtable[hashvalue].next=NULL;
					}

					dataPtr=dataPtr->next;
				}
				//printf("hash done\n");
				pfree(current->hashtable);

			}else if(current->hashtable!=NULL)
			{
				pfree(current->hashtable);
			}

			delete_tuples(current->tuples_headPtr);

			current->member_len=0;

		 	current->endPtr=NULL;

		 	next=current->next;

		 	pfree(current);
		 	current = next;
	    }
}

 void
 delete_oneGroup_nohash(tmpGroup *groupHead)
{
	 if(groupHead==NULL||groupHead->member_len<1)
	 {
		 return;
	 }

	tmpGroup *current;
	current=groupHead;

	 if(current!=NULL)
	    {
		 	if(current->gcenter!=NULL)
		 		pfree(current->gcenter);

		 	if(current->pivot1!=NULL)
		 	{
		 		pfree(current->pivot1);
		 	}

		 	if(current->pivot2!=NULL)
			{
				pfree(current->pivot2);
			}

			delete_tuples(current->tuples_headPtr);

			current->member_len=0;

		 	current->endPtr=NULL;

		 	pfree(current);
	    }
}

 void
print_groups(tmpGroup *groupHead, int numAtt)
{
	tmpGroup *ptr;
	ptr=groupHead;

	while(ptr!=NULL)
	{
		if(numAtt==1)
		{
			printf("group with center point x: %d \n",ptr->gcenter->x);
			printf("group pivot 1 is: %d \n", ptr->pivot1[0]);
			printf("group pivot 2 is: %d \n", ptr->pivot2[0]);

		}else if (numAtt==2)
		{
			printf("group with center point x: %d, y: %d \n",ptr->gcenter->x,ptr->gcenter->y);
			printf("group pivot 1 is: %d, %d \n", ptr->pivot1[0],ptr->pivot1[1]);
			printf("group pivot 2 is: %d, %d \n", ptr->pivot2[0],ptr->pivot2[1]);
		}

		printf("group is new %d \n", ptr->isexisit);
		print_tuples(ptr->tuples_headPtr);
		ptr=ptr->next;
	}
}
/**************************************************************************/

tuple *
append_data_IntoTupleList(
		tuple *tuplesHead,
		int* values,
		int numatt)
{
	tuple *end,*pre;
	if(tuplesHead==NULL)
	{
		pre=tuplesHead;

	}else
	{
		end=tuplesHead->next;
		pre=tuplesHead;

		while(end!=NULL)
			{
				pre=end;
				end=end->next;
			}
	}

	tuple *newtuples=(tuple *)palloc(sizeof(tuple));
	if(newtuples==NULL)
			return NULL;

		newtuples->next=NULL;
		newtuples->numAttr=numatt;

		int i=0;
		for(i=0;i<numatt;i++)
		{
			newtuples->values[i]=values[i];
		}

	if(pre==NULL)
	{
		pre=newtuples;
	}else
	{
		pre->next=newtuples;
	}
	return newtuples;
}

tmpGroup * append_Group_End(
		tmpGroup *GroupsHeader,
		int numAtrr,
		int x,
		int y,
		bool isexsit)
{
	tmpGroup *end,*pre;

	end=GroupsHeader->next;
	pre=GroupsHeader;

	while(end!=NULL)
	{
		pre=end;
		end=end->next;
	}

	tmpGroup *newGroup= constructGroup(numAtrr,x,y,isexsit);

	pre->next=newGroup;

	return newGroup;
}
/**************************************************************************/

/**
 * g1 will check g2's bound, and see whether it will need to join that group
 * if g1 contain g2, return 1
 * if g1 is contained by g2, return -1.
 * either g1 or g2 does not have contain relationship, return 2;
 */
int GroupBound_Checking(
		tmpGroup *g1,/*smaller ones*/
		tmpGroup *g2 /*bigger group*/
		)
{
		if(g1->member_len==0&&g2->member_len!=0)
		{
			return -1;
		}else if(g2->member_len==0&&g1->member_len!=0)
		{
			return 1;
		}else if(g2->member_len==0&&g1->member_len==0)
		{
			return 1;
		}


	int i=0;
	if(g1->tuples_headPtr->numAttr==1)
	{
		if(
		 g2->pivot1[i]==g1->pivot1[i]
					&&
		 g1->pivot2[i]==g2->pivot2[i]
		)
		{
			return 0;
		}
		else if(
		   g2->pivot1[i]<=g1->pivot1[i]
				&&
		   g1->pivot2[i]<=g2->pivot2[i]
		  )
		{
			return -1;
		}else if(
				g2->pivot1[i]>=g1->pivot1[i]
				 &&
			   g1->pivot2[i]>=g2->pivot2[i]
				)
		{
			return 1;

		}else
		{
			return 2;
		}

	}else if(g1->tuples_headPtr->numAttr==2)
	{
		 if(
			g2->pivot1[i]==g1->pivot1[i]
				&&
			g1->pivot2[i]==g2->pivot2[i]
				&&
			g2->pivot1[i+1]==g1->pivot1[i+1]
				&&
			g1->pivot2[i+1]==g2->pivot2[i+1]
			)
				{
					return 0;

				}else if(
		   g2->pivot1[i]<=g1->pivot1[i]
				&&
		   g1->pivot2[i]<=g2->pivot2[i]
				&&
		  g2->pivot1[i+1]<=g1->pivot1[i+1]
				&&
		   g1->pivot2[i+1]<=g2->pivot2[i+1]
		   )
		{
			return -1;

		}else if(
		  g2->pivot1[i]>=g1->pivot1[i]
				&&
		   g1->pivot2[i]>=g2->pivot2[i]
				&&
		  g2->pivot1[i+1]>=g1->pivot1[i+1]
				&&
		  g1->pivot2[i+1]>=g2->pivot2[i+1]
			)
		{
			return 1;
		}else
		{
			return 2;
		}

	}//

	return 2;
}

 tmpGroup *
constructGroup(
		int numAtrr,
		int centerX,
		int centerY,
		bool isexisit
		)
{

	tmpGroup *GroupsHeader= (tmpGroup *)palloc(sizeof(tmpGroup));

	if(GroupsHeader==NULL)
			printf("build failed\n");

		GroupsHeader->tuples_headPtr=NULL;
		GroupsHeader->gcenter=(AggGroupCenter *)palloc(sizeof(AggGroupCenter));

	  if(GroupsHeader->gcenter==NULL)
				printf("build center failed\n");

		GroupsHeader->gcenter->x=centerX;
		GroupsHeader->gcenter->y=centerY;
		GroupsHeader->next=NULL;
		GroupsHeader->isexisit=isexisit;
		GroupsHeader->member_len=0;
		GroupsHeader->pivot1=(int *)palloc(sizeof(int)*numAtrr);
		GroupsHeader->pivot2=(int *)palloc(sizeof(int)*numAtrr);

		GroupsHeader->hashtable=(tuple *)palloc(sizeof(tuple)*hashvalueLenth);

		memset(GroupsHeader->hashtable, NULL, sizeof(tuple)*hashvalueLenth);

		int i=0;
		for(i=0;i<numAtrr;i++)
		{
			GroupsHeader->pivot1[i]=0;
			GroupsHeader->pivot2[i]=0;
		}
		return GroupsHeader;

}

 tmpGroup *
constructGroup_nohash(
		int numAtrr,
		int centerX,
		int centerY,
		bool isexisit
		)
{

	tmpGroup *GroupsHeader= (tmpGroup *)palloc(sizeof(tmpGroup));

	if(GroupsHeader==NULL)
			printf("build failed\n");

		GroupsHeader->tuples_headPtr=NULL;
		GroupsHeader->endPtr=NULL;

		GroupsHeader->gcenter=(AggGroupCenter *)palloc(sizeof(AggGroupCenter));

	  if(GroupsHeader->gcenter==NULL)
				printf("build center failed\n");

		GroupsHeader->gcenter->x=centerX;
		GroupsHeader->gcenter->y=centerY;
		GroupsHeader->next=NULL;
		GroupsHeader->isexisit=isexisit;
		GroupsHeader->member_len=0;
		GroupsHeader->pivot1=(int *)palloc(sizeof(int)*numAtrr);
		GroupsHeader->pivot2=(int *)palloc(sizeof(int)*numAtrr);

		GroupsHeader->hashtable=NULL;

		int i=0;
		for(i=0;i<numAtrr;i++)
		{
			GroupsHeader->pivot1[i]=0;
			GroupsHeader->pivot2[i]=0;
		}
		return GroupsHeader;

}

/*
 * update the group pivot value by the inputslot
 * */
void
updatePivotByTuple_tmpGroup(
		tmpGroup * entry,
		int *inputslotdata,
		int numAttr,
		int EPS)
{
	int i=0;

	for(; i<numAttr; i++)
	{
		if(abs(entry->pivot1[i]-entry->pivot2[i])<=EPS&&
				entry->pivot1[i]!=0&&entry->pivot2[i]!=0
		)
		{
			continue;
		}

		if(entry->pivot1[i]==0&&entry->pivot2[i]==0)
		{
			entry->pivot1[i]=inputslotdata[i]-EPS;
			entry->pivot2[i]=inputslotdata[i]+EPS;
			continue;
		}

		if(inputslotdata[i]-EPS>entry->pivot1[i])
			{
				entry->pivot1[i]=inputslotdata[i]-EPS;
			}

		if(inputslotdata[i]+EPS<entry->pivot2[i])
			{
				entry->pivot2[i]=inputslotdata[i]+EPS;
			}
	}

}

/**
 * put this new group into the maxclique and update the max clique bound
 */
void
putNewGroupsIntoMaxclique(
		tmpGroup * newgroup,
		tmpGroup * maxcliqueptr,
		int eps)
{
	tuple *g1Ptr,*g2Ptr, *endPtr;
		g1Ptr=newgroup->tuples_headPtr;
		endPtr=maxcliqueptr->tuples_headPtr;

		int numAtrr=g1Ptr->numAttr;

		int len_tuple_exisit=1;

		if(maxcliqueptr->member_len!=0)
		{
			len_tuple_exisit=maxcliqueptr->member_len;
		}else
			if(maxcliqueptr->member_len==0)
		{

			tuple *endPtr_2=newgroup->tuples_headPtr;

			//update 1
			if(endPtr_2!=NULL){
				maxcliqueptr->tuples_headPtr=append_data_IntoTupleList(maxcliqueptr->tuples_headPtr, endPtr_2->values, numAtrr);
				put_hash(maxcliqueptr,endPtr_2->values, numAtrr);
			}

			endPtr=maxcliqueptr->tuples_headPtr;

			endPtr_2=endPtr_2->next;

			while(endPtr_2!=NULL)
			{
				endPtr=append_data_IntoTupleList(endPtr, endPtr_2->values, numAtrr);
				put_hash(maxcliqueptr,endPtr_2->values, numAtrr);
				endPtr_2=endPtr_2->next;
			}

			memcpy(maxcliqueptr->pivot1,
					newgroup->pivot1,
					sizeof(int)*newgroup->tuples_headPtr->numAttr);

			memcpy(maxcliqueptr->pivot2,
					newgroup->pivot2,
					sizeof(int)*newgroup->tuples_headPtr->numAttr);

			maxcliqueptr->member_len=newgroup->member_len;

			maxcliqueptr->isexisit=newgroup->isexisit;

			return;
		}

		while(g1Ptr!=NULL)
			{
				bool equal=contain_hash(maxcliqueptr,g1Ptr->values,g1Ptr->numAttr);

				//printf("***************hash check result is %d \n", equal);

				if(equal==false)
				{
					endPtr=append_data_IntoTupleList(endPtr, g1Ptr->values, numAtrr);
					updatePivotByTuple_tmpGroup(maxcliqueptr,g1Ptr->values,numAtrr,eps);
					put_hash(maxcliqueptr,g1Ptr->values, numAtrr);
					maxcliqueptr->member_len++;
					maxcliqueptr->isexisit=newgroup->isexisit;
				}

				g1Ptr=g1Ptr->next;
			}

		//printf("!!!!!!!!!!!!!!!!!!!!!update results is \n");
		//print_groups(maxcliqueptr);


}


/*
 * if this new group A, was contained by the GroupCandidate and ExsitGroup
 * (We call this sets as max clique sets.)
 * do not need to build new group,
 * break;
 * or if this new group A can contain one of group B,
 * all the news tuple in the A will join this group,
 * and update that group bounds,
 * and break.
 * or else
 * append this group A to the end of max clique sets
 * */
 bool
Judge_and_Join_GLIST(
		tmpGroup *newgroup, /*temp groups*/
		tmpGroup *maxcliqueHeader,  /*candidate and exisit group list*/
		int eps
		)
{
	tmpGroup *maxcliqueptr= maxcliqueHeader;
	tmpGroup *pre=maxcliqueHeader;
	bool isnew=true;

	int boundcount=0;

	int bound3=0;
	int bound4=0;

	//printf("new group is \n");
	//print_groups(newgroup);

	while(maxcliqueptr!=NULL)
	{
		//printf("old group is \n");
		//print_groups(maxcliqueptr);
		int checkresult=GroupBound_Checking(newgroup,maxcliqueptr);
		//printf("check result is %d \n", checkresult);

		if(checkresult==-1)
		{
			isnew=false;
			//bound2=bound2+newgroup->member_len*maxcliqueptr->member_len;
			bound3=bound3+newgroup->member_len;
			//bound4=bound4+maxcliqueptr->member_len;
			putNewGroupsIntoMaxclique(newgroup,maxcliqueptr,eps);
		}else if(checkresult==0)
		{
			isnew=false;
			break;
		}else if(checkresult==1)
		{
			isnew=false;
			putNewGroupsIntoMaxclique(newgroup,maxcliqueptr,eps);
			break;
		}
		pre=maxcliqueptr;
		maxcliqueptr=maxcliqueptr->next;
		boundcount++;

	}

	//if(bound3>100)
		//printf("number of bound1 is %d , bound3 is %d \n", boundcount, bound3);

	//append this tuple into the end of the macliquesets
	if(isnew)
	{
		if(maxcliqueptr==NULL)
		{
			pre->next=newgroup;
			maxcliqueptr=newgroup;
		}
	}

	return isnew;
}
