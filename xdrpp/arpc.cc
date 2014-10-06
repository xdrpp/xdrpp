
#include <xdrpp/arpc.h>

namespace xdr {

rpc_sock::rpc_sock(pollset &ps, int fd)
  : ms_(new msg_sock(ps, fd, std::bind(&rpc_sock::receive, this,
				       std::placeholders::_1)))
{
}

void
rpc_sock::receive(msg_ptr buf)
{
}


}
