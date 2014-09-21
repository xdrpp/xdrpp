// -*- C++ -*-

#ifndef _XDRC_PRINT_H_HEADER_INCLUDED_
#define _XDRC_PRINT_H_HEADER_INCLUDED_ 1

#include <string>
#include <utility>
#include <xdrc/types.h>

namespace xdr {

struct Printer {
  std::string buf_;
  int indent{0};

  template<typename T> void operator()(const T&) {
  }
};

template<> struct prepare_field<Printer> {
  template<typename T> static inline std::pair<const char *, const T&> &&
    prepare(const char *name, const T &t) { return { name, t }; }
};

}

#endif // !_XDRC_PRINT_H_HEADER_INCLUDED_
