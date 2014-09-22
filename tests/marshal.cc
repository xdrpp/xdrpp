
#include <iostream>
#include <sstream>
#include <xdrc/cereal.h>
#include <xdrc/printer.h>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include "xdrtest.hh"

using namespace std;

struct blah {
  int i32;
  double d;
  template<typename A> void serialize(A &a) {
    a(CEREAL_NVP(i32), CEREAL_NVP(d));
  }
};

int
main()
{
  testns::bytes b, b2;
  b.s = "Hello world\n";
  b.fixed.fill(0xc5);
  b.variable = { 2, 4, 6, 8 };

  testns::numerics n1, n2;
  n1.i32 = 32;
  n1.d = 3.141592654;
  n1.description = "\tsome random text\n";
  n1.var_cookie = {1, 2, 3, 4};
  n1.fix_cookie.fill(0xc5);
  n1.ip.activate() = 999;
  n1.iv.resize(4);
  n1.iv[0] = 1;
  n1.iv[1] = 2;
  n1.iv[2] = 3;
  n1.iv[3] = 4;

  cout << xdr::xdr_to_string(n1);

  ostringstream obuf;
  {
    //cereal::BinaryOutputArchive archive(obuf);
    cereal::JSONOutputArchive archive(obuf);
    archive(n1);
  }

  cout << obuf.str() << endl;

  {
    istringstream ibuf(obuf.str());
    //cereal::BinaryInputArchive archive(ibuf);
    cereal::JSONInputArchive archive(ibuf);
    archive(n2);
  }



  cout << xdr::xdr_to_string(n2);

  cout << xdr::xdr_bytes<xdr::opaque_array<4>>::value << endl;


#if 0
  using x = xdr::opaque_array<5>;
  using namespace cereal::traits;
  cout << boolalpha;
  cout << has_non_member_save<x, cereal::BinaryOutputArchive>::value << endl;
  cout << has_non_member_load<x, cereal::BinaryInputArchive>::value << endl;
#endif

  return 0;
}
