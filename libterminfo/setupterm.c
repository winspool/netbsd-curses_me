/* $NetBSD: setupterm.c,v 1.9 2019/04/11 23:52:08 jakllsch Exp $ */

/*
 * Copyright (c) 2009, 2011 The NetBSD Foundation, Inc.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Roy Marples.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <netbsd_sys/cdefs.h>

#include <sys/ioctl.h>
#include <assert.h>
#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <term_private.h>
#include <term.h>

/*
 * use_env is really a curses function - POSIX mandates it's in curses.h
 * But it has to live in terminfo because it must precede a call to setupterm().
 */
#include <curses.h>

static bool __use_env = true;

void
use_env(bool value)
{

	__use_env = value;
}
#define reterr(code, msg)						      \
	do {								      \
		if (errret == NULL)					      \
			errx(EXIT_FAILURE, msg);			      \
		else {							      \
			*errret = code;					      \
			return ERR;					      \
		}							      \
	} while (0 /* CONSTCOND */)

#define reterrarg(code, msg, arg) \
	do {								      \
		if (errret == NULL)					      \
			errx(EXIT_FAILURE, msg, arg);			      \
		else {							      \
			*errret = code;					      \
			return ERR;					      \
		}							      \
	} while (0 /* CONSTCOND */)


int
ti_setupterm(TERMINAL **nterm, const char *term, int fildes, int *errret)
{
	int error;
	struct winsize win;

	_DIAGASSERT(nterm != NULL);

	if (term == NULL)
		term = getenv("TERM");
	if (term == NULL || *term == '\0') {
		*nterm = NULL;
		reterr(0, "TERM environment variable not set");
	}
	if (fildes == STDOUT_FILENO && !isatty(fildes))
		fildes = STDERR_FILENO;

	*nterm = calloc(1, sizeof(**nterm));
	if (*nterm == NULL)
		reterr(-1, "not enough memory to create terminal structure");

	error = _ti_getterm(*nterm, term, 0);
	if (error != 1) {
		del_curterm(*nterm);
		*nterm = NULL;
		switch (error) {
		case -1:
			reterr(error, "cannot access the terminfo database");
			/* NOTREACHED */
		case 0:
			reterrarg(error,
			    "%s: terminal not listed in terminfo database",
			    term);
			/* NOTREACHED */
		default:
			reterr(-1, "unknown error");
			/* NOTREACHED */
		}
	}

	(*nterm)->fildes = fildes;
	_ti_setospeed(*nterm);
	if (t_generic_type(*nterm))
		reterrarg(0, "%s: generic terminal", term);
	if (t_hard_copy(*nterm))
		reterrarg(1, "%s: hardcopy terminal", term);

	/* If TIOCGWINSZ works, then set initial lines and columns. */
	if (ioctl(fildes, TIOCGWINSZ, &win) != -1 &&
	    win.ws_row != 0 && win.ws_col != 0)
	{
		t_lines(*nterm) = (short)win.ws_row;
		t_columns(*nterm) = (short)win.ws_col;
	}

	/* POSIX 1003.2 requires that the environment override. */
	if (__use_env) {
		char *p;

		if ((p = getenv("LINES")) != NULL)
			t_lines(*nterm) = (short)strtol(p, NULL, 0);
		if ((p = getenv("COLUMNS")) != NULL)
			t_columns(*nterm) = (short)strtol(p, NULL, 0);
	}

	/* POSIX requires 1 for success */
	if (errret)
		*errret = 1;
	return OK;
}

int
setupterm(const char *term, int fildes, int *errret)
{
	TERMINAL *nterm;
	int ret;

	if (errret != NULL)
		*errret = ERR;
	ret = ti_setupterm(&nterm, term, fildes, errret);
	if (nterm != NULL)
		set_curterm(nterm);
	return ret;
}
