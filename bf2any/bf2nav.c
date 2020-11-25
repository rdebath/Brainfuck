#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "bf2any.h"

/*
 * Navision V5 translation From BF, runs quite slowly.
 */

struct instruction {
    int ch;
    int count;
    int iloop;
    int icount;
    int has_inp;
    struct instruction * next;
    struct instruction * loop;
    char * cstr;
};
static struct instruction *pgm = 0, *last = 0, *jmpstack = 0;

static void loutcmd(int ch, int count, struct instruction *n);

static int do_input = 0;
static int ind = 2;
static int lblcount = 0;
static int icount = 0;
static int noheader = 0;
static const char * boilerplate;

#define pr(s)           printf("%*s" s "\n", ind*2, "")
#define prv(s,v)        printf("%*s" s "\n", ind*2, "", (v))
#define prv2(s,v,v2)    printf("%*s" s "\n", ind*2, "", (v), (v2))

static check_arg_t fn_check_arg;
static gen_code_t gen_code;
struct be_interface_s be_interface = {fn_check_arg, gen_code, .bytesonly=1};

static int
fn_check_arg(const char * arg)
{
    if (strcmp(arg, "-no-hdr") == 0) { noheader = 1; return 1; }
    return 0;
}

static void
gen_code(int ch, int count, char * strn)
{
    struct instruction * n = calloc(1, sizeof*n);
    if (!n) { perror("bf2nav"); exit(42); }

    icount ++;
    n->ch = ch;
    n->count = count;
    n->icount = icount;
    if (!last) {
	pgm = n;
    } else {
	last->next = n;
    }
    last = n;

    if (n->ch == '[' ||  n->ch == 'I') {
	n->iloop = ++lblcount;
	n->loop = jmpstack; jmpstack = n;
    } else if (n->ch == ']' || n->ch == 'E') {
	n->iloop = ++lblcount;
	n->loop = jmpstack; jmpstack = jmpstack->loop; n->loop->loop = n;
    } else if (ch == ',') {
	struct instruction * j = jmpstack;
	n->iloop = ++lblcount;
	do_input ++;
	n->has_inp = 1;

	while(j) {
	    j->has_inp++;
	    j=j->loop;
	}
    } else if (ch == '"')
	n->cstr = strdup(strn);

    if (ch != '~') return;

    for(n=pgm; n; n=n->next) {
	if (n->loop && n->loop->has_inp)
	    n->has_inp = n->loop->has_inp;

#ifdef COMMENTS
	if (n->iloop) {
	    I; printf("// %d", n->iloop);
	    if (n->loop) {
		printf(" --> %d", n->loop->iloop);
	    }
	    if (n->has_inp)
		printf(" Has %d ',' command(s)", n->has_inp);
	    printf("\n");
	}
#endif

	loutcmd(n->ch, n->count, n);
    }

    while(pgm) {
	n = pgm;
	pgm = pgm->next;
	if (n->cstr)
	    free(n->cstr);
	memset(n, '\0', sizeof*n);
	free(n);
    }
}

void
loutcmd(int ch, int count, struct instruction *n)
{
    switch(ch) {
    case '=': prv("m[p] := %d;", count%256); break;
    case 'B': pr("v := m[p];"); break;
    case 'M': prv("m[p] := (m[p] + %d*v) MOD 256;", count%256); break;
    case 'N': prv("m[p] := (m[p] - %d*v) MOD 256;", count%256); break;
    case 'S': pr("m[p] := (m[p] + v) MOD 256;"); break;
    case 'T': pr("m[p] := (m[p] - v) MOD 256;"); break;
    case '*': pr("m[p] := (m[p] * v) MOD 256;"); break;
    case '/':
	pr("IF v < 0 THEN v := v + 256;");
	pr("IF m[p] < 0 THEN m[p] := m[p] + 256;");
	pr("m[p] := ROUND(m[p] / v, 1, '<');");
	break;
    case '%':
	pr("IF v < 0 THEN v := v + 256;");
	pr("IF m[p] < 0 THEN m[p] := m[p] + 256;");
	pr("m[p] := (m[p] MOD v);");
	break;

    case 'C': prv("m[p] := (%d*v) MOD 256;", count%256); break;
    case 'D': prv("m[p] := (%d*-v) MOD 256;", count%256); break;
    case 'V': pr("m[p] := v MOD 256;"); break;
    case 'W': pr("m[p] := -v MOD 256;"); break;

    case '+': prv("m[p] := (m[p] + %d) MOD 256;", count%256); break;
    case '-': prv("m[p] := (m[p] - %d) MOD 256;", count%256); break;
    case '<': prv("p := p - %d;", count); break;
    case '>': prv("p := p + %d;", count); break;
    case '.': pr("Write(m[p]);"); break;
    case '"':
        {
            char * str = n->cstr;
            if (!str) break;;
            for(; *str; str++)
		prv("Write(%d);", *str & 0xFF);
            break;
        }

    case 'X': pr("ERROR('Abort: Infinite Loop');"); break;
    }

    if (n->has_inp == 0) {
	switch(ch) {
	case '[': pr("WHILE m[p] <> 0 DO BEGIN"); ind++; break;
	case ']': ind--; pr("END;"); break;
	case 'I': pr("IF m[p] <> 0 THEN BEGIN"); ind++; break;
	case 'E': ind--; pr("END;"); break;
	}
    } else {
	switch(ch) {
	case ',':
	    pr("moreinp := 1;");
	    prv("j := %d;", n->iloop);
	    ind--;
	    pr("END;");
	    prv("%d: BEGIN", n->iloop);
	    ind++;
	    break;
	case '[': case 'I':
	    pr("IF m[p] = 0 THEN ");
	    ind++; prv("j := %d", n->loop->iloop); ind--;
	    pr("ELSE");
	    ind++; prv("j := %d;", n->iloop); ind--;
	    ind--;
	    pr("END;");
	    prv("%d: BEGIN", n->iloop);
	    ind ++;
	    break;
	case ']':
	    pr("IF m[p] <> 0 THEN ");
	    ind++; prv("j := %d", n->loop->iloop); ind--;
	    pr("ELSE");
	    ind++; prv("j := %d;", n->iloop); ind--;
	    ind--;
	    pr("END;");
	    prv("%d: BEGIN", n->iloop);
	    ind ++;
	    break;
	case 'E':
	    prv("j := %d;", n->iloop);
	    ind--;
	    pr("END;");
	    prv("%d: BEGIN", n->iloop);
	    ind ++;
	    break;
	}
    }

    switch(ch) {
    case '!':

	if (!noheader) printf("%s\n", boilerplate);
        pr("PROCEDURE ResetProg@1000000003();");
        pr("BEGIN");
	pr("  j := 0;");
	prv("  p := %d;", tapeinit+1);
	pr("  InputLine := '';");
	pr("END;\n");

	pr("PROCEDURE NextTask@1000000004();");
	ind++;
	pr("BEGIN");

	pr("CASE j OF");
	ind++;
	pr("0: BEGIN");
	ind++;
	break;

    case '~':
	pr("j := -1;");
	ind--; pr("END;");
	ind--; pr("END;");
	ind--; pr("END;");

	if (!noheader)
	    printf("%s\n",
		"\n"    "    BEGIN"
		"\n"    "    END."
		"\n"    "  }"
		"\n"    "}");
	break;
    }
}

static const char * boilerplate =
	"OBJECT Form 73000 BF"
"\n"	"{"
"\n"	"  OBJECT-PROPERTIES"
"\n"	"  {"
"\n"	"    Date=01/06/14;"
"\n"	"    Time=12:00:00;"
"\n"	"    Version List=;"
"\n"	"  }"
"\n"	"  PROPERTIES"
"\n"	"  {"
"\n"	"    Width=15840;"
"\n"	"    Height=12430;"
"\n"	"    ActiveControlOnOpen=1000000001;"
"\n"	"    InsertAllowed=No;"
"\n"	"    ModifyAllowed=No;"
"\n"	"    TableBoxID=1000000000;"
"\n"	"    SourceTable=Table2000000040;"
"\n"	"    SourceTableTemporary=Yes;"
"\n"	"    OnOpenForm=BEGIN"
"\n"	"                 ResetProg;"
"\n"	"                 RunLoop;"
"\n"	"               END;"
"\n"	""
"\n"	"  }"
"\n"	"  CONTROLS"
"\n"	"  {"
"\n"	"    { 1000000000;ListBox;220  ;220  ;15400;11330;HorzGlue=Both;"
"\n"	"                                                 VertGlue=Both;"
"\n"	"                                                 Editable=No;"
"\n"	"                                                 Border=No }"
"\n"	"    { 1000000003;TextBox;0    ;0    ;4400 ;0    ;HorzGlue=Both;"
"\n"	"                                                 ParentControl=1000000000;"
"\n"	"                                                 InColumn=Yes;"
"\n"	"                                                 BackColor=13160660;"
"\n"	"                                                 FontName=Courier New;"
"\n"	"                                                 CaptionML=[ENU=Line;"
"\n"	"                                                            ENG=Line];"
"\n"	"                                                 SourceExpr=Text }"
"\n"	"    { 1000000004;Label  ;0    ;0    ;0    ;0    ;ParentControl=1000000003;"
"\n"	"                                                 InColumnHeading=Yes }"
"\n"	"    { 1000000001;TextBox;220  ;11770;15400;440  ;Name=InpFld;"
"\n"	"                                                 HorzGlue=Both;"
"\n"	"                                                 VertGlue=Bottom;"
"\n"	"                                                 SourceExpr=InputLine;"
"\n"	"                                                 OnAfterValidate=BEGIN"
"\n"	"                                                                   RunLoop;"
"\n"	"                                                                 END;"
"\n"	"                                                                  }"
"\n"	"  }"
"\n"	"  CODE"
"\n"	"  {"
"\n"	"    VAR"
"\n"	"      NextLine@1000000000 : Integer;"
"\n"	"      UpdateLineNo@1000000008 : Integer;"
"\n"	"      TextLine@1000000001 : Text[1024];"
"\n"	"      InputLine@1000000007 : Text[1024];"
"\n"	"      j@1000000002 : Integer;"
"\n"	"      p@1000000003 : Integer;"
"\n"	"      m@1000000004 : ARRAY [60000] OF Integer;"    /* TODO: tapesz */
"\n"	"      v@1000000005 : Integer;"
"\n"	"      moreinp@1000000006 : Integer;"
"\n"	""
"\n"	"    PROCEDURE Write@1000000000(WChar@1000000000 : Integer);"
"\n"	"    VAR"
"\n"	"      C@1000000001 : Char;"
"\n"	"    BEGIN"
"\n"	"      WChar := (WChar + 256) MOD 256;"
"\n"	"      IF WChar = 10 THEN"
"\n"	"        SaveTextLine"
"\n"	"      ELSE BEGIN"
"\n"	"        IF (STRLEN(TextLine) = MAXSTRLEN(Text)) OR"
"\n"	"           (STRLEN(TextLine) = 80)"
"\n"	"          THEN SaveTextLine;"
"\n"	"        C := WChar;"
"\n"	"        TextLine := TextLine + FORMAT(C);"
"\n"	"      END;"
"\n"	"    END;"
"\n"	""
"\n"	"    PROCEDURE SaveTextLine@1000000001();"
"\n"	"    BEGIN"
"\n"	"      NextLine := NextLine + 1;"
"\n"	"      \"Line No.\" := NextLine;"
"\n"	"      Text := TextLine;"
"\n"	"      TextLine := '';"
"\n"	"      INSERT;"
"\n"	""
"\n"	"      CurrForm.UPDATE(FALSE);"
"\n"	"      YIELD;"
"\n"	"    END;"
"\n"	""
"\n"	"    PROCEDURE RunLoop@1000000002();"
"\n"	"    VAR"
"\n"	"      C@1000000001 : Char;"
"\n"	"    BEGIN"
"\n"	""
"\n"	"      IF UpdateLineNo <> 0 THEN BEGIN"
"\n"	"        Text := COPYSTR(Text + InputLine, 1, MAXSTRLEN(Text));"
"\n"	"        MODIFY;"
"\n"	"        UpdateLineNo := 0;"
"\n"	"      END;"
"\n"	""
"\n"	"      IF moreinp <> 0 THEN BEGIN"
"\n"	"        C := 10;"
"\n"	"        InputLine := InputLine + FORMAT(C);"
"\n"	"      END;"
"\n"	""
"\n"	"      REPEAT"
"\n"	"        IF moreinp <> 0 THEN BEGIN"
"\n"	"          IF InputLine = '' THEN BEGIN"
"\n"	"            IF TextLine <> '' THEN BEGIN"
"\n"	"              SaveTextLine;"
"\n"	"              UpdateLineNo := NextLine;"
"\n"	"            END;"
"\n"	"            EXIT;"
"\n"	"          END;"
"\n"	"          m[p] := InputLine[1];"
"\n"	"          InputLine := COPYSTR(InputLine, 2);"
"\n"	"          moreinp := 0;"
"\n"	"        END;"
"\n"	""
"\n"	"        NextTask;"
"\n"	"      UNTIL j <= 0;"
"\n"	""
"\n"	"      IF j = -1 THEN CurrForm.InpFld.VISIBLE := FALSE;"
"\n"	"    END;"
"\n"	"";
