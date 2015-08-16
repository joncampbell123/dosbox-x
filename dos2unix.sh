#!/bin/bash
for ext in c cpp h; do
	find -iname \*.$ext | while read X; do dos2unix -- "$X" || exit 1; done
done

