#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Clojure translation from BF, runs at about 140,000 instructions per second.
 */

int do_input = 0;
int ind = 0;
#define I printf("%*s", ind*4, "")

int
check_arg(char * arg)
{
    return 0;
}

void
outcmd(int ch, int count)
{
    if (count == 1) switch(ch) {
    case '+': I; printf("(inc-memory)\n"); return;
    case '-': I; printf("(dec-memory)\n"); return;
    case '>': I; printf("(inc-pointer)\n"); return;
    case '<': I; printf("(dec-pointer)\n"); return;
    }

    switch(ch) {
    case '!':
	printf("%s",
	    "\n"    ";; converted to Clojure, template from:"
	    "\n"    ";; https://github.com/takano32/brainfuck/blob/master/bf2clj.clj"
	    "\n"    ""
	    "\n"    "(def pointer 0)"
	    "\n"    "(defn memoryname [] (str \"m\" pointer))"
	    "\n"    "(defn get-value-from-memory [] (eval (symbol (memoryname))))"
	    "\n"    "(defn set-memory? [] (boolean (resolve (symbol (memoryname)))))"
	    "\n"    "(defn set-or-zero [] (if (set-memory?) (get-value-from-memory) 0))"
	    "\n"    "(defn set-pointer [set-value] (intern *ns* (symbol (memoryname)) set-value))"
	    "\n"    "(defn inc-memory [] (set-pointer (+ (set-or-zero) 1)))"
	    "\n"    "(defn dec-memory [] (set-pointer (- (set-or-zero) 1)))"
	    "\n"    "(defn inc-pointer [] (def pointer (+ pointer 1)))"
	    "\n"    "(defn dec-pointer [] (def pointer (- pointer 1)))"
	);
	break;

    case '+': I; printf("(set-pointer (+ (set-or-zero) %d))\n", count); break;
    case '-': I; printf("(set-pointer (- (set-or-zero) %d))\n", count); break;
    case '>': I; printf("(def pointer (+ pointer %d))\n", count); break;
    case '<': I; printf("(def pointer (- pointer %d))\n", count); break;
    case '[':
	if(bytecell) { I; printf("(set-pointer (mod (set-or-zero) 256))\n"); }
	I; printf("((fn [] "
		    "(loop [] "
			"(if (> (set-or-zero) 0) (do\n");
	ind++;
	break;
    case ']':
	if(bytecell) { I; printf("(set-pointer (mod (set-or-zero) 256))\n"); }
	ind--; I; printf("(recur)) nil))))\n");
	break;
    case '.':
	if(bytecell) { I; printf("(set-pointer (mod (set-or-zero) 256))\n"); }
	I; printf("(print (char (set-or-zero))) (flush)\n");
	break;
    case ',': I; printf("(let [ch (int (. System/in read))] (if-not (= ch -1) (do (set-pointer ch))))\n"); break;
    }
}
