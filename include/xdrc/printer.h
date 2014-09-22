// -*- C++ -*-

#ifndef _XDRC_PRINT_H_HEADER_INCLUDED_
#define _XDRC_PRINT_H_HEADER_INCLUDED_ 1

#include <sstream>
#include <xdrc/types.h>

namespace xdr {

std::string escape_string(const std::string &s);
std::string hexdump(const void *data, size_t len);

struct Printer {
  std::ostringstream buf_;
  int indent_{0};
  bool skipnl_{true};
  bool comma_{true};

  Printer() {}
  Printer(int indent) : indent_(indent) {}

  std::ostream &bol(const char *name = nullptr);
  void operator()(const char *field, const char *s) { bol(field) << s; }
  void operator()(const char *field, const std::string &s) { bol(field) << s; }

  template<typename T> void operator()(const char *field, const pointer<T> &t) {
    if (t)
      archive(*this, field, *t);
    else
      bol(field) << "NULL";
  }

  template<typename T> ENABLE_IF(xdr_traits<T>::is_class)
  operator()(const char *field, const T &t) {
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

  template<typename T> ENABLE_IF(xdr_traits<T>::is_container)
  operator()(const char *field, const T &t) {
    bool skipnl = !field;
    bol(field) << '[';
    if (skipnl)
      buf_ << ' ';
    comma_ = false;
    skipnl_ = skipnl;
    indent_ += 2;
    for (const auto &o : t)
      archive(*this, nullptr, o);
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

template<> struct archive_adapter<Printer> {
  template<std::uint32_t N>
  static void apply(Printer &p, const char *field, const xstring<N> &s) {
    p(field, escape_string(s));
  }
  template<std::uint32_t N>
  static void apply(Printer &p, const char *field, const opaque_array<N> &v) {
    p(field, hexdump(v.data(), v.size()));
  }
  template<std::uint32_t N>
  static void apply(Printer &p, const char *field, const opaque_vec<N> &v) {
    p(field, hexdump(v.data(), v.size()));
  }

  template<typename T> static ENABLE_IF(xdr_traits<T>::is_enum)
  apply(Printer &p, const char *field, T t) {
    if (const char *n = xdr_traits<T>::enum_name(t))
      p(field, n);
    else
      p(field, std::to_string(t));
  }
  template<typename T> static ENABLE_IF(xdr_traits<T>::is_numeric)
  apply(Printer &p, const char *field, T t) {
    p(field, std::to_string(t));
  };

  template<typename T> static ENABLE_IF(xdr_traits<T>::is_class
					|| xdr_traits<T>::is_container)
  apply(Printer &p, const char *field, const T &obj) { p(field, obj); }
};

template<typename T> std::string
xdr_to_string(const T &t, int indent = 0, const char *name = nullptr)
{
  Printer p;
  p(name, t);
  p.buf_ << std::endl;
  return p.buf_.str();
}

}

#endif // !_XDRC_PRINT_H_HEADER_INCLUDED_
