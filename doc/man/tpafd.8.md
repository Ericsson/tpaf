tpafd(8) -- service discovery server
====================================

## SYNOPSIS

`tpafd` [<options>...] [<domain-addr>...]

## DESCRIPTION

tpafd is a server for use in a Pathfinder light-weight service
discovery system.

tpafd is designed to be used as a daemon (i.e., a UNIX server process),
but does not daemonize itself (e.g., it does not call fork() and runs
in the background).

## ARGUMENTS

tpafd must be supplied one or more arguments, where each argument is a
server socket address (in XCM format).

For each address, the server will instantiate a service discovery
domain, and bind to the socket. Thus, a server process may be used to
serve more than one service discovery domain.

## OPTIONS

 * `-s`
   Enable logging to console (standard error). Console logging is
   disabled by default.

 * `-y <facility>`
   Set syslog facility to use. For example, `-y local0` will make pafd
   set the facility to `LOG_LOCAL0` on all log messages.

 * `-n`
   Disable logging to syslog. Syslog logging is enabled by default.

 * `-l <level>`
   Discard log messages with a severity level below <level>. For example,
   `-l notice` will filter out any log message a level lower than
   `LOG_NOTICE` (i.e., `LOG_INFO` and `LOG_DEBUG`).

 * `-v`
   Display tpafd version information and exit.

 * `-h`
   Display tpafd usage information and exit.

Options override any configuration set by a configuration file.

## EXAMPLES

The below example spawns one server process with two service discovery
domains; one on a UNIX domain socket "foo", and one answering on TCP
port 4711:

    $ tpafd ux:foo tcp:*:4711

This example spawns a server process, instantiates one domain, and
enables console logging and disables log filtering:

    $ tpafd -s -l debug ux:fo
    tpafd[126513]: tpafd version 0.0.1 started.
    tpafd[126513]: Configured domain bound to "ux:foo".
    tpafd[126513]: Started serving domain.

## COPYRIGHT

**Pathfinder** is Copyright (c) 2020-2023, Ericsson AB, and released
under the BSD 3-Clause Revised License.
