#!/bin/sh
# We only need a POSIX shell, but if bash is available it's more likely to be good.
if [ ! -n "$BASH_VERSION" ];then if [ "`which bash`" != "" ];then exec bash "$0" "$@"; fi ; fi

MAJOR=1
MINOR=1
SUFFIX="# Unknown @ `date -R 2>/dev/null || date`"
BUILD=

# This is expanded by git archive
GITARCHVERSION=$Format:%nGITHASH=%h%nGITTIME="%cd"%nGITDECO="%d"$

[ "$GITTIME" != "" ] && SUFFIX="$GITTIME"
[ "$GITDECO" != "" ] && {
    GITDECO=`echo "$GITDECO" |
    sed \
	-e 's/[ (),][ (),]*/ /g' \
	-e 's/HEAD//' \
	-e 's/master//' \
	-e 's/  *$//' \
	-e 's/^  *//'
	`
}

# Override from the Makefile.
[ ".$2" != "." -a "$3" != "." ] && {
    [ "$GITHASH" = "" ] &&
	GITHASH="`git log -1 --format=%h 2>/dev/null`"
    GITTIME="$GITHASH"
    GITDECO="$2"
    GITHASH="$3"
}

git_describe() {
    # Get more detail than just git describe --tags
    # tag + commits to branch from upstream + local commits + local mods.

    # Find the merge base between this and the upstream.
    ORIGIN=`git merge-base @{u} HEAD`

    # Find the most recent tag.
    TAG=`git describe --tags --match='v[0-9]*.[0-9]*' --abbrev=0 $ORIGIN`
    MAJOR=`echo $TAG | sed "s/^v\([0-9]*\).*/\1/"`
    MINOR=`echo $TAG | sed "s/^v[0-9]*\.\([0-9]*\)/\1/"`

    # Commits between the tag and the merge base and from there to HEAD
    VER=`git rev-list HEAD ${TAG:+^$TAG} . | wc -l`
    DIFF=`git rev-list $ORIGIN..HEAD . | wc -l`
    VER=$((VER-DIFF))

    # Number of modified files.
    MOD=`git status --porc . | grep "^ M" | wc -l`

    # Commentary.
    echo Origin ver: $MAJOR.$MINOR.$VER, Local commits: $DIFF, Local edits: $MOD 1>&2

    SUFFIX=
    [ "$DIFF" -ne 0 ] && SUFFIX="$SUFFIX+$DIFF"
    [ "$MOD" -ne 0 -a "$DIFF" -eq 0 ] && SUFFIX="${SUFFIX}+0"
    [ "$MOD" -ne 0 ] && SUFFIX="${SUFFIX}.${MOD}"

    [ "$TAG" = "" ] && SUFFIX="$SUFFIX-pre"

    # Add the start of the hash on the end.
    SUFFIX="$SUFFIX `git rev-list HEAD -n 1 . | cut -c 1-8`"
    BUILD="$VER"
}

( (type git && git status ) >/dev/null 2>&1) && git_describe

if [ -n "$1" ]
then TMPFILE="$1".tmp
else TMPFILE=/dev/stdout
fi

{
echo "#define VERSION_MAJOR    $MAJOR"
echo "#define VERSION_MINOR    $MINOR"
echo "#define VERSION_BUILD    $BUILD"
echo "#define VERSION_SUFFIX   \"$SUFFIX\""
[ "$GITHASH" != "" ] && echo "#define GITHASH $GITHASH"
[ "$GITTIME" != "" ] && echo "#define GITTIME $GITTIME"
[ "$GITDECO" != "" ] && echo "#define GITDECO $GITDECO"
} > "$TMPFILE"

[ -n "$1" ] && {
    if ! cmp -s "$TMPFILE" "$1"
    then
	mv "$TMPFILE" "$1"
    else rm -f "$TMPFILE"
    fi
}

exit 0
