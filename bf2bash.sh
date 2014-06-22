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
 # If the "&=255" filter (on '[') is removed bash has 64bit cells.
 #
 # From the bash(1) manual ...
 # BUGS
 #      It's too big and too slow.
 #

# The *BSD don't have package management so anything not in the base install
# is dumped into /usr/local. This makes hardcoded paths ineffective.
if [ ! -n "$BASH_VERSION" ];then exec bash "$0" "$@" ;else set +o posix;fi

set -f +B

runbf() {
c=0
i=''
cmd=""
xc=0

while read -r -n 16 line
do
    while [ "$line" != "" ]
    do	case "${line:0:1}" in
	">") ni=r ; m=1 ;;
	"<") ni=l ; m=1 ;;
	"+") ni=u ; m=1 ;;
	"-") ni=d ; m=1 ;;
	",") ni=i ; m=0 ;;
	".") ni=o ; m=0 ;;
	"[") ni=b ; m=0 ;;
	"]") ni=e ; m=0 ;;
	* ) ni="";
	esac
	line="${line:1}"

	[ "$i" == "$ni" ] && {
	    : $((c+=1))
	    continue;
	}
	[ "$c" -gt 0 -a "$i" != "" ] && {
	    addcmd $i $c
	}
	c=1; i=$ni
	[ "$m" -eq 0 ] && {
	    addcmd $i 1
	    i=''
	}
    done

    [ "$((xc+=16))" -ge 30000 ] &&
	if [ "$((xc % 4096))" -eq 0 ]
	then echo -n -e " Loaded $xc\r" 1>&2
	fi
done < "$1"

addcmd $i $c
IFS='
     ' pgm="${lines[*]}"

[ "$xc" -ge 30000 ] &&
    echo 'Running program ...                    ' 1>&2

set -f +B
eval "$pgm"
}

addcmd() {
    cnt="$2"
    [ "$((cnt+=0))" -le 0 ] && return

    if [ "$cnt" -gt 1 ] 
    then
	case "$1" in
	r ) cmd='((P+='$cnt'))' ;;
	l ) cmd='((P-='$cnt'))' ;;
	u ) cmd='((M[P]+='$cnt'))' ;;
	d ) cmd='((M[P]-='$cnt'))' ;;
	esac
    else
	case "$1" in
	r ) cmd='((P++))' ;;
	l ) cmd='((P--))' ;;
	u ) cmd='((M[P]++))' ;;
	d ) cmd='((M[P]--))' ;;
	b ) cmd='while [[ $((M[P]&=255)) != 0 ]] ; do :' ;;
	e ) cmd='done' ;;
	i ) cmd='getch' ;;
	o ) cmd='o' ;;
	esac
    fi
    [ "$cmd" != "" ] && {
	lc=$((lc+1))
	lines[$lc]="$cmd" ; cmd=""
    }
}

o(){
    printf -v C '\%04o' $((M[P]&=255))
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

    printf -v C %d \'"$ch"
    ((M[P]=C))
}

runbf "$@"

exit 0
