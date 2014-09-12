% xdrc(1)
% David Mazieres
% 

# NAME

xdrc - RFC4506 XDR compiler

# SYNOPSIS

xdrc -hh [-o _outfile_.hh] [-DMACRO=val...] __input__.x

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
  value, and with a value sets the discriminant (also activating the
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

        std::cout << mt.one() << std::endl; // throws xrd::wrong_union

* XDR `bool` is translated into a C++ `bool` (with XDR's `TRUE`
  translated to `true` and `FALSE` to `false`).

* XDR [unsigned] `int` and `hyper` types are translated into the
  cstdint types `std::int32_t`, `std::uint32_t`, `std::int64_t`, and
  `std::uint64_t`.  As per RFC4506, no types narrower than 32 bits
  exist.

* XDR enums are translated into simple (non-class) unions with
  underlying type `std::uint32_t`.

* XDR pointers (`*`) are translated into C++ `std::unique_ptr`.

* XDR fixed-length arrays are translated into C++ `std::array`.

* XDR variable-length arrays (`type field<N>`) are translated into C++
  `xdr::xvector<T,N>`.  `xvector` is a subtype of `std::vector<T>`,
  where `N` represents the maximum size.  Static method `max_size()`
  returns the maximum size.

* XDR opaque is translated into C++ `std::uint8_t`, but as per
  RFC4506, opaque may only appear as part of a fixed- or
  variable-length array declaration.

* XDR `string<N>` is translated into `xdr::xstring<N>`, a subtype of
  string encoding the maximum size.  Static method `max_size()`
  returns the maximum size.

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

Each XDR data type has two extra methods:

    template<class _Archive> void save(_Archive &_archive) const;
    template<class _Archive> void load(_Archive &_archive);

These methods use `_archive` as a function object and call it on every
field in the data structure.  Hence, the type `_Archive` can have an
overloaded `operator()` that does different things for different
types.  To implement an archive, you will need to support the
following types:

> * `uint8_t` (only for containers), `bool`, `std::int32_t`,
  `std::uint32_t`, `std::int64_t`, `std::uint64_t`, `float`, `double`,
  `xdr::xstring`

> * The `std::array` and `xdr:xvector` containers of the above types.

> * Any field types that are themselves XDR structures with `save` and
>   `load` methods.

For debugging purposes and formats (such as JSON) that need access to
field names, it is also possible for the `_Archive` type to receive
the field names of fields that are traversed.  The following template
(in the `xdr::` namespace) can be specialized to prepare arguments by
bundling them with their names:

    template<typename Archive> struct prepare_field {
      template<typename T> static inline T&&nvp(const char *, T &&t) {
        return std::forward<T>(t);
      }
    };

# OPTIONS

\-hh
:   Selects C++ header file output.  This is currently the only output
    format, and hence is mandatory as an option.

\-o _outfile_
:   Specifies the output file into which to write the generated code.
    The default is to replace `.x` with `.hh` at the end of the input
    file name.  The special _outfile_ `-` sends output to standard
    output.

\-DMACRO=val
:   The input file is run through the C preprocessor, and this option
    adds additional defines.  (Note that the symbol `XDRC_CC` is
    always defined to 1, if you wish to test for xdrc vs. other RPC
    compilers.)

<!-- # EXAMPLES -->

# FILES

PREFIX/include/xdrc/types.h
:   Types used in generated C++ code.

PREFIX/include/xdrc/cereal.h
:   Integration with the [cereal](http://uscilab.github.io/cereal/)
    serialization library.

# SEE ALSO

<http://tools.ietf.org/html/rfc4506>
<http://tools.ietf.org/html/rfc5531>

# BUGS

`xdrc` does not currently support `program` definitions.

Certain names that are legal in XDR cannot be used as type or field
names.  For example, C++ keywords are not allowed (`namespace`,
`template`, etc.), while cereal reserves `archive`, `load`, and
`save`.  In addition, `xdrc` uses much larger number of names
beginning with underscores (often derived from field names).  Hence
you should avoid starting your field names with underscore.

`xdrc` translates XDR `quadruple` to C++ `quadruple`, but most
compilers do not have such a type.
