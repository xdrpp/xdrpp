
#include <cassert>
#include <ctype.h>
#include "xdrc_internal.h"

using std::endl;

std::unordered_map<string, string> xdr_type_map = {
  {"unsigned", "std::uint32_t"},
  {"int", "std::int32_t"},
  {"unsigned hyper", "std::uint62_t"},
  {"hyper", "std::int64_t"},
};

string
map_type(const string &s)
{
  auto t = xdr_type_map.find(s);
  if (t == xdr_type_map.end())
    return s;
  return t->second;
}

string
map_case(const string &s)
{
  if (s.empty())
    return "default:";
  if (s == "TRUE")
    return "case true:";
  if (s == "FALSE")
    return "case false:";
  return string("case ") + s + ':';
}

namespace {

indenter nl;

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
  string type = map_type(d.type);
  if (type == "string" || type == "opaque") {
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
  case rpc_decl::PTR:
    return string("std::unique_ptr<") + type + ">";
  case rpc_decl::ARRAY:
    return string("std::array<") + type + "," + d.bound + ">";
  case rpc_decl::VEC:
    return string("std::vector<") + type + ">";
  default:
    return type;
  }
}

inline string
id_space(const string &s)
{
  return s.empty() ? s : s + ' ';
}

void
gen(std::ostream &os, const rpc_struct &s)
{
  os << "struct " << id_space(s.id) << '{';
  ++nl;
  for(auto d : s.decls)
    os << nl << decl_type(d) << ' ' << d.id << ';';
  os << nl.close << "}";
  if (!s.id.empty())
    os << ';';
}

void
gen(std::ostream &os, const rpc_enum &e)
{
  os << "enum " << id_space(e.id) << ": std::uint32_t {";
  ++nl;
  for (rpc_const c : e.tags)
    if (c.val.empty())
      os << nl << c.id << ',';
    else
      os << nl << c.id << " = " << c.val << ',';
  os << nl.close << "}";
  if (!e.id.empty())
    os << ';';
}

void
gen(std::ostream &os, const rpc_union &u)
{
  os << "class " << u.id << " {"
     << nl.open << map_type(u.tagtype) << ' ' << u.tagid << "_;"
     << nl << "union {";
  ++nl;
  for (rpc_utag t : u.cases)
    if (t.tagvalid && t.tag.type != "void")
      os << nl << decl_type(t.tag) << ' ' << t.tag.id << "_;";
  os << nl.close << "};" << endl;

  os << nl.outdent << "public:"
     << nl << "const char *_name_of_selected() const {"
     << nl.open << "switch (" << u.tagid << "_) {";
  for (rpc_utag t : u.cases) {
    os << nl << map_case(t.swval);
    if (t.tagvalid) {
      if (t.tag.id.empty())
	os << nl << "  return nullptr;";
      else
	os << nl << "  return \"" << t.tag.id << "\";";
    }
  }
  os << nl << "}"
     << nl.close << "}" << endl;

  os << nl << "template<typename F>"
     << nl << "xdr::result_type_or_void<F> _apply_to_selected(F &_f) {"
     << nl.open << "switch (" << u.tagid << "_) {";
  for (rpc_utag t : u.cases) {
    os << nl << map_case(t.swval);
    if (t.tagvalid) {
      if (t.tag.type == "void")
	os << nl << "  return _f();";
      else
	os << nl << "  return _f(" << t.tag.id << "_);";
    }
  }
  os << nl << "}"
     << nl.close << "}" << endl;

  os << nl << map_type(u.tagtype) << ' ' << u.tagid << "() const { return "
     << u.tagid << "_; }";
  os << nl << "void set_" << u.tagid << "(" << u.tagtype << " _t) {"
     << nl.open << "_apply_to_selected(case_destroyer{});"
     << nl << u.tagid << "_ = t;"
     << nl << "_apply_to_selected(case_constructor{});"
     << nl.close << "}" << endl;

  os << nl.close << "}";
  if (!u.id.empty())
    os << ';';
}

}

void
gen_hh(std::ostream &os)
{
  os << "// -*-C++-*-"
     << nl << "// Automatically generated from " << input_file << '.' << endl;

  string gtok = guard_token();
  os << nl << "#ifndef " << gtok
     << nl << "#define " << gtok << " 1" << endl
     << nl << "#include <xdrc.h>";

  int last_type = -1;

  for (auto s : symlist) {
    switch(s.type) {
    case rpc_sym::CONST:
    case rpc_sym::TYPEDEF:
    case rpc_sym::LITERAL:
      if (s.type != last_type)
      // cascade
    default:
	os << endl;
    }
    os << nl;
    switch(s.type) {
    case rpc_sym::CONST:
      os << "constexpr std::uint32_t " << s.sconst->id << " = "
	 << s.sconst->val << ';';
      break;
    case rpc_sym::STRUCT:
      gen(os, *s.sstruct);
      break;
    case rpc_sym::UNION:
      gen(os, *s.sunion);
      break;
    case rpc_sym::ENUM:
      gen(os, *s.senum);
      break;
    case rpc_sym::TYPEDEF:
      os << "using " << s.stypedef->id << " = "
	 << decl_type(*s.stypedef) << ';';
      break;
    case rpc_sym::PROGRAM:
      /* need some action */
      break;
    case rpc_sym::LITERAL:
      os << *s.sliteral << nl;
      break;
    case rpc_sym::NAMESPACE:
      break;
    default:
      std::cerr << "unknown type " << s.type << nl;
      break;
    }
    last_type = s.type;
  }

  os << endl << nl << "#endif /* !" << gtok << " */" << nl;
}

