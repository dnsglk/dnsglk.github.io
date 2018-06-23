---
layout: post
title:  "LFS in docker. Coreutils issues"
date:   2018-06-23 14:35:30 +0200
categories: jekyll update
---
### Prerequisites 
- [LFS 8.2][lfs-main] 
- Linux 4.13.0 
- docker `Docker version 18.03.1-ce, build 9ee9f40`
- overlayfs2
- [coreutils-8.29][lfs-coreutils]

## Issue
3 tests fail when running `make check`: 
```
============================================================================
Testsuite summary for GNU coreutils 8.29
============================================================================
# TOTAL: 603
# PASS:  480
# SKIP:  121
# XFAIL: 0
# FAIL:  1
# XPASS: 0
# ERROR: 1
============================================================================
```
#### inotify-dir-recreate
The idea behind the test is explained inside it's source [script](https://github.com/coreutils/coreutils/blob/v8.29/tests/tail-2/inotify-dir-recreate.sh)

{% highlight bash %}
#!/bin/sh 
# Makes sure, inotify will switch to polling mode if directory
# of the watched file was removed and recreated.
# (...instead of getting stuck forever)
{% endhighlight %}

The expected outcome `${COREUTILS_DIR}/${TEST_OUT_DIR}/exp` differs from the actual `${COREUTILS_DIR}/${TEST_OUT_DIR}/out`. Errors are not captured via stream redirection, so we need to repeat this test and keep the output.
```
make check TESTS=tests/tail-2/inotify-dir-recreate KEEP=yes VERBOSE=yes
```

Basically, tail should output more info to the console, and apparantely it doesn't do it. And the reason is, that linux inotify API is not used by tail as expected due to the fact that all files inside container are treated as "remote". Before tail-ing there is a check, verifying if the watched file is located on a remote FS, and, yes, overlayfs and aufs which, are used by docker considered as remote. 



{% highlight c %}
if (forever && ignore_fifo_and_pipe (F, n_files))
    {

#if HAVE_INOTIFY
        /* tailable_stdin() ...
           ...
           any_remote_file() checks if the user has specified any
           files that reside on remote file systems.  inotify is not used
           in this case because it would miss any updates to the file
           that were not initiated from the local system.
           ...
        */
{% endhighlight %}

[lfs-main]:        http://www.linuxfromscratch.org/lfs/
[lfs-coreutils]:   http://www.linuxfromscratch.org/lfs/view/stable/chapter05/coreutils.html
