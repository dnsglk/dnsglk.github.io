---
layout:		post
title: 		"LFS in docker"
categories: 	lfs
date:		2018-06-28 12:00
tags:           [LFS, lfs, linux, docker]
---

One day I stumbled upon [LFS][lfs] and decided to make it in a docker

... since I usually work on different hosts, and I want to sync and sandbox my work. One of the hosts is not up all the time. Moreover, they are located in networks which use different versions of IP protocol. And hosts cannot communicte with each other when they are in differently ip-versioned networks. I have looked at the tunnel brokers and even tried to setup the tunnel for my ipv4 host. But, alas, some link in the route can't pass back the ip 41 protocol packets. 

And I wanted to become familiar with docker. 

So, that's why docker.

After hitting some bumps in a road I also found by accident that (of course 🤦) the LFS+docker enterprise has been already undertaken.

[lfs]:      http://www.linuxfromscratch.org/lfs
