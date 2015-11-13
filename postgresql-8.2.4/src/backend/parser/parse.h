
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton interface for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     ABORT_P = 258,
     ABSOLUTE_P = 259,
     ACCESS = 260,
     ACTION = 261,
     ADD_P = 262,
     ADMIN = 263,
     AFTER = 264,
     AGGREGATE = 265,
     ALL = 266,
     ALSO = 267,
     ALTER = 268,
     ANALYSE = 269,
     ANALYZE = 270,
     AND = 271,
     ANY = 272,
     AROUND = 273,
     ARRAY = 274,
     AS = 275,
     ASC = 276,
     ASSERTION = 277,
     ASSIGNMENT = 278,
     ASYMMETRIC = 279,
     AT = 280,
     AUTHORIZATION = 281,
     BACKWARD = 282,
     BEFORE = 283,
     BEGIN_P = 284,
     BETWEEN = 285,
     BIGINT = 286,
     BINARY = 287,
     BIT = 288,
     BOOLEAN_P = 289,
     BOTH = 290,
     BY = 291,
     CACHE = 292,
     CALLED = 293,
     CASCADE = 294,
     CASCADED = 295,
     CASE = 296,
     CAST = 297,
     CHAIN = 298,
     CHAR_P = 299,
     CHARACTER = 300,
     CHARACTERISTICS = 301,
     CHECK = 302,
     CHECKPOINT = 303,
     CLASS = 304,
     CLOSE = 305,
     CLUSTER = 306,
     COALESCE = 307,
     COLLATE = 308,
     COLUMN = 309,
     COMMENT = 310,
     COMMIT = 311,
     COMMITTED = 312,
     CONCURRENTLY = 313,
     CONNECTION = 314,
     CONSTRAINT = 315,
     CONSTRAINTS = 316,
     CONVERSION_P = 317,
     CONVERT = 318,
     COPY = 319,
     CREATE = 320,
     CREATEDB = 321,
     CREATEROLE = 322,
     CREATEUSER = 323,
     CROSS = 324,
     CSV = 325,
     CURRENT_DATE = 326,
     CURRENT_ROLE = 327,
     CURRENT_TIME = 328,
     CURRENT_TIMESTAMP = 329,
     CURRENT_USER = 330,
     CURSOR = 331,
     CYCLE = 332,
     DATABASE = 333,
     DAY_P = 334,
     DEALLOCATE = 335,
     DEC = 336,
     DECIMAL_P = 337,
     DECLARE = 338,
     DEFAULT = 339,
     DEFAULTS = 340,
     DEFERRABLE = 341,
     DEFERRED = 342,
     DEFINER = 343,
     DELETE_P = 344,
     DELIMITER = 345,
     DELIMITERS = 346,
     DESC = 347,
     DISABLE_P = 348,
     DISTANCEALL = 349,
     DISTANCEANY = 350,
     DISTINCT = 351,
     DO = 352,
     DOMAIN_P = 353,
     DOUBLE_P = 354,
     DROP = 355,
     DUPLICATE = 356,
     EACH = 357,
     ELIMINATE = 358,
     ELSE = 359,
     ENABLE_P = 360,
     ENCODING = 361,
     ENCRYPTED = 362,
     END_P = 363,
     ESCAPE = 364,
     EXCEPT = 365,
     EXCLUDING = 366,
     EXCLUSIVE = 367,
     EXECUTE = 368,
     EXISTS = 369,
     EXPLAIN = 370,
     EXTERNAL = 371,
     EXTRACT = 372,
     FALSE_P = 373,
     FETCH = 374,
     FIRST_P = 375,
     FLOAT_P = 376,
     FOR = 377,
     FORCE = 378,
     FOREIGN = 379,
     FORWARD = 380,
     FREEZE = 381,
     FROM = 382,
     FULL = 383,
     FUNCTION = 384,
     GLOBAL = 385,
     GRANT = 386,
     GRANTED = 387,
     GREATEST = 388,
     GROUP_P = 389,
     HANDLER = 390,
     HAVING = 391,
     HEADER_P = 392,
     HOLD = 393,
     HOUR_P = 394,
     IF_P = 395,
     ILIKE = 396,
     IMMEDIATE = 397,
     IMMUTABLE = 398,
     IMPLICIT_P = 399,
     IN_P = 400,
     INCLUDING = 401,
     INCREMENT = 402,
     INDEX = 403,
     INDEXES = 404,
     INHERIT = 405,
     INHERITS = 406,
     INITIALLY = 407,
     INNER_P = 408,
     INOUT = 409,
     INPUT_P = 410,
     INSENSITIVE = 411,
     INSERT = 412,
     INSTEAD = 413,
     INT_P = 414,
     INTEGER = 415,
     INTERSECT = 416,
     INTERVAL = 417,
     INTO = 418,
     INVOKER = 419,
     IS = 420,
     ISNULL = 421,
     ISOLATION = 422,
     JOIN = 423,
     KEY = 424,
     LANCOMPILER = 425,
     LANGUAGE = 426,
     LARGE_P = 427,
     LAST_P = 428,
     LEADING = 429,
     LEAST = 430,
     LEFT = 431,
     LEVEL = 432,
     LIKE = 433,
     LIMIT = 434,
     LISTEN = 435,
     LOAD = 436,
     LOCAL = 437,
     LOCALTIME = 438,
     LOCALTIMESTAMP = 439,
     LOCATION = 440,
     LOCK_P = 441,
     LOGIN_P = 442,
     LONE = 443,
     LTWO = 444,
     MATCH = 445,
     MAXVALUE = 446,
     METRIC = 447,
     MINUTE_P = 448,
     MINVALUE = 449,
     MODE = 450,
     MONTH_P = 451,
     MOVE = 452,
     MAXELEMSEP = 453,
     MAXGROUPDIAM = 454,
     MAXJOINDIAM = 455,
     NAMES = 456,
     NATIONAL = 457,
     NATURAL = 458,
     NCHAR = 459,
     NEW = 460,
     NEW_GROUP = 461,
     NEXT = 462,
     NO = 463,
     NOCREATEDB = 464,
     NOCREATEROLE = 465,
     NOCREATEUSER = 466,
     NOINHERIT = 467,
     NOLOGIN_P = 468,
     NONE = 469,
     NOSUPERUSER = 470,
     NOT = 471,
     NOTHING = 472,
     NOTIFY = 473,
     NOTNULL = 474,
     NOWAIT = 475,
     NULL_P = 476,
     NULLIF = 477,
     NUMERIC = 478,
     OBJECT_P = 479,
     OF = 480,
     OFF = 481,
     OFFSET = 482,
     OIDS = 483,
     OLD = 484,
     ON = 485,
     ON_OVERLAP = 486,
     ONLY = 487,
     OPERATOR = 488,
     OPTION = 489,
     OR = 490,
     ORDER = 491,
     OUT_P = 492,
     OUTER_P = 493,
     OVERLAPS = 494,
     OVERLAY = 495,
     OWNED = 496,
     OWNER = 497,
     PARTIAL = 498,
     PASSWORD = 499,
     PLACING = 500,
     POSITION = 501,
     PRECISION = 502,
     PRESERVE = 503,
     PREPARE = 504,
     PREPARED = 505,
     PRIMARY = 506,
     PRIOR = 507,
     PRIVILEGES = 508,
     PROCEDURAL = 509,
     PROCEDURE = 510,
     QUOTE = 511,
     READ = 512,
     REAL = 513,
     REASSIGN = 514,
     RECHECK = 515,
     REFERENCES = 516,
     REINDEX = 517,
     RELATIVE_P = 518,
     RELEASE = 519,
     RENAME = 520,
     REPEATABLE = 521,
     REPLACE = 522,
     RESET = 523,
     RESTART = 524,
     RESTRICT = 525,
     RETURNING = 526,
     RETURNS = 527,
     REVOKE = 528,
     RIGHT = 529,
     ROLE = 530,
     ROLLBACK = 531,
     ROW = 532,
     ROWS = 533,
     RULE = 534,
     SAVEPOINT = 535,
     SCHEMA = 536,
     SCROLL = 537,
     SECOND_P = 538,
     SECURITY = 539,
     SELECT = 540,
     SEQUENCE = 541,
     SERIALIZABLE = 542,
     SESSION = 543,
     SESSION_USER = 544,
     SET = 545,
     SETOF = 546,
     SHARE = 547,
     SHOW = 548,
     SIMILAR = 549,
     SIMPLE = 550,
     SMALLINT = 551,
     SOME = 552,
     STABLE = 553,
     START = 554,
     STATEMENT = 555,
     STATISTICS = 556,
     STDIN = 557,
     STDOUT = 558,
     STORAGE = 559,
     STRICT_P = 560,
     SUBSTRING = 561,
     SUPERUSER_P = 562,
     SYMMETRIC = 563,
     SYSID = 564,
     SYSTEM_P = 565,
     TABLE = 566,
     TABLESPACE = 567,
     TEMP = 568,
     TEMPLATE = 569,
     TEMPORARY = 570,
     THEN = 571,
     TIME = 572,
     TIMESTAMP = 573,
     TO = 574,
     TRAILING = 575,
     TRANSACTION = 576,
     TREAT = 577,
     TRIGGER = 578,
     TRIM = 579,
     TRUE_P = 580,
     TRUNCATE = 581,
     TRUSTED = 582,
     TYPE_P = 583,
     UNCOMMITTED = 584,
     UNENCRYPTED = 585,
     UNION = 586,
     UNIQUE = 587,
     UNKNOWN = 588,
     UNLISTEN = 589,
     UNTIL = 590,
     UPDATE = 591,
     USER = 592,
     USING = 593,
     VACUUM = 594,
     VALID = 595,
     VALIDATOR = 596,
     VALUES = 597,
     VARCHAR = 598,
     VARYING = 599,
     VERBOSE = 600,
     VIEW = 601,
     VOLATILE = 602,
     WHEN = 603,
     WHERE = 604,
     WITH = 605,
     WITHIN = 606,
     WITHOUT = 607,
     WORK = 608,
     WRITE = 609,
     YEAR_P = 610,
     ZONE = 611,
     WITH_CASCADED = 612,
     WITH_LOCAL = 613,
     WITH_CHECK = 614,
     IDENT = 615,
     FCONST = 616,
     SCONST = 617,
     BCONST = 618,
     XCONST = 619,
     Op = 620,
     ICONST = 621,
     PARAM = 622,
     POSTFIXOP = 623,
     UMINUS = 624,
     TYPECAST = 625
   };
#endif
/* Tokens.  */
#define ABORT_P 258
#define ABSOLUTE_P 259
#define ACCESS 260
#define ACTION 261
#define ADD_P 262
#define ADMIN 263
#define AFTER 264
#define AGGREGATE 265
#define ALL 266
#define ALSO 267
#define ALTER 268
#define ANALYSE 269
#define ANALYZE 270
#define AND 271
#define ANY 272
#define AROUND 273
#define ARRAY 274
#define AS 275
#define ASC 276
#define ASSERTION 277
#define ASSIGNMENT 278
#define ASYMMETRIC 279
#define AT 280
#define AUTHORIZATION 281
#define BACKWARD 282
#define BEFORE 283
#define BEGIN_P 284
#define BETWEEN 285
#define BIGINT 286
#define BINARY 287
#define BIT 288
#define BOOLEAN_P 289
#define BOTH 290
#define BY 291
#define CACHE 292
#define CALLED 293
#define CASCADE 294
#define CASCADED 295
#define CASE 296
#define CAST 297
#define CHAIN 298
#define CHAR_P 299
#define CHARACTER 300
#define CHARACTERISTICS 301
#define CHECK 302
#define CHECKPOINT 303
#define CLASS 304
#define CLOSE 305
#define CLUSTER 306
#define COALESCE 307
#define COLLATE 308
#define COLUMN 309
#define COMMENT 310
#define COMMIT 311
#define COMMITTED 312
#define CONCURRENTLY 313
#define CONNECTION 314
#define CONSTRAINT 315
#define CONSTRAINTS 316
#define CONVERSION_P 317
#define CONVERT 318
#define COPY 319
#define CREATE 320
#define CREATEDB 321
#define CREATEROLE 322
#define CREATEUSER 323
#define CROSS 324
#define CSV 325
#define CURRENT_DATE 326
#define CURRENT_ROLE 327
#define CURRENT_TIME 328
#define CURRENT_TIMESTAMP 329
#define CURRENT_USER 330
#define CURSOR 331
#define CYCLE 332
#define DATABASE 333
#define DAY_P 334
#define DEALLOCATE 335
#define DEC 336
#define DECIMAL_P 337
#define DECLARE 338
#define DEFAULT 339
#define DEFAULTS 340
#define DEFERRABLE 341
#define DEFERRED 342
#define DEFINER 343
#define DELETE_P 344
#define DELIMITER 345
#define DELIMITERS 346
#define DESC 347
#define DISABLE_P 348
#define DISTANCEALL 349
#define DISTANCEANY 350
#define DISTINCT 351
#define DO 352
#define DOMAIN_P 353
#define DOUBLE_P 354
#define DROP 355
#define DUPLICATE 356
#define EACH 357
#define ELIMINATE 358
#define ELSE 359
#define ENABLE_P 360
#define ENCODING 361
#define ENCRYPTED 362
#define END_P 363
#define ESCAPE 364
#define EXCEPT 365
#define EXCLUDING 366
#define EXCLUSIVE 367
#define EXECUTE 368
#define EXISTS 369
#define EXPLAIN 370
#define EXTERNAL 371
#define EXTRACT 372
#define FALSE_P 373
#define FETCH 374
#define FIRST_P 375
#define FLOAT_P 376
#define FOR 377
#define FORCE 378
#define FOREIGN 379
#define FORWARD 380
#define FREEZE 381
#define FROM 382
#define FULL 383
#define FUNCTION 384
#define GLOBAL 385
#define GRANT 386
#define GRANTED 387
#define GREATEST 388
#define GROUP_P 389
#define HANDLER 390
#define HAVING 391
#define HEADER_P 392
#define HOLD 393
#define HOUR_P 394
#define IF_P 395
#define ILIKE 396
#define IMMEDIATE 397
#define IMMUTABLE 398
#define IMPLICIT_P 399
#define IN_P 400
#define INCLUDING 401
#define INCREMENT 402
#define INDEX 403
#define INDEXES 404
#define INHERIT 405
#define INHERITS 406
#define INITIALLY 407
#define INNER_P 408
#define INOUT 409
#define INPUT_P 410
#define INSENSITIVE 411
#define INSERT 412
#define INSTEAD 413
#define INT_P 414
#define INTEGER 415
#define INTERSECT 416
#define INTERVAL 417
#define INTO 418
#define INVOKER 419
#define IS 420
#define ISNULL 421
#define ISOLATION 422
#define JOIN 423
#define KEY 424
#define LANCOMPILER 425
#define LANGUAGE 426
#define LARGE_P 427
#define LAST_P 428
#define LEADING 429
#define LEAST 430
#define LEFT 431
#define LEVEL 432
#define LIKE 433
#define LIMIT 434
#define LISTEN 435
#define LOAD 436
#define LOCAL 437
#define LOCALTIME 438
#define LOCALTIMESTAMP 439
#define LOCATION 440
#define LOCK_P 441
#define LOGIN_P 442
#define LONE 443
#define LTWO 444
#define MATCH 445
#define MAXVALUE 446
#define METRIC 447
#define MINUTE_P 448
#define MINVALUE 449
#define MODE 450
#define MONTH_P 451
#define MOVE 452
#define MAXELEMSEP 453
#define MAXGROUPDIAM 454
#define MAXJOINDIAM 455
#define NAMES 456
#define NATIONAL 457
#define NATURAL 458
#define NCHAR 459
#define NEW 460
#define NEW_GROUP 461
#define NEXT 462
#define NO 463
#define NOCREATEDB 464
#define NOCREATEROLE 465
#define NOCREATEUSER 466
#define NOINHERIT 467
#define NOLOGIN_P 468
#define NONE 469
#define NOSUPERUSER 470
#define NOT 471
#define NOTHING 472
#define NOTIFY 473
#define NOTNULL 474
#define NOWAIT 475
#define NULL_P 476
#define NULLIF 477
#define NUMERIC 478
#define OBJECT_P 479
#define OF 480
#define OFF 481
#define OFFSET 482
#define OIDS 483
#define OLD 484
#define ON 485
#define ON_OVERLAP 486
#define ONLY 487
#define OPERATOR 488
#define OPTION 489
#define OR 490
#define ORDER 491
#define OUT_P 492
#define OUTER_P 493
#define OVERLAPS 494
#define OVERLAY 495
#define OWNED 496
#define OWNER 497
#define PARTIAL 498
#define PASSWORD 499
#define PLACING 500
#define POSITION 501
#define PRECISION 502
#define PRESERVE 503
#define PREPARE 504
#define PREPARED 505
#define PRIMARY 506
#define PRIOR 507
#define PRIVILEGES 508
#define PROCEDURAL 509
#define PROCEDURE 510
#define QUOTE 511
#define READ 512
#define REAL 513
#define REASSIGN 514
#define RECHECK 515
#define REFERENCES 516
#define REINDEX 517
#define RELATIVE_P 518
#define RELEASE 519
#define RENAME 520
#define REPEATABLE 521
#define REPLACE 522
#define RESET 523
#define RESTART 524
#define RESTRICT 525
#define RETURNING 526
#define RETURNS 527
#define REVOKE 528
#define RIGHT 529
#define ROLE 530
#define ROLLBACK 531
#define ROW 532
#define ROWS 533
#define RULE 534
#define SAVEPOINT 535
#define SCHEMA 536
#define SCROLL 537
#define SECOND_P 538
#define SECURITY 539
#define SELECT 540
#define SEQUENCE 541
#define SERIALIZABLE 542
#define SESSION 543
#define SESSION_USER 544
#define SET 545
#define SETOF 546
#define SHARE 547
#define SHOW 548
#define SIMILAR 549
#define SIMPLE 550
#define SMALLINT 551
#define SOME 552
#define STABLE 553
#define START 554
#define STATEMENT 555
#define STATISTICS 556
#define STDIN 557
#define STDOUT 558
#define STORAGE 559
#define STRICT_P 560
#define SUBSTRING 561
#define SUPERUSER_P 562
#define SYMMETRIC 563
#define SYSID 564
#define SYSTEM_P 565
#define TABLE 566
#define TABLESPACE 567
#define TEMP 568
#define TEMPLATE 569
#define TEMPORARY 570
#define THEN 571
#define TIME 572
#define TIMESTAMP 573
#define TO 574
#define TRAILING 575
#define TRANSACTION 576
#define TREAT 577
#define TRIGGER 578
#define TRIM 579
#define TRUE_P 580
#define TRUNCATE 581
#define TRUSTED 582
#define TYPE_P 583
#define UNCOMMITTED 584
#define UNENCRYPTED 585
#define UNION 586
#define UNIQUE 587
#define UNKNOWN 588
#define UNLISTEN 589
#define UNTIL 590
#define UPDATE 591
#define USER 592
#define USING 593
#define VACUUM 594
#define VALID 595
#define VALIDATOR 596
#define VALUES 597
#define VARCHAR 598
#define VARYING 599
#define VERBOSE 600
#define VIEW 601
#define VOLATILE 602
#define WHEN 603
#define WHERE 604
#define WITH 605
#define WITHIN 606
#define WITHOUT 607
#define WORK 608
#define WRITE 609
#define YEAR_P 610
#define ZONE 611
#define WITH_CASCADED 612
#define WITH_LOCAL 613
#define WITH_CHECK 614
#define IDENT 615
#define FCONST 616
#define SCONST 617
#define BCONST 618
#define XCONST 619
#define Op 620
#define ICONST 621
#define PARAM 622
#define POSTFIXOP 623
#define UMINUS 624
#define TYPECAST 625




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 1676 of yacc.c  */
#line 116 "gram.y"

	int					ival;
	char				chr;
	char				*str;
	const char			*keyword;
	bool				boolean;
	JoinType			jtype;
	DropBehavior		dbehavior;
	OnCommitAction		oncommit;
	List				*list;
	Node				*node;
	Value				*value;
	ObjectType			objtype;

	TypeName			*typnam;
	FunctionParameter   *fun_param;
	FunctionParameterMode fun_param_mode;
	FuncWithArgs		*funwithargs;
	DefElem				*defelt;
	SortBy				*sortby;
	JoinExpr			*jexpr;
	IndexElem			*ielem;
	Alias				*alias;
	RangeVar			*range;
	A_Indices			*aind;
	ResTarget			*target;
	PrivTarget			*privtarget;

	InsertStmt			*istmt;
	VariableSetStmt		*vsetstmt;



/* Line 1676 of yacc.c  */
#line 826 "y.tab.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE base_yylval;

#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} YYLTYPE;
# define yyltype YYLTYPE /* obsolescent; will be withdrawn */
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif

extern YYLTYPE base_yylloc;

