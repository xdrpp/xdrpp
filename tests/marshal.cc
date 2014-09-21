
#include <iostream>
#include <sstream>
#include <xdrc/cereal.h>
#include <xdrc/printer.h>
#include <cereal/archives/binary.hpp>
#include "xdrtest.hh"

using namespace std;

int
main()
{
  testns::bytes b, b2;
  b.s = "Hello world\n";
  b.fixed.fill(0xc5);
  b.variable = { 2, 4, 6, 8 };

  cout << xdr::xdr_to_string(b);

  ostringstream obuf;
  {
    cereal::BinaryOutputArchive archive(obuf);
    archive(b);
  }

  {
    istringstream ibuf(obuf.str());
    cereal::BinaryInputArchive archive(ibuf);
    archive(b2);
  }

  cout << xdr::xdr_to_string(b2);

  return 0;
}
