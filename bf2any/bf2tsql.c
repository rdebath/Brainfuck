#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * MS T-SQL translation from BF, runs at about 48,000 instructions per second.
 */

int ind = 0;
#define I printf("%*s", ind*4, "")

static check_arg_t fn_check_arg;
struct be_interface_s be_interface = {fn_check_arg};

static int
fn_check_arg(const char * arg)
{
    if (strcmp(arg, "-M") == 0) {
	tapelen = 0;
	return 1;
    }
    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':

	printf("IF OBJECT_ID ( 'tempdb..#pch', 'P' ) IS NOT NULL\n");
	printf("DROP PROCEDURE #pch;\n");
	printf("GO\n");
	printf("CREATE PROCEDURE #pch\n");
	printf("@ccell Integer,\n");
	printf("@linebuf varchar(8000) OUT\n");
	printf("AS\n");
	printf("SET NOCOUNT ON;\n");
	printf("IF @ccell = 10\n");
	printf("BEGIN\n");
	printf("  RAISERROR (@linebuf, 0, 1) WITH NOWAIT\n");
	printf("  set @linebuf = ''\n");
	printf("END ELSE BEGIN\n");
	printf("  set @linebuf = @linebuf + char(@ccell)\n");
	printf("END\n");
	printf("GO\n");

	printf("IF OBJECT_ID ( 'tempdb..#bf', 'P' ) IS NOT NULL\n");
	printf("DROP PROCEDURE #bf;\n");
	printf("GO\n");
	printf("CREATE PROCEDURE #bf\n");
	printf("AS\n");

	printf("SET NOCOUNT ON\n");
	printf("DECLARE @Tape TABLE(pcell Integer, tcell Integer);\n");
	printf("DECLARE @pcell Integer;\n");
	printf("DECLARE @ccell Integer;\n");
	printf("DECLARE @vcell Integer;\n");
	printf("DECLARE @linebuf varchar(8000);\n");
	printf("set @linebuf = ''\n");
	printf("set @ccell = 0\n");
	printf("set @pcell = 0\n");
	printf("insert into @Tape VALUES(@pcell, @ccell);\n");
	break;

    case '~':
	printf("IF @linebuf <> ''\n");
	printf("  RAISERROR (@linebuf, 0, 1) WITH NOWAIT\n");
	printf("GO\n");
	printf("EXEC #bf\n");
	break;

    case '=': I; printf("set @ccell = %d\n", count); break;
    case 'B':
	if(bytecell) { I; printf("set @ccell = @ccell & 255\n"); }
	I; printf("set @vcell = @ccell\n");
	break;
    case 'M': I; printf("set @ccell = @ccell+@vcell*%d\n", count); break;
    case 'N': I; printf("set @ccell = @ccell-@vcell*%d\n", count); break;
    case 'S': I; printf("set @ccell = @ccell+@vcell\n"); break;
    case 'Q': I; printf("IF @vcell <> 0 set @ccell = %d\n", count); break;
    case 'm': I; printf("set @ccell = @ccell+@vcell*%d\n", count); break;
    case 'n': I; printf("set @ccell = @ccell-@vcell*%d\n", count); break;
    case 's': I; printf("set @ccell = @ccell+@vcell\n"); break;

    case 'X': I; printf("RAISERROR ('Aborting Infinite Loop.', 16, 1) RETURN\n");

    case '+': I; printf("set @ccell = @ccell + %d\n", count); break;
    case '-': I; printf("set @ccell = @ccell - %d\n", count); break;
    case '<':
	I; printf("update @Tape set tcell = @ccell where pcell = @pcell\n");
	I; printf("set @pcell = @pcell - %d\n", count);
	I; printf("select @ccell = tcell from @Tape where pcell = @pcell\n");
	I; printf("IF @@rowcount = 0 BEGIN\n");
	I; printf("  SET @ccell = 0;\n");
	I; printf("  insert into @Tape VALUES(@pcell, @ccell);\n");
	I; printf("END\n");
	break;
    case '>':
	I; printf("update @Tape set tcell = @ccell where pcell = @pcell\n");
	I; printf("set @pcell = @pcell + %d\n", count);
	I; printf("select @ccell = tcell from @Tape where pcell = @pcell\n");
	I; printf("IF @@rowcount = 0 BEGIN\n");
	I; printf("  SET @ccell = 0;\n");
	I; printf("  insert into @Tape VALUES(@pcell, @ccell);\n");
	I; printf("END\n");
	break;
    case '[':
	if(bytecell) { I; printf("set @ccell = @ccell & 255\n"); }
	I; printf("WHILE @ccell != 0 BEGIN\n");
	ind++;
	break;
    case ']':
	if(bytecell) { I; printf("set @ccell = @ccell & 255\n"); }
	ind--; I; printf("END\n");
	break;
    case '.':
	I;
	if(bytecell) { I; printf("set @ccell = @ccell & 255\n"); }
	printf("EXEC #pch @ccell,@linebuf OUTPUT\n");
	break;
    case ',':
	I; printf("RAISERROR ('Input command not implemented', 16, 1) RETURN\n");
	break;
    }
}
