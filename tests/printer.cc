
#include <iomanip>
#include <iostream>
#include <xdrc/printer.h>
#include "xdrtest.hh"

using namespace std;
using namespace xdr;

int
main()
{
  xdr::Printer p;
#if 0
  bool b = true;
  p("b", b);
  p("x", 5);
#endif
  testns::numerics n;
  n.ip.reset(new int (5));
  n.nester.resize(2);
  n.nester[0].juju = "hi";
  n.nester[1].juju = "there";
  //p(nullptr, n);
  std::cout << xdr_to_string(n);

  return 0;
}
