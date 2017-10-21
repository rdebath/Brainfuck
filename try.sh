#!/bin/bash
if [ ! -n "$BASH_VERSION" ];then exec bash "$0" "$@";else set +o posix;fi

[ -f "$1" ] || { echo >&2 "Usage: $0 BF_Program"; exit 1;}

P="$(dirname "$0")"
W=$(( ($(tput cols)-2)/3-8))

[ "$W" -ge 18 ] || W=18

echo "Correct      :$(
ruby <<\! - "$1" |
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(
	/./,
	'>' => 'p+=1;',
	'<' => 'p-=1;',
	'+' => 'm[p]+=1;',
	'-' => 'm[p]-=1;',
	'[' => '(',
	']' => ')while((m[p]&=255)!=0);',
	'.' => 'putc m[p];',
	',' => 'm[p]=STDIN.getbyte if !STDIN.eof;')
!
xxd -g0 -l$W -c$W
)"

for i in "$P"/broken-bf?.c
do  echo "$(basename "$i")" :"$(
    ( ulimit -t 2 ; tcc -run "$i" "$1" </dev/null |
    xxd -g0 -l$W -c$W ) 2>&1 )"
done


