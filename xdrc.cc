
#include <cassert>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <getopt.h>
#include <config.h>
#include "xdrc_internal.h"

using std::cout;
using std::cerr;
using std::endl;

extern FILE *yyin;
std::set<string> ids;
string input_file;
string output_file;

void
rpc_decl::set_id(const string &nid)
{
  id = nid;
  string name = string("_") + id + "_t";
  switch (ts_which) {
  case TS_ID:
    break;
  case TS_ENUM:
    ts_enum->id = name;
    break;
  case TS_STRUCT:
    ts_struct->id = name;
    break;
  case TS_UNION:
    ts_union->id = name;
    break;
  }
}

string
strip_directory(string in)
{
  size_t r = in.rfind('/');
  if (r != string::npos)
    return input_file.substr(r+1);
  return in ;
}

string
strip_dot_x(string in)
{
  size_t r = in.size();
  if (r < 2)
    return in;
  if (in.substr(r-2) == ".x")
    return in.substr(0,r-2);
  return in;
}

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

enum opttag {
  OPT_VERSION = 0x100,
  OPT_HELP,
  OPT_CC,
  OPT_HH
};

[[noreturn]] static void
usage(int err = 1)
{
  std::ostream &os = err ? cerr : cout;
  os << "usage: xdrc {-hh|-cc} [-DVAR=VALUE...] [-o OUTFILE] INPUT.x\n";
  exit(err);
}

static const struct option xdrc_options[] = {
  {"version", no_argument, nullptr, OPT_VERSION},
  {"help", no_argument, nullptr, OPT_HELP},
  {"cc", no_argument, nullptr, OPT_CC},
  {"hh", no_argument, nullptr, OPT_HH},
  {nullptr, 0, nullptr, 0}
};

void
gen_cc(std::ostream &)
{
}

int
main(int argc, char **argv)
{
  string cpp_command {CPP_COMMAND};
  void (*gen)(std::ostream &) = nullptr;

  int opt;
  while ((opt = getopt_long_only(argc, argv, "D:o:",
				 xdrc_options, nullptr)) != -1)
    switch (opt) {
    case 'D':
      cpp_command += " -D";
      cpp_command += optarg;
      break;
    case 'o':
      if (!output_file.empty())
	usage();
      output_file = optarg;
      break;
    case OPT_VERSION:
      cout << PACKAGE_STRING << endl;
      exit(0);
      break;
    case OPT_HELP:
      usage(0);
      break;
    case OPT_CC:
      if (gen)
	usage();
      gen = gen_cc;
      cpp_command += " -DXDRC_CC";
      break;
    case OPT_HH:
      if (gen)
	usage();
      gen = gen_hh;
      cpp_command += " -DXDRC_HH";
      break;
    default:
      usage();
      break;
    }

  if (optind + 1 != argc)
    usage();
  if (!gen) {
    cerr << "xdrc: missing language specifier (-cc or -hh)" << endl;
    usage();
  }
  cpp_command += " ";
  cpp_command += argv[optind];
  input_file = argv[optind];
  if (!(yyin = popen(cpp_command.c_str(), "r"))) {
    cerr << "xdrc: command failed: " << cpp_command << endl;
    exit(1);
  }

  yyparse ();
  checkliterals ();

  if (pclose(yyin))
    exit(1);

  if (output_file.empty()) {
    output_file = strip_dot_x(input_file);
    if (output_file == input_file)
      usage();
    output_file = strip_directory(output_file) + ".hh";
  }

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
