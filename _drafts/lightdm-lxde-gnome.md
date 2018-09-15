---
layout:         post
title:          lightdm and LXDE
date:           14-09-2018 23:44
categories:     linux
tags:           [linux,lightdm,lxde,LXDE,gnome]
---

## From Mate to LXDE 
It happens to me, that if I want to change or fix something in linux, it is never a piece of cake. For the last few years my linux GUI gradually changed and started to resemble a carpet sewed with unrelated pieces. Originally I had installed Kubuntu, then changed Desktop Environment (DE) few times and ended up having eye-hurting mess and Mate DE. Finally I got brave enough to get rid of the bedlam and try something new - LXDE, for example.

Before installing LXDE all kde* and kubuntu* packages were removed (though their configuration and such litter nobody cleaned up properly!). The next step was to install only minimum of the needed LXDE packages. When you try to simply `apt-get install lxde` it will dully recommend hell of a lot of other stuff. Insted I run `apt-get install lxde-common lxsession lxpanel --no-install-recommends`. Only 3 of those packages are considered enough as per Arch docs.

After rebooting the computer I still saw the same picture. Probably something was not configured. So let's look at the logs. The first thing I looked was `lightdm` logs:
```
...
[+3.40s] DEBUG: Session pid=1483: Running command /usr/sbin/lightdm-session mate-session
...
```

## In search for proper configuration
Quick look at lightdm documentation reveals that the standard directories are:
```
```

This is great and sounds like easy win, however what if gnome is suddenly ? Actually, 
