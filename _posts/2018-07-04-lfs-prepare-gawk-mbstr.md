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
- [gawk-4.2.0][lfs-gawk]

## Issue
Two tests fail the check because one output line is not printed. The line is a warning:

```
- gawk: mbstr1.awk:2: warning: Invalid multibyte data detected. There may be a mismatch between your data and your locale.
```

That's actually a great hint, but I want more context. Where this line comes from? 

`node.c`:
{% highlight c %}
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
{% endhighlight %}

As usually, it's a good idea to check the test step in the host's environment. As with the `coreutils` case everything works good. We need to look closer at the function which outputs the warning line.

The `str2wstr` conversion is apparently used when `gawk`'s input contains multibyte strings. This is exactly the case for the failed tests.  Let's find places in the code where the method is called from.

```
$ grep -i str2wstr gawk-4.2.0/*
...
gawk-4.2.0/awk.h:#define force_wstring(n)       str2wstr(n, NULL)
...
gawk-4.2.0/builtin.c:                   t1 = str2wstr(t1, & wc_indices);
...
```

and 

```
$ grep -i force_wstring gawk-4.2.0/*
gawk-4.2.0/awk.h:#define force_wstring(n)       str2wstr(n, NULL)
grep: gawk-4.2.0/awklib: Is a directory
gawk-4.2.0/builtin.c:           s1 = force_wstring(s1);
gawk-4.2.0/builtin.c:           s2 = force_wstring(s2);
...
```

Before every call the program uses a check `gawk_mb_cur_max > 1`. It looks like this condition yields `false` in docker environment, and `true` on my host.

`builtin.c`:
{% highlight c %}
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
{% endhighlight %}

`gawk_mb_cur_max` is a global static variable which initially assigned with `MB_CUR_MAX`. 

`main.c`:
{% highlight c %}
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
{% endhighlight %}

### glibc
Thanks to the hint from the comment, now we infer that it's a `glibc` function. How does it look like? 

```
$ echo -e "#include <stdlib.h> \n int main() {}" > 1.c
$ gcc -E -dM 1.c  | grep MB_C                          
#define MB_CUR_MAX (__ctype_get_mb_cur_max ())
$ nm -D /lib/x86_64-linux-gnu/libc.so.6 | grep __ctype_get_mb_cur_max
000000000002b250 W __ctype_get_mb_cur_max
```

Now that we know the real name, it has become possible to understand when the value is changed. Since `glibc` is also one of the preliminary packages which should be installed along with `gawk` in the LFS preparation phase, we can quickly untar and find the method implementation.

```
$ grep -C 2 __ctype_get_mb_cur_max glibc-2.27/stdlib/stdlib.h                                                                                            [12:04:40]

/* Maximum length of a multibyte character in the current locale.  */
#define MB_CUR_MAX      (__ctype_get_mb_cur_max ())
extern size_t __ctype_get_mb_cur_max (void) __THROW __wur;

```

{% highlight c %}
size_t
weak_function
__ctype_get_mb_cur_max (void)
{
  return _NL_CURRENT_WORD (LC_CTYPE, _NL_CTYPE_MB_CUR_MAX);
}
{% endhighlight %}

Before the moment when `gawk_mb_cur_max` is initialized in the `gawk`'s `main` there are other activities. One particular line attracted my attention.

{% highlight c %}
        set_locale_stuff();
{% endhighlight %}

{% highlight c %}
static void
set_locale_stuff(void)
{
#if defined(LC_CTYPE)
        setlocale(LC_CTYPE, locale);
#endif
#if defined(LC_COLLATE)
        setlocale(LC_COLLATE, locale);
...
{% endhighlight %}

### Correct locale
An idea came to me that this line has something to do with the number change. When I run the `gawk`'s test routine on my host environment it doesn't fail. The locales on the host and in docker are, of course, different. `docker`'s has `LC_ALL=POSIX` due to LFS book instructions. On the host: 

```
$ printenv | grep LC
LC_PAPER=pl_PL.UTF-8
LC_ADDRESS=pl_PL.UTF-8
LC_MONETARY=pl_PL.UTF-8
LC_NUMERIC=pl_PL.UTF-8
LC_TELEPHONE=pl_PL.UTF-8
LC_IDENTIFICATION=pl_PL.UTF-8
LC_MEASUREMENT=pl_PL.UTF-8
LC_TIME=pl_PL.UTF-8
LC_NAME=pl_PL.UTF-8
LC_CTYPE=en_US.UTF-8
```

Since `__ctype_get_mb_cur_max()` uses `LC_TYPE` to get the multibyte character size, the corresponding locale (LC_TYPE) should be used to support this. To prove it, I found the location of 'C.UTF-8' on my host and copied it to docker. I looked up the source directory with the help of `locate` and the target directory by running `gawk` with `strace`. Let's check it now:

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
The size of multibyte char in 'C.UTF-8' is `6`. For plain 'C' locale it returns `1`.

## Conclusions
The two failed tests which check multibyte scenarios can be safely ignored, since it has nothing to do with bugs, but with locale used when gawk is running.

[lfs-main]: http://www.linuxfromscratch.org/lfs/view/8.2/
[lfs-gawk]: http://www.linuxfromscratch.org/lfs/view/8.2/chapter05/gawk.html
