/* These functions will never be used on a modern system, they only
 * exist for compatibility with "old crap".
 */

#define DISABLE_OPENMEMSTREAM
#warning open_memstream not used; _POSIX_VERSION is not set high enough.
#warning Beware: the replacement routine uses mktemp().

FILE *
open_tempfile(void)
{
static char tempf[] = "/tmp/badtemp.XXXXXX";
    FILE * ofd;

    if ( !*mktemp(tempf) ) return 0;

    if ( (ofd = fopen(tempf, "w+")) == 0) return 0;

    unlink(tempf);
    return ofd;
}

void
reload_tempfile(FILE * ofd, char **ptr, size_t * sizeloc)
{
    char *membuf = 0;
    int ch;
    size_t maxp=0, p=0;

    rewind(ofd);

    while ((ch = getc(ofd)) != EOF) {
	while (p + 2 > maxp) {
	    membuf = realloc(membuf, maxp += BUFSIZ);
	    if (!membuf) {
		perror("realloc");
		exit(1);
	    }
	}
	membuf[p++] = ch;
    }
    membuf[p] = 0;

    fclose(ofd);
    *ptr = membuf;
    *sizeloc = p;
}
