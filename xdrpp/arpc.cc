
#include <xdrpp/arpc.h>

namespace xdr {

void
reply_cb_base::reject(accept_stat stat)
{
  hdr_.body.rbody().stat(MSG_ACCEPTED).areply().reply_data.stat(stat);
  send_reply(xdr_void{});
}

void
reply_cb_base::reject(auth_stat stat)
{
  hdr_.body.rbody().stat(MSG_DENIED).rreply().stat(AUTH_ERROR).rj_why() = stat;
  send_reply(xdr_void{});
}


arpc_sock::arpc_sock(pollset &ps, int fd)
  : ms_(new msg_sock(ps, fd, std::bind(&arpc_sock::receive, this,
				       std::placeholders::_1)))
{
}

void
arpc_sock::prepare_call(rpc_msg &hdr, std::uint32_t prog,
			std::uint32_t vers, std::uint32_t proc)
{
  while(calls_.find(xid_counter_) != calls_.end())
    ++xid_counter_;
  hdr.xid = xid_counter_++;
  hdr.body.mtype(CALL);
  hdr.body.cbody().rpcvers = 2;
  hdr.body.cbody().prog = prog;
  hdr.body.cbody().vers = vers;
  hdr.body.cbody().proc = proc;
}

void
arpc_sock::receive(msg_ptr buf)
{
}


}
