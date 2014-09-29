
struct test_recursive {
  string elem<>;
  test_recursive *next;
};

struct fix_4 {
  int i;
};

struct fix_12 {
  int i;
  double d;
};

union u_4_12 switch (int which) {
 case 4:
   fix_4 f4;
 case 12:
   fix_12 f12;
};

typedef fix_12 v12<>;

enum color {
  RED,
  REDDER,
  REDDEST
};

namespace testns {

enum other_color {
  RED,
  REDDER,
  REDDEST
};

struct bytes {
  string s<16>;
  opaque fixed[16];
  opaque variable<16>;
};

struct numerics {
  bool b;
  int i1;
  unsigned i2;
  hyper i3;
  unsigned hyper i4;
  float f1;
  double f2;
  other_color e1;
};
  
struct uniontest {
  int *ip;
  union switch (color arbitrary) {
  case ::REDDEST:
    opaque big<>;
  case ::RED:
    hyper medium;
  } key;
};

typedef string bigstr<>;

struct containertest {
  u_4_12 uvec<>;
  bigstr sarr[2];
};

struct containertest1 {
  u_4_12 uvec<2>;
  bigstr sarr[2];
};

union ContainsEnum switch (color c) {
 case RED:
   string foo<>;
 case REDDER:
   enum { ONE, TWO } num;
};

program xdrtest_prog {
  version xdrtest {
    void null(void) = 1;
    ContainsEnum nonnull(u_4_12) = 2;
  } = 1;
  version xdrtest2 {
    void null2(void) = 1;
    ContainsEnum nonnull2(u_4_12) = 2;
    void ut(uniontest) = 3;
  } = 2;
} = 0x20000000;

program other_prog {
  version opv1 {
    void o_null(void) = 1;
  } = 1;
}= 0x20000001;

}
