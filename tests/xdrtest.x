
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

struct test_struct {
  int x<5>;
};


typedef fix_12 v12<>;


struct simple {
  int field;
};

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
  string description<>;
  opaque var_cookie<4>;
  opaque fix_cookie[4];
  int i32;
  int *ip;
  bool b;
  double d;
  int iv<4>;
  union switch (color arbitrary) {
  case ::REDDEST:
    opaque big<>;
  case ::RED:
    hyper medium;
  } key;
};

union ContainsEnum switch (color c) {
 case RED:
   string foo<>;
 case REDDER:
   enum { ONE, TWO } num;
};

}
