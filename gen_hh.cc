
#include <cassert>
#include <ctype.h>
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

string
decl_type(const rpc_decl &d)
{
  string type = map_type(d.type);

  if (type == "string")
    return string("xdr::xstring<") + d.bound + ">";

  switch (d.qual) {
  case rpc_decl::PTR:
    return string("std::unique_ptr<") + type + ">";
  case rpc_decl::ARRAY:
    return string("std::array<") + type + "," + d.bound + ">";
  case rpc_decl::VEC:
    return string("xdr::xvector<") + type +
      (d.bound.empty() ? ">" : string(",") + d.bound + ">");
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
make_nvp(std::ostream &os, const string &name, bool first, bool last)
{
  if (first)
    os << nl << "_archive(";
  else
    os << nl << "         ";
  os << "xdr::prepare_field<_Archive>::nvp(\"" << name << "\", " << name << ")";
  if (last)
    os << ");";
  else
    os << ",";
}

void
gen(std::ostream &os, const rpc_struct &s)
{
  os << "struct " << id_space(s.id) << '{';
  ++nl;
  for(auto d : s.decls)
    os << nl << decl_type(d) << ' ' << d.id << ';';
  os << endl;

  for (string decl :
    { "template<class _Archive> void save(_Archive &_archive) const {",
	"template<class _Archive> void load(_Archive &_archive) {" } ) {
    os << nl << decl;
    ++nl;
    for (size_t i = 0; i < s.decls.size(); ++i)
      make_nvp(os, s.decls[i].id, i == 0, i + 1 == s.decls.size());
    os << nl.close << "}";
  }
  
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
  os << "class " << u.id << " {"
    //<< nl.open << map_type(u.tagtype) << ' ' << u.tagid << "_;"
     << nl.open << "std::uint32_t " << u.tagid << "_;"
     << nl << "union {";
  ++nl;
  for (rpc_ufield f : u.fields)
    if (f.decl.type != "void")
      os << nl << decl_type(f.decl) << ' ' << f.decl.id << "_;";
  os << nl.close << "};" << endl;

  os << nl.outdent << "public:";
  os << nl << "static_assert (sizeof (" << u.tagtype << ") <= 4,"
    " \"union discriminant must be 4 bytes\");";

  // _discriminant
  os << nl << "std::uint32_t _discriminant() const { return "
     << u.tagid << "_; }";

#if 0
  // _field_names
  os << nl << "static constexpr const char *_field_names[] = {"
     << nl << "  nullptr,";
  for (const rpc_ufield &uf : u.fields)
    if (uf.decl.type != "void")
      os << nl << "  \"" << uf.decl.id << "\",";
  os << nl << "};";
#endif

  // _field_number
  os << nl
     << "static constexpr int _field_number(std::uint32_t _which) {";
  union_function(os, u, "_which", [](const rpc_ufield *uf) {
      using std::to_string;
      if (uf)
	return to_string(uf->fieldno);
      else
	return to_string(-1);
    });
  os << nl << "}";

  // _field_name
  os << nl
     << "static constexpr const char *_field_name(std::uint32_t _which) {";
  union_function(os, u, "_which", [](const rpc_ufield *uf) {
      using std::to_string;
      if (uf && uf->decl.type != "void")
	return string("\"") + uf->decl.id + "\"";
      else
	return string("nullptr");
    });
  os << nl << "}";

  // _field_name
  os << nl << "const char *_field_name() const { return _field_name("
     << u.tagid << "_); }";

  // _on_field_ptr
  os << nl << "template<typename F, typename T> static void"
     << nl << "_on_field_ptr(F &f, T &&t, std::uint32_t _which) {"
     << nl.open << pswitch(u, "_which");
  for (rpc_ufield f : u.fields) {
    for (string c : f.cases)
      os << nl << map_case(c);
    if (f.decl.type == "void")
      os << nl << "  f(std::forward<T>(t));";
    else
      os << nl << "  f(std::forward<T>(t), &"
	 << u.id << "::" << f.decl.id << "_);";
    os << nl << "  break;";
  }
  os << nl << "}";
  if (!u.hasdefault)
    os << nl << "f();";
  os << nl.close << "}"
     << endl;

#if 0
  // _tag_is_valid
  if (u.hasdefault)
    os << nl << "static constexpr bool _tag_is_valid("
       << u.tagtype << ") { return true; }";
  else
    os << nl << "static bool _tag_is_valid(" << u.tagtype
       << " t) { return _field_number(t) >= 0; }"
       << endl;
#endif

  // Default constructor
  os << nl << u.id << "(" << map_type(u.tagtype) << " _t = "
     << map_type(u.tagtype) << "{}) : " << u.tagid << "_(_t) {"
     << nl.open << "_on_field_ptr(xdr::case_constructor, this, "
     << u.tagid << "_);"
     << nl.close << "}";

  // Copy/move constructor
  os << nl << u.id << "(const " << u.id << " &_source) : "
     << u.tagid << "_(_source." << u.tagid << "_) {"
     << nl.open << "xdr::case_construct_from _dest{this};"
     << nl << "_on_field_ptr(_dest, _source, " << u.tagid << "_);"
     << nl.close << "}";
  os << nl << u.id << "(" << u.id << " &&_source) : "
     << u.tagid << "_(_source." << u.tagid << "_) {"
     << nl.open << "xdr::case_construct_from _dest{this};"
     << nl << "_on_field_ptr(_dest, std::move(_source), " << u.tagid << "_);"
     << nl.close << "}";

  // Destructor
  os << nl << "~" << u.id
     << "() { _on_field_ptr(xdr::case_destroyer, this, "
     << u.tagid << "_); }";

  // Assignment
  os << nl << u.id << " &operator=(const " << u.id << " &_source) {"
     << nl.open << "if (_field_number(" << u.tagid
     << "_) == _field_number(_source." << u.tagid << "_)) {"
     << nl << "  xdr::case_assign_from _dest{this};"
     << nl << "  _on_field_ptr(_dest, _source, " << u.tagid << "_);"
     << nl << "}"
     << nl << "else {"
     << nl.open << "this->~" << u.id << "();"
     << nl << u.tagid << "_ = std::uint32_t(-1);" // might help with exceptions
     << nl << "xdr::case_construct_from _dest{this};"
     << nl << "_on_field_ptr(_dest, _source, " << u.tagid << "_);"
     << nl.close << "}"
     << nl << u.tagid << "_ = _source." << u.tagid << "_;"
     << nl << "return *this;"
     << nl.close << "}";
  os << nl << u.id << " &operator=(" << u.id << " &&_source) {"
     << nl.open << "if (_field_number(" << u.tagid
     << "_) == _field_number(_source." << u.tagid << "_)) {"
     << nl << "  xdr::case_assign_from _dest{this};"
     << nl << "  _on_field_ptr(_dest, std::move(_source), " << u.tagid << "_);"
     << nl << "}"
     << nl << "else {"
     << nl.open << "this->~" << u.id << "();"
     << nl << u.tagid << "_ = std::uint32_t(-1);" // might help with exceptions
     << nl << "xdr::case_construct_from _dest{this};"
     << nl << "_on_field_ptr(_dest, std::move(_source), " << u.tagid << "_);"
     << nl.close << "}"
     << nl << u.tagid << "_ = _source." << u.tagid << "_;"
     << nl << "return *this;"
     << nl.close << "}";

  os << endl;

  // Tag getter/setter
  os << nl << map_type(u.tagtype) << ' ' << u.tagid << "() const { return "
     << map_type(u.tagtype) << "(" << u.tagid << "_); }";
  os << nl << "void " << u.tagid << "(" << u.tagtype
     << " _t, bool _validate = true) {"
     << nl.open << "int _fnum = _field_number(_t);"
     << nl << "if (_fnum < 0 && _validate)"
     << nl << "  throw xdr::xdr_bad_value(\"bad value of "
     << u.tagid << " in " << u.id << "::" << u.tagid << "\");"
     << nl << "if (_fnum != _field_number(" << u.tagid << "_)) {"
     << nl.open << "this->~" << u.id << "();"
     << nl << u.tagid << "_ = _t;"
     << nl << "_on_field_ptr(xdr::case_constructor, this, "
     << u.tagid << "_);"
     << nl.close << "}"
     << nl.close << "}" << endl;

  for (const auto &f : u.fields) {
    if (f.decl.type == "void")
      continue;
    for (string cnst : {"", "const "})
      os << nl << cnst << decl_type(f.decl) << " &" << f.decl.id
	 << "() " << cnst << "{"
	 << nl.open << "if (_field_number(" << u.tagid << "_) == "
	 << f.fieldno << ")"
	 << nl << "  return " << f.decl.id << "_;"
	 << nl << "throw xdr::xdr_wrong_union(\""
	 << u.id << ": " << f.decl.id << " accessed when not selected\");"
	 << nl.close << "}";
  }

  os << endl;

  os << nl << "template<class _Archive> void save(_Archive &_archive) const {"
     << nl.open << "_archive(xdr::prepare_field<_Archive>::nvp(\""
     << u.tagid << "\", " << u.tagid << "_));"
     << nl << "xdr::case_save<_Archive> _cs{_archive, _field_name()};"
     << nl << "_on_field_ptr(_cs, this, " << u.tagid << "_);"
     << nl.close << "}";

  os << nl << "template<class _Archive> void load(_Archive &_archive) {"
     << nl.open << "std::uint32_t _which;"
     << nl << "_archive(xdr::prepare_field<_Archive>::nvp(\""
     << u.tagid << "\", _which));"
     << nl << u.tagid << "(" << u.tagtype << "(_which), true);"
     << nl << "xdr::case_load<_Archive> _cl{_archive, _field_name()};"
     << nl << "_on_field_ptr(_cl, this, " << u.tagid << "_);"
     << nl.close << "}";

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
     << nl << "#include <xdrc/types.h>";

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

  os << endl << nl << "#endif // !" << gtok << nl;
}

