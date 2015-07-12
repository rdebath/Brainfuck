#!/bin/sh -
 # Convert BF to a bash script and run it.
 #
 # This does NOT call ANY external function or programs.
 # This does not even fork any subshells.
 #
 # Bash translation from BF, runs at about 150,000 instructions per second.
 # (Runs awib-0.3 on awib-0.3 in less than 2 hours on a 3GHz i7.)
 # It does no real optimisation, but does RLE on + - < >.
 #
 # If the "&255" filter is removed bash has 64bit cells.
 #
 # From the bash(1) manual ...
 # BUGS
 #      It's too big and too slow.
 #

# The *BSD don't have package management so anything not in the base install
# is dumped into /usr/local. This makes hardcoded paths ineffective.
if [ ! -n "$BASH_VERSION" -a ! -n "$KSH_VERSION" ];then exec bash "$0" "$@" ;fi

set -f +B
export LC_COLLATE=C LC_CTYPE=C

runbf() {
c=0
i=''
cmd=""
xc=0

while read line
do  ll=${#line}
    j=0

    while (( j < ll ))
    do  ni="${line:j:1}"
	((j+=1))
	case "$ni" in
	">") m=1 ;;
	"<") ni=">"; m=-1 ;;
	"+") m=1 ;;
	"-") ni="+"; m=-1 ;;
	","|"."|"["|"]") m=0 ;;
	* ) continue;
	esac

	[[ "$i" == "$ni" ]] && {
	    ((c+=m))
	    continue;
	}
	[[ "$i" != "" ]] && addcmd $i $c

	if [[ "$m" -eq 0 ]]
	then
	    addcmd $ni 1
	    i=''
	else
	    c=$m; i="$ni"
	fi
    done

    [[ "$((xc+=1))" -ge 1024 ]] &&
	if ((xc % 256 == 0))
	then echo -n -e " Loaded $xc lines\r" 1>&2
	fi

done < "$1"

addcmd $i $c
IFS='
     ' pgm="${lines[*]}"

[[ "$xc" -ge 1024 ]] &&
    echo 'Running program ...                    ' 1>&2

P=0
eval "$pgm"
}

addcmd() {
    cnt="$2"
    if ((cnt == 0)) ;then return; fi

    case "$1" in
    ">" ) cmd='((P=P+'$cnt'))' ;;
    "+" ) cmd='((M[P]=255&(M[P]+'$cnt')))' ;;
    "[" ) cmd='while ((M[P] != 0)) ; do :' ;;
    "]" ) cmd='done' ;;
    "," ) cmd='getch' ;;
    "." ) cmd='o' ;;
    esac
    [ "$cmd" != "" ] && {
	lc=$((lc+1))
	lines[$lc]="$cmd" ; cmd=""
    }
}

o(){
    printf -v C '\%04o' $((M[P])) 2>/dev/null || {
	# Bash does this variant rather slowly.
	o(){ echo -n -e "`printf '\\\\%04o' $((M[P]))`" ; }
	o;return
    }
    echo -n -e "$C"
}

getch() {
    [ "$goteof" == "y" ] && return;
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
    line="${line:1}"

    M[P]=`printf %d \'"$ch"`
}

runbf "$@"

exit 0
