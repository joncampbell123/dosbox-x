#!/usr/bin/env bash
curbranch=$(git branch --show-current)

if [[ "${curbranch}" == "master" || "${curbranch}" == "develop" ]]; then
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
