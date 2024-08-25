# XV6 TESTS

This directory contains two folders.

1. host - which holds tests that should run
upon the Linux environment (or any other compatible
os that hosts the emulator running xv-6).
these tests are mainly unit tests designed to test different components
of the system without the need to depend on xv6 syscalls\internal mechanisms
or any additional os dependencies.

2. xv6 - these unit tests are focused on testing the behavior of core
programs under the xv6 constraints and environmental specifications.