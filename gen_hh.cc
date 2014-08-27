
#include <ctype.h>
#include "xdrc_internal.h"

using std::endl;

namespace {

string
guard_token()
{
  string in;
  if (!output_file.empty())
    in = output_file;
  else {
    size_t r = input_file.rfind('/');
    if (r != string::npos)
      in = input_file.substr(r+1);
    else
      in = input_file;
    r = in.size();
    if (r >= 2 && in.substr(r-2) == ".x")
      in.back() = 'h';
  }

  string ret = "__XDR_";
  for (char c : in)
    if (isalnum(c))
      ret += toupper(c);
    else
      ret += "_";
  ret += "_INCLUDED__";
  return ret;
}

string
decl_type(const rpc_decl &d)
{
  // XXX need to handle string and opaque
  switch (d.qual) {
  case rpc_decl::SCALAR:
    return d.type;
  case rpc_decl::PTR:
    return string("std::unique_ptr<") + d.type + ">";
  case rpc_decl::ARRAY:
    return string("std::array<") + d.type + "," + d.bound + ">";
  case rpc_decl::VEC:
    return string("std::vector<") + d.type + ">";
  }
}

}

void
gen_hh(std::ostream &os)
{
  os << "// -*-C++-*-" << endl
     << "// Automatically generated from " << input_file << '.' << endl
     << endl;

  string gtok = guard_token();
  os << "#ifndef " << gtok << endl
     << "#define " << gtok << " 1" << endl << endl;

  for (auto s : symlist) {
    switch(s.type) {
    case rpc_sym::CONST:
      os << "constexpr std::uint32_t " << s.sconst->id << " = "
	 << s.sconst->val << ';' << endl;
      break;
    case rpc_sym::TYPEDEF:
      os << "using " << s.stypedef->id << " = "
	 << decl_type(*s.stypedef) << ';' << endl;
      break;
    default:
      os << "unknown type " << s.type << endl;
      break;
    }
  }

  os << endl << "#endif /* !" << gtok << " */" << endl;
}

