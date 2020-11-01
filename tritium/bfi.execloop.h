
#ifdef FNAME

#ifndef COOLFUNC
#if defined(__GNUC__) && ((__GNUC__>4) || (__GNUC__==4 && __GNUC_MINOR__>=4))
__attribute__((optimize(3),hot))
#endif
#else
#undef COOLFUNC
#endif

static void
FNAME(int * p, void * mem)
{
    register icell * m = mem;
#ifdef DYNAMIC_MASK
    const icell msk = (icell)cell_mask;
#define M(x) ((x) &= msk)
#define MS(x) ((x) & msk)
#elif defined(EXTENDED_MASK)
    const icell msk = ((icell)2 << (cell_length-1)) - 1;
#define M(x) ((x) &= msk)
#define MS(x) ((x) & msk)
#else
#define M(x) ((icell)(x))
#define MS(x) ((icell)(x))
#endif
    for(;;) {
	m += p[0];
	switch(p[1])
	{
	case T_ADD: *m += p[2]; p += 3; break;
	case T_SET: *m = p[2]; p += 3; break;

	case T_END:
	    if(M(*m) != 0) p += p[2];
	    p += 3;
	    break;

	case T_WHL:
	    if(M(*m) == 0) p += p[2];
	    p += 3;
	    break;

	case T_ENDIF:
	    p += 2;
	    break;

	case T_CALC:
	    *m = p[2] + m[p[3]] * p[4] + m[p[5]] * p[6];
	    p += 7;
	    break;

	case T_CALC2:
	    *m = p[2] + m[p[3]] * p[4];
	    p += 5;
	    break;

	case T_CALC3:
	    *m += m[p[2]] * p[3];
	    p += 4;
	    break;

	case T_CALC4:
	    *m = m[p[2]];
	    p += 3;
	    break;

	case T_CALC5:
	    *m += m[p[2]];
	    p += 3;
	    break;

	case T_CALCMULT:
	    *m = m[p[2]] * m[p[3]];
	    p += 4;
	    break;

	case T_LT:
	    *m += (m[p[2]] < m[p[3]]);
	    p += 4;
	    break;

	case T_DIV:
	    if (M(m[1]) != 0) {
		m[2] = M(m[0]) % M(m[1]);
		m[3] = M(m[0]) / M(m[1]);
	    } else {
		m[2] = m[0];
		m[3] = 0;
	    }

	    p += 2;
	    break;

	case T_ADDWZ:
	    /* This is normally a running dec, it cleans up a rail */
	    while(M(*m)) {
		m[p[2]] += p[3];
		m += p[4];
	    }
	    p += 5;
	    break;

	case T_ZFIND:
	    /* Search along a rail til you find the end of it. */
	    while(M(*m)) {
		m += p[2];
	    }
	    p += 3;
	    break;

	case T_MFIND:
	    /* Search along a rail for a minus 1 */
            *m -= 1;
            while(MS(*m) != MS(-1)) m += p[2];
            *m += 1;
	    p += 3;
	    break;

	case T_INP:
	    *m = getch(*m);
	    p += 2;
	    break;

	case T_INPI:
	    if (sizeof(*m) <= sizeof(int))
		*m = getint(*m);
	    else {
		int ch, ret_eof = 1, do_neg = 0;
		icell a = 0;
		while((ch = getchar()) != EOF) {
		    if (ch >= '0' && ch <= '9') {
			if (!ret_eof)
			    a *= 10;
			if (ch != '0')
			    a += ch - '0';
			ret_eof = 0;
		    } else if (ret_eof && ch == '-') {
			do_neg = 1;
			ret_eof = 0;
		    } else if (!ret_eof)
			break;
		}
		ungetc(ch, stdin);
		if (do_neg)
		    a = -a;

		if (!ret_eof)
		    *m = a;
	    }
	    p += 2;
	    break;

	case T_PRT:
	    putch(*m);
	    p += 2;
	    break;

	case T_PRTI:
	    {
		char buf[4+3*sizeof(icell)], *sp=buf;
		icell ch = M(*m);
		do {
		    *sp++ = '0' + ch % 10;
		} while((ch /= 10) != 0);
		do {
		    putch(*--sp);
		} while(sp>buf);
	    }
	    p += 2;
	    break;

	case T_CHR:
	    putch(p[2]);
	    p += 3;
	    break;

	case T_STOP:
            fprintf(stderr, "STOP Command executed.\n");
            /*FALLTHROUGH*/
	case T_FINI:
	    goto break_break;
	}
    }
break_break:;
}

#undef M
#undef MS
#endif
