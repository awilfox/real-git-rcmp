# RCMP for Real Git

**real-git-rcmp** (or **RCMP for Real Git**) allows you to use GitHub Web Hooks
from any git repository hosted on any git server.

## Requirements

* A C++ compiler
* libgit2, available [on GitHub](https://github.com/libgit2/libgit2)
* The eScape library suite, available [on GitHub](https://github.com/wilcox-tech/eScape)

## How it works

**real-git-rcmp** uses libgit2 to investigate the changes received from the
post-receive hook.  It then uses [libJSON](http://libjson.sourceforge.net)
to create the payload and sends it to the one or more Web hook URLs
specified on the command line using libAmy, a part of eScape.

## Building

Unix-likes:
Mac OS X:

	./configure
	make

Note: The ./configure script is very simple, if you know what you're doing you might
be better off just running:
	
	clang++ -o real-git-rcmp main.cpp json/Source/*.cpp -I/path/to/libgit2-and-eScape \
	 -L/path/to/libgit2-and-escape -lgit2 -lAmy -lssl

Windows:

	probably won't work for this release, though you can try cygwin/msys

## Using

In the remote git repository's post-receive hook, call the binary as such:

	real-git-rcmp http://rcmp.tenthbit.net/programming
	real-git-rcmp https://internal/git-post http://rcmp.tenthbit.net/programming

== Binaries

Binaries for Mac OS X (universal, 10.2+) and FreeBSD (5.x+) are coming soon.
