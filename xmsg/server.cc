
#include <iostream>
#include <xdrc/server.h>

namespace xdr {

namespace {

rpc_msg &
rpc_mkerr(rpc_msg &m, accept_stat stat)
{
  m.body.mtype(REPLY).rbody().stat(MSG_ACCEPTED).areply()
    .reply_data.stat(stat);
  return m;
}

rpc_msg &
rpc_mkerr(rpc_msg &m, reject_stat stat)
{
  m.body.mtype(REPLY).rbody().stat(MSG_DENIED).rreply().stat(stat);
  switch(stat) {
  case RPC_MISMATCH:
    m.body.rbody().rreply().mismatch_info().low = 2;
    m.body.rbody().rreply().mismatch_info().high = 2;
    break;
  case AUTH_ERROR:
    m.body.rbody().rreply().rj_why() = AUTH_FAILED;
    break;
  }
  return m;
}

#if 0
rpc_msg &
rpc_mkerr(rpc_msg &m, auth_stat stat)
{
  m.body.mtype(REPLY).rbody().stat(MSG_DENIED).rreply()
    .stat(AUTH_ERROR).rj_why() = stat;
  return m;
}
#endif

}

void
rpc_server::register_server_base(server_base *s)
{
  servers_[s->prog_][s->vers_].reset(s);
}

msg_ptr
rpc_server::dispatch(msg_ptr m)
{
  xdr_get g(m);
  rpc_msg hdr;
  archive(g, hdr);
  if (hdr.body.mtype() != CALL)
    throw xdr_runtime_error("rpc_server received non-CALL message");

  if (hdr.body.cbody().rpcvers != 2)
    return xdr_to_msg(rpc_mkerr(hdr, RPC_MISMATCH));

  auto prog = servers_.find(hdr.body.cbody().prog);
  if (prog == servers_.end())
    return xdr_to_msg(rpc_mkerr(hdr, PROG_UNAVAIL));

  auto vers = prog->second.find(hdr.body.cbody().vers);
  if (vers == prog->second.end()) {
    rpc_mkerr(hdr, PROG_MISMATCH);
    hdr.body.rbody().areply().reply_data.mismatch_info().low =
      prog->second.cbegin()->first;
    hdr.body.rbody().areply().reply_data.mismatch_info().high =
      prog->second.crbegin()->first;
    return xdr_to_msg(hdr);
  }

  try { return vers->second->process(hdr, g); }
  catch (const xdr_runtime_error &e) {
    std::cerr << xdr_to_string(hdr, e.what());
    return xdr_to_msg(rpc_mkerr(hdr, GARBAGE_ARGS));
  }
}

}
