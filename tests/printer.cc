
#include <xdrc/printer.h>
#include "xdrtest.hh"

int
main()
{
  numerics n;
  xdr::Printer p;

  p(n);

  return 0;
}
