/* $XFree86: xc/config/makedepend/parse.c,v 1.19 2007/09/16 03:44:16 tsi Exp $ */
/*

Copyright (c) 1993, 1994, 1998 The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/

#include "def.h"

extern char	*directives[];
extern struct inclist	inclist[ MAXFILES ],
			*inclistnext,
			maininclist;
extern char	*includedirs[ ],
		**includedirsnext;

static int deftype (char *line, struct filepointer *filep,
		    struct inclist *file_red, struct inclist *file,
		    int parse_it);
static int zero_value(char *filename, char *exp, struct filepointer *filep,
		    struct inclist *file_red);
static int
gobble(struct filepointer *filep, struct inclist *file,
       struct inclist *file_red)
{
	char	*line;
	int	type;

	while ((line = getnextline(filep))) {
		switch(type = deftype(line, filep, file_red, file, FALSE)) {
		case IF:
		case IFFALSE:
		case IFGUESSFALSE:
		case IFDEF:
		case IFNDEF:
			type = gobble(filep, file, file_red);
			while ((type == ELIF) || (type == ELIFFALSE) ||
			       (type == ELIFGUESSFALSE))
			    type = gobble(filep, file, file_red);
			if (type == ELSE)
				(void)gobble(filep, file, file_red);
			break;
		case ELSE:
		case ENDIF:
			debug(0,("%s, line %d: #%s\n",
				file->i_file, filep->f_line,
				directives[type]));
			return(type);
		case DEFINE:
		case UNDEF:
		case INCLUDE:
		case INCLUDEDOT:
		case PRAGMA:
		case ERROR:
		case IDENT:
		case SCCS:
		case EJECT:
		case WARNING:
		case INCLUDENEXT:
		case INCLUDENEXTDOT:
			break;
		case ELIF:
		case ELIFFALSE:
		case ELIFGUESSFALSE:
			return(type);
		case -1:
			warning("%s", file_red->i_file);
			if (file_red != file)
				warning1(" (reading %s)", file->i_file);
			warning1(", line %d: unknown directive == \"%s\"\n",
				filep->f_line, line);
			break;
		}
	}
	return(-1);
}

/*
 * Decide what type of # directive this line is.
 */
static int
deftype (char *line, struct filepointer *filep,
	     struct inclist *file_red, struct inclist *file, int parse_it)
{
	char	*p;
	char	*directive, savechar, *q;
	int	ret;

	/*
	 * Parse the directive...
	 */
	directive=line+1;
	while (*directive == ' ' || *directive == '\t')
		directive++;

	p = directive;
	while ((*p == '_') || (*p >= 'a' && *p <= 'z'))
		p++;
	savechar = *p;
	*p = '\0';
	ret = match(directive, directives);
	*p = savechar;

	/*
	 * If we don't recognize this compiler directive or we happen to just
	 * be gobbling up text while waiting for an #endif or #elif or #else
	 * in the case of an #elif we must check the zero_value and return an
	 * ELIF or an ELIFFALSE.
	 */

	if (ret == ELIF && !parse_it)
	{
	    while (*p == ' ' || *p == '\t')
		p++;
	    /*
	     * parse an expression.
	     */
	    debug(0,("%s, line %d: #elif %s ",
		   file->i_file, filep->f_line, p));
	    ret = zero_value(file->i_file, p, filep, file_red);
	    if (ret != IF)
	    {
		debug(0,("false...\n"));
		if (ret == IFFALSE)
		    return(ELIFFALSE);
		else
		    return(ELIFGUESSFALSE);
	    }
	    else
	    {
		debug(0,("true...\n"));
		return(ELIF);
	    }
	}

	if (ret < 0 || ! parse_it)
		return(ret);

	/*
	 * now decide how to parse the directive, and do it.
	 */
	while (*p == ' ' || *p == '\t')
		p++;
	q = p + strlen(p);
	do {
		q--;
	} while (*q == ' ' || *q == '\t');
	q[1] = '\0';
	switch (ret) {
	case IF:
		/*
		 * parse an expression.
		 */
		ret = zero_value(file->i_file, p, filep, file_red);
		debug(0,("%s, line %d: %s #if %s\n",
			 file->i_file, filep->f_line, ret?"false":"true", p));
		break;
	case IFDEF:
	case IFNDEF:
		debug(0,("%s, line %d: #%s %s\n",
			file->i_file, filep->f_line, directives[ret], p));
	case UNDEF:
		/*
		 * separate the name of a single symbol.
		 */
		while (isalnum(*p) || *p == '_')
			*line++ = *p++;
		*line = '\0';
		break;
	case INCLUDE:
	case INCLUDENEXT:
		debug(2,("%s, line %d: #include%s %s\n",
			file->i_file, filep->f_line,
			(ret == INCLUDE) ? "" : "_next", p));

		/* Support ANSI macro substitution */
		while (1) {
			struct symtab **sym;

			if (!*p || *p == '"' || *p == '<')
				break;

			sym = isdefined(p, file_red);
			if (!sym)
				break;

			p = (*sym)->s_value;
			debug(3,("%s : #includes SYMBOL %s = %s\n",
			       file->i_incstring,
			       (*sym) -> s_name, p ? p : "<undefined>"));
			if (!p)
				return(-2);
		}

		/*
		 * Separate the name of the include file.
		 */
		while (*p && *p != '"' && *p != '<')
			p++;
		if (! *p)
			return(-2);
		if (*p++ == '"') {
			if (ret == INCLUDE)
				ret = INCLUDEDOT;
			else
				ret = INCLUDENEXTDOT;
			while (*p && *p != '"')
				*line++ = *p++;
		} else
			while (*p && *p != '>')
				*line++ = *p++;
		*line = '\0';
		break;
	case DEFINE:
		/*
		 * copy the definition back to the beginning of the line.
		 */
		strcpy (line, p);
		break;
	case ELSE:
	case ENDIF:
	case ELIF:
	case PRAGMA:
	case ERROR:
	case IDENT:
	case SCCS:
	case EJECT:
	case WARNING:
		debug(0,("%s, line %d: #%s\n",
			file->i_file, filep->f_line, directives[ret]));
		/*
		 * nothing to do.
		 */
		break;
	}
	return(ret);
}

static struct symtab **
fdefined(char *symbol, struct inclist *file)
{
	struct symtab	**val;
	struct inclist	*ip;

	if (file->i_flags & DEFCHECKED)
		return(NULL);
	debug(2,("Looking for %s in %s\n", symbol, file->i_file));
	file->i_flags |= DEFCHECKED;
	/* Look in most deeply #include'd unfinished file first */
	if (file->i_list && (ip = file->i_list[file->i_listlen - 1]) &&
		 !(ip->i_flags & FINISHED) && (val = fdefined(symbol, ip)))
	{
		debug(1,("%s defined in %s as %s\n",
			 symbol, file->i_list[file->i_listlen - 1]->i_file,
			 (*val)->s_value ? (*val)->s_value : "<undefined>"));
	}
	else if ((val = slookup(symbol, file)))
	{
		debug(1,("%s defined in %s as %s\n",
			 symbol, file->i_file,
			 (*val)->s_value ? (*val)->s_value : "<undefined>"));
	}
	file->i_flags &= ~DEFCHECKED;

	return(val);
}

struct symtab **
isdefined(char *symbol, struct inclist *file)
{
	struct symtab	**val;

	if ((val = fdefined(symbol, file)))
	{
		if ((*val)->s_value != NULL)
			return(val);
	}
	else if ((val = slookup(symbol, &maininclist)) &&
		 ((*val)->s_value != NULL))
	{
		debug(1,("%s defined on command line\n", symbol));
		return(val);
	}
	debug(1,("%s not defined in %s\n", symbol, file->i_file));
	return(NULL);
}

/*
 * Return type based on if the #if expression evaluates to 0
 */
static int
zero_value(char *filename,
	   char *exp,
	   struct filepointer *filep,
	   struct inclist *file_red)
{
	if (cppsetup(filename, exp, filep, file_red))
	    return(IFFALSE);
	else
	    return(IF);
}

void
define2(char *name, char *args, char *val, struct inclist *file)
{
    int first, last, below;
    struct symtab **sp = NULL, **dest;
    struct symtab *stab;

    /* Make space if it's needed */
    if (file->i_defs == NULL)
    {
	file->i_defs = (struct symtab **)
			malloc(sizeof (struct symtab*) * SYMTABINC);
	file->i_ndefs = 0;
    }
    else if (!(file->i_ndefs % SYMTABINC))
	file->i_defs = (struct symtab **)
			realloc(file->i_defs,
			   sizeof(struct symtab*)*(file->i_ndefs+SYMTABINC));

    if (file->i_defs == NULL)
	fatalerr("malloc()/realloc() failure in insert_defn()\n");

    below = first = 0;
    last = file->i_ndefs - 1;
    while (last >= first)
    {
	/* Fast inline binary search */
	char *s1;
	char *s2;
	int middle = (first + last) / 2;

	/* Fast inline strchr() */
	s1 = name;
	s2 = file->i_defs[middle]->s_name;
	while (*s1++ == *s2++)
	    if (s2[-1] == '\0') break;

	/* If exact match, set sp and break */
	if (*--s1 == *--s2)
	{
	    sp = file->i_defs + middle;
	    break;
	}

	/* If name > i_defs[middle] ... */
	if (*s1 > *s2)
	{
	    below = first;
	    first = middle + 1;
	}
	/* else ... */
	else
	{
	    below = last = middle - 1;
	}
    }

    /*
     * Search is done.  If we found an exact match to the symbol name,
     * just replace its s_args and s_value if they are changed.
     */
    if (sp != NULL)
    {
	debug(1,("redefining %s from %s to %s in file %s\n",
		name, (*sp)->s_value ? (*sp)->s_value : "<undefined>",
		val ? val : "<undefined>", file->i_file));

	if ( (*sp)->s_args )
	    free((*sp)->s_args);
	if (args)
	    (*sp)->s_args = copy(args);
	else
	    (*sp)->s_args = NULL;

	free((*sp)->s_value);
	(*sp)->s_value = copy(val);
	return;
    }

    sp = file->i_defs + file->i_ndefs++;
    dest = file->i_defs + below + 1;
    while (sp > dest)
    {
	*sp = sp[-1];
	sp--;
    }
    stab = (struct symtab *) malloc(sizeof (struct symtab));
    if (stab == NULL)
	fatalerr("malloc()/realloc() failure in insert_defn()\n");

    debug(1,("defining %s to %s in file %s\n",
	    name, val ? val : "<undefined>", file->i_file));
    stab->s_name = copy(name);
    if (args)
	stab->s_args = copy(args);
    else
	stab->s_args = NULL;
    stab->s_value = copy(val);
    *sp = stab;
}

/* we dont expect too much macro parameters usage */
#define S_ARGS_BUFLEN 1024

void
define(char *def, struct inclist *file)
{
    static char args[S_ARGS_BUFLEN];

    char *val;
    char *p_args = args;
    int fix_args = 0, var_args = 0, loop = 1;
    char *p_tmp;

    args[0] = '\0';

    /* Separate symbol name and its value */
    val = def;
    while (isalnum(*val) || *val == '_')
	val++;

    if (*val == '(') /* is this macro definition with parameters? */
    {
	*val++ = '\0';

	do /* parse the parameter list */
	{
	    while (*val == ' ' || *val == '\t')
		val++;

	    /* extract next parameter name */
	    if (*val == '.')
	    { /* it should be the var-args parameter: "..." */
		var_args++;
		p_tmp = p_args;
		while (*val == '.')
		{
		    *p_args++ = *val++;
		    if (p_args >= &args[S_ARGS_BUFLEN-1])
			fatalerr("args buffer full failure in insert_defn()\n");
		}
		*p_args = '\0';
		if (strcmp(p_tmp,"...")!=0)
		{
		    fprintf(stderr, "unrecognized qualifier, should be \"...\" for-args\n");
		}
	    }
	    else
	    { /* regular parameter name */
		fix_args++;
		while (isalnum(*val) || *val == '_')
		{
		    *p_args++ = *val++;
		    if (p_args >= &args[S_ARGS_BUFLEN-1])
			fatalerr("args buffer full failure in insert_defn()\n");
		}
	    }
	    while (*val == ' ' || *val == '\t')
		val++;

	    if (*val == ',')
	    {
		if (var_args)
		{
		    fprintf(stderr, "there are more arguments after the first var-args qualifier\n");
		}

		*p_args++ = ','; /* we are using the , as a reserved char */
		if (p_args >= &args[S_ARGS_BUFLEN-1])
		    fatalerr("args buffer full failure in insert_defn()\n");
		val++;
	    }
	    else
	    if (*val == ')')
	    {
		*p_args = '\0';
		val++;
		loop=0;
	    }
	    else
	    if (*val != '.')
	    {
		fprintf(stderr, "trailing ) on macro arguments missing\n");
		loop=0;
	    }
	} while (loop);
    }

    if (*val)
	*val++ = '\0';
    while (*val == ' ' || *val == '\t')
	val++;

    if (!*val) /* define statements without a value will get a value of 1 */
	val = "1";

    if (strlen(args) > 0)
	define2(def, args, val, file);
    else
	define2(def, NULL, val, file);
}

struct symtab **
slookup(char *symbol, struct inclist *file)
{
	int first = 0, last;

	if (!file)
	    return NULL;

	last = file->i_ndefs - 1;
	while (last >= first)
	{
	    /* Fast inline binary search */
	    char *s1;
	    char *s2;
	    int middle = (first + last) / 2;

	    /* Fast inline strchr() */
	    s1 = symbol;
	    s2 = file->i_defs[middle]->s_name;
	    while (*s1++ == *s2++)
		if (s2[-1] == '\0') break;

	    /* If exact match, we're done */
	    if (*--s1 == *--s2)
	    {
		return file->i_defs + middle;
	    }

	    /* If symbol > i_defs[middle] ... */
	    if (*s1 > *s2)
	    {
		first = middle + 1;
	    }
	    /* else ... */
	    else
	    {
		last = middle - 1;
	    }
	}

	return NULL;
}

static void
merge2defines(struct inclist *file1, struct inclist *file2)
{
	if ((file1 != NULL) && (file2 != NULL) &&
	    (file2->i_flags & FINISHED))
	{
		int first1 = 0;
		int last1 = file1->i_ndefs - 1;

		int first2 = 0;
		int last2 = file2->i_ndefs - 1;

		int first=0;
		struct symtab** i_defs = NULL;
		int deflen=file1->i_ndefs+file2->i_ndefs;

		debug(2,("merging %s into %s\n",
			file2->i_file, file1->i_file));

		if (deflen>0)
		{
			/* make sure deflen % SYMTABINC == 0 is still true */
			deflen += (SYMTABINC - deflen % SYMTABINC) % SYMTABINC;
			i_defs=(struct symtab**)
			    malloc(deflen*sizeof(struct symtab*));
			if (i_defs==NULL)
				fatalerr("out of memory");
		}

		while ((last1 >= first1) && (last2 >= first2))
		{
			char *s1=file1->i_defs[first1]->s_name;
			char *s2=file2->i_defs[first2]->s_name;

			if (strcmp(s1,s2) < 0)
				i_defs[first++]=file1->i_defs[first1++];
			else if (strcmp(s1,s2) > 0)
				i_defs[first++]=file2->i_defs[first2++];
			else /* equal */
			{
				i_defs[first++]=file2->i_defs[first2++];
				first1++;
			}
		}
		while (last1 >= first1)
		{
			i_defs[first++]=file1->i_defs[first1++];
		}
		while (last2 >= first2)
		{
			i_defs[first++]=file2->i_defs[first2++];
		}

		if (file1->i_defs) free(file1->i_defs);
		file1->i_defs=i_defs;
		file1->i_ndefs=first;
	}
}

void
undefine(char *symbol, struct inclist *file)
{
	/* Just define it to NULL in the current file */
	define2(symbol, NULL, NULL, file);
}

int
find_includes(struct filepointer *filep, struct inclist *file,
	      struct inclist *file_red, int recursion, boolean failOK)
{
	struct inclist	*inclistp, *newfile;
	char		**includedirsp;
	char		*line;
	int		type;
	boolean		recfailOK;

	while ((line = getnextline(filep))) {
		switch(type = deftype(line, filep, file_red, file, TRUE)) {
		case IF:
		doif:
			type = find_includes(filep, file,
				file_red, recursion+1, failOK);
			while ((type == ELIF) || (type == ELIFFALSE) ||
			       (type == ELIFGUESSFALSE))
				type = gobble(filep, file, file_red);
			if (type == ELSE)
				gobble(filep, file, file_red);
			break;
		case IFFALSE:
		case IFGUESSFALSE:
		    doiffalse:
			if (type == IFGUESSFALSE || type == ELIFGUESSFALSE)
			    recfailOK = TRUE;
			else
			    recfailOK = failOK;
			type = gobble(filep, file, file_red);
			if (type == ELSE)
			    find_includes(filep, file,
					  file_red, recursion+1, recfailOK);
			else
			if (type == ELIF)
			    goto doif;
			else
			if ((type == ELIFFALSE) || (type == ELIFGUESSFALSE))
			    goto doiffalse;
			break;
		case IFDEF:
		case IFNDEF:
		    {
			int isdef = (isdefined(line, file_red) != NULL);
			if (type == IFNDEF) isdef = !isdef;

			if (isdef) {
				debug(1,(type == IFNDEF ?
				    "line %d: %s !def'd in %s via %s%s\n" : "",
				    filep->f_line, line,
				    file->i_file, file_red->i_file, ": doit"));
				type = find_includes(filep, file,
					file_red, recursion+1, failOK);
				while (type == ELIF ||
				       type == ELIFFALSE ||
				       type == ELIFGUESSFALSE)
					type = gobble(filep, file, file_red);
				if (type == ELSE)
					gobble(filep, file, file_red);
			}
			else {
				debug(1,(type == IFDEF ?
				    "line %d: %s !def'd in %s via %s%s\n" : "",
				    filep->f_line, line,
				    file->i_file, file_red->i_file,
				    ": gobble"));
				type = gobble(filep, file, file_red);
				if (type == ELSE)
					find_includes(filep, file,
						file_red, recursion+1, failOK);
				else if (type == ELIF)
					goto doif;
				else if (type == ELIFFALSE ||
					 type == ELIFGUESSFALSE)
					goto doiffalse;
			}
		    }
		    break;
		case ELSE:
		case ELIFFALSE:
		case ELIFGUESSFALSE:
		case ELIF:
			if (!recursion)
				gobble(filep, file, file_red);
		case ENDIF:
			if (recursion)
				return(type);
		case DEFINE:
			define(line, file);
			break;
		case UNDEF:
			if (!*line) {
			    warning("%s", file_red->i_file);
			    if (file_red != file)
				warning1(" (reading %s)", file->i_file);
			    warning1(", line %d: incomplete undef == \"%s\"\n",
				filep->f_line, line);
			    break;
			}
			undefine(line, file);
			break;
		case INCLUDE:
		case INCLUDEDOT:
		case INCLUDENEXT:
		case INCLUDENEXTDOT:
			inclistp = inclistnext;
			includedirsp = includedirsnext;
			debug(2,("%s, reading %s, includes %s\n",
				file_red->i_file, file->i_file, line));
			newfile = add_include(filep, file, file_red, line,
					      type, failOK);
			inclistnext = inclistp;
			includedirsnext = includedirsp;
			merge2defines(file, newfile);
			break;
		case ERROR:
		case WARNING:
			warning("%s", file_red->i_file);
			if (file_red != file)
				warning1(" (reading %s)", file->i_file);
			warning1(", line %d: %s\n",
				 filep->f_line, line);
			break;

		case PRAGMA:
		case IDENT:
		case SCCS:
		case EJECT:
			break;
		case -1:
			warning("%s", file_red->i_file);
			if (file_red != file)
			    warning1(" (reading %s)", file->i_file);
			warning1(", line %d: unknown directive == \"%s\"\n",
				 filep->f_line, line);
			break;
		case -2:
			warning("%s", file_red->i_file);
			if (file_red != file)
			    warning1(" (reading %s)", file->i_file);
			warning1(", line %d: incomplete include == \"%s\"\n",
				 filep->f_line, line);
			break;
		}
	}
	file->i_flags |= FINISHED;
	debug(2,("finished with %s\n", file->i_file));
	return(-1);
}
