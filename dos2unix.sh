#!/usr/bin/env bash
for ext in c cpp h; do
	find . -iname \*.$ext | while read -r X; do dos2unix -- "${X}" || exit 1; done
done

