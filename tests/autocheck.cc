#include <xdrpp/autocheck.h>
#include <xdrpp/printer.h>
#include "tests/xdrtest.hh"

using namespace std;
using namespace xdr;

int
main()
{

#if 0
  {
    autocheck::generator<int, void> g;
    cout << g(5) << endl;
  }

  for (size_t i = 0; i < 20; i++) {
    auto x = autocheck::generator<u_4_12>{}(i);
    string name = "size " + to_string(i);
    cout << xdr_to_string(x, name.c_str());
  }
#endif
  
  return 0;
}
