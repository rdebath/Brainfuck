
static inline void
move_opt(int *pch, int *pcount, int *pmov)
{
    if (enable_optim) {
	static struct ostack { struct ostack *p; int d; } *sp;
	static int imov = 0;
	int ch = *pch;

        if (ch == '>') {
            imov += *pcount;
	    *pch = 0;
	    return;
        } else if (ch == '<') {
            imov -= *pcount;
	    *pch = 0;
	    return;
	}

	*pmov = imov;

	if (ch == '[') {
	    struct ostack * np = malloc(sizeof(struct ostack));
	    np->p = sp;
	    np->d = imov;
	    sp = np;
	} else if (ch == ']') {
	    struct ostack * np = sp;
	    sp = sp->p;
	    *pcount = imov - np->d;
	    *pmov = imov = np->d;
	    free(np);
	} else if (ch == '~') {
	    *pcount = imov;
	    *pmov = imov = 0;
	}
    } else {
	if (*pch == ']') *pcount = 0;
	*pmov = 0;
    }
}
