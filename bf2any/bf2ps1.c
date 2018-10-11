#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * MS Power shell translation from BF, runs at about 1,200,000 instructions per second.
 */

static int do_input = 0;
static int do_batfile = 0, do_unbuffered = 0;
static int ind = 0;
#define I printf("%*s", ind*4, "")

static void print_string(char*s);

#ifdef FULLPREFIX
static char cmdprefix[] =
	"@@echo off"
"\n"	"@@set POWERSHELL_BAT_ARGS=%*"
"\n"	"@@if defined POWERSHELL_BAT_ARGS set "
	"POWERSHELL_BAT_ARGS=%POWERSHELL_BAT_ARGS:\"=\\\"%"
"\n"	"@@PowerShell -ExecutionPolicy Bypass -Command Invoke-Expression "
	"$('$args=@(^&{$args} %POWERSHELL_BAT_ARGS%);'+"
	"[String]::Join([Environment]::NewLine,$((Get-Content '%~f0') "
	"-notmatch '^^@@^|^^:'))) & goto :EOF"
"\n"	"{" ;

static char cmdsuffix[] = "\n"	"}.Invoke($args)";
#else

static char cmdprefix[] =
    "@PowerShell -ExecutionPolicy Bypass -Command Invoke-Expression "
    "$([String]::Join([Environment]::NewLine,$((Get-Content '%~f0') "
    "-notmatch '^^@Po'))) & goto :EOF";

static char cmdsuffix[] =
"\n"	"# If running in the console, wait for input before closing."
"\n"	"if ($Host.Name -eq \"ConsoleHost\")"
"\n"	"{ "
"\n"	"    Write-Host \"Press any key to continue...\" -nonewline"
"\n"	"    $Host.UI.RawUI.ReadKey(\"NoEcho,IncludeKeyUp\") > $null"
"\n"	"}" ;
#endif

static check_arg_t fn_check_arg;
struct be_interface_s be_interface = {.check_arg=fn_check_arg};

static int
fn_check_arg(const char * arg)
{
    if (strcmp(arg, "-bat") == 0) { do_batfile = 1; return 1; }
    if (strcmp(arg, "-nb") == 0) { do_unbuffered = 1; return 1; }
    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	if (do_batfile)
	    printf("%s\n", cmdprefix);
	I; printf("function brainfuck() {\n");
	ind ++;
	I; printf("$m = @([int]0) * %d\n", tapesz);
	I; printf("$p = %d\n", tapeinit);
	break;

    case '=': I; printf("$m[$p]=%d\n", count); break;
    case 'B':
	if(bytecell) { I; printf("$m[$p]=$m[$p] -band 255;\n"); }
	I; printf("$v=$m[$p]\n");
	break;
    case 'M': I; printf("$m[$p]=$m[$p]+$v*%d\n", count); break;
    case 'N': I; printf("$m[$p]=$m[$p]-$v*%d\n", count); break;
    case 'S': I; printf("$m[$p]=$m[$p]+$v\n"); break;
    case 'T': I; printf("$m[$p]=$m[$p]-$v\n"); break;

    case 'X': I; printf("throw 'Aborting Infinite Loop'\n"); break;

    case '+': I; printf("$m[$p]+=%d;\n", count); break;
    case '-': I; printf("$m[$p]-=%d;\n", count); break;
    case '<': I; printf("$p-=%d;\n", count); break;
    case '>': I; printf("$p+=%d;\n", count); break;
    case '[':
	if(bytecell) { I; printf("$m[$p]=$m[$p] -band 255;\n"); }
	I; printf("while ($m[$p] -ne 0){\n"); ind++; break;
    case ']':
	if(bytecell) { I; printf("$m[$p]=$m[$p] -band 255;\n"); }
	ind--; I; printf("}\n");
	break;
    case 'I':
	if(bytecell) { I; printf("$m[$p]=$m[$p] -band 255;\n"); }
	I; printf("if ($m[$p] -ne 0){\n"); ind++; break;
    case 'E':
	ind--; I; printf("}\n");
	break;
    case '.':
	if (do_unbuffered) {
	    I; printf("Write-Host $([char]$m[$p]) -nonewline\n");
	} else {
	    I; printf("outchar $m[$p]\n");
	}
	break;
    case '"': print_string(get_string()); break;
    case ',':
	I; printf("inchar\n");
	do_input = 1;
	break;
    case '~':
	ind --;
	I; printf("}\n\n");
	if (do_input) {
	    /* Real fuckup here, if I don't prefix every single invocation of
	     * the variables with $script: it makes a local copy of the
	     * global variable. That is the automatically created local
	     * variable is init'd with the global variable's value.
	     * Talk about an obviously stupid idea!!
	     */
	    printf("function inchar() {\n");
	    printf("    if ($script:goteof -eq $true) { return; }\n");
	    printf("    if ($script:gotline -eq $false) {\n");
	    if (!do_unbuffered) {
		printf("        # The prompt string is corrupted by powershell.\n");
		printf("        Write-Host $script:obuf -nonewline\n");
		printf("        $script:obuf = \"\"\n");
	    }
	    printf("        $script:line = Read-Host\n");
	    printf("        $script:gotline = $true\n");
	    printf("        $script:goteof = $false\n");
	    printf("        # Well crap -- no EOF, I'll fake it.\n");
	    printf("        if ( $script:line.Length -eq 1 -And "
		"26 -eq [int]($script:line.Substring(0,1).ToCharArray()[0])"
		") {\n");
	    printf("            $script:goteof = $true\n");
	    printf("            return\n");
	    printf("        }\n");
	    printf("    }\n");
	    printf("    if ($script:line -eq \"\") {\n");
	    printf("        $script:gotline = $false;\n");
	    printf("        $m[$p]=10;\n");
	    printf("        return;\n");
	    printf("    }\n");
	    printf("    $m[$p] = [int]($script:line.Substring(0,1).ToCharArray()[0]);\n");
	    printf("    $script:line = $script:line.Substring(1);\n");
	    printf("}\n");
	    printf("$script:line = \"\"\n");
	    printf("$script:goteof = $false\n");
	    printf("$script:gotline = $false\n");
	    printf("\n");
	}
	if (!do_unbuffered) {
	    printf("function outchar() {\n");
	    printf("  if ($args[0] -ne 10) {\n");
	    printf("    $script:obuf=\"$script:obuf$([char]$args[0])\"\n");
	    printf("    return;\n");
	    printf("  }\n");
	    printf("  # Using Write-Output here mostly works but not well.\n");
	    printf("  Write-Host $script:obuf\n");
	    printf("  $script:obuf=\"\"\n");
	    printf("}\n");
	    printf("\n");
	}
	I; printf("brainfuck\n");

	if (do_batfile)
	    printf("%s\n", cmdsuffix);
	break;
    }
}

static void
print_string(char * str)
{
    char buf[256];
    int gotnl = 0, badchar = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || gotnl || badchar || outlen > sizeof(buf)-8))
	{
	    buf[outlen] = 0;
	    if (do_unbuffered) {
		if (gotnl) {
		    buf[outlen-2] = 0;
		    I; printf("Write-Host \"%s\"\n", buf);
		} else {
		    I; printf("Write-Host \"%s\" -nonewline\n", buf);
		}
	    } else {
		if (gotnl)
		    buf[outlen-=2] = 0;
		if (outlen == 1) {
		    I; printf("outchar %d\n", *buf);
		} else if (outlen>0) {
		    I; printf("$script:obuf=$script:obuf + \"%s\"\n", buf);
		}

		if (gotnl) {
		    I; printf("outchar 10\n");
		}
	    }
	    gotnl = 0; outlen = 0;
	}
	if (badchar) {
	    if (do_unbuffered) {
		I; printf("Write-Host $([char]%d) -nonewline\n", badchar);
	    } else {
		I; printf("outchar %d\n", badchar);
	    }
	    badchar = 0;
	}
	if (!*str) break;

	if (*str == '\n') gotnl = 1;
	if (*str == '"' || *str == '`') {
	    buf[outlen++] = '`'; buf[outlen++] = *str;
	} else if (*str >= ' ' && *str <= '~') {
	    buf[outlen++] = *str;
	} else if (*str == '\n') {
	    buf[outlen++] = '`'; buf[outlen++] = 'n';
	} else if (*str == '\t') {
	    buf[outlen++] = '`'; buf[outlen++] = 't';
	} else {
	    badchar = *(unsigned char *)str;
	}
    }
}
