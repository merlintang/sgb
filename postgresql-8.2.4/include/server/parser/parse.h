
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
     L2 = 443,
     LINF = 444,
     MATCH = 445,
     MAXVALUE = 446,
     MINUTE_P = 447,
     MINVALUE = 448,
     MODE = 449,
     MONTH_P = 450,
     MOVE = 451,
     MAXELEMSEP = 452,
     MAXGROUPDIAM = 453,
     MAXJOINDIAM = 454,
     NAMES = 455,
     NATIONAL = 456,
     NATURAL = 457,
     NCHAR = 458,
     NEW = 459,
     NEW_GROUP = 460,
     NEXT = 461,
     NO = 462,
     NOCREATEDB = 463,
     NOCREATEROLE = 464,
     NOCREATEUSER = 465,
     NOINHERIT = 466,
     NOLOGIN_P = 467,
     NONE = 468,
     NOSUPERUSER = 469,
     NOT = 470,
     NOTHING = 471,
     NOTIFY = 472,
     NOTNULL = 473,
     NOWAIT = 474,
     NULL_P = 475,
     NULLIF = 476,
     NUMERIC = 477,
     OBJECT_P = 478,
     OF = 479,
     OFF = 480,
     OFFSET = 481,
     OIDS = 482,
     OLD = 483,
     ON = 484,
     ON_OVERLAP = 485,
     ONLY = 486,
     OPERATOR = 487,
     OPTION = 488,
     OR = 489,
     ORDER = 490,
     OUT_P = 491,
     OUTER_P = 492,
     OVERLAPS = 493,
     OVERLAY = 494,
     OWNED = 495,
     OWNER = 496,
     PARTIAL = 497,
     PASSWORD = 498,
     PLACING = 499,
     POSITION = 500,
     PRECISION = 501,
     PRESERVE = 502,
     PREPARE = 503,
     PREPARED = 504,
     PRIMARY = 505,
     PRIOR = 506,
     PRIVILEGES = 507,
     PROCEDURAL = 508,
     PROCEDURE = 509,
     QUOTE = 510,
     READ = 511,
     REAL = 512,
     REASSIGN = 513,
     RECHECK = 514,
     REFERENCES = 515,
     REINDEX = 516,
     RELATIVE_P = 517,
     RELEASE = 518,
     RENAME = 519,
     REPEATABLE = 520,
     REPLACE = 521,
     RESET = 522,
     RESTART = 523,
     RESTRICT = 524,
     RETURNING = 525,
     RETURNS = 526,
     REVOKE = 527,
     RIGHT = 528,
     ROLE = 529,
     ROLLBACK = 530,
     ROW = 531,
     ROWS = 532,
     RULE = 533,
     SAVEPOINT = 534,
     SCHEMA = 535,
     SCROLL = 536,
     SECOND_P = 537,
     SECURITY = 538,
     SELECT = 539,
     SEQUENCE = 540,
     SERIALIZABLE = 541,
     SESSION = 542,
     SESSION_USER = 543,
     SET = 544,
     SETOF = 545,
     SHARE = 546,
     SHOW = 547,
     SIMILAR = 548,
     SIMPLE = 549,
     SMALLINT = 550,
     SOME = 551,
     STABLE = 552,
     START = 553,
     STATEMENT = 554,
     STATISTICS = 555,
     STDIN = 556,
     STDOUT = 557,
     STORAGE = 558,
     STRICT_P = 559,
     SUBSTRING = 560,
     SUPERUSER_P = 561,
     SYMMETRIC = 562,
     SYSID = 563,
     SYSTEM_P = 564,
     TABLE = 565,
     TABLESPACE = 566,
     TEMP = 567,
     TEMPLATE = 568,
     TEMPORARY = 569,
     THEN = 570,
     TIME = 571,
     TIMESTAMP = 572,
     TO = 573,
     TRAILING = 574,
     TRANSACTION = 575,
     TREAT = 576,
     TRIGGER = 577,
     TRIM = 578,
     TRUE_P = 579,
     TRUNCATE = 580,
     TRUSTED = 581,
     TYPE_P = 582,
     UNCOMMITTED = 583,
     UNENCRYPTED = 584,
     UNION = 585,
     UNIQUE = 586,
     UNKNOWN = 587,
     UNLISTEN = 588,
     UNTIL = 589,
     UPDATE = 590,
     USER = 591,
     USING = 592,
     VACUUM = 593,
     VALID = 594,
     VALIDATOR = 595,
     VALUES = 596,
     VARCHAR = 597,
     VARYING = 598,
     VERBOSE = 599,
     VIEW = 600,
     VOLATILE = 601,
     WHEN = 602,
     WHERE = 603,
     WITH = 604,
     WITHIN = 605,
     WITHOUT = 606,
     WORK = 607,
     WRITE = 608,
     YEAR_P = 609,
     ZONE = 610,
     WITH_CASCADED = 611,
     WITH_LOCAL = 612,
     WITH_CHECK = 613,
     IDENT = 614,
     FCONST = 615,
     SCONST = 616,
     BCONST = 617,
     XCONST = 618,
     Op = 619,
     ICONST = 620,
     PARAM = 621,
     POSTFIXOP = 622,
     UMINUS = 623,
     TYPECAST = 624
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
#define L2 443
#define LINF 444
#define MATCH 445
#define MAXVALUE 446
#define MINUTE_P 447
#define MINVALUE 448
#define MODE 449
#define MONTH_P 450
#define MOVE 451
#define MAXELEMSEP 452
#define MAXGROUPDIAM 453
#define MAXJOINDIAM 454
#define NAMES 455
#define NATIONAL 456
#define NATURAL 457
#define NCHAR 458
#define NEW 459
#define NEW_GROUP 460
#define NEXT 461
#define NO 462
#define NOCREATEDB 463
#define NOCREATEROLE 464
#define NOCREATEUSER 465
#define NOINHERIT 466
#define NOLOGIN_P 467
#define NONE 468
#define NOSUPERUSER 469
#define NOT 470
#define NOTHING 471
#define NOTIFY 472
#define NOTNULL 473
#define NOWAIT 474
#define NULL_P 475
#define NULLIF 476
#define NUMERIC 477
#define OBJECT_P 478
#define OF 479
#define OFF 480
#define OFFSET 481
#define OIDS 482
#define OLD 483
#define ON 484
#define ON_OVERLAP 485
#define ONLY 486
#define OPERATOR 487
#define OPTION 488
#define OR 489
#define ORDER 490
#define OUT_P 491
#define OUTER_P 492
#define OVERLAPS 493
#define OVERLAY 494
#define OWNED 495
#define OWNER 496
#define PARTIAL 497
#define PASSWORD 498
#define PLACING 499
#define POSITION 500
#define PRECISION 501
#define PRESERVE 502
#define PREPARE 503
#define PREPARED 504
#define PRIMARY 505
#define PRIOR 506
#define PRIVILEGES 507
#define PROCEDURAL 508
#define PROCEDURE 509
#define QUOTE 510
#define READ 511
#define REAL 512
#define REASSIGN 513
#define RECHECK 514
#define REFERENCES 515
#define REINDEX 516
#define RELATIVE_P 517
#define RELEASE 518
#define RENAME 519
#define REPEATABLE 520
#define REPLACE 521
#define RESET 522
#define RESTART 523
#define RESTRICT 524
#define RETURNING 525
#define RETURNS 526
#define REVOKE 527
#define RIGHT 528
#define ROLE 529
#define ROLLBACK 530
#define ROW 531
#define ROWS 532
#define RULE 533
#define SAVEPOINT 534
#define SCHEMA 535
#define SCROLL 536
#define SECOND_P 537
#define SECURITY 538
#define SELECT 539
#define SEQUENCE 540
#define SERIALIZABLE 541
#define SESSION 542
#define SESSION_USER 543
#define SET 544
#define SETOF 545
#define SHARE 546
#define SHOW 547
#define SIMILAR 548
#define SIMPLE 549
#define SMALLINT 550
#define SOME 551
#define STABLE 552
#define START 553
#define STATEMENT 554
#define STATISTICS 555
#define STDIN 556
#define STDOUT 557
#define STORAGE 558
#define STRICT_P 559
#define SUBSTRING 560
#define SUPERUSER_P 561
#define SYMMETRIC 562
#define SYSID 563
#define SYSTEM_P 564
#define TABLE 565
#define TABLESPACE 566
#define TEMP 567
#define TEMPLATE 568
#define TEMPORARY 569
#define THEN 570
#define TIME 571
#define TIMESTAMP 572
#define TO 573
#define TRAILING 574
#define TRANSACTION 575
#define TREAT 576
#define TRIGGER 577
#define TRIM 578
#define TRUE_P 579
#define TRUNCATE 580
#define TRUSTED 581
#define TYPE_P 582
#define UNCOMMITTED 583
#define UNENCRYPTED 584
#define UNION 585
#define UNIQUE 586
#define UNKNOWN 587
#define UNLISTEN 588
#define UNTIL 589
#define UPDATE 590
#define USER 591
#define USING 592
#define VACUUM 593
#define VALID 594
#define VALIDATOR 595
#define VALUES 596
#define VARCHAR 597
#define VARYING 598
#define VERBOSE 599
#define VIEW 600
#define VOLATILE 601
#define WHEN 602
#define WHERE 603
#define WITH 604
#define WITHIN 605
#define WITHOUT 606
#define WORK 607
#define WRITE 608
#define YEAR_P 609
#define ZONE 610
#define WITH_CASCADED 611
#define WITH_LOCAL 612
#define WITH_CHECK 613
#define IDENT 614
#define FCONST 615
#define SCONST 616
#define BCONST 617
#define XCONST 618
#define Op 619
#define ICONST 620
#define PARAM 621
#define POSTFIXOP 622
#define UMINUS 623
#define TYPECAST 624
#define METRIC 625



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
#line 824 "y.tab.h"
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

