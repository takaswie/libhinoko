=========
libhinoko
=========

2022/05/08
Takashi Sakamoto

Introduction
============

This is a sister project of libhinawa.

- https://github.com/alsa-project/libhinawa

I design this library for userspace applications to transfer/receive isochronous packets on
IEEE 1394 bus by any language binding of GObject Introspection. The applications is able to
operate OHCI 1394 controllers for any isochronous context and isochronous resources. According
to this design, this library is an application of Linux FireWire subsystem and GLib/GObject.

The latest release is `0.7.0 <https://github.com/takaswie/libhinoko/releases/tag/v0.7.0>`_.

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

- Glib 2.44.0 or later
- GObject Introspection 1.32.1 or later
- Linux kernel 3.4 or later

Requirements to build
=====================

- Meson 0.46.0 or later
- Ninja
- PyGObject (optional to run unit tests)
- gi-docgen (optional to generate API documentation)

How to build
============

::

    $ meson (--prefix=directory-to-install) build
    $ meson compile -C build
    $ meson install -C build
    ($ meson test -C build)

When working with gobject-introspection, ``Hinoko-0.0.typelib`` should be installed in your system
girepository so that ``libgirepository`` can find it. Of course, your system LD should find ELF
shared object for libhinoko0. Before installing, it's good to check path of the above and configure
``--prefix`` meson option appropriately. The environment variables, ``GI_TYPELIB_PATH`` and
``LD_LIBRARY_PATH`` are available for ad-hoc settings of the above as well.

How to refer document
=====================

::

    $ meson configure (--prefix=directory-to-install) -Ddoc=true build
    $ meson compile -C build
    $ meson install -C build

You can see documentation files under ``(directory-to-install)/share/doc/hinoko/``.

Supplemental information for language bindings
==============================================

* `PyGObject <https://pygobject.readthedocs.io/>`_ is a dynamic loader for libraries compatible
  with g-i.
* `hinoko-rs <https://github.com/takaswie/hinoko-rs>`_ includes creates to use these libraries.

Loss of backward compatibility between v0.6/v0.7 releases
=========================================================

At v0.6, internal inheritance was heavily used to share functions, signals and properties. At v0.7,
the inheritance is obsoleted by utilizing GObject Interface, therefore below base classes becomes
simple interface.

- ``Hinoko.FwIsoCtx``
- ``Hinoko.FwResource``

The former is implemented by below classes inherits GObject directly:

- ``Hinoko.FwIsoRxSingle``
- ``Hinoko.FwIsoRxMultiple``
- ``Hinoko.FwIsoTx``

The latter is implemented by below classes inherits GObject directly:

- ``Hinoko.FwIsoResourceAuto``
- ``Hinoko.FwIsoResourceOnce``

The ``Hinoko.FwIsoResourceOnce`` is newly added for allocation of isochronous resource bound
to current generation of bus topology, and some functions are available:

- ``Hinoko.FwIsoResourceOnce.deallocate_async``
- ``Hinoko.FwIsoResourceOnce.deallocate_sync``

These functions obsolete below functions. They are removed:

- ``Hinoko.FwIsoResource.allocate_once_async``
- ``Hinoko.FwIsoResource.allocate_once_sync``
- ``Hinoko.FwIsoResource.deallocate_once_async``
- ``Hinoko.FwIsoResource.deallocate_once_sync``

Below functions are removed as well:

- ``Hinoko.FwIsoRxSingle.stop``
- ``Hinoko.FwIsoRxSingle.unmap_buffer``
- ``Hinoko.FwIsoRxSingle.release``
- ``Hinoko.FwIsoRxMultiple.stop``
- ``Hinoko.FwIsoRxMultiple.unmap_buffer``
- ``Hinoko.FwIsoRxMultiple.release``
- ``Hinoko.FwIsoTx.stop``
- ``Hinoko.FwIsoTx.unmap_buffer``
- ``Hinoko.FwIsoTx.release``
- ``Hinoko.FwIsoResourceAuto.allocate_async``
- ``Hinoko.FwIsoResourceAuto.allocate_sync``

Alternatively, below functions are available:

- ``Hinoko.FwIsoCtx.stop``
- ``Hinoko.FwIsoCtx.unmap_buffer``
- ``Hinoko.FwIsoCtx.release``
- ``Hinoko.FwIsoResource.allocate_async``
- ``Hinoko.FwIsoResource.allocate_sync``

Furthermore, below puclic functions are changed to have an argument for the value of timeout to
wait for event:

- ``Hinoko.FwIsoResourceAuto.deallocate_sync``

Beside, below signal is newly added to express the value of current generation for the state of
IEEE 1394 bus:

- ``Hinoko.FwIsoResource::generation``

In GNOME convention, the throw function to report error at GError argument should return gboolean
value to report the overall operation finishes successfully or not. At v0.7, the most of public
API are rewritten according to it.

Loss of backward compatibility between v0.5/v0.6 releases
=========================================================

The status of project is under development. Below public functions have been changed since v0.6
release without backward compatibility:

- ``Hinoko.FwIsoTx.start()``
- ``Hinoko.FwIsoTx.register_packet()``
- ``Hinoko.FwIsoRxSingle.start()``

Furthermore hardware interrupt is not scheduled automatically in ``Hinoko.FwIsoTx`` and
``Hinoko.FwIsoRxSingle`` anymore. The runtime of v0.5 or before should be rewritten to schedule the
interrupt explicitly by calling ``Hinoko.FwIsoTx.register_packet()`` and
``Hinoko.FwIsoRxSingle.register_packet()`` if required. ``Hinawa.FwIsoCtx.flush_completions()``
allows applciation to process content of packet without scheduling hardware interrupt.

end
