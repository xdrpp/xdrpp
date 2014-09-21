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

}

#endif // !_XDRC_PRINT_H_HEADER_INCLUDED_
