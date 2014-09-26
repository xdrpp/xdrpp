
#include <cassert>
#include <iostream>
#include <sstream>
#include <xdrc/cereal.h>
#include <xdrc/printer.h>
#include <xdrc/marshal.h>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include "xdrtest.hh"

using namespace std;

template<typename T>
typename std::enable_if<!xdr::xdr_traits<T>::has_fixed_size, std::size_t>::type
xdr_getsize(const T &t)
{
  return xdr::xdr_size(t);
}

template<typename T>
typename std::enable_if<xdr::xdr_traits<T>::has_fixed_size, std::size_t>::type
xdr_getsize(const T &t)
{
  assert(xdr::xdr_traits<T>::fixed_size == xdr::xdr_size(t));
  return xdr::xdr_traits<T>::fixed_size;
}

#define CHECK_SIZE(v, s)						\
do {									\
  size_t __s = xdr_getsize(v);						\
  if (__s != s) {							\
    cerr << #v << " has size " << __s << " shoudl have " << s << endl;	\
    terminate();							\
  }									\
} while (0)


void
test_size()
{
  CHECK_SIZE(int32_t(), 4);
  CHECK_SIZE(fix_4(), 4);
  CHECK_SIZE(fix_12(), 12);
  CHECK_SIZE(xdr::opaque_array<5>(), 8);
  CHECK_SIZE(u_4_12(4), 8);
  CHECK_SIZE(u_4_12(12), 16);

  {
    bool ok = false;
    try { CHECK_SIZE(u_4_12(0), 9999); }
    catch (const xdr::xdr_bad_discriminant &) { ok = true; }
    assert(ok);
  }

  v12 v;
  CHECK_SIZE(v, 4);
  v.emplace_back();
  CHECK_SIZE(v, 16);
  v.emplace_back();
  CHECK_SIZE(v, 28);

  CHECK_SIZE(xdr::xstring<>(), 4);
  CHECK_SIZE(xdr::xstring<>("123"), 8);
}

int
main()
{
  test_size();

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


#if 0
  using x = xdr::opaque_array<5>;
  using namespace cereal::traits;
  cout << boolalpha;
  cout << has_non_member_save<x, cereal::BinaryOutputArchive>::value << endl;
  cout << has_non_member_load<x, cereal::BinaryInputArchive>::value << endl;
#endif

  return 0;
}
