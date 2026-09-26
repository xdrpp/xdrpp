#include "tests/xdrtest-threshold.hh"

#include <cassert>
#include <utility>
#include <xdrpp/marshal.h>

int
main()
{
  using meta = u_4_12::_xdr_union_meta;
  static_assert(meta::inline_threshold == 8);
  static_assert(meta::arm_layout<1>().size == sizeof(fix_4));
  static_assert(!meta::arm_layout<1>().is_indirect);
  static_assert(meta::arm_layout<2>().size == sizeof(fix_12));
  static_assert(meta::arm_layout<2>().is_indirect);
  using boundary_meta = uunion::_xdr_union_meta;
  static_assert(boundary_meta::arm_layout<3>().size == 8);
  static_assert(!boundary_meta::arm_layout<3>().is_indirect);

  u_4_12 original(4);
  original.f4().i = 42;
  u_4_12 copy = original;
  assert(copy.f4().i == 42);

  copy.which(12);
  copy.f12().i = 7;
  copy.f12().d = 3.5;
  u_4_12 moved = std::move(copy);
  assert(moved.f12().i == 7);
  assert(moved.f12().d == 3.5);

  u_4_12 decoded;
  xdr::xdr_from_msg(xdr::xdr_to_msg(moved), decoded);
  assert(decoded == moved);
  return 0;
}