
#include <xdrpp/arpc.h>

namespace xdr {

void
arpc_server::receive(msg_sock *ms, msg_ptr buf)
{
  dispatch(nullptr, std::move(buf), msg_sock_put_t{ms});
}

arpc_sock::arpc_sock(pollset &ps, sock_t s)
  : ms_(new msg_sock(ps, s, std::bind(&arpc_sock::receive, this,
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
