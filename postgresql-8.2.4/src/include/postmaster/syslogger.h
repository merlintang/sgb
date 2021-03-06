/*-------------------------------------------------------------------------
 *
 * syslogger.h
 *	  Exports from postmaster/syslogger.c.
 *
 * Copyright (c) 2004-2006, PostgreSQL Global Development Group
 *
 * $PostgreSQL: pgsql/src/include/postmaster/syslogger.h,v 1.7 2006/10/19 18:32:47 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef _SYSLOGGER_H
#define _SYSLOGGER_H

/* GUC options */
extern bool Redirect_stderr;
extern int	Log_RotationAge;
extern int	Log_RotationSize;
extern DLLIMPORT char *Log_directory;
extern DLLIMPORT char *Log_filename;
extern bool Log_truncate_on_rotation;

extern bool am_syslogger;

#ifndef WIN32
extern int	syslogPipe[2];
#else
extern HANDLE syslogPipe[2];
#endif


extern int	SysLogger_Start(void);

extern void write_syslogger_file(const char *buffer, int count);

#ifdef EXEC_BACKEND
extern void SysLoggerMain(int argc, char *argv[]);
#endif

#endif   /* _SYSLOGGER_H */
