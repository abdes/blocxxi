.. Structure conventions
     # with overline, for parts
     * with overline, for chapters
     = for sections
     - for subsections
     ^ for sub-subsections
     " for paragraphs

############################################################
Blocxxi reusable blockchain platform and Kademlia stack in C++
############################################################

.. |date| date::

Last Updated on |date|

.. toctree::
   :maxdepth: 2
   :titlesonly:
   :hidden:

   tools/index
   platform
   platform-core
   platform-chain
   platform-storage
   platform-node
   platform-bitcoin
   license
   changelog.md
   version

Welcome! This is the documentation for the
`blocxxi <https://github.com/abdes/blocxxi>`_ |version| project.

This project uses the `asap <https://github.com/abdes/asap>`_ integrated build
system and follows its development workflow.

Parts of the documentation
==========================

:doc:`codec <codec:api>`
------------------------
  *start here to understand the codec utility module APIs.*

:doc:`crypto <crypto:api>`
--------------------------
  *start here to understand the crypto utility module APIs.*

:doc:`nat <nat:api>`
--------------------
  *start here to understand the NAT and port mapping utility module APIs.*

:doc:`platform module guide <platform>`
--------------------------------------
  *start here to understand the new reusable blockchain platform surface: Core,
  Chain, Storage, Node, Bitcoin, and the platform-level examples.*

:doc:`p2p <p2p:api>`
--------------------
  *start here to understand Blocxxi's optional Kademlia based discovery and
  transport subsystem.*

:doc:`core <platform-core>`
--------------------------
  *developer-facing primitives, events, and plugin hooks for third-party
  blockchain applications.*

:doc:`chain <platform-chain>`
----------------------------
  *kernel contracts and local block-acceptance orchestration that sit above
  the core module and below runtime composition.*

:doc:`storage <platform-storage>`
--------------------------------
  *storage ports and in-memory reference adapters for the local-chain proof.*

:doc:`node <platform-node>`
--------------------------
  *the simple local node facade that lets external apps bootstrap a chain
  without importing DHT- or storage-specific internals.*

:doc:`bitcoin roadmap <platform-bitcoin>`
----------------------------------------
  *the bounded Bitcoin adapter plan: header/SPV-first interop later, without
  reshaping the milestone-1 kernel.*

`Developer Guides <https://abdes.github.io/asap/asap_master/html/project-development/index.html>`_
--------------------------------------------------------------------------------------------------
  *head over to the `asap` project documentation for detailed guides on how to
  build and maintain the different targets in this project.*

:doc:`Project Tools <tools/index>`
----------------------------------
*get an introduction to the programs and scripts under the `tools` folder,
specifically made for the `asap` project. These can simplify recurring tasks and
provide additional insights into the project, and sometimes examples of how to
use the project artifacts.*

Acknowledgements
================

.. figure:: https://executablebooks.org/en/latest/_static/logo-wide.svg
  :figclass: margin
  :alt: Executable Books Project
  :name: executable_book_logo

This documentation uses the theme provided by the `Executable Books Project
<https://executablebooks.org/>`_ Project.
