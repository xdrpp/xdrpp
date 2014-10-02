
#include <iostream>
#include <xdrpp/server.h>

namespace xdr {

bool xdr_trace_server = std::getenv("XDR_TRACE_SERVER");

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
rpc_server_base::register_service_base(service_base *s)
{
  servers_[s->prog_][s->vers_].reset(s);
}

msg_ptr
rpc_server_base::dispatch(msg_ptr m)
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

rpc_tcp_listener::rpc_tcp_listener(unique_fd &&fd, bool reg)
  : listen_fd_(fd ? std::move(fd) : tcp_listen()),
    use_rpcbind_(reg)
{
  set_close_on_exec(listen_fd_.get());
  ps_.fd_cb(listen_fd_.get(), pollset::Read,
	    std::bind(&rpc_tcp_listener::accept_cb, this));
}

rpc_tcp_listener::~rpc_tcp_listener()
{
  ps_.fd_cb(listen_fd_.get(), pollset::Read);
}

void
rpc_tcp_listener::accept_cb()
{
  int fd = accept(listen_fd_.get(), nullptr, 0);
  if (fd == -1) {
    std::cerr << "rpc_tcp_listener: accept: " << std::strerror(errno)
	      << std::endl;
    return;
  }
  set_close_on_exec(fd);
  msg_sock *ms = new msg_sock(ps_, fd);
  ms->setrcb(std::bind(&rpc_tcp_listener::receive_cb, this, ms,
		       std::placeholders::_1));
}

void
rpc_tcp_listener::receive_cb(msg_sock *ms, msg_ptr mp)
{
  if (!mp) {
    delete ms;
    return;
  }
  try {
    ms->putmsg(dispatch(std::move(mp)));
  }
  catch (const xdr_runtime_error &e) {
    std::cerr << e.what() << std::endl;
    delete ms;
  }
}

void
rpc_tcp_listener::run()
{
  while (ps_.pending())
    ps_.poll();
}

}
