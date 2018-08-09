#!/bin/bash
git checkout develop
git pull
git push

git checkout develop-dynamic-core
git pull
git push

git checkout develop-dynamic-core && git merge develop && git checkout develop && git fetch --all && git push --all && make -j
