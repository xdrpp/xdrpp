
#include <cassert>
#include "xdrc.h"

std::set<string> ids;

rpc_program *
get_prog(bool creat)
{
  rpc_program *r = nullptr;
  rpc_sym *s;
  if (symlist.size() && (s = &symlist.back()) &&
      s->gettype () == rpc_sym::NAMESPACE) {
    if (creat)
      s->snamespace->progs.push_back ();
    r = &s->snamespace->progs.back ();
  } else {
    if (creat) {
      s = &symlist.push_back ();
      s->settype (rpc_sym::PROGRAM);
    } else {
      s = &symlist.back ();
    }
    r = s->sprogram;
  }
  assert (r != nullptr);
  return r;
}

int
main(int argc, char **argv)
{
  yyparse ();
  checkliterals ();

  return 0;
}
