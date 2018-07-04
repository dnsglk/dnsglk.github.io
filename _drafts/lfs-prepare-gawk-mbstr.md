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
Two tests fail the check by simply outputing one extra line. The line is a warning:
```
TODO
```

Where this line comes from? gawk-4.2.0/`node.c`:
```
/* str2wstr --- convert a multibyte string to a wide string */

NODE *
str2wstr(NODE *n, size_t **ptr)
{
        ...
        for (i = 0; src_count > 0; i++) {
                /*
                 * 9/2010: Check the current byte; if it's a valid character,
                 * then it doesn't start a multibyte sequence. This brings a
                    ...
                 */
                if (is_valid_character(*sp)) {
                        count = 1;
                        wc = btowc_cache(*sp);
                } else
                        count = mbrtowc(& wc, sp, src_count, & mbs);
                switch (count) {
                case (size_t) -2:
                case (size_t) -1: 
                        ...
                        n the user something's wrong */
                        if (! warned) {
                                warned = true;
                                warning(_("Invalid multibyte data detected. There may be a mismatch between your data and your locale."));
                        }
...
```

The str2wstr conversion is apparently used when awk's input contains multibyte strings. Let's find where the method is used.

```
$ grep -i str2wstr ../lfs/docker/tmp/gawk-4.2.0/*
...
gawk-4.2.0/awk.h:#define force_wstring(n)       str2wstr(n, NULL)
...
gawk-4.2.0/builtin.c:                   t1 = str2wstr(t1, & wc_indices);
...
```

```
$ grep -i force_wstring gawk-4.2.0/*                                                                                                                     [17:46:04]
gawk-4.2.0/awk.h:#define force_wstring(n)       str2wstr(n, NULL)
grep: gawk-4.2.0/awklib: Is a directory
gawk-4.2.0/builtin.c:           s1 = force_wstring(s1);
gawk-4.2.0/builtin.c:           s2 = force_wstring(s2);
...
```

Before every call of the method program uses a check `gawk_mb_cur_max > 1`. 

builtin.c : 555
```
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
```

`gawk_mb_cur_max` is a global static variable which initially assigned with `MB_CUR_MAX`. 

`main.c`:
```
... 
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
...
```

> TODO what's the MB_CUR_MAX?

Thanks to the hint from the comment, now we infer that it's a libc function. How does it look like? After few quick guesses, here what I did and had.

```
$ echo -e "#include <stdlib.h> \n int main() {}" > 1.c
$ gcc -E -dM 1.c  | grep MB_C                          
#define MB_CUR_MAX (__ctype_get_mb_cur_max ())
$ nm -D /lib/x86_64-linux-gnu/libc.so.6 | grep __ctype_get_mb_cur_max
000000000002b250 W __ctype_get_mb_cur_max
```

Now that we know the real dude's name, it has become possible to understand when the value is changed. Since `glibc` is also one of the preliminary packages which should be installed along with `gawk` in the LFS preparation phase, we can quickly untar and find the method implementation.

```
$ grep -R __ctype_get_mb_cur_max glibc-2.27
...
glibc-2.27/locale/mb_cur_max.c:__ctype_get_mb_cur_max (void)
```

```
size_t
weak_function
__ctype_get_mb_cur_max (void)
{
  return _NL_CURRENT_WORD (LC_CTYPE, _NL_CTYPE_MB_CUR_MAX);
}
```

Before the moment when `gawk_mb_cur_max` is initialized in the `gawk`'s `main` there are other activities. One particular line attracted my attention.

```
        set_locale_stuff();

```

```
static void
set_locale_stuff(void)
{
#if defined(LC_CTYPE)
        setlocale(LC_CTYPE, locale);
#endif
#if defined(LC_COLLATE)
        setlocale(LC_COLLATE, locale);
...
```

An idea came to me that this line has something to do with the number change. When I run the `gawk`'s test routine on my host environment it doesn't fail. The locales on the host and in docker are, of course, different. `docker`'s has `LC_ALL=POSIX` due to LFS book instructions. On the host: 

```

```

The corresponding locale (LC_TYPE) should be used to support this. Let's check:

```
$ cat 1.c
#include <stdlib.h> 
#include <locale.h>
#include <stdio.h> 

int main() {
    setlocale(LC_CTYPE, "C.UTF-8");
    printf("__ctype_get_mb_cur_max() = %d\n", __ctype_get_mb_cur_max());
    return 0;
}
$ gcc 1.c
$ ./a.out
6
```

## Conclusions
My guess is that the tests can be safely ignored, since it has nothing to do with bugs, but with locale used when gawk is running.

