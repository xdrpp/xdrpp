#include "tests/xdrtest-threshold.hh"

#include <cassert>
#include <stdexcept>
#include <utility>
#include <xdrpp/marshal.h>

void
test_throwing_arm(int bad_tag)
{
  throwing_union value(1);
  value.good() = 42;
  throwing_arm_state::throw_now = true;
  try {
    value.which(bad_tag);
    assert(false);
  }
  catch (const std::runtime_error &) {}
  assert(xdr::unionfn::valueless_by_exception(value));
  try {
    xdr::unionfn::check_value(value);
    assert(false);
  }
  catch (const xdr::xdr_wrong_union &) {}

  throwing_union valueless_copy(value);
  assert(xdr::unionfn::valueless_by_exception(valueless_copy));

  throwing_arm_state::throw_now = false;
  valueless_copy.which(1);
  assert(!xdr::unionfn::valueless_by_exception(valueless_copy));
  value.which(1);
  assert(!xdr::unionfn::valueless_by_exception(value));
  value.good() = 7;

  throwing_union source(bad_tag);
  throwing_arm_state::throw_now = true;
  try {
    value = source;
    assert(false);
  }
  catch (const std::runtime_error &) {}
  assert(xdr::unionfn::valueless_by_exception(value));
  throwing_arm_state::throw_now = false;
}

int
main()
{
  test_throwing_arm(2);
  test_throwing_arm(3);

  using throwing_meta = throwing_union::_xdr_union_meta;
  static_assert(!throwing_meta::arm_layout<2>().is_indirect);
  static_assert(throwing_meta::arm_layout<3>().is_indirect);

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
  assert(!xdr::unionfn::valueless_by_exception(original));
  original.f4().i = 42;
  u_4_12 copy = original;
  assert(copy.f4().i == 42);

  copy.which(12);
  assert(!xdr::unionfn::valueless_by_exception(copy));
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