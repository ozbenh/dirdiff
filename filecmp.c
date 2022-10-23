/*
 * Tcl command procedure to compare two files and report whether
 * they are identical.
 *
 * P. Mackerras 17/7/96.
 *
 * $Id: filecmp.c,v 1.3 2001/08/15 11:12:44 paulus Exp $
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <tcl.h>
#include <unistd.h>
#include <sys/fcntl.h>

#define BSIZE		32768
#define MAXTAGLEN	512	/* max tag length for sanity, < BSIZE */

#ifdef NO_MEMMOVE
#define memmove(t, f, n)	bcopy(f, t, n)
#endif

static char *rcs_ignores[] = {
    "Log",			/* must be at index 0! */
    "Id",
    "Revision",
    "Source",
    "Author",
    "Date",
    "State",
    "Header",
    NULL
};

static char *comment_leaders[] = {
    " *",
    "#",
    NULL
};

/*
 * See if the text starting at p, for n chars, looks like an RCS tag.
 * We already know p[0] == '$' and n >= 4.
 * Return 0 if it isn't a tag, -1 if it looks like a tag but is incomplete,
 * or the length of the tag.
 */
static int tag_length(p, n)
    char *p;
    int n;
{
    int i, j, k, l;
    int maybe;
    char *t, *cl;

    /* check through list of tags */
    for (i = 0; (t = rcs_ignores[i]) != NULL; ++i) {
	for (j = 1; *t != 0; ++j, ++t) {
	    if (j >= n)
		return -1;
	    if (p[j] != *t)
		break;
	}
	if (*t == 0)
	    break;
    }
    if (t == NULL)		/* if tag wasn't in list */
	return 0;
    if (j >= n)			/* if we got to the end of the buffer */
	return -1;
    if (p[j] == '$')		/* for the \dollar Id\dollar case */
	return j + 1;
    if (p[j] != ':')		/* tag word must be followed by $ or : */
	return 0;
    while (++j < n) {		/* if by :, matching $ must appear before \n */
	if (p[j] == '\n')
	    return 0;		/* got \n without $: not a tag */
	if (p[j] == '$') {
	    if (i != 0)		/* got $: if the tag wasn't \dollar Log, */
		return j + 1;	/* that's the end of the tag */
	    /* here we skip lines that start with ' * ' or '# '. */
	    for (;;) {
		/* skip to \n */
		while (++j < n && p[j] != '\n')
		    ;
		if (++j >= n)
		    break;
		maybe = 0;
		l = n - j;
		if (l >= 3 && p[j] == ' ' && p[j+1] == '*' && p[j+2] != '/')
		    continue;	/* C-style comment leader */
		if (p[j] == '#')
		    continue;	/* Script comment leader */
		if (l < 3 && p[j] == ' ' && (l < 2 || p[j] == '*'))
		    maybe = 1;	/* may be C-style comment leader */
		if (maybe)
		    break;
		return j;	/* we've found the end of the tag */
	    }
	    /* Incomplete tag following \dollar Log;
	       the complete tag could be quite long. */
	    if (n >= BSIZE)
		return 0;
	    return -1;
	}
    }
    if (n > MAXTAGLEN)		/* incomplete tag too long: not a tag */
	return 0;
    return -1;			/* incomplete tag */
}

int rcscmp(char *p1, int *k1p, char *p2, int *k2p, int e1, int e2)
{
    int k1 = *k1p, k2 = *k2p;
    int i, t1, t2;

    for (;;) {
	for (i = 0; i < k1 && i < k2; ++i) {
	    if (p1[i] != p2[i])
		return 0;
	    if (p1[i] == '$')
		break;
	}
	p1 += i;
	k1 -= i;
	p2 += i;
	k2 -= i;
	/* 4 == strlen("<dollar>Id<dollar>") */
	if (k1 < 4 && !e1 || k2 < 4 && !e2)
	    break;
	if (k1 < 4 || k2 < 4) {
	    /* near the end of one or both files */
	    if (k1 != k2 || memcmp(p1, p2, k1) != 0)
		return 0;
	    k1 = k2 = 0;
	    break;
	}
	/* check that the tags look the same (1st 2 chars at least) */
	if (p1[1] != p2[1] || p1[2] != p2[2])
	    return 0;
	t1 = tag_length(p1, k1);
	if (t1 < 0 && e1)
	    t1 = 0;
	if (t1 != 0) {
	    t2 = tag_length(p2, k2);
	    if (t2 < 0 && e2)
		t2 = 0;
	}
	if (t1 == 0 || t2 == 0) {
	    /* one or other string isn't a tag */
	    p1 += 3;
	    k1 -= 3;
	    p2 += 3;
	    k2 -= 3;
	    continue;
	}
	if (t1 < 0 || t2 < 0)
	    break;		/* one or other tag is incomplete */
	p1 += t1;
	k1 -= t1;
	p2 += t2;
	k2 -= t2;
	if (k1 == 0 || k2 == 0)
	    break;
    }
    *k1p = k1;
    *k2p = k2;
    return 1;
}

static char bktag[] = "BK Id: ";
#define BKTAGLEN	7

int bkcmp(char *p1, int *k1p, char *p2, int *k2p, int e1, int e2)
{
	int k1 = *k1p, k2 = *k2p;
	int i, match, t1, t2;

	for (;;) {
		match = 0;
		for (i = 0; i < k1 && i < k2; ++i) {
			if (p1[i] != p2[i])
				return 0;
			if (p1[i] == bktag[match]) {
				if (++match == BKTAGLEN) {
					++i;
					break;
				}
			} else {
				match = (p1[i] == bktag[0]);
			}
		}
		p1 += i;
		k1 -= i;
		p2 += i;
		k2 -= i;
		if (match < BKTAGLEN) {
			/* we have run out of one or other buffer */
			if (k1 == 0 && e1 || k2 == 0 && e2) {
				if (k1 == k2)
					break;
				return 0;
			}
			k1 += match;
			k2 += match;
			break;
		}
		/* found a tag, skip to eol on both files */
		for (t1 = 0; t1 < k1; ++t1)
			if (p1[t1] == '\n')
				break;
		for (t2 = 0; t2 < k2; ++t2)
			if (p2[t2] == '\n')
				break;
		if (t1 < k1 && t2 < k2) {
			p1 += t1;
			k1 -= t1;
			p2 += t2;
			k2 -= t2;
			continue;
		}
		/* ran out before eol on one or both files */
		if (t1 == k1 && e1 || t2 == k2 && e2) {
			k1 -= t1;
			k2 -= t2;
			if (k1 == k2)
				break;
			return 0;
		}
		/* not at eof on either file */
		k1 += BKTAGLEN;
		k2 += BKTAGLEN;
		break;
	}
	*k1p = k1;
	*k2p = k2;
	return 1;
}

int
FileCmpCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int i, fi;
    int f1, f2;
    char *b1, *b2;
    int n1, n2, same;
    int k1, k2, t1, t2;
    int rcsflag, bkflag;
    int e1, e2;
    char res_str[16];

    fi = 1;
    rcsflag = 0;
    bkflag = 0;
    if (argc > 1 && strcmp(argv[1], "-rcs") == 0) {
	++fi;
	rcsflag = 1;
    } else if (argc > 1 && strcmp(argv[1], "-bk") == 0) {
	++fi;
	bkflag = 1;
    }
    if (argc != fi + 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
			 " ?-rcs? file1 file2\"", (char *) NULL);
	return TCL_ERROR;
    }

    if ((f1 = open(argv[fi], O_RDONLY)) == -1) {
	Tcl_AppendResult(interp, "can't open \"", argv[fi], "\": ",
			 Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }
    if ((f2 = open(argv[fi+1], O_RDONLY)) == -1) {
	Tcl_AppendResult(interp, "can't open \"", argv[fi+1], "\": ",
			 Tcl_PosixError(interp), (char *) NULL);
	close(f1);
	return TCL_ERROR;
    }

    b1 = (char *) ckalloc(BSIZE);
    b2 = (char *) ckalloc(BSIZE);

    same = 1;

    k1 = k2 = 0;
    e1 = e2 = 0;

    for (;;) {
	/* Here we already have k1 chars in b1 and k2 chars in b2. */
	if (!e1 && k1 < BSIZE) {
	    n1 = read(f1, b1 + k1, BSIZE - k1);
	    if (n1 < 0) {
		Tcl_AppendResult(interp, "error reading \"", argv[fi],
				 "\": ", Tcl_PosixError(interp),
				 (char *) NULL);
		break;
	    }
	    k1 += n1;
	    if (k1 < BSIZE)
		e1 = 1;
	}
	if (!e2 && k2 < BSIZE) {
	    n2 = read(f2, b2 + k2, BSIZE - k2);
	    if (n2 < 0) {
		Tcl_AppendResult(interp, "error reading \"", argv[fi+1],
				 "\": ", Tcl_PosixError(interp),
				 (char *) NULL);
		break;
	    }
	    k2 += n2;
	    if (k2 < BSIZE)
		e2 = 1;
	}
	if (k1 == 0 && k2 == 0)
	    break;
	t1 = k1;
	t2 = k2;
	if (rcsflag)
	    same = rcscmp(b1, &k1, b2, &k2, e1, e2);
	else if (bkflag)
	    same = bkcmp(b1, &k1, b2, &k2, e1, e2);
	else {
	    if (k1 != k2 || memcmp(b1, b2, k1) != 0)
		same = 0;
	    k1 = k2 = 0;
	}
	if (!same || (e1 && e2))
	    break;
	if (k1 > 0)
	    memmove(b1, b1 + t1 - k1, k1);
	if (k2 > 0)
	    memmove(b2, b2 + t2 - k2, k2);
    }

    close(f1);
    close(f2);
    ckfree(b1);
    ckfree(b2);
    if (n1 < 0 || n2 < 0)
	return TCL_ERROR;

    res_str[15] = 0;
    snprintf(res_str, 15, "%d", same);
    Tcl_SetResult(interp, res_str, TCL_VOLATILE);
    return TCL_OK;
}

int
Filecmp_Init(interp)
    Tcl_Interp *interp;
{
    Tcl_CreateCommand(interp, "filecmp", FileCmpCmd, NULL, NULL);
    return TCL_OK;
}
