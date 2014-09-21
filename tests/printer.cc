
#include <iomanip>
#include <iostream>
#include <xdrc/printer.h>
#include "xdrtest.hh"

using namespace std;
using namespace xdr;

int
main()
{
  bool b = true;
  xdr::Printer p;

  p("b", b);
  std::cout << p.buf_.str();

  using x_t = int32_t;
  
  std::cout << xdr_class<x_t>::value << endl;
  std::cout << xdr_enum<x_t>::value << endl;
  std::cout << xdr_container<x_t>::value << endl;
  std::cout << is_arithmetic<x_t>::value << endl;

  x_t x;
  p("x", x);

  return 0;
}
