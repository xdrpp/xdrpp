
enum color {
  RED,
  REDDER,
  REDDEST
};

struct numerics {
  int i32;
  unsigned u32;
  hyper i64;
  unsigned hyper u64;
  float f;
  double d;
};

union ContainsEnum switch (color c) {
 case RED:
   string foo<>;
 case REDDER:
   enum { ONE, TWO } num;
};


typedef string foo;
typedef string bar;
