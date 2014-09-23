Storaged, a D-Bus service for storage management
================================================

Currently, storaged implements a D-Bus API for monitoring and
manipulating [LVM2](https://sourceware.org/lvm2/) volume groups, but there is room for more.

Storaged works well with UDisks, and tries not to duplicate its
functionality.  For example, you would create a new logical volume
with storaged and then make a filesystem on it with UDisks.

CAVEAT:
------
   **This is pre-release software.  Only install it in a system that
    you are ready to lose.  It is a good idea to make a virtual
    machine to play around with storaged.**

Getting started:
-------------------

### Building the source

After you grab the source by using git do the following:

    $ ./autogen.sh

You may need to install one or more of the following for autogen.sh/configure to be successful:

* Autotools, eg. autoconf, automake, libtool
* glib2-devel
* intltool
* gobject-introspection-devel
* gcc tools
* libgudev1-devel
* lvm2-devel
* polkit-devel
* libudisks2-devel

Afterward you should be able to do a

   $ make

### Running storaged
Simplest way to use is to do a 'make install' on the system you which to run the daemon on, however if you just want to hack on it without doing a full install you can do:

    $ sudo cp data/com.redhat.storaged.conf /etc/dbus-1/system-d/
    $ sudo cp data/com.redhat.lvm2.policy /usr/share/polkit-1/actions
    $ sudo src/storaged --resource-dir $PWD/src -d -r

### Running test case
Then you can run the test case locally or remotely

    $ sudo TEST_TARGET=abuse-my-build-computer make check
    $ sudo TEST_TARGET=root@host make check
