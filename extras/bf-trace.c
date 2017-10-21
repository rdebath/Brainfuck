#include <stdio.h>
int
main (int argc, char *argv[])
{
    char *b = 0, *p, t[65536] = { 0 };
    unsigned short m = 0;
    int i = 0, ln = 1, col = 1;
    FILE *fp = argc > 1 ? fopen (argv[1], "r") : stdin;
    size_t r = 0;
    if (!fp || getdelim (&b, &r, argc > 1 ? '\0' : '!', fp) < 0)
	perror (argv[1]);
    else if (b && r > 0)
	for (p = b; *p; p++)
	{
	    if (ln == 0)
	    {
		char *s = b;
		ln = 1;
		col = 1;
		for (s = b; s < p; s++)
		{
		    if (*s == '\n')
		    {
			ln++;
			col = 0;
		    }
		    else
			col++;
		}
	    }
	    if (*p == '>' || *p == '<' || *p == '+' || *p == '-' ||
		*p == '.' || *p == ',' || *p == '[' || *p == ']')
	    {

		printf ("%2d,%3d: %c ptr=%d t[]=%d ", ln, col, *p, m, t[m]);

		if (*p == '.')
		{
		    if (t[m] > ' ' && t[m] <= '~')
			printf ("-->%c<", t[m]);
		    else
			printf ("prt(%d)", t[m]);
		}
	    }
	    col++;
	    switch (*p)
	    {
	    case '>':
		m++;
		break;
	    case '<':
		m--;
		break;
	    case '+':
		t[m]++;
		break;
	    case '-':
		t[m]--;
		break;
	    case ',':
	    {
		int c = getchar ();
		if (c != EOF)
		    t[m] = c;
		break;
	    }
	    case '[':
		if (t[m] == 0)
		    while ((i += (*p == '[') - (*p == ']')) && p[1])
			p++;
		ln = 0;
		break;
	    case ']':
		if (t[m] != 0)
		    while ((i += (*p == ']') - (*p == '[')) && p > b)
			p--;
		ln = 0;
		break;
	    case '\n':
		ln++;
		col = 1;
		break;
	    }

	    if (*p == '>' || *p == '<' || *p == '+' || *p == '-' ||
		*p == '.' || *p == ',' || *p == '[' || *p == ']')
	    {

		if (*p == '>' || *p == '<')
		    printf ("ptr=%d ", m);
		if (*p == '[' || *p == ']')
		    printf ("to=%c", *p);
		else if (*p != '.')
		    printf ("t[]=%d", t[m]);
		printf ("\n");
	    }
	}
    return 0;
}
