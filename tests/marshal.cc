
#include <cassert>
#include <iostream>
#include <sstream>
#include <xdrc/cereal.h>
#include <xdrc/printer.h>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include "xdrtest.hh"

using namespace std;

template<typename T>
typename std::enable_if<!xdr::xdr_traits<T>::has_fixed_size, std::size_t>::type
psize(const char *name, const T &t)
{
  std::size_t r = xdr::xdr_traits<T>::serial_size(t);
  cout << name << ": " << r << " [variable]" << endl;
  return r;
}

template<typename T>
typename std::enable_if<xdr::xdr_traits<T>::has_fixed_size, std::size_t>::type
psize(const char *name, const T &t)
{
  std::size_t r = xdr::xdr_traits<T>::fixed_size;
  cout << name << ": " << xdr::xdr_traits<T>::fixed_size << " [fixed]" << endl;
  assert(xdr::xdr_traits<T>::fixed_size == xdr::xdr_traits<T>::serial_size(t));
  return r;
}

#define PSIZE(T) psize(#T,T)

void
test_size()
{
  PSIZE(int32_t());
  PSIZE(fix_4());
  PSIZE(fix_12());
  PSIZE(xdr::opaque_array<5>());
  PSIZE(u_4_12(4));
  PSIZE(u_4_12(12));

  bool ok = false;
  try { PSIZE(u_4_12(0)); }
  catch (const xdr::xdr_bad_discriminant &) { ok = true; }
  assert(ok);

  v12 v;
  psize("v12 empty", v);
  v.emplace_back();
  psize("v12 * 1", v);
  v.emplace_back();
  psize("v12 * 2", v);

  psize("xdtring 0", xdr::xstring<>());
  psize("xstring 3", xdr::xstring<>("123"));
}

int
main()
{
  test_size();
  return 0;

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
