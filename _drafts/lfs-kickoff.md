---
layout:		post
title: 		"LFS and docker"
categories: 	lfs
date:		2018-06-01
---

One day I stumbled upon [LFS][lfs] and decided to make it in a docker

... since I usually work on different hosts which cannot access each other. One of them is located under ipv4 and other ipv6 networks. And as one may already know hosts cannot communicte with each other when they are in different ip version networks. I have looked at the tunnel brokers and even tried to setup the tunnel for my ipv4 host. But, alas, some of the routers in the path before broker server couldn't pass back the ip 41 protocol packets. That's why docker.

After hitting some bumps in a road I also found by accident that (of course 🤦) the LFS+docker enterprise has been already undertaken.

[lfs]:      http://www.linuxfromscratch.org/lfs
