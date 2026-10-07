
#include <iomanip>
#include <iostream>
#include <xdrpp/marshal.h>
#include <xdrpp/printer.h>

// Forward declaration before including xdrtest.hh. This is necessary for GCC
// to pick up our local test validate function properly.
struct fix_4;
void validate(const fix_4& f4);
struct u_4_12;
void validate(const u_4_12& u);

#include "tests/xdrtest.hh"

using namespace std;
using namespace xdr;

namespace testns {
template<typename T> inline void xdr_validate_enum(T);
}

void
validate(const fix_4 &f4)
{
  if (f4.i == 0)
    throw xdr::xdr_invariant_failed("fix_4::i has value 0");
}

void
validate(const u_4_12 &u)
{
  if (u.which() == 12 && u.f12().i == 0)
    throw xdr::xdr_invariant_failed("u_4_12::f12.i has value 0");
}

int
main()
{
  fix_4 f4;
  opaque_vec<> v;

  f4.i = 1;
  v = xdr_to_opaque(f4);
  xdr_from_opaque(v, f4);

  opaque_vec<> ss(v);
  fix_4 ff4;
  xdr_from_opaque(ss, ff4);
  assert (f4 == ff4);

  f4.i = 0;
  v = xdr_to_opaque(f4);
  bool ok = false;
  try {
    xdr_from_opaque(v, f4);
  } catch (const xdr::xdr_invariant_failed &) {
    ok = true;
  }
  assert(ok);

  xstring<2> s;
  static_cast<string &>(s) = "1234";
  ok = false;
  try {
    validate(s);
  } catch (const xdr::xdr_overflow &) {
    ok = true;
  }
  assert (ok);

  v = xdr_to_opaque(uint32_t{99});
  color c;
  xdr_from_opaque(v, c);

  testns::other_color oc;
  ok = false;
  try {
    xdr_from_opaque(v, oc);
  } catch (const xdr::xdr_invariant_failed &) {
    ok = true;
  }
  assert(ok);

  u_4_12 invalid_union(12);
  invalid_union.f12().i = 0;
  v = xdr_to_opaque(invalid_union);
  ok = false;
  try {
    u_4_12 decoded;
    xdr_from_opaque(v, decoded);
  } catch (const xdr::xdr_invariant_failed &) {
    ok = true;
  }
  assert(ok);

  return 0;
}
