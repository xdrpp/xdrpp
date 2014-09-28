
#include <cassert>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <unistd.h>
#include <config.h>
#include "xdrc_internal.h"

using std::cout;
using std::cerr;
using std::endl;

extern FILE *yyin;
std::set<string> ids;
string input_file;
string output_file;
string file_prefix;

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
strip_dot_x(string in)
{
  size_t r = in.size();
  if (r < 2)
    return in;
  if (in.substr(r-2) == ".x")
    return in.substr(0,r-2);
  return in;
}

enum opttag {
  OPT_VERSION = 0x100,
  OPT_HELP,
  OPT_CC,
  OPT_SERVER,
  OPT_HH
};

[[noreturn]] static void
usage(int err = 1)
{
  std::ostream &os = err ? cerr : cout;
  os << "usage: xdrc {-hh|-server} [-DVAR=VALUE...] [-o OUTFILE] INPUT.x\n";
  exit(err);
}

static const struct option xdrc_options[] = {
  {"version", no_argument, nullptr, OPT_VERSION},
  {"help", no_argument, nullptr, OPT_HELP},
  {"hh", no_argument, nullptr, OPT_HH},
  {"server", no_argument, nullptr, OPT_SERVER},
  //{"cc", no_argument, nullptr, OPT_CC},
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
  cpp_command += " -DXDRC=1";
  void (*gen)(std::ostream &) = nullptr;
  string suffix;
  bool noclobber = false;

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
      cpp_command += " -DXDRC_CC=1";
      suffix = ".cc";
      break;
    case OPT_SERVER:
      if (gen)
	usage();
      gen = gen_server;
      cpp_command += " -DXDRC_SERVER=1";
      suffix = ".server.cc";
      noclobber = true;
      break;
    case OPT_HH:
      if (gen)
	usage();
      gen = gen_hh;
      cpp_command += " -DXDRC_HH=1";
      suffix = ".hh";
      break;
    default:
      usage();
      break;
    }

  if (optind + 1 != argc)
    usage();
  if (!gen) {
    cerr << "xdrc: missing language specifier (-hh)" << endl;
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
    file_prefix = strip_dot_x(input_file);

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
