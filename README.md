##  Similarity SQL-based Group-By operator
The SQL group-by operator plays an important role in summarizing and aggregating large datasets in a data analytics
stack. The Similarity SQL-based
Group-By operator (SGB, for short) extends the semantics of the standard SQL Group-by by grouping data with similar but not
necessarily equal values. While existing similarity-based grouping operators efficiently realize these approximate semantics,
they primarily focus on one-dimensional attributes and treat multi-dimensional attributes independently. However, correlated
attributes, such as in spatial data, are processed independently, and hence, groups in the multi-dimensional space are not
detected properly. To address this problem, we introduce two new SGB operators for multi-dimensional data. We implement and test the new SGB operators and their algorithms inside PostgreSQL. The overhead introduced by these operators proves to be minimal and the execution times are comparable to those of the standard Group-by. The experimental study, based on TPC-H and a social check-in data, demonstrates that the proposed algorithms can achieve up to three orders of magnitude
enhancement in performance over baseline methods developed to solve the same problem.   

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
max(col1) as p2x, max(col2) as p2y from tmp
group by col1, col2 distanceany within 2
metric lone;

