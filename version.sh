#!/bin/sh

# This will be the "versioner" that prepares a custom config.h for obsidian.
# It will include the proper versions (date, revision, etc) for the build of obsidian if specified.
# $1 will be either n or w, for *nix or windows, this affects which config template is used.
# If $2 is not "v", the build will not be versioned.

if [ "$1" = "n" ]
then
	conf=config.h.nix
	shift
else
	conf=config.h.win
fi

if [ "$1" = "v" ] 
then 
	ver="`hg parents --template "{node|short}"` (`date +"%b %d, %Y"`)"
fi

# Includes a gross hack to remove an extra space for the nonversioned build
sed "s/_VERSION_/$ver/g;s/obsidian \"/obsidian\"/g" < $conf > config.h
