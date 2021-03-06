/*-------------------------------------------------------------------------
 *
 * utility.c
 *	  Contains functions which control the execution of the POSTGRES utility
 *	  commands.  At one time acted as an interface between the Lisp and C
 *	  systems.
 *
 * Portions Copyright (c) 1996-2006, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/tcop/utility.c,v 1.269 2006/10/04 00:29:58 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/twophase.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/namespace.h"
#include "catalog/toasting.h"
#include "commands/alter.h"
#include "commands/async.h"
#include "commands/cluster.h"
#include "commands/comment.h"
#include "commands/conversioncmds.h"
#include "commands/copy.h"
#include "commands/dbcommands.h"
#include "commands/defrem.h"
#include "commands/explain.h"
#include "commands/lockcmds.h"
#include "commands/portalcmds.h"
#include "commands/prepare.h"
#include "commands/proclang.h"
#include "commands/schemacmds.h"
#include "commands/sequence.h"
#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "commands/trigger.h"
#include "commands/typecmds.h"
#include "commands/user.h"
#include "commands/vacuum.h"
#include "commands/view.h"
#include "miscadmin.h"
#include "postmaster/bgwriter.h"
#include "rewrite/rewriteDefine.h"
#include "rewrite/rewriteRemove.h"
#include "storage/fd.h"
#include "tcop/pquery.h"
#include "tcop/utility.h"
#include "utils/acl.h"
#include "utils/guc.h"
#include "utils/syscache.h"


/*
 * Error-checking support for DROP commands
 */

struct msgstrings
{
	char		kind;
	int			nonexistent_code;
	const char *nonexistent_msg;
	const char *skipping_msg;
	const char *nota_msg;
	const char *drophint_msg;
};

static const struct msgstrings msgstringarray[] = {
	{RELKIND_RELATION,
		ERRCODE_UNDEFINED_TABLE,
		gettext_noop("table \"%s\" does not exist"),
		gettext_noop("table \"%s\" does not exist, skipping"),
		gettext_noop("\"%s\" is not a table"),
	gettext_noop("Use DROP TABLE to remove a table.")},
	{RELKIND_SEQUENCE,
		ERRCODE_UNDEFINED_TABLE,
		gettext_noop("sequence \"%s\" does not exist"),
		gettext_noop("sequence \"%s\" does not exist, skipping"),
		gettext_noop("\"%s\" is not a sequence"),
	gettext_noop("Use DROP SEQUENCE to remove a sequence.")},
	{RELKIND_VIEW,
		ERRCODE_UNDEFINED_TABLE,
		gettext_noop("view \"%s\" does not exist"),
		gettext_noop("view \"%s\" does not exist, skipping"),
		gettext_noop("\"%s\" is not a view"),
	gettext_noop("Use DROP VIEW to remove a view.")},
	{RELKIND_INDEX,
		ERRCODE_UNDEFINED_OBJECT,
		gettext_noop("index \"%s\" does not exist"),
		gettext_noop("index \"%s\" does not exist, skipping"),
		gettext_noop("\"%s\" is not an index"),
	gettext_noop("Use DROP INDEX to remove an index.")},
	{RELKIND_COMPOSITE_TYPE,
		ERRCODE_UNDEFINED_OBJECT,
		gettext_noop("type \"%s\" does not exist"),
		gettext_noop("type \"%s\" does not exist, skipping"),
		gettext_noop("\"%s\" is not a type"),
	gettext_noop("Use DROP TYPE to remove a type.")},
	{'\0', 0, NULL, NULL, NULL}
};


/*
 * Emit the right error message for a "DROP" command issued on a
 * relation of the wrong type
 */
static void
DropErrorMsgWrongType(char *relname, char wrongkind, char rightkind)
{
	const struct msgstrings *rentry;
	const struct msgstrings *wentry;

	for (rentry = msgstringarray; rentry->kind != '\0'; rentry++)
		if (rentry->kind == rightkind)
			break;
	Assert(rentry->kind != '\0');

	for (wentry = msgstringarray; wentry->kind != '\0'; wentry++)
		if (wentry->kind == wrongkind)
			break;
	/* wrongkind could be something we don't have in our table... */

	ereport(ERROR,
			(errcode(ERRCODE_WRONG_OBJECT_TYPE),
			 errmsg(rentry->nota_msg, relname),
			 (wentry->kind != '\0') ? errhint(wentry->drophint_msg) : 0));
}

/*
 * Emit the right error message for a "DROP" command issued on a
 * non-existent relation
 */
static void
DropErrorMsgNonExistent(RangeVar *rel, char rightkind, bool missing_ok)
{
	const struct msgstrings *rentry;

	for (rentry = msgstringarray; rentry->kind != '\0'; rentry++)
	{
		if (rentry->kind == rightkind)
		{
			if (!missing_ok)
			{
				ereport(ERROR,
						(errcode(rentry->nonexistent_code),
						 errmsg(rentry->nonexistent_msg, rel->relname)));
			}
			else
			{
				ereport(NOTICE, (errmsg(rentry->skipping_msg, rel->relname)));
				break;
			}
		}
	}

	Assert(rentry->kind != '\0');		/* Should be impossible */
}

/*
 * returns false if missing_ok is true and the object does not exist,
 * true if object exists and permissions are OK,
 * errors otherwise
 *
 */

static bool
CheckDropPermissions(RangeVar *rel, char rightkind, bool missing_ok)
{
	Oid			relOid;
	HeapTuple	tuple;
	Form_pg_class classform;

	relOid = RangeVarGetRelid(rel, true);
	if (!OidIsValid(relOid))
	{
		DropErrorMsgNonExistent(rel, rightkind, missing_ok);
		return false;
	}

	tuple = SearchSysCache(RELOID,
						   ObjectIdGetDatum(relOid),
						   0, 0, 0);
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for relation %u", relOid);

	classform = (Form_pg_class) GETSTRUCT(tuple);

	if (classform->relkind != rightkind)
		DropErrorMsgWrongType(rel->relname, classform->relkind,
							  rightkind);

	/* Allow DROP to either table owner or schema owner */
	if (!pg_class_ownercheck(relOid, GetUserId()) &&
		!pg_namespace_ownercheck(classform->relnamespace, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
					   rel->relname);

	if (!allowSystemTableMods && IsSystemClass(classform))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied: \"%s\" is a system catalog",
						rel->relname)));

	ReleaseSysCache(tuple);

	return true;
}

/*
 * Verify user has ownership of specified relation, else ereport.
 *
 * If noCatalogs is true then we also deny access to system catalogs,
 * except when allowSystemTableMods is true.
 */
void
CheckRelationOwnership(RangeVar *rel, bool noCatalogs)
{
	Oid			relOid;
	HeapTuple	tuple;

	relOid = RangeVarGetRelid(rel, false);
	tuple = SearchSysCache(RELOID,
						   ObjectIdGetDatum(relOid),
						   0, 0, 0);
	if (!HeapTupleIsValid(tuple))		/* should not happen */
		elog(ERROR, "cache lookup failed for relation %u", relOid);

	if (!pg_class_ownercheck(relOid, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
					   rel->relname);

	if (noCatalogs)
	{
		if (!allowSystemTableMods &&
			IsSystemClass((Form_pg_class) GETSTRUCT(tuple)))
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("permission denied: \"%s\" is a system catalog",
							rel->relname)));
	}

	ReleaseSysCache(tuple);
}


/*
 * QueryIsReadOnly: is an analyzed/rewritten query read-only?
 *
 * This is a much stricter test than we apply for XactReadOnly mode;
 * the query must be *in truth* read-only, because the caller wishes
 * not to do CommandCounterIncrement for it.
 */
bool
QueryIsReadOnly(Query *parsetree)
{
	switch (parsetree->commandType)
	{
		case CMD_SELECT:
			if (parsetree->into != NULL)
				return false;	/* SELECT INTO */
			else if (parsetree->rowMarks != NIL)
				return false;	/* SELECT FOR UPDATE/SHARE */
			else
				return true;
		case CMD_UPDATE:
		case CMD_INSERT:
		case CMD_DELETE:
			return false;
		case CMD_UTILITY:
			/* For now, treat all utility commands as read/write */
			return false;
		default:
			elog(WARNING, "unrecognized commandType: %d",
				 (int) parsetree->commandType);
			break;
	}
	return false;
}

/*
 * check_xact_readonly: is a utility command read-only?
 *
 * Here we use the loose rules of XactReadOnly mode: no permanent effects
 * on the database are allowed.
 */
static void
check_xact_readonly(Node *parsetree)
{
	if (!XactReadOnly)
		return;

	/*
	 * Note: Commands that need to do more complicated checking are handled
	 * elsewhere.
	 */

	switch (nodeTag(parsetree))
	{
		case T_AlterDatabaseStmt:
		case T_AlterDatabaseSetStmt:
		case T_AlterDomainStmt:
		case T_AlterFunctionStmt:
		case T_AlterRoleStmt:
		case T_AlterRoleSetStmt:
		case T_AlterObjectSchemaStmt:
		case T_AlterOwnerStmt:
		case T_AlterSeqStmt:
		case T_AlterTableStmt:
		case T_RenameStmt:
		case T_CommentStmt:
		case T_DefineStmt:
		case T_CreateCastStmt:
		case T_CreateConversionStmt:
		case T_CreatedbStmt:
		case T_CreateDomainStmt:
		case T_CreateFunctionStmt:
		case T_CreateRoleStmt:
		case T_IndexStmt:
		case T_CreatePLangStmt:
		case T_CreateOpClassStmt:
		case T_RuleStmt:
		case T_CreateSchemaStmt:
		case T_CreateSeqStmt:
		case T_CreateStmt:
		case T_CreateTableSpaceStmt:
		case T_CreateTrigStmt:
		case T_CompositeTypeStmt:
		case T_ViewStmt:
		case T_DropCastStmt:
		case T_DropStmt:
		case T_DropdbStmt:
		case T_DropTableSpaceStmt:
		case T_RemoveFuncStmt:
		case T_DropRoleStmt:
		case T_DropPLangStmt:
		case T_RemoveOpClassStmt:
		case T_DropPropertyStmt:
		case T_GrantStmt:
		case T_GrantRoleStmt:
		case T_TruncateStmt:
		case T_DropOwnedStmt:
		case T_ReassignOwnedStmt:
			ereport(ERROR,
					(errcode(ERRCODE_READ_ONLY_SQL_TRANSACTION),
					 errmsg("transaction is read-only")));
			break;
		default:
			/* do nothing */
			break;
	}
}


/*
 * ProcessUtility
 *		general utility function invoker
 *
 *	parsetree: the parse tree for the utility statement
 *	params: parameters to use during execution
 *	dest: where to send results
 *	completionTag: points to a buffer of size COMPLETION_TAG_BUFSIZE
 *		in which to store a command completion status string.
 *
 * completionTag is only set nonempty if we want to return a nondefault status.
 *
 * completionTag may be NULL if caller doesn't want a status string.
 */
void
ProcessUtility(Node *parsetree,
			   ParamListInfo params,
			   DestReceiver *dest,
			   char *completionTag)
{
	check_xact_readonly(parsetree);

	if (completionTag)
		completionTag[0] = '\0';

	switch (nodeTag(parsetree))
	{
			/*
			 * ******************** transactions ********************
			 */
		case T_TransactionStmt:
			{
				TransactionStmt *stmt = (TransactionStmt *) parsetree;

				switch (stmt->kind)
				{
						/*
						 * START TRANSACTION, as defined by SQL99: Identical
						 * to BEGIN.  Same code for both.
						 */
					case TRANS_STMT_BEGIN:
					case TRANS_STMT_START:
						{
							ListCell   *lc;

							BeginTransactionBlock();
							foreach(lc, stmt->options)
							{
								DefElem    *item = (DefElem *) lfirst(lc);

								if (strcmp(item->defname, "transaction_isolation") == 0)
									SetPGVariable("transaction_isolation",
												  list_make1(item->arg),
												  true);
								else if (strcmp(item->defname, "transaction_read_only") == 0)
									SetPGVariable("transaction_read_only",
												  list_make1(item->arg),
												  true);
							}
						}
						break;

					case TRANS_STMT_COMMIT:
						if (!EndTransactionBlock())
						{
							/* report unsuccessful commit in completionTag */
							if (completionTag)
								strcpy(completionTag, "ROLLBACK");
						}
						break;

					case TRANS_STMT_PREPARE:
						if (!PrepareTransactionBlock(stmt->gid))
						{
							/* report unsuccessful commit in completionTag */
							if (completionTag)
								strcpy(completionTag, "ROLLBACK");
						}
						break;

					case TRANS_STMT_COMMIT_PREPARED:
						PreventTransactionChain(stmt, "COMMIT PREPARED");
						FinishPreparedTransaction(stmt->gid, true);
						break;

					case TRANS_STMT_ROLLBACK_PREPARED:
						PreventTransactionChain(stmt, "ROLLBACK PREPARED");
						FinishPreparedTransaction(stmt->gid, false);
						break;

					case TRANS_STMT_ROLLBACK:
						UserAbortTransactionBlock();
						break;

					case TRANS_STMT_SAVEPOINT:
						{
							ListCell   *cell;
							char	   *name = NULL;

							RequireTransactionChain((void *) stmt, "SAVEPOINT");

							foreach(cell, stmt->options)
							{
								DefElem    *elem = lfirst(cell);

								if (strcmp(elem->defname, "savepoint_name") == 0)
									name = strVal(elem->arg);
							}

							Assert(PointerIsValid(name));

							DefineSavepoint(name);
						}
						break;

					case TRANS_STMT_RELEASE:
						RequireTransactionChain((void *) stmt, "RELEASE SAVEPOINT");
						ReleaseSavepoint(stmt->options);
						break;

					case TRANS_STMT_ROLLBACK_TO:
						RequireTransactionChain((void *) stmt, "ROLLBACK TO SAVEPOINT");
						RollbackToSavepoint(stmt->options);

						/*
						 * CommitTransactionCommand is in charge of
						 * re-defining the savepoint again
						 */
						break;
				}
			}
			break;

			/*
			 * Portal (cursor) manipulation
			 */
		case T_DeclareCursorStmt:
			PerformCursorOpen((DeclareCursorStmt *) parsetree, params);
			break;

		case T_ClosePortalStmt:
			{
				ClosePortalStmt *stmt = (ClosePortalStmt *) parsetree;

				PerformPortalClose(stmt->portalname);
			}
			break;

		case T_FetchStmt:
			PerformPortalFetch((FetchStmt *) parsetree, dest,
							   completionTag);
			break;

			/*
			 * relation and attribute manipulation
			 */
		case T_CreateSchemaStmt:
			CreateSchemaCommand((CreateSchemaStmt *) parsetree);
			break;

		case T_CreateStmt:
			{
				Oid			relOid;

				//CreateStmt *test = (CreateStmt *) parsetree;
				//pprint(test);

				relOid = DefineRelation((CreateStmt *) parsetree,
										RELKIND_RELATION);

				/*
				 * Let AlterTableCreateToastTable decide if this one needs a
				 * secondary relation too.
				 */
				CommandCounterIncrement();
				AlterTableCreateToastTable(relOid);
			}
			break;

		case T_CreateTableSpaceStmt:
			CreateTableSpace((CreateTableSpaceStmt *) parsetree);
			break;

		case T_DropTableSpaceStmt:
			DropTableSpace((DropTableSpaceStmt *) parsetree);
			break;

		case T_DropStmt:
			{
				DropStmt   *stmt = (DropStmt *) parsetree;
				ListCell   *arg;

				foreach(arg, stmt->objects)
				{
					List	   *names = (List *) lfirst(arg);
					RangeVar   *rel;

					switch (stmt->removeType)
					{
						case OBJECT_TABLE:
							rel = makeRangeVarFromNameList(names);
							if (CheckDropPermissions(rel, RELKIND_RELATION,
													 stmt->missing_ok))
								RemoveRelation(rel, stmt->behavior);
							break;

						case OBJECT_SEQUENCE:
							rel = makeRangeVarFromNameList(names);
							if (CheckDropPermissions(rel, RELKIND_SEQUENCE,
													 stmt->missing_ok))
								RemoveRelation(rel, stmt->behavior);
							break;

						case OBJECT_VIEW:
							rel = makeRangeVarFromNameList(names);
							if (CheckDropPermissions(rel, RELKIND_VIEW,
													 stmt->missing_ok))
								RemoveView(rel, stmt->behavior);
							break;

						case OBJECT_INDEX:
							rel = makeRangeVarFromNameList(names);
							if (CheckDropPermissions(rel, RELKIND_INDEX,
													 stmt->missing_ok))
								RemoveIndex(rel, stmt->behavior);
							break;

						case OBJECT_TYPE:
							/* RemoveType does its own permissions checks */
							RemoveType(names, stmt->behavior,
									   stmt->missing_ok);
							break;

						case OBJECT_DOMAIN:

							/*
							 * RemoveDomain does its own permissions checks
							 */
							RemoveDomain(names, stmt->behavior,
										 stmt->missing_ok);
							break;

						case OBJECT_CONVERSION:
							DropConversionCommand(names, stmt->behavior,
												  stmt->missing_ok);
							break;

						case OBJECT_SCHEMA:

							/*
							 * RemoveSchema does its own permissions checks
							 */
							RemoveSchema(names, stmt->behavior,
										 stmt->missing_ok);
							break;

						default:
							elog(ERROR, "unrecognized drop object type: %d",
								 (int) stmt->removeType);
							break;
					}

					/*
					 * We used to need to do CommandCounterIncrement() here,
					 * but now it's done inside performDeletion().
					 */
				}
			}
			break;

		case T_TruncateStmt:
			ExecuteTruncate((TruncateStmt *) parsetree);
			break;

		case T_CommentStmt:
			CommentObject((CommentStmt *) parsetree);
			break;

		case T_CopyStmt:
			{
				uint64		processed = DoCopy((CopyStmt *) parsetree);

				if (completionTag)
					snprintf(completionTag, COMPLETION_TAG_BUFSIZE,
							 "COPY " UINT64_FORMAT, processed);
			}
			break;

		case T_PrepareStmt:
			PrepareQuery((PrepareStmt *) parsetree);
			break;

		case T_ExecuteStmt:
			ExecuteQuery((ExecuteStmt *) parsetree, params,
						 dest, completionTag);
			break;

		case T_DeallocateStmt:
			DeallocateQuery((DeallocateStmt *) parsetree);
			break;

			/*
			 * schema
			 */
		case T_RenameStmt:
			ExecRenameStmt((RenameStmt *) parsetree);
			break;

		case T_AlterObjectSchemaStmt:
			ExecAlterObjectSchemaStmt((AlterObjectSchemaStmt *) parsetree);
			break;

		case T_AlterOwnerStmt:
			ExecAlterOwnerStmt((AlterOwnerStmt *) parsetree);
			break;

		case T_AlterTableStmt:
			AlterTable((AlterTableStmt *) parsetree);
			break;

		case T_AlterDomainStmt:
			{
				AlterDomainStmt *stmt = (AlterDomainStmt *) parsetree;

				/*
				 * Some or all of these functions are recursive to cover
				 * inherited things, so permission checks are done there.
				 */
				switch (stmt->subtype)
				{
					case 'T':	/* ALTER DOMAIN DEFAULT */

						/*
						 * Recursively alter column default for table and, if
						 * requested, for descendants
						 */
						AlterDomainDefault(stmt->typename,
										   stmt->def);
						break;
					case 'N':	/* ALTER DOMAIN DROP NOT NULL */
						AlterDomainNotNull(stmt->typename,
										   false);
						break;
					case 'O':	/* ALTER DOMAIN SET NOT NULL */
						AlterDomainNotNull(stmt->typename,
										   true);
						break;
					case 'C':	/* ADD CONSTRAINT */
						AlterDomainAddConstraint(stmt->typename,
												 stmt->def);
						break;
					case 'X':	/* DROP CONSTRAINT */
						AlterDomainDropConstraint(stmt->typename,
												  stmt->name,
												  stmt->behavior);
						break;
					default:	/* oops */
						elog(ERROR, "unrecognized alter domain type: %d",
							 (int) stmt->subtype);
						break;
				}
			}
			break;

		case T_GrantStmt:
			ExecuteGrantStmt((GrantStmt *) parsetree);
			break;

		case T_GrantRoleStmt:
			GrantRole((GrantRoleStmt *) parsetree);
			break;

			/*
			 * **************** object creation / destruction *****************
			 */
		case T_DefineStmt:
			{
				DefineStmt *stmt = (DefineStmt *) parsetree;

				switch (stmt->kind)
				{
					case OBJECT_AGGREGATE:
						DefineAggregate(stmt->defnames, stmt->args,
										stmt->oldstyle, stmt->definition);
						break;
					case OBJECT_OPERATOR:
						Assert(stmt->args == NIL);
						DefineOperator(stmt->defnames, stmt->definition);
						break;
					case OBJECT_TYPE:
						Assert(stmt->args == NIL);
						DefineType(stmt->defnames, stmt->definition);
						break;
					default:
						elog(ERROR, "unrecognized define stmt type: %d",
							 (int) stmt->kind);
						break;
				}
			}
			break;

		case T_CompositeTypeStmt:		/* CREATE TYPE (composite) */
			{
				CompositeTypeStmt *stmt = (CompositeTypeStmt *) parsetree;

				DefineCompositeType(stmt->typevar, stmt->coldeflist);
			}
			break;

		case T_ViewStmt:		/* CREATE VIEW */
			{
				ViewStmt   *stmt = (ViewStmt *) parsetree;

				DefineView(stmt->view, stmt->query, stmt->replace);
			}
			break;

		case T_CreateFunctionStmt:		/* CREATE FUNCTION */
			CreateFunction((CreateFunctionStmt *) parsetree);
			break;

		case T_AlterFunctionStmt:		/* ALTER FUNCTION */
			AlterFunction((AlterFunctionStmt *) parsetree);
			break;

		case T_IndexStmt:		/* CREATE INDEX */
			{
				IndexStmt  *stmt = (IndexStmt *) parsetree;

				if (stmt->concurrent)
					PreventTransactionChain(stmt, "CREATE INDEX CONCURRENTLY");

				CheckRelationOwnership(stmt->relation, true);

				DefineIndex(stmt->relation,		/* relation */
							stmt->idxname,		/* index name */
							InvalidOid, /* no predefined OID */
							stmt->accessMethod, /* am name */
							stmt->tableSpace,
							stmt->indexParams,	/* parameters */
							(Expr *) stmt->whereClause,
							stmt->rangetable,
							stmt->options,
							stmt->unique,
							stmt->primary,
							stmt->isconstraint,
							false,		/* is_alter_table */
							true,		/* check_rights */
							false,		/* skip_build */
							false,		/* quiet */
							stmt->concurrent);	/* concurrent */
			}
			break;

		case T_RuleStmt:		/* CREATE RULE */
			DefineQueryRewrite((RuleStmt *) parsetree);
			break;

		case T_CreateSeqStmt:
			DefineSequence((CreateSeqStmt *) parsetree);
			break;

		case T_AlterSeqStmt:
			AlterSequence((AlterSeqStmt *) parsetree);
			break;

		case T_RemoveFuncStmt:
			{
				RemoveFuncStmt *stmt = (RemoveFuncStmt *) parsetree;

				switch (stmt->kind)
				{
					case OBJECT_FUNCTION:
						RemoveFunction(stmt);
						break;
					case OBJECT_AGGREGATE:
						RemoveAggregate(stmt);
						break;
					case OBJECT_OPERATOR:
						RemoveOperator(stmt);
						break;
					default:
						elog(ERROR, "unrecognized object type: %d",
							 (int) stmt->kind);
						break;
				}
			}
			break;

		case T_CreatedbStmt:
			createdb((CreatedbStmt *) parsetree);
			break;

		case T_AlterDatabaseStmt:
			AlterDatabase((AlterDatabaseStmt *) parsetree);
			break;

		case T_AlterDatabaseSetStmt:
			AlterDatabaseSet((AlterDatabaseSetStmt *) parsetree);
			break;

		case T_DropdbStmt:
			{
				DropdbStmt *stmt = (DropdbStmt *) parsetree;

				dropdb(stmt->dbname, stmt->missing_ok);
			}
			break;

			/* Query-level asynchronous notification */
		case T_NotifyStmt:
			{
				NotifyStmt *stmt = (NotifyStmt *) parsetree;

				Async_Notify(stmt->relation->relname);
			}
			break;

		case T_ListenStmt:
			{
				ListenStmt *stmt = (ListenStmt *) parsetree;

				Async_Listen(stmt->relation->relname);
			}
			break;

		case T_UnlistenStmt:
			{
				UnlistenStmt *stmt = (UnlistenStmt *) parsetree;

				Async_Unlisten(stmt->relation->relname);
			}
			break;

		case T_LoadStmt:
			{
				LoadStmt   *stmt = (LoadStmt *) parsetree;

				closeAllVfds(); /* probably not necessary... */
				/* Allowed names are restricted if you're not superuser */
				load_file(stmt->filename, !superuser());
			}
			break;

		case T_ClusterStmt:
			cluster((ClusterStmt *) parsetree);
			break;

		case T_VacuumStmt:
			vacuum((VacuumStmt *) parsetree, NIL);
			break;

		case T_ExplainStmt:
			ExplainQuery((ExplainStmt *) parsetree, params, dest);
			break;

		case T_VariableSetStmt:
			{
				VariableSetStmt *n = (VariableSetStmt *) parsetree;

				/*
				 * Special cases for special SQL syntax that effectively sets
				 * more than one variable per statement.
				 */
				if (strcmp(n->name, "TRANSACTION") == 0)
				{
					ListCell   *head;

					foreach(head, n->args)
					{
						DefElem    *item = (DefElem *) lfirst(head);

						if (strcmp(item->defname, "transaction_isolation") == 0)
							SetPGVariable("transaction_isolation",
										  list_make1(item->arg), n->is_local);
						else if (strcmp(item->defname, "transaction_read_only") == 0)
							SetPGVariable("transaction_read_only",
										  list_make1(item->arg), n->is_local);
					}
				}
				else if (strcmp(n->name, "SESSION CHARACTERISTICS") == 0)
				{
					ListCell   *head;

					foreach(head, n->args)
					{
						DefElem    *item = (DefElem *) lfirst(head);

						if (strcmp(item->defname, "transaction_isolation") == 0)
							SetPGVariable("default_transaction_isolation",
										  list_make1(item->arg), n->is_local);
						else if (strcmp(item->defname, "transaction_read_only") == 0)
							SetPGVariable("default_transaction_read_only",
										  list_make1(item->arg), n->is_local);
					}
				}
				else
					SetPGVariable(n->name, n->args, n->is_local);
			}
			break;

		case T_VariableShowStmt:
			{
				VariableShowStmt *n = (VariableShowStmt *) parsetree;

				GetPGVariable(n->name, dest);
			}
			break;

		case T_VariableResetStmt:
			{
				VariableResetStmt *n = (VariableResetStmt *) parsetree;

				ResetPGVariable(n->name);
			}
			break;

		case T_CreateTrigStmt:
			CreateTrigger((CreateTrigStmt *) parsetree, false);
			break;

		case T_DropPropertyStmt:
			{
				DropPropertyStmt *stmt = (DropPropertyStmt *) parsetree;
				Oid			relId;

				relId = RangeVarGetRelid(stmt->relation, false);

				switch (stmt->removeType)
				{
					case OBJECT_RULE:
						/* RemoveRewriteRule checks permissions */
						RemoveRewriteRule(relId, stmt->property,
										  stmt->behavior, stmt->missing_ok);
						break;
					case OBJECT_TRIGGER:
						/* DropTrigger checks permissions */
						DropTrigger(relId, stmt->property,
									stmt->behavior, stmt->missing_ok);
						break;
					default:
						elog(ERROR, "unrecognized object type: %d",
							 (int) stmt->removeType);
						break;
				}
			}
			break;

		case T_CreatePLangStmt:
			CreateProceduralLanguage((CreatePLangStmt *) parsetree);
			break;

		case T_DropPLangStmt:
			DropProceduralLanguage((DropPLangStmt *) parsetree);
			break;

			/*
			 * ******************************** DOMAIN statements ****
			 */
		case T_CreateDomainStmt:
			DefineDomain((CreateDomainStmt *) parsetree);
			break;

			/*
			 * ******************************** ROLE statements ****
			 */
		case T_CreateRoleStmt:
			CreateRole((CreateRoleStmt *) parsetree);
			break;

		case T_AlterRoleStmt:
			AlterRole((AlterRoleStmt *) parsetree);
			break;

		case T_AlterRoleSetStmt:
			AlterRoleSet((AlterRoleSetStmt *) parsetree);
			break;

		case T_DropRoleStmt:
			DropRole((DropRoleStmt *) parsetree);
			break;

		case T_DropOwnedStmt:
			DropOwnedObjects((DropOwnedStmt *) parsetree);
			break;

		case T_ReassignOwnedStmt:
			ReassignOwnedObjects((ReassignOwnedStmt *) parsetree);
			break;

		case T_LockStmt:
			LockTableCommand((LockStmt *) parsetree);
			break;

		case T_ConstraintsSetStmt:
			AfterTriggerSetState((ConstraintsSetStmt *) parsetree);
			break;

		case T_CheckPointStmt:
			if (!superuser())
				ereport(ERROR,
						(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
						 errmsg("must be superuser to do CHECKPOINT")));
			RequestCheckpoint(true, false);
			break;

		case T_ReindexStmt:
			{
				ReindexStmt *stmt = (ReindexStmt *) parsetree;

				switch (stmt->kind)
				{
					case OBJECT_INDEX:
						ReindexIndex(stmt->relation);
						break;
					case OBJECT_TABLE:
						ReindexTable(stmt->relation);
						break;
					case OBJECT_DATABASE:
						ReindexDatabase(stmt->name,
										stmt->do_system, stmt->do_user);
						break;
					default:
						elog(ERROR, "unrecognized object type: %d",
							 (int) stmt->kind);
						break;
				}
				break;
			}
			break;

		case T_CreateConversionStmt:
			CreateConversionCommand((CreateConversionStmt *) parsetree);
			break;

		case T_CreateCastStmt:
			CreateCast((CreateCastStmt *) parsetree);
			break;

		case T_DropCastStmt:
			DropCast((DropCastStmt *) parsetree);
			break;

		case T_CreateOpClassStmt:
			DefineOpClass((CreateOpClassStmt *) parsetree);
			break;

		case T_RemoveOpClassStmt:
			RemoveOpClass((RemoveOpClassStmt *) parsetree);
			break;

		default:
			elog(ERROR, "unrecognized node type: %d",
				 (int) nodeTag(parsetree));
			break;
	}
}

/*
 * UtilityReturnsTuples
 *		Return "true" if this utility statement will send output to the
 *		destination.
 *
 * Generally, there should be a case here for each case in ProcessUtility
 * where "dest" is passed on.
 */
bool
UtilityReturnsTuples(Node *parsetree)
{
	switch (nodeTag(parsetree))
	{
		case T_FetchStmt:
			{
				FetchStmt  *stmt = (FetchStmt *) parsetree;
				Portal		portal;

				if (stmt->ismove)
					return false;
				portal = GetPortalByName(stmt->portalname);
				if (!PortalIsValid(portal))
					return false;		/* not our business to raise error */
				return portal->tupDesc ? true : false;
			}

		case T_ExecuteStmt:
			{
				ExecuteStmt *stmt = (ExecuteStmt *) parsetree;
				PreparedStatement *entry;

				if (stmt->into)
					return false;
				entry = FetchPreparedStatement(stmt->name, false);
				if (!entry)
					return false;		/* not our business to raise error */
				switch (ChoosePortalStrategy(entry->query_list))
				{
					case PORTAL_ONE_SELECT:
					case PORTAL_ONE_RETURNING:
					case PORTAL_UTIL_SELECT:
						return true;
					case PORTAL_MULTI_QUERY:
						/* will not return tuples */
						break;
				}
				return false;
			}

		case T_ExplainStmt:
			return true;

		case T_VariableShowStmt:
			return true;

		default:
			return false;
	}
}

/*
 * UtilityTupleDescriptor
 *		Fetch the actual output tuple descriptor for a utility statement
 *		for which UtilityReturnsTuples() previously returned "true".
 *
 * The returned descriptor is created in (or copied into) the current memory
 * context.
 */
TupleDesc
UtilityTupleDescriptor(Node *parsetree)
{
	switch (nodeTag(parsetree))
	{
		case T_FetchStmt:
			{
				FetchStmt  *stmt = (FetchStmt *) parsetree;
				Portal		portal;

				if (stmt->ismove)
					return NULL;
				portal = GetPortalByName(stmt->portalname);
				if (!PortalIsValid(portal))
					return NULL;	/* not our business to raise error */
				return CreateTupleDescCopy(portal->tupDesc);
			}

		case T_ExecuteStmt:
			{
				ExecuteStmt *stmt = (ExecuteStmt *) parsetree;
				PreparedStatement *entry;

				if (stmt->into)
					return NULL;
				entry = FetchPreparedStatement(stmt->name, false);
				if (!entry)
					return NULL;	/* not our business to raise error */
				return FetchPreparedStatementResultDesc(entry);
			}

		case T_ExplainStmt:
			return ExplainResultDesc((ExplainStmt *) parsetree);

		case T_VariableShowStmt:
			{
				VariableShowStmt *n = (VariableShowStmt *) parsetree;

				return GetPGVariableResultDesc(n->name);
			}

		default:
			return NULL;
	}
}


/*
 * QueryReturnsTuples
 *		Return "true" if this Query will send output to the destination.
 */
bool
QueryReturnsTuples(Query *parsetree)
{
	switch (parsetree->commandType)
	{
		case CMD_SELECT:
			/* returns tuples ... unless it's SELECT INTO */
			if (parsetree->into == NULL)
				return true;
			break;
		case CMD_INSERT:
		case CMD_UPDATE:
		case CMD_DELETE:
			/* the forms with RETURNING return tuples */
			if (parsetree->returningList)
				return true;
			break;
		case CMD_UTILITY:
			return UtilityReturnsTuples(parsetree->utilityStmt);
		case CMD_UNKNOWN:
		case CMD_NOTHING:
			/* probably shouldn't get here */
			break;
	}
	return false;				/* default */
}


/*
 * CreateCommandTag
 *		utility to get a string representation of the
 *		command operation, given a raw (un-analyzed) parsetree.
 *
 * This must handle all raw command types, but since the vast majority
 * of 'em are utility commands, it seems sensible to keep it here.
 *
 * NB: all result strings must be shorter than COMPLETION_TAG_BUFSIZE.
 * Also, the result must point at a true constant (permanent storage).
 */
const char *
CreateCommandTag(Node *parsetree)
{
	const char *tag;

	switch (nodeTag(parsetree))
	{
		case T_InsertStmt:
			tag = "INSERT";
			break;

		case T_DeleteStmt:
			tag = "DELETE";
			break;

		case T_UpdateStmt:
			tag = "UPDATE";
			break;

		case T_SelectStmt:
			tag = "SELECT";
			break;

		case T_TransactionStmt:
			{
				TransactionStmt *stmt = (TransactionStmt *) parsetree;

				switch (stmt->kind)
				{
					case TRANS_STMT_BEGIN:
						tag = "BEGIN";
						break;

					case TRANS_STMT_START:
						tag = "START TRANSACTION";
						break;

					case TRANS_STMT_COMMIT:
						tag = "COMMIT";
						break;

					case TRANS_STMT_ROLLBACK:
					case TRANS_STMT_ROLLBACK_TO:
						tag = "ROLLBACK";
						break;

					case TRANS_STMT_SAVEPOINT:
						tag = "SAVEPOINT";
						break;

					case TRANS_STMT_RELEASE:
						tag = "RELEASE";
						break;

					case TRANS_STMT_PREPARE:
						tag = "PREPARE TRANSACTION";
						break;

					case TRANS_STMT_COMMIT_PREPARED:
						tag = "COMMIT PREPARED";
						break;

					case TRANS_STMT_ROLLBACK_PREPARED:
						tag = "ROLLBACK PREPARED";
						break;

					default:
						tag = "???";
						break;
				}
			}
			break;

		case T_DeclareCursorStmt:
			tag = "DECLARE CURSOR";
			break;

		case T_ClosePortalStmt:
			tag = "CLOSE CURSOR";
			break;

		case T_FetchStmt:
			{
				FetchStmt  *stmt = (FetchStmt *) parsetree;

				tag = (stmt->ismove) ? "MOVE" : "FETCH";
			}
			break;

		case T_CreateDomainStmt:
			tag = "CREATE DOMAIN";
			break;

		case T_CreateSchemaStmt:
			tag = "CREATE SCHEMA";
			break;

		case T_CreateStmt:
			tag = "CREATE TABLE";
			break;

		case T_CreateTableSpaceStmt:
			tag = "CREATE TABLESPACE";
			break;

		case T_DropTableSpaceStmt:
			tag = "DROP TABLESPACE";
			break;

		case T_DropStmt:
			switch (((DropStmt *) parsetree)->removeType)
			{
				case OBJECT_TABLE:
					tag = "DROP TABLE";
					break;
				case OBJECT_SEQUENCE:
					tag = "DROP SEQUENCE";
					break;
				case OBJECT_VIEW:
					tag = "DROP VIEW";
					break;
				case OBJECT_INDEX:
					tag = "DROP INDEX";
					break;
				case OBJECT_TYPE:
					tag = "DROP TYPE";
					break;
				case OBJECT_DOMAIN:
					tag = "DROP DOMAIN";
					break;
				case OBJECT_CONVERSION:
					tag = "DROP CONVERSION";
					break;
				case OBJECT_SCHEMA:
					tag = "DROP SCHEMA";
					break;
				default:
					tag = "???";
			}
			break;

		case T_TruncateStmt:
			tag = "TRUNCATE TABLE";
			break;

		case T_CommentStmt:
			tag = "COMMENT";
			break;

		case T_CopyStmt:
			tag = "COPY";
			break;

		case T_RenameStmt:
			switch (((RenameStmt *) parsetree)->renameType)
			{
				case OBJECT_AGGREGATE:
					tag = "ALTER AGGREGATE";
					break;
				case OBJECT_CONVERSION:
					tag = "ALTER CONVERSION";
					break;
				case OBJECT_DATABASE:
					tag = "ALTER DATABASE";
					break;
				case OBJECT_FUNCTION:
					tag = "ALTER FUNCTION";
					break;
				case OBJECT_INDEX:
					tag = "ALTER INDEX";
					break;
				case OBJECT_LANGUAGE:
					tag = "ALTER LANGUAGE";
					break;
				case OBJECT_OPCLASS:
					tag = "ALTER OPERATOR CLASS";
					break;
				case OBJECT_ROLE:
					tag = "ALTER ROLE";
					break;
				case OBJECT_SCHEMA:
					tag = "ALTER SCHEMA";
					break;
				case OBJECT_COLUMN:
				case OBJECT_TABLE:
					tag = "ALTER TABLE";
					break;
				case OBJECT_TABLESPACE:
					tag = "ALTER TABLESPACE";
					break;
				case OBJECT_TRIGGER:
					tag = "ALTER TRIGGER";
					break;
				default:
					tag = "???";
					break;
			}
			break;

		case T_AlterObjectSchemaStmt:
			switch (((AlterObjectSchemaStmt *) parsetree)->objectType)
			{
				case OBJECT_AGGREGATE:
					tag = "ALTER AGGREGATE";
					break;
				case OBJECT_DOMAIN:
					tag = "ALTER DOMAIN";
					break;
				case OBJECT_FUNCTION:
					tag = "ALTER FUNCTION";
					break;
				case OBJECT_SEQUENCE:
					tag = "ALTER SEQUENCE";
					break;
				case OBJECT_TABLE:
					tag = "ALTER TABLE";
					break;
				case OBJECT_TYPE:
					tag = "ALTER TYPE";
					break;
				default:
					tag = "???";
					break;
			}
			break;

		case T_AlterOwnerStmt:
			switch (((AlterOwnerStmt *) parsetree)->objectType)
			{
				case OBJECT_AGGREGATE:
					tag = "ALTER AGGREGATE";
					break;
				case OBJECT_CONVERSION:
					tag = "ALTER CONVERSION";
					break;
				case OBJECT_DATABASE:
					tag = "ALTER DATABASE";
					break;
				case OBJECT_DOMAIN:
					tag = "ALTER DOMAIN";
					break;
				case OBJECT_FUNCTION:
					tag = "ALTER FUNCTION";
					break;
				case OBJECT_OPERATOR:
					tag = "ALTER OPERATOR";
					break;
				case OBJECT_OPCLASS:
					tag = "ALTER OPERATOR CLASS";
					break;
				case OBJECT_SCHEMA:
					tag = "ALTER SCHEMA";
					break;
				case OBJECT_TABLESPACE:
					tag = "ALTER TABLESPACE";
					break;
				case OBJECT_TYPE:
					tag = "ALTER TYPE";
					break;
				default:
					tag = "???";
					break;
			}
			break;

		case T_AlterTableStmt:
			{
				AlterTableStmt *stmt = (AlterTableStmt *) parsetree;

				/*
				 * We might be supporting ALTER INDEX here, so set the
				 * completion table appropriately. Catch all other
				 * possibilities with ALTER TABLE
				 */

				if (stmt->relkind == OBJECT_INDEX)
					tag = "ALTER INDEX";
				else
					tag = "ALTER TABLE";
			}
			break;

		case T_AlterDomainStmt:
			tag = "ALTER DOMAIN";
			break;

		case T_AlterFunctionStmt:
			tag = "ALTER FUNCTION";
			break;

		case T_GrantStmt:
			{
				GrantStmt  *stmt = (GrantStmt *) parsetree;

				tag = (stmt->is_grant) ? "GRANT" : "REVOKE";
			}
			break;

		case T_GrantRoleStmt:
			{
				GrantRoleStmt *stmt = (GrantRoleStmt *) parsetree;

				tag = (stmt->is_grant) ? "GRANT ROLE" : "REVOKE ROLE";
			}
			break;

		case T_DefineStmt:
			switch (((DefineStmt *) parsetree)->kind)
			{
				case OBJECT_AGGREGATE:
					tag = "CREATE AGGREGATE";
					break;
				case OBJECT_OPERATOR:
					tag = "CREATE OPERATOR";
					break;
				case OBJECT_TYPE:
					tag = "CREATE TYPE";
					break;
				default:
					tag = "???";
			}
			break;

		case T_CompositeTypeStmt:
			tag = "CREATE TYPE";
			break;

		case T_ViewStmt:
			tag = "CREATE VIEW";
			break;

		case T_CreateFunctionStmt:
			tag = "CREATE FUNCTION";
			break;

		case T_IndexStmt:
			tag = "CREATE INDEX";
			break;

		case T_RuleStmt:
			tag = "CREATE RULE";
			break;

		case T_CreateSeqStmt:
			tag = "CREATE SEQUENCE";
			break;

		case T_AlterSeqStmt:
			tag = "ALTER SEQUENCE";
			break;

		case T_RemoveFuncStmt:
			switch (((RemoveFuncStmt *) parsetree)->kind)
			{
				case OBJECT_FUNCTION:
					tag = "DROP FUNCTION";
					break;
				case OBJECT_AGGREGATE:
					tag = "DROP AGGREGATE";
					break;
				case OBJECT_OPERATOR:
					tag = "DROP OPERATOR";
					break;
				default:
					tag = "???";
			}
			break;

		case T_CreatedbStmt:
			tag = "CREATE DATABASE";
			break;

		case T_AlterDatabaseStmt:
			tag = "ALTER DATABASE";
			break;

		case T_AlterDatabaseSetStmt:
			tag = "ALTER DATABASE";
			break;

		case T_DropdbStmt:
			tag = "DROP DATABASE";
			break;

		case T_NotifyStmt:
			tag = "NOTIFY";
			break;

		case T_ListenStmt:
			tag = "LISTEN";
			break;

		case T_UnlistenStmt:
			tag = "UNLISTEN";
			break;

		case T_LoadStmt:
			tag = "LOAD";
			break;

		case T_ClusterStmt:
			tag = "CLUSTER";
			break;

		case T_VacuumStmt:
			if (((VacuumStmt *) parsetree)->vacuum)
				tag = "VACUUM";
			else
				tag = "ANALYZE";
			break;

		case T_ExplainStmt:
			tag = "EXPLAIN";
			break;

		case T_VariableSetStmt:
			tag = "SET";
			break;

		case T_VariableShowStmt:
			tag = "SHOW";
			break;

		case T_VariableResetStmt:
			tag = "RESET";
			break;

		case T_CreateTrigStmt:
			tag = "CREATE TRIGGER";
			break;

		case T_DropPropertyStmt:
			switch (((DropPropertyStmt *) parsetree)->removeType)
			{
				case OBJECT_TRIGGER:
					tag = "DROP TRIGGER";
					break;
				case OBJECT_RULE:
					tag = "DROP RULE";
					break;
				default:
					tag = "???";
			}
			break;

		case T_CreatePLangStmt:
			tag = "CREATE LANGUAGE";
			break;

		case T_DropPLangStmt:
			tag = "DROP LANGUAGE";
			break;

		case T_CreateRoleStmt:
			tag = "CREATE ROLE";
			break;

		case T_AlterRoleStmt:
			tag = "ALTER ROLE";
			break;

		case T_AlterRoleSetStmt:
			tag = "ALTER ROLE";
			break;

		case T_DropRoleStmt:
			tag = "DROP ROLE";
			break;

		case T_DropOwnedStmt:
			tag = "DROP OWNED";
			break;

		case T_ReassignOwnedStmt:
			tag = "REASSIGN OWNED";
			break;

		case T_LockStmt:
			tag = "LOCK TABLE";
			break;

		case T_ConstraintsSetStmt:
			tag = "SET CONSTRAINTS";
			break;

		case T_CheckPointStmt:
			tag = "CHECKPOINT";
			break;

		case T_ReindexStmt:
			tag = "REINDEX";
			break;

		case T_CreateConversionStmt:
			tag = "CREATE CONVERSION";
			break;

		case T_CreateCastStmt:
			tag = "CREATE CAST";
			break;

		case T_DropCastStmt:
			tag = "DROP CAST";
			break;

		case T_CreateOpClassStmt:
			tag = "CREATE OPERATOR CLASS";
			break;

		case T_RemoveOpClassStmt:
			tag = "DROP OPERATOR CLASS";
			break;

		case T_PrepareStmt:
			tag = "PREPARE";
			break;

		case T_ExecuteStmt:
			tag = "EXECUTE";
			break;

		case T_DeallocateStmt:
			tag = "DEALLOCATE";
			break;

		default:
			elog(WARNING, "unrecognized node type: %d",
				 (int) nodeTag(parsetree));
			tag = "???";
			break;
	}

	return tag;
}

/*
 * CreateQueryTag
 *		utility to get a string representation of a Query operation.
 *
 * This is exactly like CreateCommandTag, except it works on a Query
 * that has already been through parse analysis (and possibly further).
 */
const char *
CreateQueryTag(Query *parsetree)
{
	const char *tag;

	Assert(IsA(parsetree, Query));

	switch (parsetree->commandType)
	{
		case CMD_SELECT:

			/*
			 * We take a little extra care here so that the result will be
			 * useful for complaints about read-only statements
			 */
			if (parsetree->into != NULL)
				tag = "SELECT INTO";
			else if (parsetree->rowMarks != NIL)
			{
				if (((RowMarkClause *) linitial(parsetree->rowMarks))->forUpdate)
					tag = "SELECT FOR UPDATE";
				else
					tag = "SELECT FOR SHARE";
			}
			else
				tag = "SELECT";
			break;
		case CMD_UPDATE:
			tag = "UPDATE";
			break;
		case CMD_INSERT:
			tag = "INSERT";
			break;
		case CMD_DELETE:
			tag = "DELETE";
			break;
		case CMD_UTILITY:
			tag = CreateCommandTag(parsetree->utilityStmt);
			break;
		default:
			elog(WARNING, "unrecognized commandType: %d",
				 (int) parsetree->commandType);
			tag = "???";
			break;
	}

	return tag;
}


/*
 * GetCommandLogLevel
 *		utility to get the minimum log_statement level for a command,
 *		given a raw (un-analyzed) parsetree.
 *
 * This must handle all raw command types, but since the vast majority
 * of 'em are utility commands, it seems sensible to keep it here.
 */
LogStmtLevel
GetCommandLogLevel(Node *parsetree)
{
	LogStmtLevel lev;

	switch (nodeTag(parsetree))
	{
		case T_InsertStmt:
		case T_DeleteStmt:
		case T_UpdateStmt:
			lev = LOGSTMT_MOD;
			break;

		case T_SelectStmt:
			if (((SelectStmt *) parsetree)->into)
				lev = LOGSTMT_DDL;		/* CREATE AS, SELECT INTO */
			else
				lev = LOGSTMT_ALL;
			break;

		case T_TransactionStmt:
			lev = LOGSTMT_ALL;
			break;

		case T_DeclareCursorStmt:
			lev = LOGSTMT_ALL;
			break;

		case T_ClosePortalStmt:
			lev = LOGSTMT_ALL;
			break;

		case T_FetchStmt:
			lev = LOGSTMT_ALL;
			break;

		case T_CreateDomainStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_CreateSchemaStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_CreateStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_CreateTableSpaceStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_DropTableSpaceStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_DropStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_TruncateStmt:
			lev = LOGSTMT_MOD;
			break;

		case T_CommentStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_CopyStmt:
			if (((CopyStmt *) parsetree)->is_from)
				lev = LOGSTMT_MOD;
			else
				lev = LOGSTMT_ALL;
			break;

		case T_RenameStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_AlterObjectSchemaStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_AlterOwnerStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_AlterTableStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_AlterDomainStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_AlterFunctionStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_GrantStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_GrantRoleStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_DefineStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_CompositeTypeStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_ViewStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_CreateFunctionStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_IndexStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_RuleStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_CreateSeqStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_AlterSeqStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_RemoveFuncStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_CreatedbStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_AlterDatabaseStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_AlterDatabaseSetStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_DropdbStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_NotifyStmt:
			lev = LOGSTMT_ALL;
			break;

		case T_ListenStmt:
			lev = LOGSTMT_ALL;
			break;

		case T_UnlistenStmt:
			lev = LOGSTMT_ALL;
			break;

		case T_LoadStmt:
			lev = LOGSTMT_ALL;
			break;

		case T_ClusterStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_VacuumStmt:
			lev = LOGSTMT_ALL;
			break;

		case T_ExplainStmt:
			{
				ExplainStmt *stmt = (ExplainStmt *) parsetree;

				/* Look through an EXPLAIN ANALYZE to the contained stmt */
				if (stmt->analyze)
					return GetCommandLogLevel((Node *) stmt->query);
				/* Plain EXPLAIN isn't so interesting */
				lev = LOGSTMT_ALL;
			}
			break;

		case T_VariableSetStmt:
			lev = LOGSTMT_ALL;
			break;

		case T_VariableShowStmt:
			lev = LOGSTMT_ALL;
			break;

		case T_VariableResetStmt:
			lev = LOGSTMT_ALL;
			break;

		case T_CreateTrigStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_DropPropertyStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_CreatePLangStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_DropPLangStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_CreateRoleStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_AlterRoleStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_AlterRoleSetStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_DropRoleStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_DropOwnedStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_ReassignOwnedStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_LockStmt:
			lev = LOGSTMT_ALL;
			break;

		case T_ConstraintsSetStmt:
			lev = LOGSTMT_ALL;
			break;

		case T_CheckPointStmt:
			lev = LOGSTMT_ALL;
			break;

		case T_ReindexStmt:
			lev = LOGSTMT_ALL;	/* should this be DDL? */
			break;

		case T_CreateConversionStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_CreateCastStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_DropCastStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_CreateOpClassStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_RemoveOpClassStmt:
			lev = LOGSTMT_DDL;
			break;

		case T_PrepareStmt:
			{
				PrepareStmt *stmt = (PrepareStmt *) parsetree;

				/* Look through a PREPARE to the contained stmt */
				return GetCommandLogLevel((Node *) stmt->query);
			}
			break;

		case T_ExecuteStmt:
			{
				ExecuteStmt *stmt = (ExecuteStmt *) parsetree;
				PreparedStatement *pstmt;
				ListCell   *l;

				/* Look through an EXECUTE to the referenced stmt(s) */
				lev = LOGSTMT_ALL;
				pstmt = FetchPreparedStatement(stmt->name, false);
				if (pstmt)
				{
					foreach(l, pstmt->query_list)
					{
						Query	   *query = (Query *) lfirst(l);
						LogStmtLevel stmt_lev;

						stmt_lev = GetQueryLogLevel(query);
						lev = Min(lev, stmt_lev);
					}
				}
			}
			break;

		case T_DeallocateStmt:
			lev = LOGSTMT_ALL;
			break;

		case T_Query:

			/*
			 * In complicated situations (eg, EXPLAIN ANALYZE in an extended
			 * Query protocol), we might find an already-analyzed query within
			 * a utility statement.  Cope.
			 */
			lev = GetQueryLogLevel((Query *) parsetree);
			break;

		default:
			elog(WARNING, "unrecognized node type: %d",
				 (int) nodeTag(parsetree));
			lev = LOGSTMT_ALL;
			break;
	}

	return lev;
}

/*
 * GetQueryLogLevel
 *		utility to get the minimum log_statement level for a Query operation.
 *
 * This is exactly like GetCommandLogLevel, except it works on a Query
 * that has already been through parse analysis (and possibly further).
 */
LogStmtLevel
GetQueryLogLevel(Query *parsetree)
{
	LogStmtLevel lev;

	Assert(IsA(parsetree, Query));

	switch (parsetree->commandType)
	{
		case CMD_SELECT:
			if (parsetree->into != NULL)
				lev = LOGSTMT_DDL;		/* CREATE AS, SELECT INTO */
			else
				lev = LOGSTMT_ALL;
			break;

		case CMD_UPDATE:
		case CMD_INSERT:
		case CMD_DELETE:
			lev = LOGSTMT_MOD;
			break;

		case CMD_UTILITY:
			lev = GetCommandLogLevel(parsetree->utilityStmt);
			break;

		default:
			elog(WARNING, "unrecognized commandType: %d",
				 (int) parsetree->commandType);
			lev = LOGSTMT_ALL;
			break;
	}

	return lev;
}
