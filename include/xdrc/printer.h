// -*- C++ -*-

#ifndef _XDRC_PRINT_H_HEADER_INCLUDED_
#define _XDRC_PRINT_H_HEADER_INCLUDED_ 1

#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <xdrc/types.h>

namespace xdr {

inline std::string
escape_string(const std::string &s)
{
  std::ostringstream os;
  for (char c : s) {
    if (c < 0x20 || c >= 0x7f)
      os << '\\' << std::setw(3) << std::setfill('0')
	 << std::oct << (unsigned(c) & 0xff);
    else if (c == '"')
      os << "\\\"";
    else
      os << c;
  }
  return os.str();
}

struct Printer {
  std::ostringstream buf_;
  int indent_{0};

  Printer() { buf_ << std::boolalpha; }

  std::ostream &bol() {
    return buf_ << std::string(2*indent_, ' ');
  }

  void operator()(const char *field, bool b) {
    if (field)
      bol() << field << " = " << (b ? "TRUE" : "FALSE") << std::endl;
    else
      bol() << (b ? "TRUE" : "FALSE") << std::endl;
  }

  void operator()(const char *field, const std::string &s) {
    if (field)
      bol() << field << " = " << escape_string(s) << std::endl;
    else
      bol() << escape_string(s) << std::endl;
  }
  template<std::uint32_t N>
  void operator()(const char *field, const opaque_array<N> &a) {
    if (field)
      bol() << field << " = ";
    else
      bol();
    std::ostream os (buf_.rdbuf());
    os.fill('0');
    os.setf(std::ios::hex, std::ios::basefield);
    for (std::uint8_t c : a)
      os << std::setw(2) << (unsigned(c) & 0xff);
    os << std::endl;
  }
  template<std::uint32_t N>
  void operator()(const char *field, const opaque_vec<N> &a) {
    if (field)
      bol() << field << " = ";
    else
      bol();
    std::ostream os (buf_.rdbuf());
    os.fill('0');
    os.setf(std::ios::hex, std::ios::basefield);
    for (std::uint8_t c : a)
      os << std::setw(2) << c;
    os << std::endl;
  }

  template<typename T> typename
  std::enable_if<std::is_arithmetic<T>::value>::type
  operator()(const char *field, T t) {
    if (field)
      bol() << field << " = " << t << std::endl;
    else
      bol() << t << std::endl;
  }

  template<typename T> typename std::enable_if<xdr_enum<T>::value>::type
  operator()(const char *field, T t) {
    if (field)
      bol() << field << " = ";
    else
      bol();
    if (const char *n = xdr_enum<T>::name(t))
      buf_ << n << std::endl;
    else
      buf_ << t << std::endl;
  }

  template<typename T> typename std::enable_if<xdr_class<T>::value>::type
  operator()(const char *field, const T &t) {
    if (field)
      bol() << field << " = {" << std::endl;
    else
      bol() << "{" << std::endl;
    ++indent_;
    xdr_class<T>::save(*this, t);
    --indent_;
    bol() << "}" << std::endl;
  }

  template<typename T> void operator()(const char *field, const optional<T> &t)
  {
    if (!t) {
      if (field)
	bol() << field << " = NULL" << std::endl;
      else
	bol() << "NULL" << std::endl;
    }
    else
      (*this)(field, *t);
  }

  template<typename T> typename std::enable_if<!xdr_container<T>::pointer>::type
  operator()(const char *field, const T &t) {
    if (field)
      bol() << field << " = {" << std::endl;
    else
      bol() << "{" << std::endl;
    ++indent_;
    for (auto e = t.cbegin(); e != t.cend(); ++e) {
      (*this)(nullptr, *e);
    }
    --indent_;
    bol() << "}" << std::endl;
  }
};

}

#endif // !_XDRC_PRINT_H_HEADER_INCLUDED_
