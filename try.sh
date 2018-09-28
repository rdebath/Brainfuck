#!/bin/bash
if [ ! -n "$BASH_VERSION" ];then exec bash "$0" "$@";else set +o posix;fi

[ -f "$1" ] || { echo >&2 "Usage: $0 BF_Program"; exit 1;}

echo Proper 1=8bit, 2=16bit, 3=32bit, 4=7bit, 5=Hard-byte, 6=Error-byte

P="$(dirname "$0")"
W=$(( ($(tput cols)-2)/3-8))

[ "$W" -ge 18 ] || W=18

for i in "$P"/proper-bf*.c "$P"/repeat-bf*.c "$P"/broken-bf?.c
do  echo "$(basename "$i")" :"$(
    ( ulimit -t 2 ; tcc -w -run "$i" "$1" </dev/null |
    tr -d '\0' | xxd -g0 -l$W -c$W ) 2>&1 )"
done


