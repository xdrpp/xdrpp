
#include <cassert>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <config.h>
#include "xdrc_internal.h"

#if MSVC
#include <io.h>
#define CPP_COMMAND "cpp.exe"
#define popen _popen
#define pclose _pclose
#define access _access
#else
#include <unistd.h>
#endif

using std::cout;
using std::cerr;
using std::endl;

extern FILE *yyin;
std::set<string> ids;
string input_file;
string output_file;
string file_prefix;
string server_session;
bool server_ptr;
bool server_async;

string
guard_token(const string &extra)
{
  string in;
  if (!output_file.empty() && output_file != "-")
    in = output_file;
  else
    in = strip_directory(strip_suffix(input_file, ".x")) + extra + ".hh";

  string ret = "__XDR_";
  for (char c : in)
    if (isalnum(c))
      ret += toupper(c);
    else
      ret += "_";
  ret += "_INCLUDED__";
  return ret;
}

void
rpc_decl::set_id(const string &nid)
{
  id = nid;
  string name = string("_") + id + "_t";
  switch (ts_which) {
  case TS_ID:
    break;
  case TS_ENUM:
    ts_enum->id = type = name;
    break;
  case TS_STRUCT:
    ts_struct->id = type = name;
    break;
  case TS_UNION:
    ts_union->id = type = name;
    break;
  }
}

string
strip_directory(string in)
{
  size_t r = in.rfind('/');
  if (r != string::npos)
    return in.substr(r+1);
  return in;
}

string
strip_suffix(string in, string suffix)
{
  size_t r = in.size();
  if (r < suffix.size())
    return in;
  if (in.substr(r-suffix.size()) == suffix)
    return in.substr(0,r-suffix.size());
  return in;
}

typedef void (*codegen)(std::ostream &);

#if !MSVC
[[noreturn]]
#endif // !MSVC
static void
usage(int err = 1)
{
  std::ostream &os = err ? cerr : cout;
  os << R"(
usage: xdrc MODE [OPTIONAL] [-DVAR=VALUE...] [-o OUTFILE] INPUT.x
where MODE is one of:
      -hh           To generate header with XDR and RPC program definitions
      -serverhh     To generate scaffolding for server header file
      -servercc     To generate scaffolding for server cc
      -version      To print version info
and OPTIONAL arguments for -server{hh,cc} can contain:
      -s[ession] T  Use type T to track client sessions
      -p[tr]        To accept arguments by std::unique_ptr
      -a[sync]      To generate arpc server scaffolding (with callbacks)
)";
  exit(err);
}

bool
next_arg(int &argc, char **&argv, std::string &arg)
{
  if (--argc <= 0) {
    arg = "";
    return false;
  } else {
    arg = std::string(*++argv);
    return true;
  }
}

void
parse_options(int &argc,
              char **&argv,
              string &cpp_command,
              codegen &gen,
              string &suffix,
              bool &noclobber)
{
  std::string arg;
  while (next_arg(argc, argv, arg)) {
    if (arg == "-D") {
      if (!next_arg(argc, argv, arg))
        usage();
      cpp_command += " -D";
      cpp_command += arg;

    } else if (arg == "-o") {
      if (!output_file.empty())
        usage();
      if (!next_arg(argc, argv, arg))
        usage();
      output_file = arg;

    } else if (arg == "-version") {
      cout << "xdrc " << PACKAGE_VERSION << endl;
      exit(0);

    } else if (arg == "-help") {
      usage(0);

    } else if (arg == "-serverhh") {
      if (gen)
        usage();
      gen = gen_server;
      cpp_command += " -DXDRC_SERVER=1";
      suffix = ".server.hh";
      noclobber = true;

    } else if (arg == "-servercc") {
      if (gen)
        usage();
      gen = gen_servercc;
      cpp_command += " -DXDRC_SERVER=1";
      suffix = ".server.cc";
      noclobber = true;

    } else if (arg == "-hh") {
      if (gen)
        usage();
      gen = gen_hh;
      cpp_command += " -DXDRC_HH=1";
      suffix = ".hh";

    } else if (arg == "-p" || arg == "-ptr") {
      server_ptr = true;

    } else if (arg == "-a" || arg == "-async") {
      server_async = true;

    } else if (arg == "-s" || arg == "-session") {
      if (!next_arg(argc, argv, arg))
        usage();
      server_session = arg;

    } else if (arg.size() > 0 && arg[0] != '-') {
      if (!gen) {
        cerr << "xdrc: missing mode specifier (e.g., -hh)" << endl;
        usage();
      }
      cpp_command += " ";
      cpp_command += arg;
      input_file = arg;
      return;
    } else {
      usage();
    }
  }
  usage();
}

int
main(int argc, char **argv)
{
  string cpp_command {CPP_COMMAND};
  cpp_command += " -DXDRC=1";
  codegen gen = nullptr;
  string suffix;
  bool noclobber = false;

  parse_options(argc, argv, cpp_command, gen, suffix, noclobber);

  if (!(yyin = popen(cpp_command.c_str(), "r"))) {
    cerr << "xdrc: command failed: " << cpp_command << endl;
    exit(1);
  }

  yyparse ();
  checkliterals ();

  if (pclose(yyin))
    exit(1);

  if (output_file.empty()) {
    output_file = strip_suffix(input_file, ".x");
    if (output_file == input_file)
      usage();
    output_file = strip_directory(output_file);
    output_file += suffix;
  }

  if (noclobber && output_file != "-" && !access(output_file.c_str(), 0)
      && output_file != "/dev/null") {
    cerr << output_file << ": already exists, refusing to clobber it." << endl;
    exit(1);
  }

  if (output_file.size() > suffix.size()
      && output_file.substr(output_file.size() - suffix.size()) == suffix)
    file_prefix = output_file.substr(0, output_file.size() - suffix.size());
  else
    file_prefix = strip_suffix(input_file, ".x");

  if (output_file == "-")
    gen(cout);
  else {
    std::ofstream out(output_file);
    if (out.is_open())
      gen(out);
    else {
      perror(output_file.c_str());
      exit(1);
    }
  }   

  return 0;
}

