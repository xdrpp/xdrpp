
#include <cassert>
#include "tests/xdrtest.hh"


int
main()
{

#if XDRPP_STRONG_ORDER
  fix_12_int f1, f2;
  std::int64_t fix_12_int::* second_field = &fix_12_int::h;
#else
  fix_12 f1, f2;
  double fix_12::* second_field = &fix_12::d;
#endif
  f1.i = 5;
  f1.*second_field = 0;
  f2 = f1;

  assert(f2 == f1);
  assert(!(f1 < f2));
  assert(!(f2 < f1));
  f2.*second_field = -1;
  assert(!(f1 == f2));
  assert(!(f1 < f2));
  assert(f2 < f1);
  f2.i++;
  assert(f1 < f2);
  assert(!(f2 < f1));

  testns::ContainsEnum ce1(REDDER);
  ce1.num() = testns::ContainsEnum::ONE;
  testns::ContainsEnum ce2 = ce1;

  assert(ce1 == ce2);
  ce2.num() = testns::ContainsEnum::TWO;
  assert(!(ce1 == ce2));
  assert(ce1 < ce2);
  assert(!(ce2 < ce1));
  ce1.c(RED);
  ce1.foo() = "hello world";
  assert(!(ce1 == ce2));
  assert(ce1 < ce2);
  assert(!(ce2 < ce1));

  testns::bytes b1, b2;
  assert(b1 == b2);
  //assert(testns::operator==(b1, b2));

  testns::hasbytes hb1, hb2;
  assert(hb1 == hb2);
  
  return 0;
}
