
enum color {
  RED,
  REDDER,
  REDDEST
};

struct simple {
  int field;
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
  int i32;
  int *ip;
  double d;
  int iv<4>;
};

union ContainsEnum switch (color c) {
 case RED:
   string foo<>;
 case REDDER:
   enum { ONE, TWO } num;
};

}
