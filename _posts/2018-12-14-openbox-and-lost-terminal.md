---
layout:         post
title:          Openbox and lost terminal
date:           14-12-2018 22:00
categories:     linux
tags:           [linux,openbox,terminal]
---

## Intro 
It always happens to me, that if I want to change something in my linux environment, it is never a piece of cake. My recent experience with switching desktop environment from Kubuntu to LXDE wasn't an easy walk. Though I managed to overcome the transition, several small bugs still annoyed me. For instance, during the change of windows with `Alt-Tab` my terminal disappeared from the list. And it's wasn't possible to get the terminal except for manually minimizing all windows. LXDE taskbar - `lxpanel` - didn't had it as well.  

There was a workaround, but still uncomfortable: after all windows where minimized several times ('iconified' onto the taskbar), terminal icon appeared again on the windows switch list. But it disappeared again from that list once you switched into it. Thus, it was held to fix the nasty state of things.

<iframe width="420" height="315" src="https://goo.gl/NeLhuJ" allowfullscreen></iframe>

How does one approach this matter? Well, if you have enough experience either with `Openbox` or any window manager, probably you already know what's wrong or what it might be. If there was no prior acquaintance with the software, then, perhaps, one would search similar problem on the net. However, I had a strong believe that it was a real bug, and repairing `Openbox` would earn me something. 

### How it was fixed
I had already tweaked `Openbox` configuration only a little bit - added few key mappings to my taste. However, I didn't know that it was enough to add few lines there in order to get what I wanted. So, first of all I knew that I had to get a debug version of the binary. After the binary is built, it needs to be run somehow. Debugging window manager from it's running session is not possible, since the breakpoint will hang it. It means that you need a second running `Openbox` on the machine (if you do not run it remotely, of course). It is not difficult at all to have several window managers running simultaneously. You just need to switch to another linux virtual terminal (Ctrl+Alt+F\<#\>), and type
```
startx lxsession -- :1 vt6
``` 
`lxsession` is 'the standard session manager used by LXDE. LXSession automatically starts a set of applications and sets up a working desktop environment.' `:1` is next display/server number, because :0 is used by already running system. `vt6` is the number of virtual terminal I switched to. 

After the new desktop is running, we are ready to debug it. I chose the combination of `eclipse` and `gdbserver`, because it seemed more comfortable to debug `Openbox` with the help of GUI. However, when I tried to attach `gdb`, it told me that it's not possible:
```
$ gdbserver localhost:6666 --attach 32639     
Cannot attach to process 32639: Operation not permitted (1)
Exiting
```
It was not enough to understand why this happened. The first thought was to check `uid` of the process. But, it had the same `uid` as my user - no suprise since I logged into the VT6 and run the `startx` without `sudo`. Then I tried to run it in `gdb` and it gave the answer:
```
gdb --pid 32639 -q
Attaching to process 32639
Could not attach to process.  If your uid matches the uid of the target
process, check the setting of /proc/sys/kernel/yama/ptrace_scope, or try
again as the root user.  For more details, see /etc/sysctl.d/10-ptrace.conf
ptrace: Operation not permitted.
(gdb) 
```
After reading kernel's documentation, I was suprised that I had never encountered ever during my debugging experience. Anyway, I knew what I did and temporarily disabled this security measure.

Finally, I had `eclipse` attached to `Openbox` via gdbserver. It didn't take much time to find the place where the switch list was generated. I just serached for related words in the code and 'focus' led me to 

![Eclipse]({{ site.baseurl }}/assets/img/openbox-and-lost-terminal-1.png)

My terminal's window (I use terminator) didn't pass the check in the function 'focus_cycle_valid', which prepares a list of candidate windows. In particular, the 'skip_taskbar' field was set to '1', whereas other windows, which didn't disappear, had it '0'. The fact that terminal appeared after windows minimization now was clear. The field 'iconic' in that case was '1', which made the checks pass. After having this on my hands, I decided to google the 'skip_taskbar' and it turned out that somebody already had the same issue, but was smart enough to ask it on the `Openbox`'s [mail list](https://openbox.icculus.narkive.com/RFUZNCQF/no-focus-if-skip-taskbar-yes). The feature was explained

> It is by design. Apps that skip the taskbar are generally background
> type windows that you don't want to give focus to on start. Openbox
> doesn't discriminate between windows that request the skip-taskbar
> themselves verses those set that way in the config file.

\- one of the `Openbox`s authors, Dana Jansens.

I didn't tell my terminal to be 'skip_taskbar'. It might be that I configured something wrong in the beginning. My choice of the fix was easy - few additional lines in .xml and terminal is never lost again.
```
    ...
    <applications>
       <application name="terminator" class="Terminator" role="">
        <skip_taskbar>no</skip_taskbar>
    ...
```
