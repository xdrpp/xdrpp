
#include <cassert>
#include <ctype.h>
#include <sstream>
#include <utility>
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
  return s;
}

namespace {

indenter nl;
vec<string> scope;
vec<string> namespaces;
std::ostringstream top_material;

string
cur_ns()
{
#if 0
  if (namespaces.empty())
    return "";
  string out;
  for (const auto &ns : namespaces) {
    if (!out.empty())
      out += "::";
    out += ns;
  }
  return out;
#else
  string out;
  for (const auto &ns : namespaces)
    out += string("::") + ns;
  return out.empty() ? "::" : out;
#endif
}

string
cur_scope()
{
  string out;
  if (!namespaces.empty())
    out = cur_ns();
  if (!scope.empty()) {
    out += "::";
    out += scope.back();
  }
  return out;
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
    os << nl << decl_type(d) << ' ' << d.id << "{};";
  os << endl
     << nl << s.id << "() = default;"
     << nl << "template<";
  bool first{true};
  for (auto &d : s.decls) {
    os << "typename _" << d.id << "_T,"
       << nl << "         ";
  }
  os << "typename = typename"
     << nl << "         std::enable_if<";
  first = true;
  for (auto &d : s.decls) {
    if (first)
      first = false;
    else
      os << nl << "                        && ";
    os << "std::is_constructible<" << decl_type(d) << ", _"
       << d.id << "_T>::value";
  }
  os << nl << "                       >::type>"
     << nl << "explicit " << s.id << "(";
  first = true;
  for (auto &d : s.decls) {
    if (first)
      first = false;
    else
      os << "," << nl << string(10 + s.id.size(), ' ');
    os << "_" << d.id << "_T &&_" << d.id;
  }
  os << ")"
     << nl << "  : ";
  first = true;
  for (auto &d : s.decls) {
    if (first)
      first = false;
    else
      os << "," << nl << "    ";
    os << d.id << "(std::forward<_" << d.id << "_T>(_" << d.id << "))";
  }
  os << " {}";
#if 0
  os << nl << "friend bool operator==(const " << s.id
     << "&, const " << s.id << "&)"
     << " = default;"
     << nl << "friend auto operator<=>(const " << s.id
     << "&, const " << s.id << "&)"
     << " = default;"
#endif
    os << nl.close << "}";

  top_material
    << "template<> struct xdr_traits<" << cur_scope()
    << ">" << endl;
  top_material
    << "  : xdr_struct_base<" << cur_scope();
  for (auto &d : s.decls)
    top_material
      << "," << endl << "                    "
      << "xdr::field_access_t<&" << cur_scope() << "::" << d.id
      << ", \"" << d.id << "\">";

#if 0
  top_material
    << "> {" << endl;
  int round = 0;
  for (string decl :
    { string("  template<typename Archive> static void\n"
	     "  save(Archive &ar, const ")
	+ cur_scope() + " &obj) {",
      string("  template<typename Archive> static void\n"
	     "  load(Archive &ar, ")
	+ cur_scope() + " &obj) {" } ) {
    top_material << decl << endl;
    for (size_t i = 0; i < s.decls.size(); ++i)
      top_material << "    archive(ar, obj." << s.decls[i].id
		   << ", \"" << s.decls[i].id << "\");" << endl;
    if (round++)
      top_material << "    xdr::validate(obj);" << endl;
    top_material << "  }" << endl;
  }
  top_material << "};" << endl;
#else
  top_material
    << "> {};" << endl;
#endif

  scope.pop_back();
}

void
gen(std::ostream &os, const rpc_enum &e)
{
  os << "enum " << id_space(e.id) << ": std::int32_t {";
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
    << "template<> struct xdr_traits<" << qt << ">" << endl
    << "  : xdr_numeric_base<" << qt << ", std::uint32_t> {" << endl
    << "  using case_type = " << qt << ";" << endl
    << "  static constexpr const bool is_enum = true;" << endl
    << "  static constexpr const bool is_numeric = false;" << endl
    << "  static const char *enum_name("
    << qt << " val) {" << endl
    << "    switch (val) {" << endl;
  for (const rpc_const &c : e.tags)
    top_material << "    case " << myscope + c.id << ":" << endl
		 << "      return \"" << c.id << "\";" << endl;
  top_material
    << "    default:" << endl
    << "      return nullptr;" << endl
    << "    }" << endl
    << "  }" << endl
    << "  static const std::vector<int32_t> &enum_values() {" << endl
    << "    static const std::vector<int32_t> _xdr_enum_vec = {";
  bool first = true;
  for (const rpc_const &c : e.tags) {
    if (first)
      first = false;
    else
      top_material << ",";
    top_material
      << endl
      << "      " << myscope + c.id;
  }
  top_material
    << endl
    << "    };" << endl
    << "    return _xdr_enum_vec;" << endl
    << "  }" << endl
    << "};" << endl;
}

string
pswitch(const rpc_union &u, string id = string())
{
  if (id.empty())
    id = u.tagid + '_';
  string ret = "switch (";
#if 0
  if (u.tagtype == "bool")
    ret += "std::int32_t{" + id + "}";
  else
    ret += id;
#else
  ret += "_xdr_union_traits::case_type{" + id + "}";
#endif
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
print_cases(std::ostream &os, const rpc_ufield &f)
{
  if (f.hasdefault)
    os << nl.outdent << "default:";
  else
    for (const string &c : f.cases)
      os << nl.outdent << "case " << map_tag(c) << ":";
}

std::string
union_field_access(const rpc_union &u, const rpc_ufield &f)
{
  std::ostringstream os;
  if (f.decl.type == "void")
    os << "xdr::void_access_t";
  else if (opt_uptr)
    os << "xdr::uptr_access_t<" << decl_type(f.decl) << ", &"
       << u.id << "::u_, \"" << f.decl.id << "\">";
  else
    os << "xdr::field_access_t<&" << u.id << "::" << f.decl.id
       << "_, \"" << f.decl.id << "\">";
  return os.str();
}

void
gen_union_traits(std::ostream &os, const rpc_union &u)
{
  // _xdr_case_values
  std::vector<std::string> cases;
  for (const auto &f : u.fields)
    cases.insert(cases.end(), f.cases.begin(), f.cases.end());
  os << nl << "static constexpr std::array<" << map_tag(u.tagtype)
     << ", " << cases.size() << "> _xdr_case_values = {";
  for (const auto &c : cases)
    os << nl << "  " << map_tag(c) << ",";
  os << nl << "};";

  // _xdr_union_fieldno
  os << nl << "static constexpr int _xdr_union_fieldno("
     << map_type(u.tagtype) << " _which) {"
     << nl.open << "switch (xdr::xdr_traits<" << u.tagtype
     << ">::case_type{_which}) {";
  ++nl;
  for (const auto &f : u.fields) {
    print_cases(os, f);
    os << nl << "return " << f.fieldno << ";";
  }
  if (!u.hasdefault)
    os << nl.outdent << "default:"
       << nl << "return -1;";
  os << nl.close << "}"
     << nl.close << "}";

  // _xdr_union_traits
  os << nl << "using _xdr_union_traits ="
     << nl << "  xdr::xdr_union_common<" << u.id << ", "
     << (u.hasdefault ? "true" : "false")
     << ", \"" << u.id << "\","
     << nl << "    xdr::field_access_t<&" << u.id << "::" << u.tagid << "_"
     << ", \"" << u.tagid << "\">,"
     << nl << "    _xdr_union_fieldno";
  for (const auto &f : u.fields)
    if (f.fieldno > 0)
      os << "," << nl << "    " << union_field_access(u, f);
  os << ">;" << endl;
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

  // Union tag field
  os << nl.outdent << "private:"
     << nl << map_type(u.tagtype) << " " << u.tagid << "_;";

  // Union arm fields
  if (u.maxfield > 0) {
    if (opt_uptr)
      os << nl << "void *u_ = nullptr;"
	 << endl;
    else {
      os << nl << "union {";
      ++nl;
      for (const rpc_ufield &f : u.fields)
	if (f.decl.type != "void")
	  os << nl << decl_type(f.decl) << ' ' << f.decl.id << "_;";
      os << nl.close << "};";
    }
  }

  os << endl
     << nl.outdent << "public:";

  gen_union_traits(os, u);

  // Default constructor
  os << nl << "explicit " << u.id << "(" << map_type(u.tagtype)
     << " _which = {}) : " << u.tagid << "_(_which) {"
     << nl.open << "_xdr_union_traits::with_current_arm(*this, [this](auto f) {"
     << nl << "  _xdr_union_traits::construct_field(f, *this);"
     << nl << "});"
     << nl.close << "}";

  // Copy/move constructor
  os << nl << u.id << "(const " << u.id << " &source) : "
     << u.tagid << "_(source." << u.tagid << "_) {"
     << nl.open << "_xdr_union_traits::with_current_arm(*this, [&](auto f) {"
     << nl << "  _xdr_union_traits::construct_field(f, *this, f(source));"
     << nl << "});"
     << nl.close << "}";
  os << nl << u.id << "(const " << u.id << " &&source) : "
     << u.tagid << "_(source." << u.tagid << "_) {"
     << nl.open << "_xdr_union_traits::with_current_arm(*this, [&](auto f) {"
     << nl << "  _xdr_union_traits::construct_field(f, *this, "
     << "f(std::move(source)));"
     << nl << "});"
     << nl.close << "}";

  // Destructor
  os << nl << "~" << u.id << "() {"
     << nl.open << "_xdr_union_traits::with_current_arm(*this, [this](auto f) {"
     << nl << "  _xdr_union_traits::destroy_field(f, *this);"
     << nl << "});"
     << nl.close << "}" << endl;

  // Assignment
  os << nl << u.id << " &operator=(const " << u.id << "&s) {"
     << nl.open << "return _xdr_union_traits::assign(*this, s);"
     << nl.close << "}";
  os << nl << u.id << " &operator=(const " << u.id << "&&s) {"
     << nl.open << "return _xdr_union_traits::assign(*this, std::move(s));"
     << nl.close << "}";
  os << endl;

  // Get/set discriminant
  os << nl << u.tagtype << " " << u.tagid << "() const { return "
     << u.tagid << "_; }"
     << nl << u.id << " &" << u.tagid << "(" << u.tagtype << " _v) {"
     << nl.open << "_xdr_union_traits::check_tag(_v);"
     << nl << "_xdr_union_traits::set_tag(*this, _v);"
     << nl << "return *this;"
     << nl.close << "}";

  // Access union arms
  for (const auto &f : u.fields) {
    if (f.decl.type == "void")
      continue;
    os << endl;
    for (string cnst : {"", "const "})
      os << nl << cnst << decl_type(f.decl) << " &" << f.decl.id
	 << "() " << cnst << "{"
	 << nl.open << "return _xdr_union_traits::arm<"
	 << f.fieldno << ">(*this);"
	 << nl.close << "}";
  }

  top_material
    << "template<> struct xdr_traits<" << cur_scope()
    << "> : " << cur_scope() << "::_xdr_union_traits {};" << endl;

  os << nl.close << "}";

  scope.pop_back();
}

void
gen_vers(std::ostream &os, const rpc_program &u, const rpc_vers &v)
{
  os << "struct " << v.id << " {"
     << nl.open << "static constexpr const std::uint32_t program = "
     << u.val << ";"
     << nl << "static constexpr const char *program_name() { return \""
     << u.id << "\"; }"
     << nl << "static constexpr const std::uint32_t version = " << v.val << ";"
     << nl << "static constexpr const char *version_name() { return \""
     << v.id << "\"; }";

  for (const rpc_proc &p : v.procs) {
    string call = "c." + p.id + "(std::forward<A>(a)...)";
    os << endl
       << nl << "struct " << p.id << "_t {"
       << nl.open << "using interface_type = " << v.id << ";"
       << nl << "static constexpr const std::uint32_t proc = " << p.val << ";"
       << nl << "static constexpr const char *proc_name() { return \""
       << p.id << "\"; }";
    if (p.arg.size() == 0)
      os << nl << "using arg_type = void;";
    else if (p.arg.size() == 1)
      os << nl << "using arg_type = " << p.arg[0] << ';';
    os << nl << "using arg_tuple_type = std::tuple<";
    for (size_t i = 0; i < p.arg.size(); ++i) {
      if (i)
	os << ", ";
      os << p.arg[i];
    }
    os << ">;";
    os << nl << "using res_type = " << p.res << ";"
       << nl << "using res_wire_type = "
       << (p.res == "void" ? "xdr::xdr_void" : p.res) << ";"
       << endl
       << nl << "template<typename C, typename...A>"
       << nl << "static decltype(auto) dispatch(C &&c, A &&...a) {"
       << nl << "  return " << call << ";"
       << nl << "}";

    os << nl.close << "};";
  }

  os << endl
     << nl << "template<typename T, typename...A> static bool"
     << nl << "call_dispatch(T &&t, std::uint32_t proc, A &&...a) {"
     << nl.open << "switch(proc) {";
  for (const rpc_proc &p : v.procs) {
    os << nl << "case " << p.val << ":"
       << nl << "  t.template dispatch<" << p.id
       << "_t";
    os << ">(std::forward<A>(a)...);"
       << nl << "  return true;";
  }
  os << nl << "}"
     << nl << "return false;"
     << nl.close << "}";

  // client
  os << endl
     << nl << "template<typename _XDR_INVOKER> struct _xdr_client {";
  ++nl;
  os << nl << "_XDR_INVOKER _xdr_invoker_;"
     << nl << "template<typename...ARGS> _xdr_client(ARGS &&...args)"
     << nl << "  : _xdr_invoker_(std::forward<ARGS>(args)...) {}";
  for (const rpc_proc &p : v.procs) {
    string invoke = string("_xdr_invoker_->template invoke<")
      + p.id + "_t";
    for (const string &a : p.arg) {
      invoke += ",\n                                            ";
      invoke += a;
    }
    invoke += ">(\n             std::forward<_XDR_ARGS>(_xdr_args)...)";
    os << endl << nl << "template<typename..._XDR_ARGS>"
       << nl << "decltype(auto) " << p.id << "(_XDR_ARGS &&..._xdr_args) {"
       << nl << "  return " << invoke << ";"
       << nl << "}";
  }
  os << nl.close << "};";

  os << nl.close << "};";
}

void
gen(std::ostream &os, const rpc_program &u)
{
  bool first{true};
  for (const rpc_vers &v : u.vers) {
    if (first)
      first = false;
    else
      os << endl << nl;
    gen_vers(os, u, v);
  }
}

}

void
gen_hh(std::ostream &os)
{
  os << "// -*- C++ -*-"
     << nl << "// Automatically generated from " << input_file << '.' << endl
     << "// DO NOT EDIT or your changes may be overwritten" << endl;

  string gtok = guard_token("");
  os << nl << "#ifndef " << gtok
     << nl << "#define " << gtok << " 1" << endl
     << nl << "#include <xdrpp/types.h>";

  int last_type = -1;

  os << nl;
  for (const auto &s : symlist) {
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
      os << "constexpr const std::uint32_t " << s.sconst->id << " = "
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
      gen(os, *s.sprogram);
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

