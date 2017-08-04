#!/bin/sh -
 # Convert BF to a shell script and run it.
 #
 # Bash translation from BF, runs at about 150,000 instructions per second.
 # (Runs awib-0.3 on awib-0.3 in less than 2 hours on a 3GHz i7.)
 # It does no real optimisation, but does RLE on + - < >.
 #
 # It runs quite a bit faster with ksh93
 #
 # If the "&255" filter is removed bash has 64bit cells.
 #
 # From the bash(1) manual ...
 # BUGS
 #      It's too big and too slow.
 #

# Nothing failed yet.
FAILED=

# All this initial code must run cleanly on a ported UNIX v7 shell.
# But, I'm not leaving out the '#' comments.
#;FAILED=completely

# A couple of initial tweaks to fix ill advised defaults.
# Initial tweaks on a not completely broken shell.
( : ${var:=value} ) 2>/dev/null && {
    (eval 'set -o sh && set +o sh') >/dev/null 2>&1 && set +o sh
    (eval 'set -o posix && set +o posix') >/dev/null 2>&1 && set +o posix
}

# Zsh is weird.
if test -n "${ZSH_VERSION+set}" && (emulate ksh) >/dev/null 2>&1; then
    emulate ksh
fi

# Now test this shell, run the test code in a 'eval' in a subshell to
# stop any complaints going to the terminal. If the exit code is ok
# it worked, if not we need to make other arrangments.

# Test that this shell has substrings.
(eval 'line=nnynnn; j=2; [[ "${line:$j:1}" == y ]]') 2>/dev/null ||
    FAILED=substring

# Test arrays work in the correct way.
(eval 'typeset -i M P=0 && M[1]=3 && ((M[500]+=1)) && ((M[P+2]=1)) &&
   ((M[1]+=1)) && [[ ${M[1]} -eq 4 && ${M[500]} -eq 1 ]]') 2>/dev/null || {

    # Try without the typeset
    (eval 'P=0 && M[1]=3 && ((M[500]+=1)) && ((M[1]+=1)) && ((M[P+2]=1)) &&
       [[ ${M[1]} -eq 4 && ${M[500]} -eq 1 ]]') 2>/dev/null || {
	[ "$FAILED" != "" ] && FAILED="$FAILED and "
	FAILED="${FAILED}array"
    }
    NOTYPESET=1
}

[ "$FAILED" != "" ] && {
    # The *BSD don't have package management so anything not in the base install
    # is dumped into /usr/local. This makes hardcoded paths ineffective.
    #
    # I may be trying some VERY broken shells here, so I'm calling it
    # in a subshell and making sure I get a very positive ACK that the
    # shell is okay. Not just a good exit status.
    [ ".$1" = ".-tryme" ] || {
	# Ksh93 is all good
	if [ ! -n "${KSH_VERSION+x}" ];then
	    OKAY="`(ksh93 \"$0\" -tryme ) 2>&1`"
	    [ "$OKAY" = OK ] && exec ksh93 "$0" "$@"
	fi
	# Other versions of ksh usually work fine (but not 88)
	if [ ! -n "${KSH_VERSION+x}" ];then
	    OKAY="`(ksh \"$0\" -tryme ) 2>&1`"
	    [ "$OKAY" = OK ] && exec ksh "$0" "$@"
	fi
	# Bash is slow, but effective.
	if [ ! -n "${BASH_VERSION+x}" ];then
	    OKAY="`(bash \"$0\" -tryme ) 2>&1`"
	    [ "$OKAY" = OK ] && exec bash "$0" "$@"
	fi
	# Zsh is very slow and weird, but should work.
	if [ ! -n "${ZSH_VERSION+x}" ];then
	    OKAY="`(zsh \"$0\" -tryme ) 2>&1`"
	    [ "$OKAY" = OK ] && exec zsh "$0" "$@"
	fi
    }
    echo "This shell's $FAILED support doesn't work correctly" 1>&2
    exit 1
}

################################################################################
# We have found a good shell, now actually do the job we're here for.

[ ".$1" = ".-tryme" ] && {
    echo OK
    exit 0
}

################################################################################

export LC_ALL=C
(set -f ) 2>/dev/null && set -f
(set +B 2>/dev/null ) 2>/dev/null && set +B

if [ "$NOTYPESET" = "" ]
then alias int='typeset -i'
     # Bash turns aliases off
     (shopt -q -s expand_aliases) 2>/dev/null && shopt -q -s expand_aliases
else alias int=''
fi

################################################################################

runbf() {
int c=0
int xc=0
int lflg=0
i=''
cmd=""
pc=
int mask=255

case "$1" in
-b* )
    mask="${1:2}"
    ((mask <= 0)) && mask=8
    mask=$((2**mask-1))
    shift ;;
"" )
    echo "Usage: $0 bfprogram.b" >&2
    exit 1
    ;;
esac

if ((mask <= 0)) ; then maskop=''; mask=-1; else maskop="${mask}&"; fi

while read line
do  ll=${#line}
    j=0

    while (( j < ll ))
    do  ni="${line:$j:1}"
	((j+=1))
	case "$ni" in
	">") m=1 ;;
	"<") ni=">"; m=-1 ;;
	"+") m=1 ;;
	"-") ni="+"; m=-1 ;;
	","|"."|"["|"]") m=0 ;;
	* ) continue;
	esac

	# Check for a "[-]" sequence.
	if [[ "$lflg" == 0 && "$ni" == "[" ]] ; then
	    [[ "$i" != "" ]] && addcmd $i $c
	    i=''; c=0
	    lflg=1
	    continue
	elif [[ "$lflg" == 1 ]] ; then
	    if [[ "$ni" == "+" && "$m" == -1 ]] ; then
		lflg=2; continue
	    fi
	    addcmd "[" 1
	    lflg=0
	elif [[ "$lflg" == 2 ]] ; then
	    lflg=0
	    if [[ "$ni" == "]" ]] ; then
		addcmd "=" 1
		continue;
	    fi
	    addcmd "[" 1
	    i='+'; c=-1;
	fi

	# Otherwise just RLE
	[[ "$i" == "$ni" ]] && {
	    ((c+=m))
	    continue;
	}
	[[ "$i" != "" ]] && addcmd $i $c

	if ((m==0))
	then
	    [ "$pc" = "[" -a "$ni" = "]" ] && addcmd "X" 1
	    addcmd $ni 1
	    i=''
	else
	    c=$m; i="$ni"
	fi
    done

    if (( (xc+=1) >= 1024 && xc % 256 == 0))
    then echo -n -e " Loaded $xc lines\r" 1>&2
    fi

done < "$1"

addcmd $i $c
IFS='
 ' pgm="${lines[*]}"

((xc>=1024)) && echo 'Running program ...                    ' 1>&2

[ "$NOTYPESET" = "" ] && typeset -i M P
P=0

eval "$pgm"
}

addcmd() {
    cnt="$2"
    pc="$1"
    if ((cnt == 0)) ;then return; fi

    case "$1" in
    ">" ) cmd='((P=P+'$cnt'))' ;;
    "+" ) cmd='((M[P]='"$maskop"'(M[P]+'$((mask & cnt))')))' ;;
    "=" ) cmd='((M[P]=0))' ;;
    "[" ) cmd='while ((M[P])) ; do' ;;
    "]" ) cmd='done' ;;
    "," ) cmd='getch' ;;
    "." ) cmd='o' ;;
    "X" ) cmd='echo Infinite loop 1>&2 ; exit 1' ;;
    esac
    [ "$cmd" != "" ] && {
	((lc=lc+1))
	lines[$lc]="$cmd" ; cmd=""
    }
}

# o() { printf "\x$(printf %x $((M[P])) )" ; }
# o() { echo -n -e "`printf '\\\\%04o' $((M[P]))`" ; }

# Bash does the other variant rather slowly.
o(){
    C=XX
    printf -v C '\%04o' $((M[P])) >/dev/null 2>&1 ||:
    [[ "$C" = "XX" ]] && {
	o() { printf "\x$(printf %x $((M[P])) )" ; }
	o;return
    }
    echo -n -e "$C"
}

getch() {
    [ "$goteof" = "y" ] && return;
    [ "$gotline" != "y" ] && {
	if read -r line
	then
	    gotline=y
	else
	    goteof=y
	    return
	fi
    }
    [ "$line" = "" ] && {
	gotline=n
	((M[P]=10))
	return
    }
    ch="${line:0:1}"
    line="${line#?}"

    M[P]=`printf %d "'$ch"`
}

runbf "$@"

exit 0
