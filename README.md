# tpafd

## Introduction

Pathfinder is a light-weight service discovery system for embedded or
cloud use.

This repository contains `tpafd`, a footprint-optimized server-side
implementation of the [Pathfinder
protocol](https://github.com/Ericsson/paf/blob/master/doc/protocol/PROTOCOLv2.md)
version 2.

## Dependencies

tpafd is implemented in C, and is designed to have very few
dependencies.

* libjansson
* libxcm
* Automake
* libevent

## Installation

To build and install tpafd, run:

```
autoreconf -i && ./configure && make install
```

## Test Suites

The tpaf repository includes unit tests. To run these tests, issue:

```
make check
```

To run the test suites in valgrind, supply ``--enable-valgrind`` to
the configure script.

The unit tests have a limited scope, and primarily target low-level
data structures and other things that lends themselves to unit
testing.

To address domain logic correctness, tpafd relies on the test suite
found in the Python-based [Pathfinder
server](https://github.com/Ericsson/paf/).

To run the server-level tests, check out and change the current
working directory to the `paf` repository, and run:

``
make check TESTOPTS="--server tpaf"
``

## Documentation

The [Pathfinder application protocol
specification](https://github.com/Ericsson/paf/blob/master/doc/PROTOCOL.md),
in particular the data model section, includes a lot of useful
information on how a Pathfinder service discovery system works.

### Manual pages

* [tpafd](https://ericsson.github.io/tpaf/man/tpafd.8.html)
