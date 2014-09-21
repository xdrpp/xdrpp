
enum color {
  RED,
  REDDER,
  REDDEST
};

struct numerics {
  int i32;
  int *ip;
  double d;
  int iv[4];
  struct {
    string juju<>;
  } nester<>;
  opaque hash[20];
  opaque longone<>;
};

union ContainsEnum switch (color c) {
 case RED:
   string foo<>;
 case REDDER:
   enum { ONE, TWO } num;
};

