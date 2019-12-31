#!/bin/bash
if [ ! -n "$BASH_VERSION" ];then exec bash "$0" "$@";else set +o posix;fi

[ -f "$1" ] || { echo >&2 "Usage: $0 BF_Program"; exit 1;}

echo 'Proper: (">" tape) 1=8bit, 2=16bit, 3=32bit, 4=7bit'
echo 'Weird: 1=DoubleTape 8bit, 2=8bit Saturate, 3=8bit Error overflow, 4=NoWrapping'

P="$(dirname "$0")"
W=$(( ($(tput cols)-2)/3-8))
TMP=/tmp/_tf$$

[ "$W" -ge 18 ] || W=18

for i in "$P"/proper-bf*.c "$P"/weirds-bf*.c "$P"/broken-bf?.c
do  echo "$(basename "$i")" :"$(
    ( ulimit -t 2 ; gcc -w -O2 -o "$TMP" "$i" && {
	:| "$TMP" "$1" | tr -d '\0' | xxd -g0 -l$W -c$W
    } ) 2>&1 | head -1
    rm -f "$TMP" )"
done

