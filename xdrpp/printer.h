// -*- C++ -*-

//! \file printer.h Support for pretty-printing XDR data types.
//! Function xdr::xdr_to_string converts an arbitrary XDR data type to
//! a string.  In addition, if you say <tt>using
//! xdr::operator<<;</tt>, you can use the standard \c operator<< to
//! print XDR types.
//!
//! You can customize how a particular user-defined type \c T gets
//! printed by defining a function <tt>std::string xdr_printer(const
//! T&)</tt> in the same namespace as <tt>T</tt>.

#ifndef _XDRPP_PRINT_H_HEADER_INCLUDED_
#define _XDRPP_PRINT_H_HEADER_INCLUDED_ 1

#include <sstream>
#include <xdrpp/types.h>

namespace xdr {

//! Use hex escapes for non-printable characters, and prefix
//! backslashes and quotes with backslash.
std::string escape_string(const std::string &s);
//! Turn a string into a double-length sequence of hex nibbles.
std::string hexdump(const void *data, size_t len);

namespace detail {

struct Printer {
  std::ostringstream buf_;
  int indent_{0};
  bool skipnl_{true};
  bool comma_{true};

  Printer() {}
  Printer(int indent) : indent_(indent) {}

  std::ostream &bol(const char *name = nullptr);
  void p(const char *field, const char *s) { bol(field) << s; }
  void p(const char *field, const std::string &s) { bol(field) << s; }

  void operator()(const char *field, xdr_void) { bol(field) << "void"; }

  template<std::uint32_t N> void
  operator()(const char *field, const xstring<N> &s) {
    p(field, escape_string(s));
  }
  template<std::uint32_t N> void
  operator()(const char *field, const opaque_array<N> &v) {
    p(field, hexdump(v.data(), v.size()));
  }
  template<std::uint32_t N>
  void operator()(const char *field, const opaque_vec<N> &v) {
    p(field, hexdump(v.data(), v.size()));
  }

  template<xdr_enum T>
  void operator()(const char *field, T t) {
    if (const char *n = xdr_traits<T>::enum_name(t))
      p(field, n);
    else
      p(field, std::to_string(t));
  }

  void operator()(const char *field, xdr_numeric auto t) {
    p(field, std::to_string(t));
  }

  // Don't print 1-tuple as tuple (even Haskell doesn't have 1-tuples).
  template<typename T> void
  operator()(const char *field, const std::tuple<T> &t) {
    archive(*this, get<0>(t), field);
  }

  template<typename T> void operator()(const char *field, const pointer<T> &t) {
    if (t)
      archive(*this, *t, field);
    else
      bol(field) << "NULL";
  }

  template<xdr_class T>
  void operator()(const char *field, const T &t) {
    bool skipnl = !field;
    bol(field) << "{";
    if (skipnl)
      buf_ << ' ';
    comma_ = false;
    skipnl_ = skipnl;
    indent_ += 2;
    xdr_traits<T>::save(*this, t);
    if (skipnl) {
      buf_ << " }";
      indent_ -= 2;
    }
    else {
      comma_ = false;
      indent_ -= 2;
      bol() << "}";
    }
  }

  template<xdr_container T>
  void operator()(const char *field, const T &t) {
    bool skipnl = !field;
    bol(field) << '[';
    if (skipnl)
      buf_ << ' ';
    comma_ = false;
    skipnl_ = skipnl;
    indent_ += 2;
    for (const auto &o : t)
      archive(*this, o);
    if (skipnl) {
      buf_ << " ]";
      indent_ -= 2;
    }
    else {
      comma_ = false;
      indent_ -= 2;
      bol() << "]";
    }
  }
};

template<typename T> concept has_xdr_printer =
  requires(const T t) { xdr_printer(t); };

} // namespace detail

template<> struct archive_adapter<detail::Printer> {
  using Printer = detail::Printer;

  template<typename T> requires (!detail::has_xdr_printer<T>)
  static void apply(Printer &p, const T &obj, const char *field) {
    p(field, obj);
  }

  template<detail::has_xdr_printer T>
  static void apply(Printer &p, const T &obj, const char *field) {
    p.p(field, xdr_printer(obj));
  }
};

//! Return a std::string containing a pretty-printed version an XDR
//! data type.  The string will contain multiple lines and end with a
//! newline.  \arg name if non-NULL, the string begins with the name
//! and an equals sign.  \arg indent specifies a non-zero minimum
//! indentation.
template<typename T> std::string
xdr_to_string(const T &t, const char *name = nullptr, int indent = 0)
{
  detail::Printer p(indent);
  archive(p, t, name);
  p.buf_ << std::endl;
  return p.buf_.str();
}

//! Print an arbitrary XDR structure to a \c std::ostream.  To use
//! this function, you will have to say <tt>using xdr::operator<<</tt>
//! within the namespace of your XDR file.  As per the C++ standard, a
//! using \e directive (i.e., <tt>using namespace xdr</tt>) will not
//! allow argument-dependent lookup.
template<xdr_type T> inline std::ostream &
operator<<(std::ostream &os, const T &t)
{
  return os << xdr_to_string(t);
}

} // namespace xdr

#endif // !_XDRPP_PRINT_H_HEADER_INCLUDED_
