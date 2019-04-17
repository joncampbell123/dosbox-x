#!/bin/bash
curbranch=`git branch | grep \* | cut -d ' ' -f 2`

if [[ x"$curbranch" == x"master" || x"$curbranch" == x"develop" ]]; then
    rm -f include/build_timestamp.h
    git checkout include/build_timestamp.h

    git checkout develop
    git pull
    git push

    rm -f include/build_timestamp.h
    git checkout include/build_timestamp.h

    git checkout master
    git pull
    git push

    rm -f include/build_timestamp.h
    git checkout include/build_timestamp.h

    git checkout master && git merge develop && git checkout develop && git merge master && git fetch --all && git push --all && make -j
fi
