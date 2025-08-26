# Notes on symbol naming

This library is going to be used by third-party code, so there are a few
things we need to do to make sure it doesn't conflict with whatever that
code does.

## Contents of the public lpcsdr.h header

`lpcsdr.h` is the public header that third-party code is going to include.
We need to make sure that nothing in that header is going to conflict with
things in the third-party code.

All names -- functions, type names, macro names, enum names, etc -- visible
in lpcsdr.h should start with `lpcsdr_` or `LPCSDR_`.

The only things that should be present in `lpcsdr.h` are things that are
necessary for external use of the library.

Internal typedefs, functions, etc should be declared in a separate header.
In the internal headers we can do whatever we want (except for the non-static-
function naming rule below) as the internal headers won't be included in
third-party code.

## Naming non-static functions

Every non-static function should start with `lpcsdr_`. This includes both
internal and external functions.

These functions will be visible as public symbols in the compiled library, so
we need to make sure that they cannot clash with whatever function names
third-party code uses.

For internal functions that are non-static, maybe we should follow a convention
like starting them with `lpcsdr__` (note two underscores) to distinguish them
from functions that are intended to be used externally.

For static functions, call them what you want, they will not turn into public
symbols. Generally, use static functions for anything internal that does not
need to be used from more than one `.c` source file (and isn't needed by
tests)

