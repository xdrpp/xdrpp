
#include <cassert>
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
  string type = d.type;
  if (d.type == "string" || d.type == "opaque") {
    switch (d.qual) {
    case rpc_decl::ARRAY:
      return string("std::array<char,") + d.bound + ">";
    case rpc_decl::VEC:
      return string("xdr::xstring<") + d.bound + ">";
    default:
      assert(!"Invalid size for opaque/string");
    }
  }

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

void
gen_union(std::ostream &os, const rpc_union &u)
{
  // XXX - need to translate int to std::int32_t, etc.
  os << "class " << u.id << " {" << endl
     << "  " << u.tagtype << ' ' << u.tagid << "_;" << endl
     << "  union {" << endl;
  for (rpc_utag t : u.cases)
    if (t.tag.type != "void")
      os << "    " << decl_type(t.tag) << ' ' << t.tag.id << "_;" << endl;
  os << "  };" << endl << endl
     << "  template<typename F> auto __apply_to_selected(F &f) {" << endl
     << "    switch (" << u.id << ") {" << endl
     << "    }" << endl
     << "  }" << endl
     << endl
     << "public:" << endl;

  os << "};" << endl;
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
     << "#define " << gtok << " 1" << endl;

  int last_type = -1;

  for (auto s : symlist) {
    switch(s.type) {
    case rpc_sym::CONST:
    case rpc_sym::TYPEDEF:
    case rpc_sym::LITERAL:
      if (s.type != last_type)
    default:
	os << endl;
    }
    switch(s.type) {
    case rpc_sym::CONST:
      os << "constexpr std::uint32_t " << s.sconst->id << " = "
	 << s.sconst->val << ';' << endl;
      break;
    case rpc_sym::STRUCT:
      os << "struct " << s.sstruct->id << " {" << endl;
      for(auto d : s.sstruct->decls)
	os << "  " << decl_type(d) << ' ' << d.id << ';' << endl;
      os << "};" << endl;
      break;
    case rpc_sym::UNION:
      gen_union(os, *s.sunion);
      break;
    case rpc_sym::ENUM:
      os << "enum class " << s.senum->id << " : std::uint32_t {" << endl;
      for (rpc_const c : s.senum->tags)
	if (c.val.empty())
	  os << "  " << c.id << ',' << endl;
	else
	  os << "  " << c.id << " = " << c.val << ',' << endl;
      os << "};" << endl;
      break;
    case rpc_sym::TYPEDEF:
      os << "using " << s.stypedef->id << " = "
	 << decl_type(*s.stypedef) << ';' << endl;
      break;
    case rpc_sym::PROGRAM:
      /* need some action */
      break;
    case rpc_sym::LITERAL:
      os << *s.sliteral << endl;
      break;
    case rpc_sym::NAMESPACE:
      break;
    default:
      std::cerr << "unknown type " << s.type << endl;
      break;
    }
    last_type = s.type;
  }

  os << endl << "#endif /* !" << gtok << " */" << endl;
}

