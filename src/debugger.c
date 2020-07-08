/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

/*
 * debugger.c: Vim script debugger functions
 */

#include "vim.h"
#include "option.h"

#if defined(FEAT_EVAL) || defined(PROTO)
static int debug_greedy = FALSE;	// batch mode debugging: don't save
					// and restore typeahead.
static void do_setdebugtracelevel(char_u *arg);
static void do_checkbacktracelevel(void);
static void do_showbacktrace(char_u *cmd);

static char_u *debug_oldval = NULL;	// old and newval for debug expressions
static char_u *debug_newval = NULL;
static int     debug_expr   = 0;        // use debug_expr

static int	debug_busy = 0;		// if > 0, we're already in do_debug;
					// don't do it recusively.

    int
has_watchexpr(void)
{
    // TODO/FIXME(Benj): It would appear that this will always return 0, as
    // debug_expr is never assigned anything other than 0
    return debug_expr;
}

/*
 * do_debug(): Debug mode.
 * Repeatedly get Ex commands, until told to continue normal execution.
 *
 * TODO: Add a 'reason' parameter to be passed to the custom debug func to
 * explain _why_ we stopped. This is particularly relevant when we break on
 * exiting a function due to a throw/error, and when we hit a catch/endif etc.
 */
    void
do_debug(char_u *cmd, char_u* reason)
{
    int		save_msg_scroll = msg_scroll;
    int		save_State = State;
    int		save_did_emsg = did_emsg;
    int		save_cmd_silent = cmd_silent;
    int		save_msg_silent = msg_silent;
    int		save_emsg_silent = emsg_silent;
    int		save_redir_off = redir_off;
    int		save_debug_break_level;
    char_u	*cmdline = NULL;
    char_u	*p;
    char_u	*sname;
    char	*tail = NULL;
    static int	last_cmd = 0;
#define CMD_CONT	1
#define CMD_NEXT	2
#define CMD_STEP	3
#define CMD_FINISH	4
#define CMD_QUIT	5
#define CMD_INTERRUPT	6
#define CMD_BACKTRACE	7
#define CMD_FRAME	8
#define CMD_UP		9
#define CMD_DOWN	10

#ifdef ALWAYS_USE_GUI
    // Can't do this when there is no terminal for input/output.
    if (!gui.in_use)
    {
	// Break as soon as possible.
	debug_break_level = 9999;
	return;
    }
#endif

    if ( debug_busy )
    {
	return;
    }

    // Make sure we are in raw mode and start termcap mode.  Might have side
    // effects...
    settmode(TMODE_RAW);
    starttermcap();

    ++RedrawingDisabled;	// don't redisplay the window
    ++no_wait_return;		// don't wait for return
    did_emsg = FALSE;		// don't use error from debugged stuff
    cmd_silent = FALSE;		// display commands
    msg_silent = FALSE;		// display messages
    emsg_silent = FALSE;	// display error messages
    redir_off = TRUE;		// don't redirect debug commands

    State = NORMAL;
    debug_mode = TRUE;

    if ( !*p_debugfunc )
    {
	if (!debug_did_msg)
	    msg(_("Entering Debug mode.  Type \"cont\" to continue."));
	if (debug_oldval != NULL)
	{
	    smsg(_("Oldval = \"%s\""), debug_oldval);
	    vim_free(debug_oldval);
	    debug_oldval = NULL;
	}
	if (debug_newval != NULL)
	{
	    smsg(_("Newval = \"%s\""), debug_newval);
	    vim_free(debug_newval);
	    debug_newval = NULL;
	}
	sname = estack_sfile(ESTACK_NONE);
	if (sname != NULL)
	    msg((char *)sname);
	vim_free(sname);
	if (SOURCING_LNUM != 0)
	    smsg(_("line %ld: %s"), SOURCING_LNUM, cmd);
	else
	    smsg(_("cmd: %s"), cmd);
    }

    // Repeat getting a command and executing it.
    for (;;)
    {
	vim_free(cmdline);
	// If the user configured a debugfunc, call it and get a command
	if ( *p_debugfunc && find_func( p_debugfunc, TRUE, NULL ) )
	{
	    // see the stuff in time.c that gets saved when running a timer
	    // callback. We have to save a _lot_ of state so that running this
	    // command appears like it's running isolated

	    int		save_debug_backtrace_level = debug_backtrace_level;
	    int		save_need_rethrow = need_rethrow;
	    int		save_did_throw = did_throw;
	    int		save_trylevel = trylevel;
	    except_T	*save_current_exception = current_exception;
	    int		save_called_emsg = called_emsg;
	    vimvars_save_T vvsave;
	    typval_T	argv[] = { {VAR_STRING}, {VAR_UNKNOWN} };
	    argv[0].vval.v_string = reason;

	    save_debug_break_level = debug_break_level;
	    save_did_emsg = did_emsg;

	    debug_backtrace_level = 0;
	    need_rethrow = FALSE;
	    did_throw = FALSE;
	    trylevel = 0;
	    current_exception = NULL;
	    called_emsg = 0;
	    save_vimvars(&vvsave);

	    debug_break_level = -1;
	    did_emsg = 0;

	    // Disable all debugging for the custom-debug function. This would
	    // lead to an infinite loop!
	    ++debug_busy;
	    cmdline = call_func_retstr( p_debugfunc, 1, argv );
	    --debug_busy;

	    did_emsg = save_did_emsg;
	    debug_break_level = save_debug_break_level;
	    restore_vimvars(&vvsave);
	    called_emsg = save_called_emsg;
	    current_exception = save_current_exception;
	    trylevel = save_trylevel;
	    did_throw = save_did_throw;
	    need_rethrow = save_need_rethrow;
	    debug_backtrace_level = save_debug_backtrace_level;

	    if (!*cmdline)
	    {
		break;
	    }
	}
	else
	{
	    msg_scroll = TRUE;
	    need_wait_return = FALSE;

	    int		typeahead_saved = FALSE;
	    int		save_ex_normal_busy;
	    int		save_ignore_script = 0;
	    tasave_T	typeaheadbuf;

	    // Save the current typeahead buffer and replace it with an empty
	    // one.  This makes sure we get input from the user here and don't
	    // interfere with the commands being executed.  Reset
	    // "ex_normal_busy" to avoid the side effects of using ":normal".
	    // Save the stuff buffer and make it empty. Set ignore_script to
	    // avoid reading from script input.
	    save_ex_normal_busy = ex_normal_busy;
	    ex_normal_busy = 0;
	    if (!debug_greedy)
	    {
		save_typeahead(&typeaheadbuf);
		typeahead_saved = TRUE;
		save_ignore_script = ignore_script;
		ignore_script = TRUE;
	    }

	    cmdline = getcmdline_prompt('>', NULL, 0, EXPAND_NOTHING, NULL);
	    if (typeahead_saved)
	    {
		restore_typeahead(&typeaheadbuf);
		ignore_script = save_ignore_script;
	    }
	    ex_normal_busy = save_ex_normal_busy;
	    cmdline_row = msg_row;
	    msg_starthere();
	}

	if (cmdline != NULL)
	{
	    // If this is a debug command, set "last_cmd".
	    // If not, reset "last_cmd".
	    // For a blank line use previous command.
	    p = skipwhite(cmdline);
	    if (*p != NUL)
	    {
		switch (*p)
		{
		    case 'c': last_cmd = CMD_CONT;
			      tail = "ont";
			      break;
		    case 'n': last_cmd = CMD_NEXT;
			      tail = "ext";
			      break;
		    case 's': last_cmd = CMD_STEP;
			      tail = "tep";
			      break;
		    case 'f':
			      last_cmd = 0;
			      if (p[1] == 'r')
			      {
				  last_cmd = CMD_FRAME;
				  tail = "rame";
			      }
			      else
			      {
				  last_cmd = CMD_FINISH;
				  tail = "inish";
			      }
			      break;
		    case 'q': last_cmd = CMD_QUIT;
			      tail = "uit";
			      break;
		    case 'i': last_cmd = CMD_INTERRUPT;
			      tail = "nterrupt";
			      break;
		    case 'b': last_cmd = CMD_BACKTRACE;
			      if (p[1] == 't')
				  tail = "t";
			      else
				  tail = "acktrace";
			      break;
		    case 'w': last_cmd = CMD_BACKTRACE;
			      tail = "here";
			      break;
		    case 'u': last_cmd = CMD_UP;
			      tail = "p";
			      break;
		    case 'd': last_cmd = CMD_DOWN;
			      tail = "own";
			      break;
		    default: last_cmd = 0;
		}
		if (last_cmd != 0)
		{
		    // Check that the tail matches.
		    ++p;
		    while (*p != NUL && *p == *tail)
		    {
			++p;
			++tail;
		    }
		    if (ASCII_ISALPHA(*p) && last_cmd != CMD_FRAME)
			last_cmd = 0;
		}
	    }

	    if (last_cmd != 0)
	    {
		// Execute debug command: decided where to break next and
		// return.
		switch (last_cmd)
		{
		    case CMD_CONT:
			debug_break_level = -1;
			break;
		    case CMD_NEXT:
			debug_break_level = ex_nesting_level;
			break;
		    case CMD_STEP:
			debug_break_level = 9999;
			break;
		    case CMD_FINISH:
			debug_break_level = ex_nesting_level - 1;
			break;
		    case CMD_QUIT:
			got_int = TRUE;
			debug_break_level = -1;
			break;
		    case CMD_INTERRUPT:
			got_int = TRUE;
			debug_break_level = 9999;
			// Do not repeat ">interrupt" cmd, continue stepping.
			last_cmd = CMD_STEP;
			break;
		    case CMD_BACKTRACE:
			if (*p_debugfunc)
			    continue;

			do_showbacktrace(cmd);
			continue;
		    case CMD_FRAME:
			if (*p_debugfunc)
			    continue;

			if (*p == NUL)
			{
			    do_showbacktrace(cmd);
			}
			else
			{
			    p = skipwhite(p);
			    do_setdebugtracelevel(p);
			}
			continue;
		    case CMD_UP:
			if (*p_debugfunc)
			    continue;

			debug_backtrace_level++;
			do_checkbacktracelevel();
			continue;
		    case CMD_DOWN:
			if (*p_debugfunc)
			    continue;

			debug_backtrace_level--;
			do_checkbacktracelevel();
			continue;
		}
		// Going out reset backtrace_level
		debug_backtrace_level = 0;
		break;
	    }

	    // don't debug this command
	    save_debug_break_level = debug_break_level;
	    debug_break_level = -1;
	    (void)do_cmdline(cmdline, getexline, NULL,
						DOCMD_VERBOSE|DOCMD_EXCRESET);
	    debug_break_level = save_debug_break_level;
	}
	if (!*p_debugfunc)
	    lines_left = Rows - 1;
    }
    vim_free(cmdline);

    --RedrawingDisabled;
    --no_wait_return;
    redraw_all_later(NOT_VALID);
    need_wait_return = FALSE;
    msg_scroll = save_msg_scroll;
    lines_left = Rows - 1;
    State = save_State;
    debug_mode = FALSE;
    did_emsg = save_did_emsg;
    cmd_silent = save_cmd_silent;
    msg_silent = save_msg_silent;
    emsg_silent = save_emsg_silent;
    redir_off = save_redir_off;

    // Only print the message again when typing a command before coming back
    // here.
    debug_did_msg = TRUE;
}

    static int
get_maxbacktrace_level(char_u *sname)
{
    // TODO(BenJ): use the estack
    char	*p, *q;
    int		maxbacktrace = 0;

    if (sname != NULL)
    {
	p = (char *)sname;
	while ((q = strstr(p, "..")) != NULL)
	{
	    p = q + 2;
	    maxbacktrace++;
	}
    }
    return maxbacktrace;
}

    static void
do_setdebugtracelevel(char_u *arg)
{
    int level;

    level = atoi((char *)arg);
    if (*arg == '+' || level < 0)
	debug_backtrace_level += level;
    else
	debug_backtrace_level = level;

    do_checkbacktracelevel();
}

    static void
do_checkbacktracelevel(void)
{
    // TODO(BenJ): use the estack
    if (debug_backtrace_level < 0)
    {
	debug_backtrace_level = 0;
	msg(_("frame is zero"));
    }
    else
    {
	char_u	*sname = estack_sfile(ESTACK_NONE);
	int	max = get_maxbacktrace_level(sname);

	if (debug_backtrace_level > max)
	{
	    debug_backtrace_level = max;
	    smsg(_("frame at highest level: %d"), max);
	}
	vim_free(sname);
    }
}

    static void
do_showbacktrace(char_u *cmd)
{
    // TODO(BenJ): This is strying to write a backtrace based on the
    // estack_sfile() output, but it would really just use the estack directly
    char_u  *sname;
    char    *cur;
    char    *next;
    int	    i = 0;
    int	    max;

    sname = estack_sfile(ESTACK_NONE);
    max = get_maxbacktrace_level(sname);
    if (sname != NULL)
    {
	cur = (char *)sname;
	while (!got_int)
	{
	    next = strstr(cur, "..");
	    if (next != NULL)
		*next = NUL;
	    if (i == max - debug_backtrace_level)
		smsg("->%d %s", max - i, cur);
	    else
		smsg("  %d %s", max - i, cur);
	    ++i;
	    if (next == NULL)
		break;
	    *next = '.';
	    cur = next + 2;
	}
	vim_free(sname);
    }

    if (SOURCING_LNUM != 0)
       smsg(_("line %ld: %s"), (long)SOURCING_LNUM, cmd);
    else
       smsg(_("cmd: %s"), cmd);
}

/*
 * ":debug".
 */
    void
ex_debug(exarg_T *eap)
{
    int		debug_break_level_save = debug_break_level;

    debug_break_level = 9999;
    do_cmdline_cmd(eap->arg);
    debug_break_level = debug_break_level_save;
}

static char_u	*debug_breakpoint_name = NULL;
static linenr_T	debug_breakpoint_lnum;

/*
 * When debugging or a breakpoint is set on a skipped command, no debug prompt
 * is shown by do_one_cmd().  This situation is indicated by debug_skipped, and
 * debug_skipped_name is then set to the source name in the breakpoint case.  If
 * a skipped command decides itself that a debug prompt should be displayed, it
 * can do so by calling dbg_check_skipped().
 */
static int	debug_skipped;
static char_u	*debug_skipped_name;

/*
 * Go to debug mode when a breakpoint was encountered or "ex_nesting_level" is
 * at or below the break level.  But only when the line is actually
 * executed.  Return TRUE and set breakpoint_name for skipped commands that
 * decide to execute something themselves.
 * Called from do_one_cmd() before executing a command.
 */
    void
dbg_check_breakpoint(exarg_T *eap)
{
    char_u	*p;

    if ( debug_busy )
    {
	return;
    }
;
    debug_skipped = FALSE;
    if (debug_breakpoint_name != NULL)
    {
	if (!eap->skip)
	{
	    // replace K_SNR with "<SNR>"
	    if (debug_breakpoint_name[0] == K_SPECIAL
		    && debug_breakpoint_name[1] == KS_EXTRA
		    && debug_breakpoint_name[2] == (int)KE_SNR)
		p = (char_u *)"<SNR>";
	    else
		p = (char_u *)"";
	    smsg(_("Breakpoint in \"%s%s\" line %ld"),
		    p,
		    debug_breakpoint_name + (*p == NUL ? 0 : 3),
		    (long)debug_breakpoint_lnum);
	    debug_breakpoint_name = NULL;
	    do_debug(eap->cmd, (char_u*)"breakpoint");
	}
	else
	{
	    debug_skipped = TRUE;
	    debug_skipped_name = debug_breakpoint_name;
	    debug_breakpoint_name = NULL;
	}
    }
    else if (ex_nesting_level <= debug_break_level)
    {
	if (!eap->skip)
	    do_debug(eap->cmd, (char_u*)"step");
	else
	{
	    debug_skipped = TRUE;
	    debug_skipped_name = NULL;
	}
    }
}

/*
 * Go to debug mode if skipped by dbg_check_breakpoint() because eap->skip was
 * set.  Return TRUE when the debug mode is entered this time.
 */
    int
dbg_check_skipped(exarg_T *eap)
{
    int		prev_got_int;

    if ( debug_busy )
    {
	return FALSE;
    }

    if (debug_skipped)
    {
	// Save the value of got_int and reset it.  We don't want a previous
	// interruption cause flushing the input buffer.
	prev_got_int = got_int;
	got_int = FALSE;
	debug_breakpoint_name = debug_skipped_name;
	// eap->skip is TRUE
	eap->skip = FALSE;
	(void)dbg_check_breakpoint(eap);
	eap->skip = TRUE;
	got_int |= prev_got_int;
	return TRUE;
    }
    return FALSE;
}

/*
 * The list of breakpoints: dbg_breakp.
 * This is a grow-array of structs.
 */
struct debuggy
{
    int		dbg_nr;		// breakpoint number
    int		dbg_type;	// DBG_FUNC, DBG_FILE or DBG_EXPR
    char_u	*dbg_name;	// function, expression or file name
    regprog_T	*dbg_prog;	// regexp program
    linenr_T	dbg_lnum;	// line number in function or file
    int		dbg_forceit;	// ! used
#ifdef FEAT_EVAL
    typval_T    *dbg_val;       // last result of watchexpression
#endif
    int		dbg_level;      // stored nested level for expr
};

static garray_T dbg_breakp = {0, 0, sizeof(struct debuggy), 4, NULL};
#define BREAKP(idx)		(((struct debuggy *)dbg_breakp.ga_data)[idx])
#define DEBUGGY(gap, idx)	(((struct debuggy *)gap->ga_data)[idx])
static int last_breakp = 0;	// nr of last defined breakpoint

#ifdef FEAT_PROFILE
// Profiling uses file and func names similar to breakpoints.
static garray_T prof_ga = {0, 0, sizeof(struct debuggy), 4, NULL};
#endif
#define DBG_FUNC	1
#define DBG_FILE	2
#define DBG_EXPR	3

static linenr_T debuggy_find(int file,char_u *fname, linenr_T after, garray_T *gap, int *fp);

/*
 * Parse the arguments of ":profile", ":breakadd" or ":breakdel" and put them
 * in the entry just after the last one in dbg_breakp.  Note that "dbg_name"
 * is allocated.
 * Returns FAIL for failure.
 */
    static int
dbg_parsearg(
    char_u	*arg,
    garray_T	*gap)	    // either &dbg_breakp or &prof_ga
{
    char_u	*p = arg;
    char_u	*q;
    struct debuggy *bp;
    int		here = FALSE;

    if (ga_grow(gap, 1) == FAIL)
	return FAIL;
    bp = &DEBUGGY(gap, gap->ga_len);

    // Find "func" or "file".
    if (STRNCMP(p, "func", 4) == 0)
	bp->dbg_type = DBG_FUNC;
    else if (STRNCMP(p, "file", 4) == 0)
	bp->dbg_type = DBG_FILE;
    else if (
#ifdef FEAT_PROFILE
	    gap != &prof_ga &&
#endif
	    STRNCMP(p, "here", 4) == 0)
    {
	if (curbuf->b_ffname == NULL)
	{
	    emsg(_(e_noname));
	    return FAIL;
	}
	bp->dbg_type = DBG_FILE;
	here = TRUE;
    }
    else if (
#ifdef FEAT_PROFILE
	    gap != &prof_ga &&
#endif
	    STRNCMP(p, "expr", 4) == 0)
	bp->dbg_type = DBG_EXPR;
    else
    {
	semsg(_(e_invarg2), p);
	return FAIL;
    }
    p = skipwhite(p + 4);

    // Find optional line number.
    if (here)
	bp->dbg_lnum = curwin->w_cursor.lnum;
    else if (
#ifdef FEAT_PROFILE
	    gap != &prof_ga &&
#endif
	    VIM_ISDIGIT(*p))
    {
	bp->dbg_lnum = getdigits(&p);
	p = skipwhite(p);
    }
    else
	bp->dbg_lnum = 0;

    // Find the function or file name.  Don't accept a function name with ().
    if ((!here && *p == NUL)
	    || (here && *p != NUL)
	    || (bp->dbg_type == DBG_FUNC && strstr((char *)p, "()") != NULL))
    {
	semsg(_(e_invarg2), arg);
	return FAIL;
    }

    if (bp->dbg_type == DBG_FUNC)
	bp->dbg_name = vim_strsave(p);
    else if (here)
	bp->dbg_name = vim_strsave(curbuf->b_ffname);
    else if (bp->dbg_type == DBG_EXPR)
    {
	bp->dbg_name = vim_strsave(p);
	if (bp->dbg_name != NULL)
	    bp->dbg_val = eval_expr(bp->dbg_name, NULL);
    }
    else
    {
	// Expand the file name in the same way as do_source().  This means
	// doing it twice, so that $DIR/file gets expanded when $DIR is
	// "~/dir".
	q = expand_env_save(p);
	if (q == NULL)
	    return FAIL;
	p = expand_env_save(q);
	vim_free(q);
	if (p == NULL)
	    return FAIL;
	if (*p != '*')
	{
	    bp->dbg_name = fix_fname(p);
	    vim_free(p);
	}
	else
	    bp->dbg_name = p;
    }

    if (bp->dbg_name == NULL)
	return FAIL;
    return OK;
}

/*
 * ":breakint". interrupt and go into debug mode asap
 */
    void
ex_breakint(exarg_T *eap)
{
    debug_break_level = 9999;
}

/*
 * ":breakadd".  Also used for ":profile".
 */
    void
ex_breakadd(exarg_T *eap)
{
    struct debuggy *bp;
    char_u	*pat;
    garray_T	*gap;

    gap = &dbg_breakp;
#ifdef FEAT_PROFILE
    if (eap->cmdidx == CMD_profile)
	gap = &prof_ga;
#endif

    if (dbg_parsearg(eap->arg, gap) == OK)
    {
	bp = &DEBUGGY(gap, gap->ga_len);
	bp->dbg_forceit = eap->forceit;

	if (bp->dbg_type != DBG_EXPR)
	{
	    pat = file_pat_to_reg_pat(bp->dbg_name, NULL, NULL, FALSE);
	    if (pat != NULL)
	    {
		bp->dbg_prog = vim_regcomp(pat, RE_MAGIC + RE_STRING);
		vim_free(pat);
	    }
	    if (pat == NULL || bp->dbg_prog == NULL)
		vim_free(bp->dbg_name);
	    else
	    {
		if (bp->dbg_lnum == 0)	// default line number is 1
		    bp->dbg_lnum = 1; // TODO: elsewhere we check for 0
#ifdef FEAT_PROFILE
		if (eap->cmdidx != CMD_profile)
#endif
		{
		    DEBUGGY(gap, gap->ga_len).dbg_nr = ++last_breakp;
		    ++debug_tick;
		}
		++gap->ga_len;
	    }
	}
	else
	{
	    // DBG_EXPR
	    DEBUGGY(gap, gap->ga_len++).dbg_nr = ++last_breakp;
	    ++debug_tick;
	}
    }
}

/*
 * ":debuggreedy".
 */
    void
ex_debuggreedy(exarg_T *eap)
{
    if (eap->addr_count == 0 || eap->line2 != 0)
	debug_greedy = TRUE;
    else
	debug_greedy = FALSE;
}

/*
 * ":breakdel" and ":profdel".
 */
    void
ex_breakdel(exarg_T *eap)
{
    struct debuggy *bp, *bpi;
    int		nr;
    int		todel = -1;
    int		del_all = FALSE;
    int		i;
    linenr_T	best_lnum = 0;
    garray_T	*gap;

    gap = &dbg_breakp;
    if (eap->cmdidx == CMD_profdel)
    {
#ifdef FEAT_PROFILE
	gap = &prof_ga;
#else
	ex_ni(eap);
	return;
#endif
    }

    if (vim_isdigit(*eap->arg))
    {
	// ":breakdel {nr}"
	nr = atol((char *)eap->arg);
	for (i = 0; i < gap->ga_len; ++i)
	    if (DEBUGGY(gap, i).dbg_nr == nr)
	    {
		todel = i;
		break;
	    }
    }
    else if (*eap->arg == '*')
    {
	todel = 0;
	del_all = TRUE;
    }
    else
    {
	// ":breakdel {func|file|expr} [lnum] {name}"
	if (dbg_parsearg(eap->arg, gap) == FAIL)
	    return;
	bp = &DEBUGGY(gap, gap->ga_len);
	for (i = 0; i < gap->ga_len; ++i)
	{
	    bpi = &DEBUGGY(gap, i);
	    if (bp->dbg_type == bpi->dbg_type
		    && STRCMP(bp->dbg_name, bpi->dbg_name) == 0
		    && (bp->dbg_lnum == bpi->dbg_lnum
			|| (bp->dbg_lnum == 0 // TODO: can it be?
			    && (best_lnum == 0
				|| bpi->dbg_lnum < best_lnum))))
	    {
		todel = i;
		best_lnum = bpi->dbg_lnum;
	    }
	}
	vim_free(bp->dbg_name);
    }

    if (todel < 0)
	semsg(_("E161: Breakpoint not found: %s"), eap->arg);
    else
    {
	while (gap->ga_len > 0)
	{
	    vim_free(DEBUGGY(gap, todel).dbg_name);
#ifdef FEAT_EVAL
	    if (DEBUGGY(gap, todel).dbg_type == DBG_EXPR
		    && DEBUGGY(gap, todel).dbg_val != NULL)
		free_tv(DEBUGGY(gap, todel).dbg_val);
#endif
	    vim_regfree(DEBUGGY(gap, todel).dbg_prog);
	    --gap->ga_len;
	    if (todel < gap->ga_len)
		mch_memmove(&DEBUGGY(gap, todel), &DEBUGGY(gap, todel + 1),
			      (gap->ga_len - todel) * sizeof(struct debuggy));
#ifdef FEAT_PROFILE
	    if (eap->cmdidx == CMD_breakdel)
#endif
		++debug_tick;
	    if (!del_all)
		break;
	}

	// If all breakpoints were removed clear the array.
	if (gap->ga_len == 0)
	    ga_clear(gap);
    }
}

/*
 * ":breaklist".
 */
    void
ex_breaklist(exarg_T *eap UNUSED)
{
    struct debuggy *bp;
    int		i;

    if (dbg_breakp.ga_len == 0)
	msg(_("No breakpoints defined"));
    else
	for (i = 0; i < dbg_breakp.ga_len; ++i)
	{
	    bp = &BREAKP(i);
	    if (bp->dbg_type == DBG_FILE)
		home_replace(NULL, bp->dbg_name, NameBuff, MAXPATHL, TRUE);
	    if (bp->dbg_type != DBG_EXPR)
		smsg(_("%3d  %s %s  line %ld"),
		    bp->dbg_nr,
		    bp->dbg_type == DBG_FUNC ? "func" : "file",
		    bp->dbg_type == DBG_FUNC ? bp->dbg_name : NameBuff,
		    (long)bp->dbg_lnum);
	    else
		smsg(_("%3d  expr %s"),
		    bp->dbg_nr, bp->dbg_name);
	}
}

/*
 * Find a breakpoint for a function.
 *
 * If there's a function breakpoint defined, returns that. Otherwise if there's
 * a file/line breakpoint defined within the body of the funciton, return that.
 *
 * Returns the ealiest line number within the function at which to break or 0 if
 * there is no appropriate breakpoint.
 */
    linenr_T
dbg_find_breakpoint_in_func(
    funccall_T* func_call,
    linenr_T    after )
{
    linenr_T func_breakpoint = dbg_find_breakpoint( FALSE,
						    func_name( func_call ),
						    after );
    scriptitem_T *script =
	SCRIPT_ITEM( func_call->func->uf_script_ctx.sc_sid );

    // TODO(BenJ): can you define a funciton outside of a script context ?!
    assert( script );

    int func_start = func_call->func->uf_script_ctx.sc_lnum;
    int func_end = func_call->func->uf_script_ctx.sc_lnum +
	func_call->func->uf_lines.ga_len;


    linenr_T file_breakpoint = dbg_find_breakpoint( TRUE,
						    script->sn_name,
						    func_start + after );
    linenr_T breakpoint = 0;

    if (file_breakpoint > 0)
    {
	// we need to return an offset into function's lines, so calculate that
	// based on where the function is defined

	assert( file_breakpoint > func_start );
	file_breakpoint -= func_start;

	assert( file_breakpoint > after );

	// we might have found a breakpoint outside of the function, so
	// don't overflow the function's lines -
	if (file_breakpoint < 0 ||
	    file_breakpoint >= func_call->func->uf_lines.ga_len)
	{
	    file_breakpoint = 0;
	}
    }

    if ( func_breakpoint > 0 && file_breakpoint > 0 )
    {
	// Pick the smallest
	breakpoint = ( func_breakpoint < file_breakpoint )
		   ? func_breakpoint
		   : file_breakpoint;

    }
    else if ( file_breakpoint > 0 )
    {
	breakpoint = file_breakpoint;
    }
    else
    {
	breakpoint = func_breakpoint;
    }


    assert( breakpoint == 0 || breakpoint > after );
    assert( breakpoint == 0 || breakpoint < func_call->func->uf_lines.ga_len );

    return breakpoint;
}


/*
 * Find a breakpoint for a function or sourced file.
 * Returns line number at which to break; zero when no matching breakpoint.
 */
    linenr_T
dbg_find_breakpoint(
    int		file,	    // TRUE for a file, FALSE for a function
    char_u	*fname,	    // file or function name
    linenr_T	after)	    // after this line number
{
    return debuggy_find(file, fname, after, &dbg_breakp, NULL);
}

#if defined(FEAT_PROFILE) || defined(PROTO)
/*
 * Return TRUE if profiling is on for a function or sourced file.
 */
    int
has_profiling(
    int		file,	    // TRUE for a file, FALSE for a function
    char_u	*fname,	    // file or function name
    int		*fp)	    // return: forceit
{
    return (debuggy_find(file, fname, (linenr_T)0, &prof_ga, fp)
							      != (linenr_T)0);
}
#endif

/*
 * Common code for dbg_find_breakpoint() and has_profiling().
 */
    static linenr_T
debuggy_find(
    int		is_file,    // TRUE for a file, FALSE for a function
    char_u	*fname,	    // file or function name
    linenr_T	after,	    // after this line number
    garray_T	*gap,	    // either &dbg_breakp or &prof_ga
    int		*fp)	    // if not NULL: return forceit
{
    struct debuggy *bp;
    int		i;
    linenr_T	lnum = 0;
    char_u	*name = NULL;
    char_u	*short_name = fname;
    int		prev_got_int;

    // Return quickly when there are no breakpoints.
    if (gap->ga_len == 0)
	return (linenr_T)0;

    // For a script-local function remove the prefix, so that
    // "profile func Func" matches "Func" in any script.  Otherwise it's very
    // difficult to profile/debug a script-local function.  It may match a
    // function in the wrong script, but that is much better than not being
    // able to profile/debug a function in a script with unknown ID.
    // Also match a script-specific name.
    if (!is_file && fname[0] == K_SPECIAL)
    {
	short_name = vim_strchr(fname, '_') + 1;
	name = alloc(STRLEN(fname) + 3);
	if (name != NULL)
	{
	    STRCPY(name, "<SNR>");
	    STRCPY(name + 5, fname + 3);
	}
    }

    for (i = 0; i < gap->ga_len; ++i)
    {
	// Skip entries that are not useful or are for a line that is beyond
	// an already found breakpoint.
	bp = &DEBUGGY(gap, i);
	if (((bp->dbg_type == DBG_FILE) == is_file
		    && bp->dbg_type != DBG_EXPR && (
#ifdef FEAT_PROFILE
		gap == &prof_ga ||
#endif
		(bp->dbg_lnum > after && (lnum == 0 || bp->dbg_lnum < lnum)))))
	{
	    // Save the value of got_int and reset it.  We don't want a
	    // previous interruption cancel matching, only hitting CTRL-C
	    // while matching should abort it.
	    prev_got_int = got_int;
	    got_int = FALSE;
	    if ((name != NULL
		   && vim_regexec_prog(&bp->dbg_prog, FALSE, name, (colnr_T)0))
		    || vim_regexec_prog(&bp->dbg_prog, FALSE,
						       short_name, (colnr_T)0))
	    {
		lnum = bp->dbg_lnum;
		if (fp != NULL)
		    *fp = bp->dbg_forceit;
	    }
	    got_int |= prev_got_int;
	}
#ifdef FEAT_EVAL
	else if (bp->dbg_type == DBG_EXPR)
	{
	    typval_T *tv;
	    int	      line = FALSE;

	    prev_got_int = got_int;
	    got_int = FALSE;

	    tv = eval_expr(bp->dbg_name, NULL);
	    if (tv != NULL)
	    {
		if (bp->dbg_val == NULL)
		{
		    debug_oldval = typval_tostring(NULL, TRUE);
		    bp->dbg_val = tv;
		    debug_newval = typval_tostring(bp->dbg_val, TRUE);
		    line = TRUE;
		}
		else
		{
		    if (typval_compare(tv, bp->dbg_val, EXPR_IS, FALSE) == OK
			    && tv->vval.v_number == FALSE)
		    {
			typval_T *v;

			line = TRUE;
			debug_oldval = typval_tostring(bp->dbg_val, TRUE);
			// Need to evaluate again, typval_compare() overwrites
			// "tv".
			v = eval_expr(bp->dbg_name, NULL);
			debug_newval = typval_tostring(v, TRUE);
			free_tv(bp->dbg_val);
			bp->dbg_val = v;
		    }
		    free_tv(tv);
		}
	    }
	    else if (bp->dbg_val != NULL)
	    {
		debug_oldval = typval_tostring(bp->dbg_val, TRUE);
		debug_newval = typval_tostring(NULL, TRUE);
		free_tv(bp->dbg_val);
		bp->dbg_val = NULL;
		line = TRUE;
	    }

	    if (line)
	    {
		lnum = after > 0 ? after : 1;
		break;
	    }

	    got_int |= prev_got_int;
	}
#endif
    }
    if (name != fname)
	vim_free(name);

    return lnum;
}

/*
 * Called when a breakpoint was encountered.
 */
    void
dbg_breakpoint(char_u *name, linenr_T lnum)
{
    if ( debug_busy )
    {
	return;
    }

    // We need to check if this line is actually executed in do_one_cmd()
    debug_breakpoint_name = name;
    debug_breakpoint_lnum = lnum;
}
#endif
