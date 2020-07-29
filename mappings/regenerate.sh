#!/bin/bash
./sb2u.pl cp437_to_unicode <CP437.TXT >../include/cp437_uni.h || exit 1
./sb2u.pl cp850_to_unicode <CP850.TXT >../include/cp850_uni.h || exit 1
./sb2u.pl cp852_to_unicode <CP852.TXT >../include/cp852_uni.h || exit 1
./sb2u.pl cp855_to_unicode <CP855.TXT >../include/cp855_uni.h || exit 1
./sb2u.pl cp857_to_unicode <CP857.TXT >../include/cp857_uni.h || exit 1
./sb2u.pl cp858_to_unicode <CP858.TXT >../include/cp858_uni.h || exit 1
./sb2u.pl cp866_to_unicode <CP866.TXT >../include/cp866_uni.h || exit 1
./sb2u.pl cp869_to_unicode <CP869.TXT >../include/cp869_uni.h || exit 1
./sb2u.pl cp872_to_unicode <CP872.TXT >../include/cp872_uni.h || exit 1
./sjis2u.pl cp932_to_unicode <CP932.TXT >../include/cp932_uni.h || exit 1

