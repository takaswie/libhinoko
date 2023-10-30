=====================
The libhinoko project
=====================

2023/10/29
Takashi Sakamoto

Introduction
============

This is a sister project of `libhinawa <https://git.kernel.org/pub/scm/libs/ieee1394/libhinawa.git/>`_.

I design this library for userspace applications to transfer/receive isochronous packets on
IEEE 1394 bus by any language binding of GObject Introspection. The applications is able to
operate 1394 OHCI hardware for any isochronous context and isochronous resources. According
to this design, this library is an application of Linux FireWire subsystem and GLib/GObject.

The latest release is `0.9.0 <https://git.kernel.org/pub/scm/libs/ieee1394/libhinoko.git/tag/?h=v0.9.0>`_

Example of Python 3 with PyGobject
==================================

See scripts under ``samples`` directory

Documentation
=============

- `<https://alsa-project.github.io/gobject-introspection-docs/hinoko/>`_

License
=======

- GNU Lesser General Public License version 2.1 or later

Repository location
===================

- Upstream is `<https://git.kernel.org/pub/scm/libs/ieee1394/libhinoko.git/>`_.
* Mirror at `<https://github.com/takaswie/libhinoko>`_ for user support and continuous
  integration.

Dependencies
============

- Glib 2.44.0 or later
- GObject Introspection 1.32.1 or later
- Libhinawa 4.0.0 or later
- Linux kernel 3.4 or later

Requirements to build
=====================

- Meson 0.56.0 or later
- Ninja
- PyGObject (optional to run unit tests)
- gi-docgen 2023.1 or later (optional to generate API documentation)

How to build
============

::

    $ meson setup (--prefix=directory-to-install) build
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

* `PyGObject <https://pygobject.readthedocs.io/>`_ is a dynamic loader in Python 3 language for
  libraries compatible with g-i.
* `hinoko-rs <https://git.kernel.org/pub/scm/libs/ieee1394/hinoko-rs.git/>`_ includes crates to
  use these libraries.

Meson subproject
================

This is a sample of wrap file to satisfy dependency on libhinoko by
`Meson subprojects <https://mesonbuild.com/Subprojects.html>`_.

::

    $ cat subprojects/hinoko.wrap
    [wrap-git]
    directory = hinoko
    url = https://git.kernel.org/pub/scm/libs/ieee1394/libhinoko.git
    revision = v0.9.1
    depth = 1
    
    [provide]
    dependency_names = hinoko

After installation of the wrap file, the dependency can be solved by ``hinoko`` name since it is
common in both pkg-config and the wrap file. The implicit or explicit fallback to subproject is
available.

::

    $ cat meson.build
    hinoko_dependency = dependency('hinoko',
      version: '>=0.9.1'
    )

In the case of subproject, the wrap file for ``hinawa`` should be installed as well, since
``hinoko`` depends on it. For ``hinawa.wrap``, please refer to README of
[libhinawa](https://git.kernel.org/pub/scm/libs/ieee1394/libhinawa.git/).

Loss of backward compatibility between v0.8/v0.9 releases
=========================================================

At v0.9, the library newly depends on
`libhinawa <https://git.kernel.org/pub/scm/libs/ieee1394/libhinawa.git/>`_ to use
`Hinawa.CycleTime <https://alsa-project.github.io/gobject-introspection-docs/hinawa/struct.CycleTime.html>`_
for
`Hinoko.FwIsoCtx.read_cycle_time() <https://alsa-project.github.io/gobject-introspection-docs/hinoko/method.FwIsoCtx.read_cycle_time.html>`_
abstract method. The previous implementation, ``Hinoko.CycleTimer`` and
``Hinoko.FwIsoCtx.get_cycle_timer()``, is unused anymore and dropped.

Loss of backward compatibility between v0.7/v0.8 releases
=========================================================

At v0.8, some main object classes are renamed so that their names are straightforward to express
corresponding isochronous contexts in 1394 OHCI.

- ``Hinoko.FwIsoIrSingle`` from ``Hinoko.FwIsoRxSingle`` for IR context of packet-per-buffer mode
- ``Hinoko.FwIsoIrMultiple`` from ``Hinoko.FwIsoRxMultiple`` for IR context of buffer-fill mode
- ``Hinoko.FwIsoIt`` from ``Hinoko.FwIsoTx`` for IT context

The enumrations to express the mode of context are renamed as well:

- ``Hinoko.FwIsoCtxMode.IR_SINGLE`` from ``Hinoko.FwIsoCtxMode.RX_SINGLE``
- ``Hinoko.FwIsoCtxMode.IR_MULTIPLE`` from ``Hinoko.FwIsoCtxMode.RX_MULTIPLE``
- ``Hinoko.FwIsoCtxMode.IT`` from ``Hinoko.FwIsoCtxMode.TX``


The symbols for previous names are not public anymore.

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

About Hinoko
============

``Hinoko`` is Japanese word which expresses quite a small piece of fire scattered from burning
flame continuously. ``Hi`` (U+2F55 |kanji-hi|) and ``Ko`` (U+7C89 |kanji-ko|) are connected by
`No` (U+306E |hiragana-no|) is case markers in Japanese particles. The former means ``fire``.
The latter means ``flour``.

.. |kanji-hi| unicode:: &#x2f55 .. Hi spelled in Kanji
.. |kanji-ko| unicode:: &#7c89 .. Ko spelled in Kanji
.. |hiragana-no| unicode:: &#x306e .. No spelled in Hiragana

We can see ``Hinoko`` flying from burning fire consecutively, like a stream of isochronous packet
in IEEE 1394 bus.

end
