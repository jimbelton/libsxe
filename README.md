libsxe
======

This repo contains libsxe library. While it has been built in the distant past with Visual C on Windows, it is currently being
built and used on Debian 10, Debian 11, and Ubuntu 20.04 Linux distributions.

submodules
==========

Submodules packaged in __libsxe__ include:

 * lib-ev
 * lib-lookup3
 * lib-md5
 * lib-mock
 * lib-murmurhash3
 * lib-port
 * lib-sha1
 * lib-sxe
 * lib-sxe-buffer
 * lib-sxe-cdb
 * lib-sxe-cstr
 * lib-sxe-dict
 * lib-sxe-dirwatch
 * lib-sxe-hash
 * lib-sxe-http
 * lib-sxe-httpd
 * lib-sxe-jitson
 * lib-sxe-list
 * lib-sxe-log
 * lib-sxe-mmap
 * lib-sxe-pool
 * lib-sxe-pool-tcp
 * lib-sxe-ring-buffer
 * lib-sxe-socket
 * lib-sxe-spawn
 * lib-sxe-sync-ev
 * lib-sxe-test
 * lib-sxe-thread
 * lib-sxe-util
 * lib-tap (only included if MAK_VERSION = 1)

build
=====

To build libsxe, you need to create a project that includes the mak and libsxe repositories as subprojects. See the sxe project,
which already does this.

To build with the libtap (test anything protocol) library installed from a debian package:
 # Run: sudo ./dev-setup.sh
 # Run: make release test

To build with the libtap library embedded in libsxe:
 # Run: sudo ./dev-setup.sh
 # Run: make release test MAK_VERSION=1

In either case, the resulting libsxe.a, libsxe.so and include files will be found in the build-linux-64-release directory.
