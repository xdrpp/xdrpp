
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
  int iv[4];
  opaque hash[20];
  opaque longone<>;
};

union ContainsEnum switch (color c) {
 case RED:
   string foo<>;
 case REDDER:
   enum { ONE, TWO } num;
};

