#include <xdrpp/autocheck.h>
#include <xdrpp/printer.h>
#include "tests/xdrtest.hh"

using namespace std;
using namespace xdr;

int
main()
{

  bool b = autocheck::generator<bool>{}(100);
  cout << boolalpha << b << endl;

  autocheck::generator<test_recursive> g;
  for (size_t i = 0; i < 100; i++) {
    auto x = g(i);
    string name = "size " + to_string(i);
    cout << xdr_to_string(x, name.c_str());
  }
  
  return 0;
}
