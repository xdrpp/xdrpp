
#include <cassert>
#include <ctype.h>
#include <sstream>
#include "xdrc_internal.h"

using std::endl;

std::unordered_map<string, string> xdr_type_map = {
  {"unsigned", "std::uint32_t"},
  {"int", "std::int32_t"},
  {"unsigned hyper", "std::uint64_t"},
  {"hyper", "std::int64_t"},
  {"opaque", "std::uint8_t"},
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
map_tag(const string &s)
{
  assert (!s.empty());
  if (s == "TRUE")
    return "true";
  if (s == "FALSE")
    return "false";
  if (s[0] == '-')
    // Stuff should get implicitly converted to unsigned anyway, but
    // this avoids clang++ warnings.
    return string("std::uint32_t(") + s + ")";
  else
    return s;
}

string
map_case(const string &s)
{
  if (s.empty())
    return "default:";
  return "case " + map_tag(s) + ":";
}

namespace {

indenter nl;
vec<string> scope;
vec<string> namespaces;
std::ostringstream top_material;

string
cur_ns()
{
  if (namespaces.empty())
    return "";
  string out;
  for (const auto &ns : namespaces) {
    out += "::";
    out += ns;
  }
  return out;
}

string
cur_scope()
{
  if (scope.empty())
    return cur_ns();
  else
    return cur_ns() + "::" + scope.back();
}

string
guard_token()
{
  string in;
  if (!output_file.empty() && output_file != "-")
    in = output_file;
  else
    in = strip_directory(strip_dot_x(input_file)) + ".h";

  string ret = "__XDR_";
  for (char c : in)
    if (isalnum(c))
      ret += toupper(c);
    else
      ret += "_";
  ret += "_INCLUDED__";
  return ret;
}

inline string
id_space(const string &s)
{
  return s.empty() ? s : s + ' ';
}

void gen(std::ostream &os, const rpc_struct &s);
void gen(std::ostream &os, const rpc_enum &e);
void gen(std::ostream &os, const rpc_union &u);

string
decl_type(const rpc_decl &d)
{
  string type = map_type(d.type);

  if (type == "string")
    return string("xdr::xstring<") + d.bound + ">";
  if (d.type == "opaque")
    switch (d.qual) {
    case rpc_decl::ARRAY:
      return string("xdr::opaque_array<") + d.bound + ">";
    case rpc_decl::VEC:
      return string("xdr::opaque_vec<") + d.bound + ">";
    default:
      assert(!"bad opaque qualifier");
    }

  switch (d.qual) {
  case rpc_decl::PTR:
    return string("xdr::pointer<") + type + ">";
  case rpc_decl::ARRAY:
    return string("xdr::xarray<") + type + "," + d.bound + ">";
  case rpc_decl::VEC:
    return string("xdr::xvector<") + type +
      (d.bound.empty() ? ">" : string(",") + d.bound + ">");
  default:
    return type;
  }
}

bool
gen_embedded(std::ostream &os, const rpc_decl &d)
{
  switch (d.ts_which) {
  case rpc_decl::TS_ENUM:
    os << nl;
    gen(os, *d.ts_enum);
    break;
  case rpc_decl::TS_STRUCT:
    os << nl;
    gen(os, *d.ts_struct);
    break;
  case rpc_decl::TS_UNION:
    os << nl;
    gen(os, *d.ts_union);
    break;
  default:
    return false;
  }
  os << ";";
  return true;
}

void
gen(std::ostream &os, const rpc_struct &s)
{
  if(scope.empty())
    scope.push_back(s.id);
  else
    scope.push_back(scope.back() + "::" + s.id);

  os << "struct " << id_space(s.id) << '{';
  ++nl;
  bool blank{false};
  for(auto &d : s.decls) {
    if (gen_embedded(os, d))
      blank = true;
  }
  if (blank)
    os << endl;

  for (auto &d : s.decls)
    os << nl << decl_type(d) << ' ' << d.id << ';';
  os << nl.close << "}";

  top_material
    << "template<> struct xdr_traits<" << cur_scope()
    //<< "> : xdr_traits_base {" << endl;
    << ">" << endl
    << "  : xdr_struct_base<";
  bool first{true};
  for (auto &d : s.decls) {
    if (first)
      first = false;
    else
      top_material << "," << endl << "                    ";
    top_material
      << "field_ptr<" << cur_scope() << "," << endl
      << "                              decltype("
      << cur_scope() << "::" << d.id << ")," << endl
      << "                              &"
      << cur_scope() << "::" << d.id << ">";
  }

  top_material
    << "> {" << endl
    << "  static constexpr bool is_class = true;" << endl;
  for (string decl :
    { string("  template<typename _Archive> static void\n"
	     "  save(_Archive &_archive, const ")
	+ cur_scope() + " &_xdr_obj) {",
      string("  template<typename _Archive> static void\n"
	     "  load(_Archive &_archive, ")
	+ cur_scope() + " &_xdr_obj) {" } ) {
    top_material << decl << endl;
    for (size_t i = 0; i < s.decls.size(); ++i)
      top_material << "    archive(_archive, \""
		   << s.decls[i].id << "\", _xdr_obj."
		   << s.decls[i].id << ");" << endl;
    top_material << "  }" << endl;;
  }
  top_material << "};" << endl;

  scope.pop_back();
}

void
gen(std::ostream &os, const rpc_enum &e)
{
  os << "enum " << id_space(e.id) << ": std::uint32_t {";
  ++nl;
  for (const rpc_const &c : e.tags)
    if (c.val.empty())
      os << nl << c.id << ',';
    else
      os << nl << c.id << " = " << c.val << ',';
  os << nl.close << "}";

  string myscope = cur_scope();
  if (myscope != "::")
    myscope += "::";
  string qt = myscope + e.id;
  top_material
    << "template<> struct xdr_traits<"
    << qt << "> : xdr_traits_base {" << endl
    << "  static constexpr bool is_enum = true;" << endl
    << "  static constexpr bool has_fixed_size = true;" << endl
    << "  static constexpr std::size_t fixed_size = 4;" << endl
    << "  static constexpr std::size_t serial_size("
    << qt << ") { return 4; }" << endl
    << "  static const char *enum_name("
    << qt << " _xdr_enum_val) {" << endl
    << "    switch (_xdr_enum_val) {" << endl;
  for (const rpc_const &c : e.tags)
    top_material << "    case " << myscope + c.id << ":" << endl
		 << "      return \"" << c.id << "\";" << endl;
  top_material
    << "    default:" << endl
    << "      return nullptr;" << endl
    << "    }" << endl
    << "  }" << endl
    << "};" << endl;
}

string
pswitch(const rpc_union &u, string id = string())
{
  if (id.empty())
    id = u.tagid + '_';
  string ret = "switch (";
  if (u.tagtype == "bool")
    ret += "std::uint32_t{" + id + "}";
  else
    ret += id;
  ret += ") {";
  return ret;
}

std::ostream &
ufbol(bool *first, std::ostream &os)
{
  if (*first) {
    os << nl << "return ";
    *first = false;
  }
  else
    os << nl << "  : ";
  return os;
}

void
union_function(std::ostream &os, const rpc_union &u, string tagcmp,
	       const std::function<string(const rpc_ufield *)> &cb)
{
  const rpc_ufield *def = nullptr;
  bool first = true;
  if (tagcmp.empty())
    tagcmp = u.tagid + "_ == ";
  else
    tagcmp += " == ";

  ++nl;

  for (auto fi = u.fields.cbegin(), e = u.fields.cend();
       fi != e; ++fi) {
    if (fi->hasdefault) {
      def = &*fi;
      continue;
    }
    ufbol(&first, os) << tagcmp << map_tag(fi->cases[0]);
    for (size_t i = 1; i < fi->cases.size(); ++i)
      os << " || " << tagcmp << map_tag(fi->cases[i]);
    os << " ? " << cb(&*fi);
  }
  ufbol(&first, os) << cb(def);
  os << ";";

  --nl;
}

void
gen(std::ostream &os, const rpc_union &u)
{
  if(scope.empty())
    scope.push_back(u.id);
  else
    scope.push_back(scope.back() + "::" + u.id);

  os << "struct " << u.id << " {";
  ++nl;
  bool blank{false};
  for (const rpc_ufield &f : u.fields)
    if (gen_embedded(os, f.decl))
      blank = true;
  if (blank)
    os << endl;
  os << nl.outdent << "private:"
     << nl << "std::uint32_t " << u.tagid << "_;"
     << nl << "union {";
  ++nl;
  for (const rpc_ufield &f : u.fields)
    if (f.decl.type != "void")
      os << nl << decl_type(f.decl) << ' ' << f.decl.id << "_;";
  os << nl.close << "};" << endl;

  os << nl.outdent << "public:";
  os << nl << "static_assert (sizeof (" << u.tagtype << ") <= 4,"
    " \"union discriminant must be 4 bytes\");";

  // _xdr_discriminant
  os << nl << "using _xdr_discriminant_t = " << u.tagtype << ";";
  os << nl << "std::uint32_t _xdr_discriminant() const { return "
     << u.tagid << "_; }";
  os << nl << "void _xdr_discriminant(std::uint32_t _xdr_d,"
     << nl << "                       bool _xdr_validate = true) {"
     << nl.open << "int _xdr_fnum = _xdr_field_number(_xdr_d);"
     << nl << "if (_xdr_fnum < 0 && _xdr_validate)"
     << nl << "  throw xdr::xdr_bad_discriminant(\"bad value of "
     << u.tagid << " in " << u.id << "\");"
     << nl << "if (_xdr_fnum != _xdr_field_number(" << u.tagid << "_)) {"
     << nl.open << "this->~" << u.id << "();"
     << nl << u.tagid << "_ = _xdr_d;"
     << nl << "_xdr_on_field_ptr(xdr::case_constructor, this, "
     << u.tagid << "_);"
     << nl.close << "}"
     << nl.close << "}" << endl;

  // _xdr_field_number
  os << nl
     << "static constexpr int _xdr_field_number(std::uint32_t _which) {";
  union_function(os, u, "_which", [](const rpc_ufield *uf) {
      using std::to_string;
      if (uf)
	return to_string(uf->fieldno);
      else
	return to_string(-1);
    });
  os << nl << "}";

  // _xdr_field_name
  os << nl
     << "static constexpr const char *_xdr_field_name(std::uint32_t _which) {";
  union_function(os, u, "_which", [](const rpc_ufield *uf) {
      using std::to_string;
      if (uf && uf->decl.type != "void")
	return string("\"") + uf->decl.id + "\"";
      else
	return string("nullptr");
    });
  os << nl << "}";

  // _xdr_field_name
  os << nl << "const char *_xdr_field_name() const { return _xdr_field_name("
     << u.tagid << "_); }";

  // _xdr_with_field_ptr
  os << nl << "template<typename _F> static bool"
     << nl << "_xdr_with_field_ptr(_F &_f, std::uint32_t _which) {"
     << nl.open << pswitch(u, "_which");
  for (const rpc_ufield &f : u.fields) {
    for (string c : f.cases)
      os << nl << map_case(c);
    if (f.decl.type == "void")
      os << nl << "  _f();";
    else 
      os << nl << "  _f(xdr::field_ptr<" << u.id << ", " << decl_type(f.decl)
	 << ", &" << u.id << "::" << f.decl.id << "_>());"
	 << nl << "  return true;";
  }
  os << nl << "}";
  if (!u.hasdefault)
    os << nl << "return false;";
  os << nl.close << "}"
     << endl;

  // _xdr_on_field_ptr
  os << nl << "template<typename F, typename T> static void"
     << nl << "_xdr_on_field_ptr(F &f, T &&t, std::uint32_t _which) {"
     << nl.open << pswitch(u, "_which");
  for (const rpc_ufield &f : u.fields) {
    for (string c : f.cases)
      os << nl << map_case(c);
    if (f.decl.type == "void")
      os << nl << "  f(std::forward<T>(t));";
    else
      os << nl << "  f(std::forward<T>(t), &"
	 << u.id << "::" << f.decl.id << "_);";
    os << nl << "  return;";
  }
  os << nl << "}";
  if (!u.hasdefault)
    os << nl << "f();";
  os << nl.close << "}"
     << endl;

  // Default constructor
  os << nl << u.id << "(" << map_type(u.tagtype) << " _t = "
     << map_type(u.tagtype) << "{}) : " << u.tagid << "_(_t) {"
     << nl.open << "_xdr_on_field_ptr(xdr::case_constructor, this, "
     << u.tagid << "_);"
     << nl.close << "}";

  // Copy/move constructor
  os << nl << u.id << "(const " << u.id << " &_source) : "
     << u.tagid << "_(_source." << u.tagid << "_) {"
     << nl.open << "xdr::case_construct_from _dest{this};"
     << nl << "_xdr_on_field_ptr(_dest, _source, " << u.tagid << "_);"
     << nl.close << "}";
  os << nl << u.id << "(" << u.id << " &&_source) : "
     << u.tagid << "_(_source." << u.tagid << "_) {"
     << nl.open << "xdr::case_construct_from _dest{this};"
     << nl << "_xdr_on_field_ptr(_dest, std::move(_source), "
     << u.tagid << "_);"
     << nl.close << "}";

  // Destructor
  os << nl << "~" << u.id
     << "() { _xdr_on_field_ptr(xdr::case_destroyer, this, "
     << u.tagid << "_); }";

  // Assignment
  os << nl << u.id << " &operator=(const " << u.id << " &_source) {"
     << nl.open << "if (_xdr_field_number(" << u.tagid
     << "_) == _xdr_field_number(_source." << u.tagid << "_)) {"
     << nl << "  xdr::case_assign_from _dest{this};"
     << nl << "  _xdr_on_field_ptr(_dest, _source, " << u.tagid << "_);"
     << nl << "}"
     << nl << "else {"
     << nl.open << "this->~" << u.id << "();"
     << nl << u.tagid << "_ = std::uint32_t(-1);" // might help with exceptions
     << nl << "xdr::case_construct_from _dest{this};"
     << nl << "_xdr_on_field_ptr(_dest, _source, " << u.tagid << "_);"
     << nl.close << "}"
     << nl << u.tagid << "_ = _source." << u.tagid << "_;"
     << nl << "return *this;"
     << nl.close << "}";
  os << nl << u.id << " &operator=(" << u.id << " &&_source) {"
     << nl.open << "if (_xdr_field_number(" << u.tagid
     << "_) == _xdr_field_number(_source." << u.tagid << "_)) {"
     << nl << "  xdr::case_assign_from _dest{this};"
     << nl << "  _xdr_on_field_ptr(_dest, std::move(_source), "
     << u.tagid << "_);"
     << nl << "}"
     << nl << "else {"
     << nl.open << "this->~" << u.id << "();"
     << nl << u.tagid << "_ = std::uint32_t(-1);" // might help with exceptions
     << nl << "xdr::case_construct_from _dest{this};"
     << nl << "_xdr_on_field_ptr(_dest, std::move(_source), "
     << u.tagid << "_);"
     << nl.close << "}"
     << nl << u.tagid << "_ = _source." << u.tagid << "_;"
     << nl << "return *this;"
     << nl.close << "}";

  os << endl;

  // Tag getter/setter
  os << nl << map_type(u.tagtype) << ' ' << u.tagid << "() const { return "
     << map_type(u.tagtype) << "(" << u.tagid << "_); }";
  os << nl << "void " << u.tagid << "(" << u.tagtype
     << " _xdr_d, bool _xdr_validate = true) {"
     << nl.open << "_xdr_discriminant(_xdr_d, _xdr_validate);"
     << nl.close << "}" << endl;

  for (const auto &f : u.fields) {
    if (f.decl.type == "void")
      continue;
    for (string cnst : {"", "const "})
      os << nl << cnst << decl_type(f.decl) << " &" << f.decl.id
	 << "() " << cnst << "{"
	 << nl.open << "if (_xdr_field_number(" << u.tagid << "_) == "
	 << f.fieldno << ")"
	 << nl << "  return " << f.decl.id << "_;"
	 << nl << "throw xdr::xdr_wrong_union(\""
	 << u.id << ": " << f.decl.id << " accessed when not selected\");"
	 << nl.close << "}";
  }

  top_material
    << "template<> struct xdr_traits<" << cur_scope()
    << "> : xdr_traits_base {" << endl
    << "  static constexpr bool is_class = true;" << endl
    << "  static constexpr bool has_fixed_size = false;" << endl
    << "  static std::size_t serial_size(const " << cur_scope()
    << " &o) {" << endl
    << "    case_serial_size ss;" << endl
    << "    o._xdr_on_field_ptr(ss, &o, o._xdr_discriminant());" << endl
    << "    return ss.size;" << endl
    << "  }" << endl;
  top_material
    << "  template<typename _Archive> static void" << endl
    << "  save(_Archive &_archive, const "
    << cur_scope() << " &_xdr_obj) {" << endl
    << "    xdr::archive(_archive, \"" << u.tagid << "\", _xdr_obj."
    << u.tagid << "());" << endl
    << "    xdr::case_save<_Archive> _cs{_archive, _xdr_obj._xdr_field_name()};"
    << endl
    << "    _xdr_obj._xdr_on_field_ptr(_cs, &_xdr_obj, _xdr_obj."
    << u.tagid << "());" << endl
    << "  }" << endl;
  top_material
    << "  template<typename _Archive> static void" << endl
    << "  load(_Archive &_archive, "
    << cur_scope() << " &_xdr_obj) {" << endl
    << "    " << cur_scope() << "::_xdr_discriminant_t _xdr_which;" << endl
    << "    xdr::archive(_archive, \"" << u.tagid << "\", _xdr_which);" << endl
    << "    _xdr_obj." << u.tagid << "(_xdr_which);" << endl
    << "    xdr::case_load<_Archive> _cs{_archive, _xdr_obj._xdr_field_name()};"
    << endl
    << "    _xdr_obj._xdr_on_field_ptr(_cs, &_xdr_obj, _xdr_obj."
    << u.tagid << "());" << endl
    << "  }" << endl;
  top_material
    << "};" << endl;

  os << nl.close << "}";

  scope.pop_back();
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
     << nl << "#include <xdrc/types.h>";

  int last_type = -1;

  os << nl;
  for (auto &s : symlist) {
    switch(s.type) {
    case rpc_sym::CONST:
    case rpc_sym::TYPEDEF:
    case rpc_sym::LITERAL:
    case rpc_sym::NAMESPACE:
    case rpc_sym::CLOSEBRACE:
      if (s.type != last_type)
      // cascade
    default:
	os << endl;
    }
    switch(s.type) {
    case rpc_sym::CONST:
      os << "constexpr std::uint32_t " << s.sconst->id << " = "
	 << s.sconst->val << ';';
      break;
    case rpc_sym::STRUCT:
      gen(os, *s.sstruct);
      os << ';';
      break;
    case rpc_sym::UNION:
      gen(os, *s.sunion);
      os << ';';
      break;
    case rpc_sym::ENUM:
      gen(os, *s.senum);
      os << ';';
      break;
    case rpc_sym::TYPEDEF:
      os << "using " << s.stypedef->id << " = "
	 << decl_type(*s.stypedef) << ';';
      break;
    case rpc_sym::PROGRAM:
      /* need some action */
      break;
    case rpc_sym::LITERAL:
      os << *s.sliteral;
      break;
    case rpc_sym::NAMESPACE:
      namespaces.push_back(*s.sliteral);
      os << "namespace " << *s.sliteral << " {";
      break;
    case rpc_sym::CLOSEBRACE:
      namespaces.pop_back();
      os << "}";
      break;
    default:
      std::cerr << "unknown type " << s.type << nl;
      break;
    }
    last_type = s.type;
    os << nl;
    if (!top_material.str().empty()) {
      for (size_t i = 0; i < namespaces.size(); i++)
	os << "} ";
      os << "namespace xdr {" << nl;
      os << top_material.str();
      top_material.str("");
      os << "}";
      for (const std::string &ns : namespaces)
	os << " namespace " << ns << " {";
      os << nl;
    }
  }

  os << nl << "#endif // !" << gtok << nl;
}

