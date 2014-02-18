#!/bin/sh
[ -n "$1" ] && cd $1

git_status() {
    VER=`git rev-list origin | wc -l`
    DIFF=`git rev-list origin..HEAD | wc -l`
    MOD=`git status --porc | grep "^ M" | wc -l`

    echo Origin commits: $VER, Local commits: $DIFF, Local edits: $MOD 1>&2

    SUFFIX=
    [ "$DIFF" -ne 0 ] && SUFFIX="$SUFFIX.$DIFF"
    [ "$MOD" -ne 0 -a "$DIFF" -eq 0 ] && SUFFIX="${SUFFIX}.0"
    [ "$MOD" -ne 0 ] && SUFFIX="${SUFFIX}.${MOD}"

    SUFFIX="$SUFFIX `git rev-list HEAD -n 1 | cut -c 1-7`"
}

VER=MINOR_BUILD
SUFFIX=unknown
((type git && git status ) >/dev/null 2>&1) && git_status

echo "#define VERSION_BUILD    $VER"
echo "#define VERSION_SUFFIX   \"$SUFFIX\""
echo "#define BUILD_DATE       \"`date --rfc-3339=seconds`\""
