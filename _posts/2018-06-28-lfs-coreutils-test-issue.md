---
layout: 	post
title:  	"LFS in docker, preparation: coreutils check fails."
date:   	2018-06-28 16:00
categories: 	lfs
tags:		[LFS, lfs, linux, docker, coreutils]
---
### Prerequisites 
- [LFS 8.2][lfs-main] 
- Linux 4.13.0 
- docker `Docker version 18.03.1-ce, build 9ee9f40`
- [coreutils-8.29][lfs-coreutils]

## Intro
TL;DR just go to [Conclusions](#conclusions) section.

This post is about a lesson learned from *NOT* learning the docker's [official documentation][docker-storage] before starting doing something with it!

To understand what's happening first read [this post]({{ site.baseurl }}{% link _posts/2018-06-28-lfs-kickoff.md %}).

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

*Did you know?* that there is a project which automates the LFS build? And there is a [comment](https://github.com/reinterpretcat/lfs/blob/master/scripts/prepare/5.19-make-coreutils.sh#L9) telling that `coreutils`'s tests fail. 😌 at least I am not alone!

### Errors 
#### AppArmor and PATH_MAX
Two tests, which result in error, show that some operations juggle the filepaths longer than [PATH_MAX](https://eklitzke.org/path-max-is-tricky). These operations are restricted by host's linux security mechanism - AppArmor. 
```
$ dmesg | grep "Failed name look"
[ 9104.641035] audit: type=1400 audit(1529927897.941:34): apparmor="DENIED" operation="mkdir" info="Failed name lookup - name too long" error=-36 profile="docker-default" name="" pid=19727 comm="perl" requested_mask="c" denied_mask="c" fsuid=1000 ouid=1000
[ 9104.646367] audit: type=1400 audit(1529927897.946:35): apparmor="DENIED" operation="open" info="Failed name lookup - name too long" error=-36 profile="docker-default" name="" pid=19731 comm="chmod" requested_mask="r" denied_mask="r" fsuid=1000 ouid=1000
[ 9104.651598] audit: type=1400 audit(1529927897.951:36): apparmor="DENIED" operation="open" info="Failed name lookup - name too long" error=-36 profile="docker-default" name="" pid=19732 comm="rm" requested_mask="r" denied_mask="r" fsuid=1000 ouid=1000
```

BTW, the issue may also manifest itself during the configuration step:
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
After failing to find a quick way to tame AppArmor, I found that (of course) somebody faced similar [issues in docker container][app-armor-issue]. However, changing the FS driver doesn't help with this specific issue, but fixes 2 other failed tests. 😏

So, to fix this we can:
- disable it AppArmor at all;
- create a new app-armor profile for docker and use it instead of `docker-default`;
- or run container as privileged (my choice).

#### Priviliged Container
Instead of bothering with AppArmor, you can run your container in privileged mode. 
{% highlight bash %}
docker run -it --privileged <CONTAINER> [...other options]
{% endhighlight %}
This command probably runs container with other apparmor profile, which is apparantely not so restrictive as `docker-default`. Here is an [official explanation][docker-priv] and interesting [insight from developers][docker-insight].
> When the operator executes docker run --privileged, Docker will enable access to all devices on the host as well as set some configuration in AppArmor or SELinux to allow the container nearly all the same access to the host as processes running outside containers on the host.

Now there is no process 'enforced' in `docker-default` domain.
```
$ sudo apparmor_status | grep -A 10 "processes are in enforce mode"                                                                                   [14:38:19]
6 processes are in enforce mode.
   /sbin/dhclient (1448) 
   /sbin/dhclient (2288) 
   /usr/sbin/cups-browsed (15468) 
   /usr/sbin/cupsd (15467) 
   /usr/sbin/cupsd (15487) 
   /usr/sbin/ntpd (3707) 
0 processes are in complain mode.
0 processes are unconfined but have a profile defined.
```

### Failed Tests
#### aufs vs overlay
2 out of 3 failed tests can pass if the `aufs` docker driver is changed to `overlay2`. Here is the [documentation][docker-overlay2] how to change docker storage driver. Don't forget to enter extra command to make the backup. Now it's better:
```
$ grep -E "(ERROR|FAIL) " tests/test-suite.log  
FAIL tests/tail-2/inotify-dir-recreate.sh (exit status: 1)
ERROR tests/rm/deep-2.sh (exit status: 99)
ERROR tests/du/long-from-unreadable.sh (exit status: 99)
```
#### inotify-dir-recreate
Another test which fails is tail-2/inotify-dir-recreate. The idea behind the test according to it's source [script](https://github.com/coreutils/coreutils/blob/v8.29/tests/tail-2/inotify-dir-recreate.sh) is

{% highlight bash %}
#!/bin/sh 
# Makes sure, inotify will switch to polling mode if directory
# of the watched file was removed and recreated.
# (...instead of getting stuck forever)
{% endhighlight %}

To see the output of the test run the command:
{% highlight bash %}
make check TESTS=tests/tail-2/inotify-dir-recreate KEEP=yes VERBOSE=yes
{% endhighlight %}
The test fails because it's output is not as expected. It expects:
``` 
$ cat gt-inotify-dir-recreate.sh.7W1d/exp
inotify
tail: 'dir/file' has become inaccessible: No such file or directory
tail: directory containing watched file was removed
tail: inotify cannot be used, reverting to polling
tail: 'dir/file' has appeared;  following new file
``` 
Instead, there is only one line from `stdout`:
```
$ cat gt-inotify-dir-recreate.sh.Fm7C/out
inotify
```
Errors are not captured via stream redirection for some reason. To see the errors from a program I run the test manually. Here is what I get inside the docker:

![Coreutils test fail in docker]({{ site.baseurl }}/assets/img/coreutils-test-fail-in-docker.gif)

As for the expected behavior, it can be achieved on the host's side. However, the default `tail` doesn't switch to polling either. So I built `tail` binary from `coretuils` sources:

![How it should work]({{ site.baseurl }}/assets/img/coreutils-should-work.gif)

Basically, `tail` should tell that it abandons inotify API and should use polling when the file is removed. 

There is a cool [intro to inotify API][linux-api] which is good to familiarize with. 

From inotify manual page:
> Inotify  reports  only  events that a user-space program triggers through the filesystem API.  As a result, it does not catch remote events that occur on network filesystems. Applications must fall back to polling the filesystem to catch such events.)  Furthermore, various pseudo-filesystems such as /proc, /sys, and /dev/pts are  not  monitorable with inotify.

And the reason why `tail` is not switching to polling is that  it's already using it! inotify API is not used by tail as expected at all! Since I build packages inside container's writable layer all files there are treated as "remote". In the main() of `tail` there is a check, verifying if the watched file is located on a remote FS. If it's true, then program defaults to polling. Thus we cannot get the message about reverting to polling. 

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

#### Docker Volumes
It only occurred to me afterwards that these FS issues can be avoided if all the build-related work is done in docker volumes. 🤦. Volumes are available by the means of the same FS which the host's dir is located on.
> Ideally, very little data is written to a container’s writable layer, and you use Docker volumes to write data. 

> A volume is a specially-designated directory within one or more containers that bypasses the Union File System. 

Now let's build `coreutils` inside the volume (new container) and run tests again.

```
============================================================================
Testsuite summary for GNU coreutils 8.29
============================================================================
# TOTAL: 603
# PASS:  482
# SKIP:  120
# XFAIL: 0
# FAIL:  1
# XPASS: 0
# ERROR: 0
============================================================================
```

Now another test fails - [df-symlink](https://github.com/coreutils/coreutils/blob/master/tests/df/df-symlink.sh). The first step of the test checks that `df` shows the same filesystem and mount point when given device file or symlink to that file. The second check is the same, except that now symlink is changed to the '.' (current directory). Test fails on the second step due to the specifics of [get_disk()](https://github.com/coreutils/coreutils/blob/v8.29/src/df.c#L1259) method. In the case if one FS is mounted several times on different mount entries, `df` chooses the shortest mount point to print in the results table! The mount point path for docker volume I choose was longer, than one those default mounts automatically created by docker. In my case, `/home/lfs/work/sources/coreutils-8.29` was longer than `/etc/hosts`. Yeah, `docker` makes some mounts for you:

```
$ mount | grep /dev/sda7
/dev/sda7 on /home/lfs/work/ type ext4 (rw,relatime,errors=remount-ro,data=ordered)
/dev/sda7 on /etc/resolv.conf type ext4 (rw,relatime,errors=remount-ro,data=ordered)
/dev/sda7 on /etc/hostname type ext4 (rw,relatime,errors=remount-ro,data=ordered)
/dev/sda7 on /etc/hosts type ext4 (rw,relatime,errors=remount-ro,data=ordered)
```

This issue is gone when a shorter than `/etc/hosts` mount point is chosen for the volume. For example, simply `/work`.

## Conclusions
{: #conclusions }
0. Use volumes instead of container writable layer to avoid any issues with running tests or building the project.
1. Run container in privileged mode to avoid any possible problems with building LFS.
2. Learn `docker` documentation to avoid issues in the future!

[lfs-main]:        http://www.linuxfromscratch.org/lfs/view/8.2/
[lfs-coreutils]:   http://www.linuxfromscratch.org/lfs/view/8.2/chapter05/coreutils.html
[app-armor-issue]: https://github.com/moby/moby/issues/13451
[docker-overlay2]: https://docs.docker.com/storage/storagedriver/overlayfs-driver/#configure-docker-with-the-overlay-or-overlay2-storage-driver
[docker-priv]:	   https://docs.docker.com/engine/reference/run/#runtime-privilege-and-linux-capabilities
[docker-insight]:  https://blog.docker.com/2013/09/docker-can-now-run-within-docker/
[docker-storage]:  https://docs.docker.com/storage/
[linux-api]:	   https://lwn.net/Articles/604686/
----
### References
- [LFS][lfs-main]
- [Coreutils](https://github.com/coreutils/coreutils/)
- [AppArmor and docker issue][app-armor-issue]
- [Intro to inotify API][linux-api]
