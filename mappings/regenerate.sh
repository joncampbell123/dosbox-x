#!/bin/bash
./sb2u.pl cp437_to_unicode <CP437.TXT >../include/cp437_uni.h || exit 1
./sb2u.pl cp866_to_unicode <CP866.TXT >../include/cp866_uni.h || exit 1
./sjis2u.pl cp932_to_unicode <CP932.TXT >../include/cp932_uni.h || exit 1

