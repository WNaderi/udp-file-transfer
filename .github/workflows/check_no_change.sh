#!/bin/bash

git remote add teacher-upstream git@github.com:uwb-bopan-c/assignment-tftp-server-and-client-part2.git
git fetch teacher-upstream main:teacher-main

CHANGES=$(git diff teacher-main -- .github/ main -- .github/)

if [ -z "$CHANGES" ]
then
  echo "No changes to .github"
else
  echo "Detected changes to .github:"
  echo "$CHANGES"
  exit 1
fi
