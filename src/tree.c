/***             analog 6.0             http://www.analog.cx/             ***/
/*** This program is copyright (c) Stephen R. E. Turner 1995 - 2004 except as
 *** stated otherwise.
 ***
 *** This program is free software. You can redistribute it and/or modify it
 *** under the terms of version 2 of the GNU General Public License, which you
 *** should have received with it.
 ***
 *** This program is distributed in the hope that it will be useful, but
 *** without any warranty, expressed or implied.   ***/

#include "anlghea3.h"

/*** tree.c; functions for tree construction and processing. ***/

/* first, treefind(), analogous to hashfind() */
/* NB Don't really need as much data as Hashindex *, but allows us to use
   previous sort routines etc. */
Hashindex *treefind(char *name, char *nameend, Hashtable **tree,
		    Hashindex *item, cutfnp cutfn, logical build,
		    logical transient, logical reuse, Memman *space,
		    choice datacols[OUTCOME_NUMBER][DATACOLS_NUMBER][2],
		    unsigned int data_number) {
  /* datacols == NULL means we guarantee that item->own is all zero's */
  Hashindex *lp, *lastlp, *newone;   /* not called new because of C++ */
  unsigned long magic;

  if (*tree == NULL && !build)
    return(NULL);
  else if (*tree == NULL)
    *tree = rehash(NULL, TREEHASHSIZE, space);
  else if (TOO_FULL_TREE((*tree)->n, (*tree)->size))
    *tree = rehash(*tree, NEW_SIZE_TREE((*tree)->size), space);
  if (build && (strchr(name, '*') != NULL || strchr(name, '?') != NULL))
    magic = 0;
  else
    MAGICNOTREE(magic, name, nameend, (*tree)->size);

  lp = (*tree)->head[magic];
  lastlp = NULL;

  while (TRUE) {
    if (lp == NULL) { /* not found */
      if (build) {
	newone = newtreeentry(name, nameend, item, transient, reuse, space,
			      data_number);
	if (lastlp == NULL)
	  (*tree)->head[magic] = newone;
	else
	  lastlp->next = newone;
	((*tree)->n)++;
	if (cutfn != NULL) {
	  cutfn(&name, &nameend, item->name, build);
	  if (name != NULL)
	    (void)treefind(name, nameend, (Hashtable **)&(newone->other),
			   item, cutfn, build, transient, reuse, space,
			   datacols, data_number);
	}
      }
      else {  /* !build */
	for (newone = NULL, lp = (*tree)->head[0]; lp != NULL; TO_NEXT(lp)) {
	  if (genwildmatch(name, nameend, lp->name)) {
	    if (newone == NULL) {
	      newone = newtreeentry(name, nameend, item, transient, reuse,
				    space, data_number);
	      if (lastlp == NULL)
		(*tree)->head[magic] = newone;
	      else
		lastlp->next = newone;
	      ((*tree)->n)++;
	    }
	    graft((Hashtable **)&(newone->other), (Hashtable *)(lp->other),
		  space, data_number);
	  }
	}
	if (newone != NULL) {
	  if (cutfn != NULL) {
	    cutfn(&name, &nameend, item->name, build);
	    if (name != NULL)
	      (void)treefind(name, nameend, (Hashtable **)&(newone->other),
			     item, cutfn, build, transient, reuse, space,
			     datacols, data_number);
	  }
	}
      }
      return(newone);
    }
    else if (genstreq(name, nameend, lp->name)) {  /* found it */
      if (lp->own->reused)
	lp->own = newtreedata(lp->own, space, data_number);
      treescore(lp->own, item->own, datacols);
      if (cutfn != NULL) {
	cutfn(&name, &nameend, item->name, build);
	if (name != NULL)
	  (void)treefind(name, nameend, (Hashtable **)&(lp->other),
			 item, cutfn, build, transient, reuse, space,
			 datacols, data_number);
      }
      return(lp);
    }
    else {
      lastlp = lp;
      TO_NEXT(lp);
    }
  }
}

void graft(Hashtable **newone, Hashtable *old, Memman *space,
	   unsigned int data_number) {
  Hashindex *lp, *found;
  unsigned long magic;
  if (old != NULL) {
    for (magic = 0; magic < old->size; magic++) {
      for (lp = old->head[magic]; lp != NULL; TO_NEXT(lp)) {
	found = treefind(lp->name, strchr(lp->name, '\0'), newone, lp,
			 NULL, TRUE, FALSE, FALSE, space, NULL, data_number);
	graft((Hashtable **)&(found->other), (Hashtable *)(lp->other), space,
	      data_number);
      }
    }
  }
}

void allgraft(Hashtable *t, Memman *space, unsigned int data_number) {
  Hashindex *lp, *lp0;
  unsigned long magic;

  if (t != NULL) {
    for (magic = 1; magic < t->size; magic++) {
      for (lp = t->head[magic]; lp != NULL; TO_NEXT(lp)) {
	for (lp0 = t->head[0]; lp0 != NULL; TO_NEXT(lp0)) {
	  if (MATCHES(lp->name, lp0->name)) {
	    graft((Hashtable **)&(lp->other), (Hashtable *)(lp0->other),
		  space, data_number);
	  }
	}
	allgraft((Hashtable *)(lp->other), space, data_number);
      }
    }
  }
}

Hashindex *newtreeentry(char *name, char *nameend, Hashindex *item,
			logical transient, logical reuse, Memman *space,
			unsigned int data_number) {
  Hashindex *ans = (Hashindex *)submalloc(space, sizeof(Hashindex));

  if (*nameend == '\0' && !transient)
    ans->name = name;
  else {
    ans->name = (char *)submalloc(space, (size_t)(nameend - name + 1));
    memcpy((void *)(ans->name), (void *)name, (size_t)(nameend - name));
    ans->name[(size_t)(nameend - name)] = '\0';
  }
  if (reuse)
    ans->own = item->own;  /* will get newtreedata if hit a 2nd time */
  else
    ans->own = newtreedata(item->own, space, data_number);
  ans->other = NULL;
  ans->next = NULL;
  return(ans);
}

Hashentry *newtreedata(Hashentry *from, Memman *space,
		       unsigned int data_number) {
  Hashentry *ans;
  unsigned int i;

  ans = (Hashentry *)submalloc(space, sizeof(Hashentry));
  ans->data = (unsigned long *)submalloc(space,
					 data_number * sizeof(unsigned long));
  for (i = 0; i < data_number; i++)
    ans->data[i] = from->data[i];
  ans->bytes = from->bytes;
  ans->bytes7 = from->bytes7;
  ans->ispage = from->ispage;    /* this is good enough */
  ans->reused = FALSE;
  /* Basically, when we first create a node on the tree, we just store as its
     data a pointer to the data of the item which caused it to be created. But
     we can't accumulate more data there (that would change the original item),
     so if we hit it again, we call this function to create new space. Setting
     the "reused" flag to FALSE is a signal that we can safely accumulate in it
     on future hits. */
  return(ans);
}

void treescore(Hashentry *to, Hashentry *from,
	       choice datacols[OUTCOME_NUMBER][DATACOLS_NUMBER][2]) {
  choice i, j, k;

  if (datacols != NULL) {
    for (i = 0; i < OUTCOME_NUMBER; i++) {
      for (j = 0; (k = datacols[i][j][0]) >= 0; j++) {
	if (datacols[i][j][1] == DATE2)
	  to->data[k] = MAX(to->data[k], from->data[k]);
	else if (datacols[i][j][1] == FIRSTD2 && to->data[k] != 0 &&
		 from->data[k] != 0)
	  /* kludge for first entry */
	  to->data[k] = MIN(to->data[k], from->data[k]);
	else
	  to->data[k] += from->data[k];
      }
    }
    to->bytes += from->bytes;
    to->bytes7 += from->bytes7;
  }
}

Hashindex *sorttree(Outchoices *od, Hashtable *tree, choice rep, Floor *floor,
		    choice sortby, Floor *subfloor, choice subsortby,
		    logical alphaback, unsigned int level, Strlist *partname,
		    Alias *notcorrupt, choice requests, choice requests7,
		    choice pages, choice pages7, choice date, choice firstd,
		    unsigned long *totr, unsigned long *totr7,
		    unsigned long *totp, unsigned long *totp7,
		    double *totb, double *totb7, unsigned long *maxr,
		    unsigned long *maxr7, unsigned long *maxp,
		    unsigned long *maxp7, double *maxb, double *maxb7,
		    timecode_t *maxd, timecode_t *mind, Hashentry **badp,
		    unsigned long *badn, Memman *space,
		    choice datacols[OUTCOME_NUMBER][DATACOLS_NUMBER][2]) {
  Hashindex *gooditems, *baditems, *p, *np;
  Strlist *pn, sp;
  size_t need = (size_t)level + 3;
  Alias *ap;
  unsigned long i;
  logical ok;

  unhash(tree, &gooditems, &baditems);
  if (notcorrupt != NULL) {    /* NB notcorrupt only used for doms */
    baditems = (Hashindex *)submalloc(space, sizeof(Hashindex));
    baditems->name = LNGSTR_UNKDOMAIN;
    baditems->own = newhashentry(DATA_NUMBER, FALSE);
    baditems->other = NULL;
    baditems->next = NULL;
    for (p = gooditems, np = NULL; p != NULL; TO_NEXT(p)) {
      for (ok = FALSE, ap = notcorrupt; !ok && ap != NULL; TO_NEXT(ap)) {
	if (STREQ(p->name, ap->from))
	  ok = TRUE;
      }
      if (ok)
	np = p;
      else {   /* erase p from list */
	if (strchr(p->name, '*') == NULL && strchr(p->name, '?') == NULL)
	  debug('U', "%s", p->name);
	treescore(baditems->own, p->own, datacols);
	if (np == NULL)
	  gooditems = p->next;
	else
	  np->next = p->next;
      }
    }
  }   /* end notcorrupt != NULL */
  for (i = 0; i < tree->size; i++)
    tree->head[i] = NULL;
  for (pn = partname; pn != NULL; TO_NEXT(pn))
    need += strlen(pn->name);
  my_sort(&gooditems, &baditems, partname, &pn, &sp, need, rep, floor,
	  sortby, alphaback, od->wanthead[G(rep)], requests, requests7, pages,
	  pages7, date, firstd, totr, totr7, totp, totp7, totb, totb7, maxr,
	  maxr7, maxp, maxp7, maxb, maxb7, maxd, mind, (logical)(level != 0),
	  badp, badn, FALSE);
  for (p = gooditems; p != NULL; TO_NEXT(p)) {
    if (p->other != NULL) {
      (void)maketreename(partname, p, &pn, &sp, need, rep, FALSE);
      ((Hashtable *)(p->other))->head[0] =
	sorttree(od, (Hashtable *)(p->other), rep, subfloor, subsortby,
		 subfloor, subsortby, alphaback, level + 1, pn, NULL, requests,
		 requests7, pages, pages7, date, firstd, totr, totr7, totp,
		 totp7, totb, totb7, maxr, maxr7, maxp, maxp7, maxb, maxb7,
		 maxd, mind, NULL, NULL, space, datacols);
    }
  }
  return(gooditems);
}

void maketree(Tree *treex, Hashindex *gooditems, Hashindex *baditems,
	      choice datacols[OUTCOME_NUMBER][DATACOLS_NUMBER][2],
	      unsigned int data_number) {
  Hashindex *p;
  char *name, *nameend;

  for (p = gooditems; p != NULL; TO_NEXT(p)) {
    if (p->own != NULL) {
      name = NULL;
      treex->cutfn(&name, &nameend, p->name, FALSE);
      (void)treefind(name, nameend, &(treex->tree), p, treex->cutfn, FALSE,
		     FALSE, TRUE, treex->space, datacols, data_number);
    }
  }
  for (p = baditems; p != NULL; TO_NEXT(p)) {
    if (p->own != NULL) {
      name = NULL;
      treex->cutfn(&name, &nameend, p->name, FALSE);
      (void)treefind(name, nameend, &(treex->tree), p, treex->cutfn, FALSE,
		     FALSE, TRUE, treex->space, datacols, data_number);
    }
  }
}

void makederived(Derv *derv, Hashindex *gooditems, Hashindex *baditems,
		 unsigned char convfloor, logical multibyte, choice rep,
		 choice datacols[OUTCOME_NUMBER][DATACOLS_NUMBER][2],
		 unsigned int data_number) {
  extern Hashentry *blank_entry;

  Hashindex *p;
  Hashentry *f;
  char *name, *nameend;
  size_t len;
  logical donegood;

  for (p = gooditems, donegood = FALSE; p != NULL || !donegood; ) {
    if (p == NULL) {
      donegood = TRUE;
      p = baditems;
    }
    else {
      if (p->own != NULL) {
	name = NULL;
	for (derv->cutfn(&name, &nameend, p->name, derv->arg);
	     name != NULL;
	     derv->cutfn(&name, &nameend, p->name, derv->arg)) {
	  len = nameend - name;
	  memcpy(submalloc(derv->space, len + 1), (void *)name, len);
	  *((char *)(derv->space->next_pos) - 1) = '\0';
	                       /* = curr_pos + len */
	  f = (Hashentry *)(hashfind(derv->space, &(derv->table), data_number,
				     NULL, UNSET, NULL, NULL, "", 0, FALSE,
				     convfloor, multibyte, rep, FALSE)->other);
	  if (!ENTRY_BLANK(f))
	    treescore(f, p->own, datacols);
	}
      }
      TO_NEXT(p);
    }
  }
}

char *maketreename(Strlist *pn, Hashindex *p, Strlist **newpn, Strlist *space,
		   size_t need, choice rep, logical delims) {
  /* Compile name from strlist. We end up doing the most-significant portion
     several times, but it really is the best way, at least if the name is
     little-endian, otherwise we have to try and work out which portion we
     want when we go back down a level. Further calls will overwrite name. */
  static char *name = NULL;
  static size_t len = 0;
  logical back;
  char glue;
  char *t;
  Strlist *sp;
  size_t l;

  if (rep == REP_OS)    /* just use last component of name */
    return(p->name);

  if (rep == REP_TYPE || rep == REP_DOM || rep == REP_ORG) {
    glue = '.';
    /* back = TRUE unless numerical subdomains */
    if (rep == REP_DOM && pn != NULL && STREQ(pn->name, LNGSTR_UNRESOLVED))
      back = FALSE;
    else if (rep == REP_ORG && pn != NULL &&
	     ISDIGIT(pn->name[strlen(pn->name) - 1]))
      back = FALSE;
    else
      back = TRUE;
  }
  else {
    back = FALSE;
    if (rep == REP_DIR || rep == REP_REFSITE)
      glue = '/';
    else if (rep == REP_BROWSUM)
      glue = '.';  /* see below */
    else
      glue = '?';
  }

  /* make newpn */
  space->name = p->name;
  space->next = NULL;
  ENSURE_LEN(name, len, need + strlen(p->name));
  if (back || pn == NULL) {
    space->next = pn;
    *newpn = space;
  }
  else {
    for (sp = pn; sp->next != NULL; TO_NEXT(sp))
      ;
    sp->next = space;
    space->next = NULL;
    *newpn = pn;
  }

  /* assemble newpn */
  sp = *newpn;
  if (rep == REP_DOM && !back) /* i.e. numerical subdomain: special case */
    TO_NEXT(sp);
  t = name;
  if (rep == REP_TYPE && delims && !STREQ(p->name, LNGSTR_NOEXT) &&
      !STREQ(p->name, LNGSTR_BRKDIRS))
    *(t++) = glue;      /* delimiter at start */
  if (rep == REP_BROWSUM && sp != NULL) {
    /* special case to enable two different delimiters */
    l = strlen(sp->name);
    memcpy(t, sp->name, l);
    t += l;
    if (sp->next != NULL)
      *(t++) = '/';
    TO_NEXT(sp);
  }
  for ( ; sp != NULL; TO_NEXT(sp)) {
    l = strlen(sp->name);
    memcpy(t, sp->name, l);
    t += l;
    if (sp->next != NULL)
      *(t++) = glue;
  }
  if (delims && (rep == REP_REFSITE || rep == REP_DIR) &&
      !STREQ(p->name, LNGSTR_NODIR) && !STREQ(p->name, LNGSTR_ROOTDIR))
    *(t++) = glue;      /* delimiter at end */
  *t = '\0';

  return(name);
}

/* genstreq is like streq but takes double-pointer strings */
logical genstreq(char *a, char *b, char *t) {
  for ( ; *a == *t && a < b; a++)
    t++;
  if (a == b && *t == '\0')
    return(TRUE);
  else
    return(FALSE);
}

/* Now the various nextname functions, which vary by type of item. There are
   two patterns, cutfnp and dcutfnp. The former is used in building a tree
   report, the latter in building a dervrep.

   Pattern for cutfnp (up to pnextname) is
   void ?nextname(char **name, char **nameend, char *whole, logical build).
   whole is whole name; [*name, *nameend) is last chunk (*name == NULL if
   none); build is because behaviour of leading/trailing delimiters may be
   different if building tree; return new [*name, *nameend), or *name = NULL
   if no more; never return NULL if given NULL.

   From Bnextname onwards, the functions follow dcutfnp. They look like
   void ?nextname(char **name, char **nameend, char *whole, void *arg).
   Otherwise they are the same, except Nnextname and nnextname can return NULL
   if given NULL. Bnextname and Pnextname return NULL if _and only if_ they
   are not given NULL, and we make use of this fact in checkonerep(). */

void rnextname(char **name, char **nameend, char *whole, logical build) {
  if (*name == NULL) {
    *name = whole;
    if ((*nameend = strchr(whole + 1, '?')) == NULL)
      *nameend = strchr(whole, '\0');
  }
  else if (IS_EMPTY_STRING(*nameend))
    *name = NULL;
  else {
    *name = *nameend + 1;
    *nameend = strchr(*name, '\0');
  }
}

void inextname(char **name, char **nameend, char *whole, logical build) {
  static char *s0 = LNGSTR_NODIR;
  static char *s1 = LNGSTR_ROOTDIR;
  logical first = FALSE;

  if (*name == NULL) {
    *name = whole;
    first = TRUE;
  }
  else if (IS_EMPTY_STRING(*nameend) || IS_EMPTY_STRING((*nameend) + 1) ||
	   **nameend == '?' || *((*nameend) + 1) == '?') {
    /* including s0 and s1 */
    *name = NULL;
    return;
  }
  else
    *name = *nameend + 1;

  for (*nameend = *name + (ptrdiff_t)(first && **name != '\0');
       **nameend != '/' && **nameend != '\0' && **nameend != '?'; (*nameend)++)
    ;  /* run nameend to next '/', '\0' or '?' */
  if (**nameend == '/') {
    for ( ; *((*nameend) + 1) == '/'; (*nameend)++)
      ;  /* run to last consecutive '/' */
  }
  if ((**nameend == '\0' || **nameend == '?') && !build) {
    if (first) {
      if (**name != '/')
	*name = s0;
      else
	*name = s1;
      *nameend = strchr(*name, '\0');
    }
    else
      *name = NULL;
  }
}

static logical isnum = FALSE;  /* used in onextname() and Znextname() */

void onextname(char **name, char **nameend, char *whole, logical build) {
  static char *s0 = LNGSTR_UNRESOLVED;
  static char *s1 = LNGSTR_NODOMAIN;

  if (*name == NULL) {
    isnum = FALSE;
    *nameend = strchr(whole, '\0');
    if (!build) {
      if (strchr(whole, '.') == NULL) {
	*name = s1;
	*nameend = strchr(*name, '\0');
	debug('V', "%s", whole);
	return;
      }
      if (ISDIGIT(*(*nameend - 1))) {
	*name = s0;
	*nameend = strchr(*name, '\0');
	isnum = TRUE;
	return;
      }
    }
    else if (ISDIGIT(*whole) &&  /* test for num. more subtle while building */
	     (ISDIGIT(*(*nameend - 1)) || *(*nameend - 1) == '*')) {
      *name = s0;
      *nameend = strchr(*name, '\0');
      isnum = TRUE;
      return;
    }
    for (*name = *nameend - 1; **name != '.' && *name != whole; (*name)--)
      ;
    if (**name == '.')
      (*name)++;
  }
  else if (isnum) {
    if (*name == s0)
      *name = whole;
    else if (IS_EMPTY_STRING(*nameend) || IS_EMPTY_STRING((*nameend) + 1)) {
      *name = NULL;
      return;
    }
    else
      *name = *nameend + 1;
    for (*nameend = *name; **nameend != '.' && **nameend != '\0'; (*nameend)++)
      ;   /* run to first '.' or '\0' */
  }
  else if (*name == s1 || *name - whole < 2)
    *name = NULL;
  else {
    *nameend = *name - 1;
    for (*name -= 2; **name != '.' && *name != whole; (*name)--)
      ;
    if (**name == '.')
      (*name)++;
  }
}

void tnextname(char **name, char **nameend, char *whole, logical build) {
  static char *s0 = LNGSTR_NOEXT;
  static char *s1 = LNGSTR_BRKDIRS;

  logical first = FALSE;

  if (*name == NULL) {
    if ((*nameend = strchr(whole, '?')) == NULL)
      *nameend = strchr(whole, '\0');
    first = TRUE;
  }
  else if (*name == s0 || *name == s1 || *name == whole ||
	   *name == whole + 1 || **name == '/') {
    *name = NULL;
    return;
  }
  else
    *nameend = *name - 1;

  *name = *nameend - 1;
  if (**name == '/' && first) {
    *name = s1;
    *nameend = strchr(*name, '\0');
    return;
  }
  for ( ; **name != '.' && **name != '/' && *name != whole; (*name)--)
    ;
  if (**name == '.' && (*name) + 1 != *nameend)
    (*name)++;
  else if (build)
    return;
  else if (first) {
    *name = s0;
    *nameend = strchr(*name, '\0');
  }
  else
    *name = NULL;
}

void snextname(char **name, char **nameend, char *whole, logical build) {
  if (*name == NULL) {
    *name = whole;
    for (*nameend = *name; **nameend != ':' && **nameend != '\0'; (*nameend)++)
      ;
    if (*nameend == '\0') {
      *nameend = *name;
      return;
    }
    (*nameend)++;
    if (**nameend == '/')
      (*nameend)++;
    if (**nameend == '/')
      (*nameend)++;
    for ( ; **nameend != '/' && **nameend != '\0'; (*nameend)++)
      ;
  }
  else
    inextname(name, nameend, whole, build);
}

void Znextname(char **name, char **nameend, char *whole, logical build) {
  static char *s1 = LNGSTR_NODOMAIN;
  static char *s2 = LNGSTR_UNKDOMAIN;
  extern Strpairlist **domlevels;

  Strpairlist *ap;
  unsigned int c;

  if (*name == NULL) {
    isnum = FALSE;
    *nameend = strchr(whole, '\0');
    if (ISDIGIT(*(*nameend - 1)) ||
	(build && ISDIGIT(*whole) && *(*nameend - 1) == '*')) { /* num. addr */
      *name = whole;
      for (c = 0, *nameend = *name;
	   *nameend - *name <= 3 && ISDIGIT(**nameend); (*nameend)++) {
	c *= 10;
	c += **nameend - '0';
      }   /* c is now first component of IP address */
      if (**nameend != '.' || *nameend == *name || c > 255) {
	/* coz of this check, Org Rep can get more s1's & s2's than Dom Rep */
	if (strchr(*name, '.') == NULL)
	  *name = s1;
	else
	  *name = s2;
	*nameend = strchr(*name, '\0');
	return;
      }
      /* change c to be number of components required */
      if (c >= 128 || (c >= 61 && c <= 68) || c == 24 || c == 80 || c == 81)
	c = 2;
      else
	c = 1;
      for ( ; c > 1; c--) {
	(*nameend)++;
	while (ISDIGIT(**nameend))
	  (*nameend)++;
	if (**nameend != '.' && !build) {
	  *name = s2;
	  *nameend = strchr(*name, '\0');
	  return;
	}
      }
      isnum = TRUE;
      return;
    }
    /* non-numerical addresses */
    for (*name = *nameend; **name != '.' && *name != whole; (*name)--)
      ;
    if (*name == whole) {
      if (!build) {
	*name = s1;
	*nameend = strchr(*name, '\0');
      }
      return;
    }
    (*name)++;
    c = 26 * ((int)(**name - 'a'));
    if (**name != '\0')
      c += (int)(*((*name) + 1) - 'a');
    if (c >= DOMLEVEL_NUMBER)
      c = DOMLEVEL_NUMBER - 1;
    for (ap = domlevels[c]; ap != NULL && !STREQ(*name, ap->name);
	 TO_NEXT(ap))
      ;
    if (ap == NULL) {  /* domain not found */
      *name = s2;
      *nameend = strchr(*name, '\0');
      return;
    }
    /* otherwise we've now found the right number of levels */
    (*name)--;
    for (c = (unsigned int)(*(ap->data) - '0'); c > 1 && *name != whole;
	 c--) {
      for((*name)--; **name != '.' && *name != whole; (*name)--)
	;
    }
    if (*name == whole && (unsigned int)(*(ap->data) - '0') - c >= 2) {
      /* don't use whole name even if <= levels (but don't just use domain) */
      for ( ; **name != '.'; (*name)++)
	;
    }
    if (**name == '.')
    (*name)++;
  }
  else
    onextname(name, nameend, whole, build);
}

void bnextname(char **name, char **nameend, char *whole, logical build) {
  /* See Bnextname() below for annotation */

  if (*name == NULL) {
    *name = whole;
    for (*nameend = *name; **nameend != '/' && **nameend != '\0';
	 (*nameend)++)
      ;
  }
  else if (**nameend == '/') {
    *name = *nameend + 1;
    for (*nameend = *name; **nameend != '.' && **nameend != ' ' &&
	   **nameend != '\0'; (*nameend)++)
      ;
  }
  else if (**nameend == '.') {
    *name = *nameend + 1;
    for (*nameend = *name; **nameend != ' ' && **nameend != '\0'; (*nameend)++)
      ;
  }
  else
    *name = NULL;
}

void pnextname(char **name, char **nameend, char *whole, logical build) {

  if (*name == NULL) {
    *name = whole;
    for (*nameend = *name; **nameend != ':' && **nameend != '\0';
	 (*nameend)++)
      ;
  }
  else if (**nameend == ':') {
    *name = *nameend + 1;
    for (*nameend = *name; **nameend != '\0'; (*nameend)++)
      ;
  }
  else
    *name = NULL;
}

/* When compiling the Browser Summary, browser names go through three
   functions. First Bnextname() locates the bit corresponding to the browser
   name and version (this is most of the work). Then do_aliasb() makes slight
   adjustments to get it into canonical form (Name/v.v). Then bnextname()
   does the actual tree-ification of this canonical form.                    */
void Bnextname(char **name, char **nameend, char *whole, void *arg) {
  /* NB "arg" is ignored */
  static char *s1 = "Netscape (compatible)";
  size_t len;

  if (*name == NULL) {
    /* First recognise some strings anywhere in the name (mostly claiming to be
       "Mozilla (compatible)"). The order may matter, for example if some
       browsers claim to both be Mozilla (compatible) and MSIE (compatible).
       Note that "len =" is simple assignment and will always return true; it's
       just a way of setting len. */
    if (((*name = strstr(whole, "Mosaic")) != NULL && (len = 6)) ||
	((*name = strstr(whole, "mosaic")) != NULL && (len = 6)) ||
	((*name = strstr(whole, "Konqueror")) != NULL && (len = 9)) ||
	((*name = strstr(whole, "Galeon")) != NULL && (len = 6)) ||
	((*name = strstr(whole, "Phoenix")) != NULL && (len = 7)) ||
	((*name = strstr(whole, "Firebird")) != NULL && (len = 8)) ||
	((*name = strstr(whole, "Firefox")) != NULL && (len = 7)) ||
	((*name = strstr(whole, "Chimera")) != NULL && (len = 7)) ||
	((*name = strstr(whole, "Camino")) != NULL && (len = 6)) ||
	((*name = strstr(whole, "Chrome")) != NULL && (len = 6)) ||
	((*name = strstr(whole, "Safari")) != NULL && (len = 6)) ||
	((*name = strstr(whole, "WebTV")) != NULL && (len = 5)) ||
	((*name = strstr(whole, "Opera")) != NULL && (len = 5)) ||
	((*name = strstr(whole, "NetFront")) != NULL && (len = 8)) ||
	((*name = strstr(whole, "MSIE")) != NULL && (len = 4))) {
      *nameend = *name + len;
      if (**nameend == '/' || **nameend == ' ') {
	for ((*nameend)++; ISALNUM(**nameend) || **nameend == '.' ||
	       **nameend == '-' || **nameend == '+'; (*nameend)++)
	  ;  /* run to end of version number */
      }
      else if (headmatch(*name, "Galeon") /* Galeon uses "Galeon; v.vv" */ &&
	       **nameend == ';' && *(*nameend + 1) == ' ' &&
	       ISDIGIT(*(*nameend + 2))) {
	for ((*nameend) += 2; ISALNUM(**nameend) || **nameend == '.' ||
	       **nameend == '-' || **nameend == '+'; (*nameend)++)
	  ;
      }
    }
    else if (headmatch(whole, "Mozilla")) {
      if (strstr(whole + 9, "compatible") || strstr(whole + 9, "Compatible")) {
	*name = s1;
	*nameend = s1 + 21;
      }
      else {  /* probably genuine Netscape/Mozilla */
	*name = whole;
	/* Mozilla has version number much later so keep the whole string */
	if (*(*name + 8) == '5' ||
	    (*nameend = strchr(whole + 7, ' ')) == NULL)
	  *nameend = strchr(whole + 7, '\0');
      }
    }
    else {
      *name = whole;
      *nameend = strchr(whole, '\0');
    }
  }
  else
    *name = NULL;
}

void Pnextname(char **name, char **nameend, char *whole, void *arg) {
  char *c, *d;

  if (*name == NULL) {
    if (arg != NULL && included(whole, FALSE, (Include *)arg)) {
      *name = "Robots";
      *nameend = strchr(*name, '\0');
      return;
    }
    if (headmatch(whole, "Mozilla"))
	whole += 7;  /* just to save searching time */

    /* First find Windows versions, starting with "Windows" or "WinNT" or
       "Win9" */
    if ((c = strstr(whole, "Windows")) != NULL) {
      c += 7;
      if (*c == ';') {
	/* Mozilla/5 uses strings like "Windows; U; Win98" so we have to look
	   for the second Windows or Win. */
	if ((d = strstr(c + 1, "Windows")) != NULL)
	  c = d + 7;
	else if ((d = strstr(c + 1, "WinNT")) != NULL ||
		 (d = strstr(c + 1, "Win9")) != NULL)
	  c = d + 3;
      }
      if (*c == ' ')
	c++;
    }
    else if ((c = strstr(whole, "WinNT")) != NULL ||
	     (c = strstr(whole, "Win9")) != NULL)
      c += 3;

    if (c != NULL) {  /* We did find "Windows" or "Win" */
      if (*c == '9' && *(c + 1) == '5')
	  *name = "Windows:Windows 95";
      else if (*c == '9' && *(c + 1) == '8') {
	if (strstr(c, "Win 9x 4.9"))
	  *name = "Windows:Windows ME";
	else
	  *name = "Windows:Windows 98";
      }
      else if (*c == 'N' && *(c + 1) == 'T') {
	/* advance to NT version number, which corresponds to advertised OS */
	c += 2;
	if (*c == ' ')
	  c++;
	if (*c == '5') {
	  if (*(c + 1) == '.' && (*(c + 2) == '0'))
	    *name = "Windows:Windows 2000";
	  else if (*(c + 1) == '.' && (*(c + 2) == '1'))
	    *name = "Windows:Windows XP";
	  else if (*(c + 1) == '.' && (*(c + 2) == '2'))
	    *name = "Windows:Windows Server 2003";
	  else
	    *name = "Windows:Unknown Windows (v5)";
	}
	// ## 2009-02-04 SPH modification: Added support for Windows Vista & Windows 7
	// ## 2010-02-01 Changed Win7 to Win7 & Server 2008
  // ## 2012-11-06 Add ed support for Windows 8
	else if (*c == '6') {
		if (*(c+1) == '.' && (*(c+2) == '0'))
			*name = "Windows:Windows Vista/Server 2008";
		else if (*(c+1) == '.' && (*(c+2) == '1'))
			*name = "Windows:Windows 7/Server 2008 R2";
    else if (*(c+1) == '.' && (*(c+2) == '2'))
      *name = "Windows:Windows 8/Server 2012";
	  else {
	    *name = "Windows:Unknown Windows (UA 6.x)";
	  }
	}
	else if (*c == '7') {
	  *name = "Windows:Unknown Windows (UA 7.x)";
	}
	else if (*c == '8') {
	  *name = "Windows:Unknown Windows (UA 8.x)";
	}
	else if (*c <= '9')
	  *name = "Windows:Unknown Windows";
	else
	  *name = "Windows:Windows NT 4.0";
      }
      else if (*c == 'C' && *(c + 1) == 'E')
	*name = "Windows:Windows CE";
      /* next three not MSIE, but some other vendor might use them */
      else if (*c == 'X' && *(c + 1) == 'P')
	*name = "Windows:Windows XP";
      else if (*c == '2' && *(c + 1) == '0' && *(c + 2) == '0' &&
	       *(c + 3) == '0')
	*name = "Windows:Windows 2000";
      else if (*c == 'M' && (*(c + 1) == 'E' || (*(c + 1) == 'e') ||
			     headmatch(c + 1, "illennium")))
	*name = "Windows:Windows ME";
      else if (*c == '3' && *(c + 1) == '.' && *(c + 2) == '1')
	*name = "Windows:Windows 3.1x/NT 3.51";
      else if ((*c == '1' && *(c + 1) == '6') || strstr(c + 1, "16bit") ||
	       strstr(c + 1, "16-bit"))
	*name = "Windows:Windows 16-bit";
      else if ((*c == '3' && *(c + 1) == '2') || strstr(c + 1, "32bit") ||
	       strstr(c + 1, "32-bit"))
	*name = "Windows:Windows 32-bit";
      else
	*name = "Windows:Unknown Windows";
    }

    /* Now non-Windows operating systems */
    else if ((c = strstr(whole, "Mac")) != NULL)
	*name = "Macintosh";
    else if (strstr(whole, "Linux") != NULL || strstr(whole, "linux") != NULL)
      *name = "Unix:Linux";
    else if (strstr(whole, "BSD") != NULL)
      *name = "Unix:BSD";
    else if (strstr(whole, "SunOS") != NULL || strstr(whole, "sunos") != NULL)
      *name = "Unix:SunOS";
    else if (strstr(whole, "HP-UX") != NULL || strstr(whole, "HPUX") != NULL ||
	     strstr(whole, "hp-ux") != NULL || strstr(whole, "hpux") != NULL)
      *name = "Unix:HP-UX";
    else if (strstr(whole, "IRIX") != NULL || strstr(whole, "irix") != NULL)
      *name = "Unix:IRIX";
    else if (strstr(whole, "AIX") != NULL || strstr(whole, "aix") != NULL)
      *name = "Unix:AIX";
    else if (strstr(whole, "OSF1") != NULL)
      *name = "Unix:OSF1";
    else if (strstr(whole, "VMS") != NULL)
      *name = "OpenVMS";
    else if (strstr(whole, "X11") != NULL)
      *name = "Unix:Other Unix";
    else if (strstr(whole, "WebTV") != NULL)
      *name = "WebTV";
    else if (strstr(whole, "OS/2") != NULL)
      *name = "OS/2";
    else if (strstr(whole, "BeOS") != NULL)
      *name = "BeOS";
    else if (strstr(whole, "RISC OS") != NULL)
      *name = "RISC OS";
    else if (strstr(whole, "Amiga") != NULL)
      *name = "Amiga";
    else if (strstr(whole, "Symbian") != NULL)
      *name = "Symbian OS";
    else if (strstr(whole, "PalmOS") != NULL ||
	     strstr(whole, "PalmSource") != NULL)
      *name = "Palm OS";
    else if (strstr(whole, "Atari") != NULL)
      *name = "Atari";
    else
      *name = "OS unknown";
    *nameend = strchr(*name, '\0');
  }
  else
    *name = NULL;
}

void Nnextname(char **name, char **nameend, char *whole, void *arg) {
  /* NB Quite a lot is held over to do_aliasN(), which is called from
     hashfind(), from makederived() */
  Strpairlist *al;
  char *c, *d;
  logical done;

  if (*name == NULL) {
    if ((d = strchr(whole, '?')) != NULL && *(d + 1) != '\0') {
      *d = '\0';
      for (al = (Strpairlist *)arg, done = FALSE; al != NULL && !done;
	   TO_NEXT(al)) {
	if (MATCHES(whole, al->name)) {  /* find right engine in list */
	  for (c = d; c != NULL && !done; c = strpbrk(c, "&;")) {
	    c++;                            /* find right arg */
	    if (headmatch(c, al->data) && *(c + strlen(al->data)) == '=') {
	      done = TRUE;
	      *name = c + strlen(al->data) + 1;
	      for (*nameend = *name; **nameend != '&' && **nameend != ';' &&
		     **nameend != '\0'; (*nameend)++)
		;  /* run nameend to next & or ; */
	      if (*name == *nameend)
		name = NULL;
	    }    /* if headmatch */
	  }    /* for c */
	}    /* if MATCHES */
      }    /* for al */
      *d = '?';
    }    /* if d = strchr */
  }    /* if *name == NULL */
  else
    *name = NULL;
}

void nnextname(char **name, char **nameend, char *whole, void *arg) {

  if (*name == NULL)
    Nnextname(name, nameend, whole, arg);   /* to find right CGI arg etc. */
  else if (**nameend == '&' || **nameend == ';' || **nameend == '\0')
    *name = NULL;
  else
    *name = *nameend + 1;

  if (*name == NULL)
    return;

  for ( ; **name == '+' || **name == ',' || **name == ';' || **name == '"' ||
	  **name == '(' || **name == ')' || **name == '.' || ISSPACE(**name) ||
	  (**name == '-' && (*(*name + 1) == '+' || *(*name + 1) == ',' ||
			     *(*name + 1) == ';' || *(*name + 1) == '"' ||
			     *(*name + 1) == '(' || *(*name + 1) == ')' ||
			     *(*name + 1) == '.' || *(*name + 1) == '-' ||
			     *(*name + 1) == '&' || *(*name + 1) == '\0' ||
			     ISSPACE(*(*name + 1))));
	(*name)++)
    ;  /* run to first wanted character; cf list in do_aliasN() and below */
       /* NB 'good' dots never occur at beginning of word */
  if (**name == '&' || **name == ';' || **name == '\0')
    *name = NULL;
  else for (*nameend = *name; **nameend != '+' && **nameend != '&' &&
	      **nameend != '\0' && **nameend != '"' && **nameend != ',' &&
	      **nameend != ';' && **nameend != '(' && **nameend != ')' &&
	      (**nameend != '.' || (ISALNUM(*(*nameend - 1)) &&
				    ISALNUM(*(*nameend + 1)))) &&
	      (**nameend != '-' || *nameend == *name) && !ISSPACE(**nameend);
	    (*nameend)++)
      ;  /* run to first unwanted character; see above */
}
