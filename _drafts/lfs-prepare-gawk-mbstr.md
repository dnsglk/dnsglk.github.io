---
title: "LFS in docker, preparation: gawk check fails."
date:   2018-07-04
categories:     lfs
tags:   [LFS, lfs, linux, gawk, docker]
---

### Prerequisites
- [LFS 8.2][lfs-main]
- Linux 4.13.0
- docker `Docker version 18.03.1-ce, build 9ee9f40`
- gawk-4.2.0

## Issue
 
------------------
builtin.c : 555
        if (gawk_mb_cur_max > 1) {
                tmp = force_wstring(tmp);
                len = tmp->wstlen;
                /*
                 * If the bytes don't make a valid wide character
                 * string, fall back to the bytes themselves.
                 */
                 if (len == 0 && tmp->stlen > 0)
                         len = tmp->stlen;
        } else
                len = tmp->stlen;
------------
main.c:

#if defined(LOCALEDEBUG)
        if (locale != initial_locale)
                set_locale_stuff();
#endif

        /*
         * In glibc, MB_CUR_MAX is actually a function.  This value is
         * tested *a lot* in many speed-critical places in gawk. Caching
         * this value once makes a speed difference.
         */
        gawk_mb_cur_max = MB_CUR_MAX;

------------------
../lfs/docker/tmp/gawk-4.2.0/mbsupport.h:# undef MB_CUR_MAX
../lfs/docker/tmp/gawk-4.2.0/mbsupport.h:# define MB_CUR_MAX 1
------------------
$ echo -e "#include <stdlib.h> \n int main() {}" > 1.c

$ gcc -E -dM 1.c  | grep MB_C                          
#define MB_CUR_MAX (__ctype_get_mb_cur_max ())

$ nm -D /lib/x86_64-linux-gnu/libc.so.6 | grep __ctype_get_mb_cur_max
000000000002b250 W __ctype_get_mb_cur_max
------------------
