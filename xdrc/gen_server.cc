
#include "xdrc_internal.h"

using std::endl;

namespace {

indenter nl;

void
gen_vers(std::ostream &os, const rpc_program &u, const rpc_vers &v)
{
  string name = v.id + "_server";
  os << endl
     << nl << "class " << name << " {"
     << nl << "public:";
  ++nl;
  for (const rpc_proc &p : v.procs) {
    string arg = p.arg == "void" ? ""
      : (string("std::unique_ptr<") + p.arg + "> arg");
    string res = p.res == "void" ? "void"
      : (string("std::unique_ptr<") + p.res + ">");
    os << nl << res << " " << p.id << "(" << arg << ");";
  }
  os << nl.close << "};";

  for (const rpc_proc &p : v.procs) {
    string arg = p.arg == "void" ? ""
      : (string("std::unique_ptr<") + p.arg + "> arg");
    string res = p.res == "void" ? "void"
      : (string("std::unique_ptr<") + p.res + ">");
    os << endl
       << nl << res
       << nl << name << "::" << p.id
       << "(" << arg << ")"
       << nl << "{";
    if (res != "void")
       os << nl.open << "std::unique_ptr<" << p.res << "> res(new "
	  << p.res << ");"
	  << nl
	  << nl << "// Fill in function body here"
	  << nl
	  << nl << "return res;"
	  << nl.close << "}";
    else
       os << nl.open
	  << nl << "// Fill in function body here"
	  << nl
	  << nl.close << "}";
  }
}

}

void
gen_server(std::ostream &os)
{
  os << "/* Scaffolding originally generated from " << input_file << "."
     << nl << " * Edit to add functionality. */" << endl
     << nl << "#include \"" << file_prefix << ".h\"";

  int last_type = -1;

  for (auto &s : symlist) {
    switch (s.type) {
    case rpc_sym::PROGRAM:
      for (const rpc_vers &v : s.sprogram->vers)
	gen_vers(os, *s.sprogram, v);
      break;
    case rpc_sym::NAMESPACE:
      if (last_type != rpc_sym::NAMESPACE)
	os << endl;
      os << nl << "namespace " << *s.sliteral << " {";
      break;
    case rpc_sym::CLOSEBRACE:
      if (last_type != rpc_sym::CLOSEBRACE)
	os << endl;
      os << nl << "}";
      break;
    default:
      break;
    }
    last_type = s.type;
  }
  os << nl;
}

