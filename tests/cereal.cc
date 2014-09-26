
#include <cassert>
#include <iostream>
#include <sstream>
#include <xdrc/cereal.h>
#include <xdrc/printer.h>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include "xdrtest.hh"



int
main()
{
#if 0
  testns::bytes b1, b2;
  b1.s = "Hello world\n";
  b1.fixed.fill(0xc5);
  b1.variable = { 2, 4, 6, 8 };
  
  xdr::xdr_from_msg(xdr::xdr_to_msg(b1), b2);
  assert(b1.s == b2.s);
  assert(b1.fixed == b2.fixed);
  assert(b1.variable == b2.variable);

  testns::numerics n1, n2;
  n1.b = true;
  n2.b = false;
  n1.i1 = 0x7eeeeeee;
  n1.i2 = 0xffffffff;
  n1.i3 = UINT64_C(0x7ddddddddddddddd);
  n1.i4 = UINT64_C(0xfccccccccccccccc);
  n1.f1 = 3.141592654;
  n1.f2 = 2.71828182846;
  n1.e1 = testns::REDDER;
  n2.e1 = testns::REDDEST;
#endif

#if 0
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

  testns::numerics n3;
  xdr::msg_ptr m = xdr::xdr_to_msg(n2);
  cout << "got message\n";
  cout << xdr::xdr_to_string(xdr::xdr_from_msg(m, n3));

#if 0
  using x = xdr::opaque_array<5>;
  using namespace cereal::traits;
  cout << boolalpha;
  cout << has_non_member_save<x, cereal::BinaryOutputArchive>::value << endl;
  cout << has_non_member_load<x, cereal::BinaryInputArchive>::value << endl;
#endif
#endif

  return 0;
}
