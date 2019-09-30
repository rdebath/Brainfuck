#!/usr/bin/env python2

import sys

b2c = {
    '>':'++p;',
    '<':'--p;',
    '+':'++*p;',
    '-':'--*p;',
    '.':'putchar(*p);',
    ',':'*p=getchar();',
    '[':'while(*p){',
    ']':'}'
}

def translate(c):
    try:
        return b2c[c]
    except KeyError:
        return ''

print('#include<stdio.h>\n#include<stdlib.h>\n' +
    'int main(){unsigned char* p=calloc(sizeof(unsigned char), 8388608);\n' + 
    '\n'.join(translate(c) for c in open(sys.argv[1]).read()) + '\nreturn 0;}')
