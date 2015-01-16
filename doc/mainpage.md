xdrpp
=====

This package consists of an
[RFC4506](http://tools.ietf.org/html/rfc4506) XDR compiler, `xdrc`,
and a RPC library `libxdrpp` for C++11.

To use this library, you will first want to define a protocol in XDR
format, producing a "`.x`" file.  Then use the included XDR compiler,
`xdrc`, to translate it into C++ types.  See the
[xdrc manual page](md_doc_xdrc_81.html) to get started.
