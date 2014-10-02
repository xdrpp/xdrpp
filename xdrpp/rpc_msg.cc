
#include <xdrpp/exception.h>

namespace xdr {

using std::string;

const char *
rpc_errmsg(accept_stat ev)
{
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
    return "unknown accept_stat error";
  }
}

const char *
rpc_errmsg(auth_stat ev)
{
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
    return "auth_stat error";
  }
}

const char *
rpc_errmsg(reject_stat ev)
{
  switch (ev) {
  case RPC_MISMATCH:
    return "rpcvers field mismatch";
  case AUTH_ERROR:
    return rpc_errmsg(AUTH_FAILED);
  default:
    return "unknown reject_stat status";
  }
}

xdr_call_error::xdr_call_error(accept_stat ev)
  : xdr_runtime_error(rpc_errmsg(ev)), accept_(ev), type_(ACCEPT_STAT) {}
xdr_call_error::xdr_call_error(auth_stat ev)
  : xdr_runtime_error(rpc_errmsg(ev)), auth_(ev), type_(AUTH_STAT) {}
xdr_call_error::xdr_call_error(reject_stat ev)
  : xdr_runtime_error(rpc_errmsg(ev)), reject_(ev), type_(REJECT_STAT) {}

void
check_call_hdr(const rpc_msg &hdr)
{
  if (hdr.body.mtype() != REPLY)
    throw xdr_runtime_error("call received when reply expected");
  switch (hdr.body.rbody().stat()) {
  case MSG_ACCEPTED:
    if (hdr.body.rbody().areply().reply_data.stat() == SUCCESS)
      return;
    throw xdr_call_error(hdr.body.rbody().areply().reply_data.stat());
  case MSG_DENIED:
    if (hdr.body.rbody().rreply().stat() == AUTH_ERROR)
      throw xdr_call_error(hdr.body.rbody().rreply().rj_why());
    throw xdr_call_error(hdr.body.rbody().rreply().stat());
  default:
    throw xdr_runtime_error("check_call_hdr: garbage reply_stat");
  }
}

}
