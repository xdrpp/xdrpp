% xdrc(1)
% David Mazieres
% 

# NAME

xdrc - RFC4506 XDR compiler for libxdrpp

# SYNOPSIS

xdrc {-hh|-serverhh|-servercc} [-o _outfile_] [-DMACRO=val...] _input_.x

# DESCRIPTION

`xdrc` compiles an RFC4506 XDR (external data representation) file
into a C++11 header file, creating a new C++ type for each type
defined in XDR.

## Native representations

`xdrc` uses the following representations for XDR types in C++:

* XDR structs are translated field-for-field into C++ structures.

* XDR unions are translated into a structure in which each field is
  actually a method that either returns a reference to the underlying
  value, or throws an exception if that field is not currently
  selected.  The discriminant method with no arguments returns the
  value, and with a value sets the discriminant (also constructing the
  appropriate union field).  For example, if the XDR file contains:

        union MyType switch (unsigned discriminant) {
        case 0:
          int zero;
        case 1:
          string one<>;
        default:
          void;
        }

    The C++ type can be accessed as follows:

        MyType mt;
        mt.discriminant(1);
        mt.one() = "hello";
        std::cout << mt.one() << std::endl;
        mt.discriminant(0);
        mt.zero() = 5;

        std::cout << mt.one() << std::endl; // throws xdr_wrong_union

* XDR `bool` is represented as a C++ `bool` (with XDR's `TRUE`
  translated to `true` and `FALSE` to `false`).

* XDR [unsigned] `int` and `hyper` types are represented as the
  cstdint types `std::int32_t`, `std::uint32_t`, `std::int64_t`, and
  `std::uint64_t`.  As per RFC4506, no types narrower than 32 bits
  exist.

* XDR enums are translated into simple (non-class) unions with
  underlying type `std::uint32_t`.

* XDR pointers (`*`) are translated into C++ `xdr:pointer`, a subtype
  of `std::unique_ptr`.  `xdr::pointer` adds an `activate()` method
  that allocates an object of the appropriate type (if the pointer is
  null) and returns a reference to the current object.

* XDR fixed-length arrays are translated into C++ `xdr::xarray`, a
  subtype of `std::array`.

* XDR variable-length arrays (`type field<N>`) are translated into C++
  `xdr::xvector<T,N>`, a subtype of `std::vector<T>`, where `N`
  represents the maximum size.  Static constexpr method `max_size()`
  returns the maximum size.

* XDR opaque is translated into C++ `std::uint8_t`, but as per
  RFC4506, opaque may only appear as part of a fixed- or
  variable-length array declaration.

* XDR `string<N>` is translated into `xdr::xstring<N>`, a subtype of
  string encoding the maximum size.  Static constexpr method
  `max_size()` returns the maximum size.

## Extensions to RFC4506

`xdrc` supports the following extensions to the syntax defined in
RFC4506:

* Portions of the input file may be bracketed by `namespace myns {`
  ... `}`.  The corresponding C++ will be embedded in the same
  namespace.

* Lines beginning with a `%` sign are copied verbatim into the output
  file.

* Type names may include a namespace scope `::`, so as to be able to
  make use of types defined in a different namespace.  (Alternatively,
  a literal can be used, such as `%using namespace otherns;`.)

## Serialization and traversing data structures

A template class `xdr::xdr_traits<T>` is used to hold metadata for
each native C++ type `T` representing an XDR type.  For XDR structs,
unions, arrays and pointers, this traits class contains two important
static methods:

    template<class Archive> void save(Archive &archive, const T &);
    template<class Archive> void load(Archive &archive, T &);

These methods use `archive` as a function object and call it on every
field in the data structure.  Hence, the type `Archive` can have an
overloaded `operator()` that does different things for different
types.  To implement an archive, you will need to support the
following types:

> * `bool`, `std::int32_t`, `std::uint32_t`, `std::int64_t`,
>   `std::uint64_t`, `float`, `double`, `xdr::xstring`,
>   `xdr::opaque_array` and `xdr::opaque_vec` (the latter two are not
>   considered containers, despite being implemented in terms of
>   `xarray` and `xvector`).

> * The `xdr::xarray`, `xdr:xvector`, and `xdr::pointer` containers of
>   the above types (or their supertypes `std::array`, `std::vector`,
>   and `std::unique_ptr`).

> * Any field types that are themselves XDR structures.

For debugging purposes and formats (such as JSON) that need access to
field names, it is also possible for the `Archive` type to receive the
field names of fields that are traversed.  The following template (in
the `xdr::` namespace) can be specialized to prepare arguments by
bundling them with their names:

    template<typename Archive> struct archive_adapter {
      template<typename T> static void
      apply(Archive &ar, T &&t, const char *) {
        ar(std::forward<T>(t));
      }
    };

# OPTIONS

\-hh
:   Selects C++ header file output.  This is the main output format,
    and its output is required for use with libxdrpp.

\-serverhh
:   Generates a C++ header file containing declarations of objects you
    can use to implement a server for each interface, using
    `rpc_tcp_listener`.  See the EXAMPLES section.

\-servercc
:   Generates a .cc file containing empty method definitions
    corresponding to the object files created with `-serverhh`.

\-o _outfile_
:  Specifies the output file into which to write the generated code.
    The default, for `-hh`, is to replace `.x` with `.hh` at the end
    of the input file name.  `-serverhh` and `-servercc` append
    `.server.hh` and `.server.cc`, respectively.  The special
    _outfile_ `-` sends output to standard output.

\-DMACRO=val
:   The input file is run through the C preprocessor, and this option
    adds additional defines.  (Note that the symbol `XDRC` is always
    defined to 1, if you wish to test for xdrc vs. other RPC
    compilers.)

# EXAMPLES

Consider the following XDR program definition in a file myprog.x:

    typedef string big_string<>;

    program MyProg {
      version MyProg1 {
        void null(void) = 0;
        big_string hello(int) = 1;
        big_string goodbye(big_string) = 2;
      } = 1;
    } = 0x2dee1645;

The `-serverhh` option will generate a header with the following
class:

    class MyProg1_server {
    public:
      using rpc_interface_type = MyProg1;

      void null();

      unique_ptr<big_string>
      hello(unique_ptr<int> arg);

      unique_ptr<big_string>
      goodbye(unique_ptr<big_string> arg);
    };

You have to add any fields you need to this structure, then
implement the three methods corresponding to the interface.  (Note
the very important type `rpc_interface_type` tells the library
which interface this object implements.)  Given such an object,
you can then implement a TCP RPC server (that registers its TCP
port with rpcbind) as follows:

    #include <xdrpp/server.h>
    #include "xdrpp/myprog.server.h"

    using namespace xdr;

    int
    main(int argc, char **argv)
    {
      MyProg1_server s;
      rpc_tcp_listener rl;
      rl.register_service(s);
      rl.run();
      return 1;
    }

To implement a simple client that talks to this server, you can use
code like the following:

    #include <iostream>
    #include <xdrpp/srpc.h>
    #include "myprog.h"

    using namespace std;
    using namespace xdr;

    int
    main(int argc, char **argv)
    {
      unique_fd fd = tcp_connect_rpc(argc > 2 ? argv[2] : nullptr,
                                     MyProg1::program, MyProg1::version);
      srpc_client<MyProg1> c{fd.get()};
      unique_ptr<big_string> result = c.hello(5);
      cout << "The result of hello(5) is " << *result << endl;
      return 0;
    }

## Compilation

The generated `xdrc` output files must be compiled with a compiler
supporting C++11 (or later, such as C++14) and expect to be used with
libxdrpp.  This typically requires extra compiler flags (e.g.,
`-std=c++11`).  You can use pkgconfig to find the location of the
headers and libraries.  With CXX set to both g++ and clang++, the
following Makefile variables work:

    CXXFLAGS = -std=c++11 `pkg-config --cflags xdrpp`
    LIBS = `pkg-config --libs xdrpp`

# FILES

PREFIX/include/xdrc/types.h
:   Types used in generated C++ code.

PREFIX/include/xdrc/cereal.h
:   Integration with the [cereal](http://uscilab.github.io/cereal/)
    serialization library.

# SEE ALSO

<http://tools.ietf.org/html/rfc4506>,
<http://tools.ietf.org/html/rfc5531>

# BUGS

Certain names that are legal in XDR cannot be used as type or field
names.  For example, C++ keywords are not allowed (`namespace`,
`template`, etc.).  In addition, `xdrc` uses a number of names
beginning with underscores (especially names beginning with prefix
`_xdr_`).  Hence you should avoid starting your field names with
underscore.  Union types use private fields that have the names of the
XDR fields with underscore appended.  Hence, in a union you cannot use
two field names one of which is the other with an underscore appended.

`xdrc` translates XDR `quadruple` to C++ type called `quadruple`, but
most compilers do not have such a type.  Moreover, libxdrpp does
nothing to support such a type.
