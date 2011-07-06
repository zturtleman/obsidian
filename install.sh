#!/bin/sh

jobs=2 # Number of make jobs to use, generally cpu cores x2 if you have hyperthreading.

# Build enet:
cd enet
./configure || exit 1 
make -j${jobs} || exit 1
cd ..

# compile obsidian:
make -j${jobs} || exit 1
