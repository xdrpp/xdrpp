/* -*-fundamental-*- */

%{
#define YYSTYPE YYSTYPE
#include "xdrc.h"
#include "parse.hh"

string filename = "(stdin)";
int lineno;
vec<string> litq;

inline string
msg_yytext(const char *msg)
{
  return string(yytext) + " '" + yytext + "'";
}
%}

%option nounput
%option noyywrap

ID	[a-zA-Z_][a-zA-Z_0-9]*
WSPACE	[ \t]

%x GFILE GNL

%%
\n		++lineno;
{WSPACE}+	/* discard */;
^%.*		litq.emplace_back (yytext + 1);

^#pragma\ 	{ BEGIN (GNL); }
^#\ *[0-9]+\ *	{ lineno = atoi (yytext + 1); BEGIN (GFILE); }
<GFILE>.*	{ filename.assign(yytext+1, yyleng-2); BEGIN (GNL); }
<GFILE>\n	{ filename = "(stdin)"; BEGIN (GNL); }
<GNL>.		/* discard */;
<GNL>\n		BEGIN (0);

const		return T_CONST;
struct		return T_STRUCT;
union		return T_UNION;
enum		return T_ENUM;
typedef		return T_TYPEDEF;
program		return T_PROGRAM;
namespace       return T_NAMESPACE;

unsigned	return T_UNSIGNED;
int		return T_INT;
hyper		return T_HYPER;
double		return T_DOUBLE;
quadruple	return T_QUADRUPLE;
void		{ yylval.string_ = yytext; return T_VOID; }

version		return T_VERSION;
switch		return T_SWITCH;
case		return T_CASE;
default		return T_DEFAULT;

opaque		{ yylval.string_ = yytext; return T_OPAQUE; }
string		{ yylval.string_ = yytext; return T_STRING; }

array		|
bytes		|
destroy		|
free		|
getpos		|
inline		|
pointer		|
reference	|
setpos		|
sizeof		|
vector		{ yyerror(msg_yytext("illegal use of reserved word")); }

{ID}		{ yylval.string_ = yytext; return T_ID; }
[+-]?[0-9]+	|
[+-]?0x[0-9a-fA-F]+	{ yylval.string_ = yytext; return T_NUM; }

[=;{}<>\[\]*,:()] return yytext[0];

[^ \t\n0-9a-zA-Z_=;{}<>\[\]*,:()][^ \t\n0-9a-zA-Z_]*	|
[0-9]*		{ yyerror(msg_yytext("syntax error at")); }
%%

#include <iostream>

int
yyerror(string msg)
{
  std::cerr << filename << ":" << lineno << ": " << msg << "\n";
  exit(1);
  // return 1;
}

int
yywarn(string msg)
{
  std::cerr << filename << ":" << lineno << ": Warning: " << msg << "\n";
  return 0;
}
