
#include <iomanip>
#include <iostream>
#include <xdrpp/printer.h>
#include "tests/xdrtest.hh"

using namespace std;
using namespace xdr;

void
withshow()
{
  using xdr::operator<<;

  bool b = true;
  
  u_4_12 u(12);
  u.f12().i = 99;
  u.f12().d = 99.99;

  cout << boolalpha << b << " " << u << endl;

  testns::numerics n;

  cout << noboolalpha << n << endl;

  // Should print "REDDEST"
  cout << REDDEST << endl;
}

int
main()
{
  withshow();

  testns::uniontest ut;
  ut.ip.activate()++;
  ut.key.arbitrary(REDDEST);
  ut.key.big().resize(4);
  cout << xdr_to_string(ut) << endl;

  // Should print "2"
  cout << REDDEST << endl;

  return 0;
}
