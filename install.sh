#!/bin/sh

# Build enet:
cd enet
./configure && make || exit 1
cd ..

# Configure and compile obsidian:
./autogen.sh $@ && make || exit 1
