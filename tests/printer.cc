
#include <iomanip>
#include <iostream>
#include <xdrpp/printer.h>
#include "tests/xdrtest.hh"

using namespace std;
using namespace xdr;

int
main()
{
#if 0
  bool b = true;
  p("b", b);
  p("x", 5);
  testns::numerics n;
  n.ip.reset(new int (5));
  n.nester.resize(2);
  n.nester[0].juju = "hi";
  n.nester[1].juju = "there";
  std::cout << xdr_to_string(n);
#endif

  return 0;
}
