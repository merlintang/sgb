The similarity group-by operators (short as SGB) perform the similair function as clustering algorithm for two dimenisonal data. The major difference is that SGB perform data clustering inside the database kernel, and work with the database pipeline to win performance.    

##Documentation
Similarity Group-by Operators for Multi-dimensional Relational Data
Mingjie Tang, Ruby Y. Tahboub, Walid G. Aref, Mikhail Atallah, Qutaibah Malluhi, Mourad Ouzzani, Yasin. Siva 
in IEEE Transactions on Knowledge and Data Engineering (TKDE2015)

## Building like PostgreSQL
http://www.postgresql.org/docs/9.2/static/install-procedure.html

## Sample Sql queries

```sql

//create table
CREATE TABLE tmp (
    id         integer,
    col1       double,
    col2       double
);

//insert data 
insert into tmp values (1, 2.0,3.0)
insert into tmp values (2, 3.0,4.0)

//execute query
//SGB-All
         select  avg(col1) as centerx, avg(col2) as centery
   			 min(col1) as p1x, min(col2) as p1y
   			 max(col1) as p2x, max(col2) as p2y from tmp
   			 group by col1, col2 distanceall within 2
   			 on_overlap any 
   			 metric lone;
   			 
   			  select  avg(col1) as centerx, avg(col2) as centery
   			 min(col1) as p1x, min(col2) as p1y
   			 max(col1) as p2x, max(col2) as p2y from tmp
   			 group by col1, col2 distanceall within 2
   			 on_overlap eliminate 
   			 metric lone;
   			 
   			  select  avg(col1) as centerx, avg(col2) as centery
   			 min(col1) as p1x, min(col2) as p1y
   			 max(col1) as p2x, max(col2) as p2y from tmp
   			 group by col1, col2 distanceall within 2
   			 on_overlap new_group 
   			 metric lone;
   			 

//SGB-Any	 
   			 select  avg(col1) as centerx, avg(col2) as centery
   			 min(col1) as p1x, min(col2) as p1y
   			 max(col1) as p2x, max(col2) as p2y from table
   			 group by col1, col2 distanceany within 2
   		   metric lone;

