#!/bin/bash
git checkout master && git merge develop && git checkout develop && git merge master && git fetch --all && git push --all && make -j
