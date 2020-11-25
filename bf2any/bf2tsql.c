#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * MS T-SQL translation from BF, runs at about 48,000 instructions per second.
 */

static int ind = 0;
#define I printf("%*s", ind*4, "")

static gen_code_t gen_code;
struct be_interface_s be_interface = {.gen_code=gen_code};

static void
gen_code(int ch, int count, char * strn)
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
	printf("DECLARE @ccell BigInt;\n");
	printf("DECLARE @vcell BigInt;\n");
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
	else { I; printf("set @ccell = @ccell & 0xFFFFFFFF\n"); }
	I; printf("set @vcell = @ccell\n");
	break;
    case 'M': I; printf("set @ccell = @ccell+@vcell*%d\n", count); break;
    case 'N': I; printf("set @ccell = @ccell-@vcell*%d\n", count); break;
    case 'S': I; printf("set @ccell = @ccell+@vcell\n"); break;
    case 'T': I; printf("set @ccell = @ccell-@vcell\n"); break;
    case '*':
	if(bytecell) { I; printf("set @vcell = @vcell & 255\n"); }
	else {
	    I; printf("set @vcell = @vcell & 0xFFFFFFFF\n");
	    I; printf("IF @vcell > 0x7FFFFFFF set @vcell = @vcell - 0x100000000\n");
	}
	I; printf("set @ccell = @ccell*@vcell\n");
	break;
    case '/':
	if(bytecell) { I; printf("set @ccell = @ccell & 255\n"); }
	else { I; printf("set @ccell = @ccell & 0xFFFFFFFF\n"); }
	if(bytecell) { I; printf("set @vcell = @vcell & 255\n"); }
	else { I; printf("set @vcell = @vcell & 0xFFFFFFFF\n"); }
	I; printf("set @ccell = @ccell/@vcell\n");
	break;
    case '%':
	if(bytecell) { I; printf("set @ccell = @ccell & 255\n"); }
	else { I; printf("set @ccell = @ccell & 0xFFFFFFFF\n"); }
	if(bytecell) { I; printf("set @vcell = @vcell & 255\n"); }
	else { I; printf("set @vcell = @vcell & 0xFFFFFFFF\n"); }
	I; printf("set @ccell = @ccell%%@vcell\n");
	break;

    case 'C': I; printf("set @ccell = @vcell*%d\n", count); break;
    case 'D': I; printf("set @ccell = -@vcell*%d\n", count); break;
    case 'V': I; printf("set @ccell = @vcell\n"); break;
    case 'W': I; printf("set @ccell = -@vcell\n"); break;

    case 'X': I; printf("RAISERROR ('Aborting Infinite Loop.', 16, 1) RETURN\n");

    case '+': I; printf("set @ccell = @ccell + %d\n", count); break;
    case '-': I; printf("set @ccell = @ccell - %d\n", count); break;
    case '<':
	if(bytecell) { I; printf("set @ccell = @ccell & 255\n"); }
	else {
	    I; printf("set @ccell = @ccell & 0xFFFFFFFF\n");
	    I; printf("IF @ccell > 0x7FFFFFFF set @ccell = @ccell - 0x100000000\n");
	}

	I; printf("update @Tape set tcell = @ccell where pcell = @pcell\n");

	I; printf("set @pcell = @pcell - %d\n", count);

	I; printf("select @ccell = tcell from @Tape where pcell = @pcell\n");
	I; printf("IF @@rowcount = 0 BEGIN\n");
	I; printf("  SET @ccell = 0;\n");
	I; printf("  insert into @Tape VALUES(@pcell, @ccell);\n");
	I; printf("END\n");
	break;
    case '>':
	if(bytecell) { I; printf("set @ccell = @ccell & 255\n"); }
	else {
	    I; printf("set @ccell = @ccell & 0xFFFFFFFF\n");
	    I; printf("IF @ccell > 0x7FFFFFFF set @ccell = @ccell - 0x100000000\n");
	}

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
    case 'I':
	if(bytecell) { I; printf("set @ccell = @ccell & 255\n"); }
	I; printf("IF @ccell != 0 BEGIN\n");
	ind++;
	break;
    case 'E':
	ind--; I; printf("END\n");
	break;
    case '.':
	if(bytecell) { I; printf("set @ccell = @ccell & 255\n"); }
	I; printf("EXEC #pch @ccell,@linebuf OUTPUT\n");
	break;
    case '"':
        {
            char * str = strn;
            if (!str) break;
            for(; *str; str++) {
		I; printf("EXEC #pch %d,@linebuf OUTPUT\n", *str & 0xFF);
            }
            break;
        }
    case ',':
	I; printf("RAISERROR ('Input command not implemented', 16, 1) RETURN\n");
	break;
    }
}
