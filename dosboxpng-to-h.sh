#!/bin/bash
for h in 93 163 186 224; do xxd -i dosbox224x$h.png >include/dosbox224x$h.h || break; done
