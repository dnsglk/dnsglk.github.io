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
- [coreutils-8.29][lfs-coreutils]

## Issue
3 tests fail when running `make check` under docker and 2 return with error. 
```
============================================================================
Testsuite summary for GNU coreutils 8.29
============================================================================
# TOTAL: 603
# PASS:  459
# SKIP:  139
# XFAIL: 0
# FAIL:  3
# XPASS: 0
# ERROR: 2
============================================================================
```
```
$ grep -E "(ERROR|FAIL) " tests/test-suite.log
FAIL tests/tail-2/inotify-dir-recreate.sh (exit status: 1)
ERROR tests/rm/deep-2.sh (exit status: 99)
FAIL tests/dd/direct.sh (exit status: 1)
FAIL tests/dd/sparse.sh (exit status: 1)
ERROR tests/du/long-from-unreadable.sh (exit status: 99)
```
In comparison, running the check on the host doesn't yield any issue.

*Did you know?* that there is a project which automates the LFS build? And there is a [comment](https://github.com/reinterpretcat/lfs/blob/master/scripts/prepare/5.19-make-coreutils.sh#L9) telling that `coreutils`'s tests fail. [ε-(´・｀) ﾌ](http://www.fastemoji.com/%CE%B5-(%C2%B4%E3%83%BB%EF%BD%80)-%EF%BE%8C-Meaning-Emoji-Emoticon-Phew!-Ascii-Art-Relieved-Grateful-Phew-Japanese-Kaomoji-Smileys-141.html) at least I am not alone!

### Dealing with Errors
A brief analysis of the problem surfaced a difficulty in aufs driver which tries to operate with file paths longer than PATH_MAX. The obstacle is hidden in the linux security mechanism - AppArmor. It blocks program operations which deal with extremely long paths.

BTW, the issue also manifests itself during the configuretion step:
```
$ ./configure --prefix=/tools --enable-install-program=hostname
checking for a BSD-compatible install... /usr/bin/install -c
checking whether build environment is sane... yes
...
# a lot of output
...
config.status: creating po/Makefile
rm: cannot remove 'confdir3/confdir3/confdir3/.../confdir3': File name too long
```
I thought of either trying to configure AppArmor so that it can allow the operation, or ... try other solution with google's help. After failing to find a quick way to tame AppArmor, I found that (of course) somebody faced similar [issues in docker container][app-armor-issue].
> TODO Learn how to configure AppArmor

Here is the [documentation][docker-overlay2] how to change docker storage driver. Don't forget to enter extra command to make the backup. 
After changing the driver some of failed tests are passing.

```
$ grep -E "(ERROR|FAIL) " tests/test-suite.log  
FAIL tests/tail-2/inotify-dir-recreate.sh (exit status: 1)
ERROR tests/rm/deep-2.sh (exit status: 99)
ERROR tests/du/long-from-unreadable.sh (exit status: 99)
```

### Failed Tests
#### inotify-dir-recreate
The idea behind the test according to it's source [script](https://github.com/coreutils/coreutils/blob/v8.29/tests/tail-2/inotify-dir-recreate.sh):

{% highlight bash %}
#!/bin/sh 
# Makes sure, inotify will switch to polling mode if directory
# of the watched file was removed and recreated.
# (...instead of getting stuck forever)
{% endhighlight %}

To see the output of the test run the command:
```
make check TESTS=tests/tail-2/inotify-dir-recreate KEEP=yes VERBOSE=yes
```
Test fails because it's output is not as expected. It expects:
``` 
$ cat gt-inotify-dir-recreate.sh.7W1d/exp
inotify
tail: 'dir/file' has become inaccessible: No such file or directory
tail: directory containing watched file was removed
tail: inotify cannot be used, reverting to polling
tail: 'dir/file' has appeared;  following new file
``` 
Results contain only the first line which is the standard output. Errors are not captured via stream redirection in my shell in the docker container. To see errors from a program I need to launch it manually. Here is the behavior I get in docker:

![Coreutils test fail in docker](/assets/img/coreutils-test-fail-in-docker.gif)

The expected behavior can be reproduced on the host. However, the default `tail` in my environment doesn't switch to polling either. So I copied, configured and run `make` to get the `tail` binary from the `coreutils` archive which is used in LFS.

![How it should work](/assets/img/coreutils-should-work.gif)

Basically, `tail` should output more info to the console, and apparently it doesn't do it. And the reason why `tail` is not switching to polling is that linux inotify API is not used by tail as expected. All files inside container are treated as "remote". In the main() of `tail` there is a check, verifying if the watched file is located on a remote FS. If file is remote, than inotify is not used at all, and thus we cannot get that message. 

{% highlight c %}
if (forever && ignore_fifo_and_pipe (F, n_files))
    {
    ...
#if HAVE_INOTIFY
        /* tailable_stdin() ...
           ...
           any_remote_file() checks if the user has specified any
           files that reside on remote file systems.  inotify is not used
           in this case because it would miss any updates to the file
           that were not initiated from the local system.
           ...
        */
         if (!disable_inotify && (tailable_stdin (F, n_files)
                               || any_remote_file (F, n_files)
                               || ! any_non_remote_file (F, n_files)
                               || any_symlinks (F, n_files)
                               || any_non_regular_fifo (F, n_files)
                               || (!ok && follow_mode == Follow_descriptor)))
            disable_inotify = true;

      if (!disable_inotify)
        {
          int wd = inotify_init ();
          if (0 <= wd)
            {
              ...
              if (! tail_forever_inotify (wd, F, n_files, sleep_interval))
                return EXIT_FAILURE;
            }
          error (0, errno, _("inotify cannot be used, reverting to polling"));
          ...
        }
#endif
      disable_inotify = true;
      tail_forever (F, n_files, sleep_interval);
    }
    ...
}
{% endhighlight %}

`overlayfs2` and `aufs`, which are used by docker, are considered as remote FS. Look at the [tail.c](https://github.com/coreutils/coreutils/blob/v8.29/src/tail.c#L2039) and locally generated file `fs-is-local.h`.

# ... continue 
1. Explain what is polling, brief inotify documentation 
2. inotifywait example with removal -> explains why file removal should switch to polling

[lfs-main]:        http://www.linuxfromscratch.org/lfs/
[lfs-coreutils]:   http://www.linuxfromscratch.org/lfs/view/stable/chapter05/coreutils.html
[app-armor-issue]: https://github.com/moby/moby/issues/13451
[docker-overlay2]: https://docs.docker.com/storage/storagedriver/overlayfs-driver/#configure-docker-with-the-overlay-or-overlay2-storage-driver
----
### References
- [LFS][lfs-main]
- [Coreutils](https://github.com/coreutils/coreutils/)
- [AppArmor and docker issue][app-armor-issue]
