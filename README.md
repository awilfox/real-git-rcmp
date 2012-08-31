= RCMP for Real Git

*real-git-rcmp* (or *RCMP for Real Git*) allows you to use GitHub Web Hooks
from any git repository hosted on any git server.

== Requirements

* A C++ compiler
* libgit2, available [on GitHub](https://github.com/libgit2/libgit2)
* The eScape library suite, available [on GitHub](https://github.com/wilcox-tech/eScape)

*A note of caution*: This code has not been well-tested on Linux; getprogname()
may not be implemented on your version of glibc and it may be necessary to
change the code to work properly.  Better Linux support is planned.

== How it works

*real-git-rcmp* uses libgit2 to investigate the changes received from the
post-receive hook.  It then uses [libJSON](http://libjson.sourceforge.net)
to create the payload and sends it to the one or more Web hook URLs
specified on the command line using libAmy, a part of eScape.

== Building

Unix-likes:

  g++ -o real-git-rcmp main.cpp -I/path/to/libgit2-and-eScape \
   -L/path/to/libgit2-and-escape -lgit2 -lAmy

Mac OS X:

  clang++ -o real-git-rcmp main.cpp -I/path/to/libgit2-and-eScape \
   -L/path/to/libgit2-and-escape -lgit2 -lAmy

Windows:

  probably won't work for this release, though you can try cygwin/msys

== Using

In the remote git repository's post-receive hook, call the binary as such:

  real-git-rcmp http://rcmp.tenthbit.net/programming
  real-git-rcmp https://internal/git-post http://rcmp.tenthbit.net/programming

== Binaries

Binaries for Mac OS X (universal, 10.2+) and FreeBSD (5.x+) are coming soon.
