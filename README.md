# gcdvdfs

This is a fusefs port of my gcdvdfs filesystem kernel driver I wrote for the
[Gamecube Linux project](http://sourceforge.net/projects/gc-linux/). It allows you to mount uncompressed
Gamecube ISO files as a read-only filesystem. Should be feature complete and compatible with all Linux flavors.

## How do I build it?

Use [bazel](http://bazel.io)

## How do I use it?

gcdvdfs [options]

    -u, --uid                 uid of files
    -g, --gid                 gid of files
    -l, --logfile=file        debug logfile location
    -i, --iso=file            Gamecube ISO file location
    -m, --mount_point=file    mount point
    -h, --help                this help menu

## Future plans

I'd like to add support for compressed images, such as wbfs and gcz formats
