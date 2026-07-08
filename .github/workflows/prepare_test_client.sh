#!/bin/bash

git clone git@github.com:bopan-uw/css432-tftp.git
cd css432-tftp || exit
echo "Building test client."
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
