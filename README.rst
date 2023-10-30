=====================
The libhinoko project
=====================

2023/10/30
Takashi Sakamoto

Introduction
============

This is a sister project of `libhinawa <https://git.kernel.org/pub/scm/libs/ieee1394/libhinawa.git/>`_.

I design this library for userspace applications to transfer/receive isochronous packets on
IEEE 1394 bus by any language binding of GObject Introspection. The applications is able to
operate 1394 OHCI hardware for any isochronous context and isochronous resources. According
to this design, this library is an application of Linux FireWire subsystem and GLib/GObject.

The latest release is `1.0.0 <https://git.kernel.org/pub/scm/libs/ieee1394/libhinoko.git/tag/?h=v1.0.0>`_

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

When working with gobject-introspection, ``Hinoko-1.0.typelib`` should be installed in your system
girepository so that ``libgirepository`` can find it. Of course, your system LD should find ELF
shared object for libhinoko1. Before installing, it's good to check path of the above and configure
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
    revision = v1.0.0
    depth = 1
    
    [provide]
    dependency_names = hinoko

After installation of the wrap file, the dependency can be solved by ``hinoko`` name since it is
common in both pkg-config and the wrap file. The implicit or explicit fallback to subproject is
available.

::

    $ cat meson.build
    hinoko_dependency = dependency('hinoko',
      version: '>=1.0.0'
    )

In the case of subproject, the wrap file for ``hinawa`` should be installed as well, since
``hinoko`` depends on it. For ``hinawa.wrap``, please refer to README of
[libhinawa](https://git.kernel.org/pub/scm/libs/ieee1394/libhinawa.git/).

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
