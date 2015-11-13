
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
     K_ALIAS = 258,
     K_ASSIGN = 259,
     K_BEGIN = 260,
     K_BY = 261,
     K_CLOSE = 262,
     K_CONSTANT = 263,
     K_CONTINUE = 264,
     K_CURSOR = 265,
     K_DEBUG = 266,
     K_DECLARE = 267,
     K_DEFAULT = 268,
     K_DIAGNOSTICS = 269,
     K_DOTDOT = 270,
     K_ELSE = 271,
     K_ELSIF = 272,
     K_END = 273,
     K_EXCEPTION = 274,
     K_EXECUTE = 275,
     K_EXIT = 276,
     K_FOR = 277,
     K_FETCH = 278,
     K_FROM = 279,
     K_GET = 280,
     K_IF = 281,
     K_IN = 282,
     K_INFO = 283,
     K_INSERT = 284,
     K_INTO = 285,
     K_IS = 286,
     K_LOG = 287,
     K_LOOP = 288,
     K_NEXT = 289,
     K_NOT = 290,
     K_NOTICE = 291,
     K_NULL = 292,
     K_OPEN = 293,
     K_OR = 294,
     K_PERFORM = 295,
     K_ROW_COUNT = 296,
     K_RAISE = 297,
     K_RENAME = 298,
     K_RESULT_OID = 299,
     K_RETURN = 300,
     K_REVERSE = 301,
     K_STRICT = 302,
     K_THEN = 303,
     K_TO = 304,
     K_TYPE = 305,
     K_WARNING = 306,
     K_WHEN = 307,
     K_WHILE = 308,
     T_FUNCTION = 309,
     T_TRIGGER = 310,
     T_STRING = 311,
     T_NUMBER = 312,
     T_SCALAR = 313,
     T_ROW = 314,
     T_RECORD = 315,
     T_DTYPE = 316,
     T_LABEL = 317,
     T_WORD = 318,
     T_ERROR = 319,
     O_OPTION = 320,
     O_DUMP = 321
   };
#endif
/* Tokens.  */
#define K_ALIAS 258
#define K_ASSIGN 259
#define K_BEGIN 260
#define K_BY 261
#define K_CLOSE 262
#define K_CONSTANT 263
#define K_CONTINUE 264
#define K_CURSOR 265
#define K_DEBUG 266
#define K_DECLARE 267
#define K_DEFAULT 268
#define K_DIAGNOSTICS 269
#define K_DOTDOT 270
#define K_ELSE 271
#define K_ELSIF 272
#define K_END 273
#define K_EXCEPTION 274
#define K_EXECUTE 275
#define K_EXIT 276
#define K_FOR 277
#define K_FETCH 278
#define K_FROM 279
#define K_GET 280
#define K_IF 281
#define K_IN 282
#define K_INFO 283
#define K_INSERT 284
#define K_INTO 285
#define K_IS 286
#define K_LOG 287
#define K_LOOP 288
#define K_NEXT 289
#define K_NOT 290
#define K_NOTICE 291
#define K_NULL 292
#define K_OPEN 293
#define K_OR 294
#define K_PERFORM 295
#define K_ROW_COUNT 296
#define K_RAISE 297
#define K_RENAME 298
#define K_RESULT_OID 299
#define K_RETURN 300
#define K_REVERSE 301
#define K_STRICT 302
#define K_THEN 303
#define K_TO 304
#define K_TYPE 305
#define K_WARNING 306
#define K_WHEN 307
#define K_WHILE 308
#define T_FUNCTION 309
#define T_TRIGGER 310
#define T_STRING 311
#define T_NUMBER 312
#define T_SCALAR 313
#define T_ROW 314
#define T_RECORD 315
#define T_DTYPE 316
#define T_LABEL 317
#define T_WORD 318
#define T_ERROR 319
#define O_OPTION 320
#define O_DUMP 321




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 1676 of yacc.c  */
#line 52 "gram.y"

		int32					ival;
		bool					boolean;
		char					*str;
		struct
		{
			char *name;
			int  lineno;
		}						varname;
		struct
		{
			char *name;
			int  lineno;
			PLpgSQL_datum   *scalar;
			PLpgSQL_rec     *rec;
			PLpgSQL_row     *row;
		}						forvariable;
		struct
		{
			char *label;
			int  n_initvars;
			int  *initvarnos;
		}						declhdr;
		struct
		{
			char *end_label;
			List *stmts;
		}						loop_body;
		List					*list;
		PLpgSQL_type			*dtype;
		PLpgSQL_datum			*scalar;	/* a VAR, RECFIELD, or TRIGARG */
		PLpgSQL_variable		*variable;	/* a VAR, REC, or ROW */
		PLpgSQL_var				*var;
		PLpgSQL_row				*row;
		PLpgSQL_rec				*rec;
		PLpgSQL_expr			*expr;
		PLpgSQL_stmt			*stmt;
		PLpgSQL_stmt_block		*program;
		PLpgSQL_condition		*condition;
		PLpgSQL_exception		*exception;
		PLpgSQL_exception_block	*exception_block;
		PLpgSQL_nsitem			*nsitem;
		PLpgSQL_diag_item		*diagitem;



/* Line 1676 of yacc.c  */
#line 231 "y.tab.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE plpgsql_yylval;


