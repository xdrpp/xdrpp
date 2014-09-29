
#include <xdrc/exception.h>

namespace xdr {

class accept_stat_impl : public std::error_category {
public:
  const char* name() const noexcept override { return "RPC"; }
  std::string message(int ev) const override {
    switch(ev) {
    case SUCCESS:
      return "RPC executed successfully";
    case PROG_UNAVAIL:
      return "remote hasn't exported program";
    case PROG_MISMATCH:
      return "remote can't support version #";
    case PROC_UNAVAIL:
      return "program can't support procedure";
    case GARBAGE_ARGS:
      return "procedure can't decode params";
    case SYSTEM_ERR:
      return "RPC system error";
    default:
      return "unknown error";
    }
  }
};

const std::error_category &
accept_stat_category()
{
  static accept_stat_impl val;
  return val;
}


class auth_stat_impl : public std::error_category {
  const char* name() const noexcept override { return "RPC Authentication"; }
  std::string message(int ev) const override {
    switch(ev) {
    case AUTH_OK:
      return "success";
    case AUTH_BADCRED:
      return "bad credential (seal broken)";
    case AUTH_REJECTEDCRED:
      return "client must begin new session";
    case AUTH_BADVERF:
      return "bad verifier (seal broken)";
    case AUTH_REJECTEDVERF:
      return "verifier expired or replayed";
    case AUTH_TOOWEAK:
      return "rejected for security reasons";
    case AUTH_INVALIDRESP:
      return "bogus response verifier";
    case AUTH_FAILED:
      return "reason unknown";
    case AUTH_KERB_GENERIC:
      return "kerberos generic error";
    case AUTH_TIMEEXPIRE:
      return "time of credential expired";
    case AUTH_TKT_FILE:
      return "problem with ticket file";
    case AUTH_DECODE:
      return "can't decode authenticator";
    case AUTH_NET_ADDR:
      return "wrong net address in ticket";
    case RPCSEC_GSS_CREDPROBLEM:
      return "no credentials for user";
    case RPCSEC_GSS_CTXPROBLEM:
      return "problem with context";
    default:
      return "unkown error";
    }
  }
};

const std::error_category &
auth_stat_category()
{
  static auth_stat_impl val;
  return val;
}


}
