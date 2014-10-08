
#include <xdrpp/arpc.h>

namespace xdr {

void
reply_cb_base::send_reject(const rpc_msg &hdr)
{
  assert(ms_);
  if (*ms_destroyed_)
    return;
  ms_->putmsg(xdr_to_msg(hdr));
  ms_ = nullptr;
}

void
reply_cb_base::reject(accept_stat stat)
{
  rpc_msg hdr(xid_, REPLY);
  hdr.body.rbody().stat(MSG_ACCEPTED).areply().reply_data.stat(stat);
  send_reject(hdr);
}

void
reply_cb_base::reject(auth_stat stat)
{
  rpc_msg hdr(xid_, REPLY);
  hdr.body.rbody().stat(MSG_DENIED).rreply().stat(AUTH_ERROR).rj_why() = stat;
  send_reject(hdr);
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
