#!/usr/bin/env bash
url="https://github.com/wcp16/dosbox-xe.git"
branch="dosbox-sdl2-upstream"
git remote add "${branch}" "${url}" || git remote set-url "${branch}" "${url}" || exit 1
git fetch "${branch}" || exit 1
git checkout -t "remotes/origin/master-sdl2" || git checkout "master-sdl2" || exit 1

