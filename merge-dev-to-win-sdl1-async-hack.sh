#!/bin/bash
# do NOT merge develop-win-sdl1-async-hack-201802 back into develop. I will do that manually when it is time.
git checkout develop-win-sdl1-async-hack-201802
git pull
git push

git checkout develop
git pull
git push

git checkout master
git pull
git push

git checkout develop-win-sdl1-async-hack-201802 && git merge develop && git checkout develop && git fetch --all && git push --all && make -j
