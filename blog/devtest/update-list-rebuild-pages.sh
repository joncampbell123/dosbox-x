#!/bin/bash
for i in 2*; do
    if [ -d "$i" ]; then
        if [ -f "$i/_page.html" ]; then
            ./update-list.py "$i" || exit 1
        fi
    fi
done

