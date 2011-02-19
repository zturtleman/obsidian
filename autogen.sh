#!/bin/sh

# Build enet:
cd enet
./configure && make || exit 1
cd ..

mkdir autotools

aclocal
autoheader
automake -a -c
autoconf
automake

./configure "$@"

