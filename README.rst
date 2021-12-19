=========
libhinoko
=========

2020/12/12
Takashi Sakamoto

Instruction
===========

This is a sister project of libhinawa.

- https://github.com/alsa-project/libhinawa

I design this library for userspace applications to transfer/receive isochronous packets on
IEEE 1394 bus by any language binding of GObject Introspection. The applications is able to
operate OHCI 1394 controllers for any isochronous context and isochronous resources. According
to this design, this library is an application of Linux FireWire subsystem and GLib/GObject.

Example of Python 3 with PyGobject
==================================

See scripts under ``samples`` directory

Documentation
=============

- https://takaswie.github.io/libhinoko-docs/

License
=======

- GNU Lesser General Public License version 2.1 or later

Dependencies
============

- Glib 2.34.0 or later
- GObject Introspection 1.32.1 or later
- Linux kernel 3.4 or later

Requirements to build
=====================

- Meson 0.42.0 or later
- Ninja
- PyGObject (optional to run unit tests)
- GTK-Doc 1.18-2 (optional to generate API documentation)

How to build
============

::

    $ meson . build
    $ cd build
    $ ninja
    $ ninja install
    ($ ninja test)

When working with gobject-introspection, ``Hinoko-0.0.typelib`` should be installed in your system
girepository so that ``libgirepository`` can find it. Of course, your system LD should find ELF
shared object for libhinoko0. Before installing, it's good to check path of the above and configure
``--prefix`` meson option appropriately. The environment variables, ``GI_TYPELIB_PATH`` and
``LD_LIBRARY_PATH`` are available for ad-hoc settings of the above as well.

When disabling object-introspection support, use ``-Dgir=false`` for meson command.

How to refer document
=====================

::

    $ meson -Dgtk_doc=true . build
    $ cd build
    $ ninja
    $ ninja install

end
