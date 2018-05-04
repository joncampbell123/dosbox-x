#!/bin/bash
rm -f include/build_timestamp.h
git checkout include/build_timestamp.h

git checkout develop
git pull
git push

git checkout master
git pull
git push

git checkout master && git merge develop && git checkout develop && git merge master && git fetch --all && git push --all && make -j
