// -*- C++ -*-

//! \file srpc.h Simple synchronous RPC functions.

#include <xdrc/marshal.h>
#include <xdrc/rpc_msg.hh>
#include <xdrc/printer.h>
#include <mutex>
#include <condition_variable>

namespace xdr {

msg_ptr read_message(int fd);
void write_message(int fd, const msg_ptr &m);

//! Synchronous file descriptor demultiplexer.
class synchronous_client {
  const int fd_;
  std::uint32_t xid_{0};

  static void moveret(xdr_void &) {}
  template<typename T> static T &&moveret(T &t) { return std::move(t); }

public:
  synchronous_client(int fd) : fd_(fd) {}

  template<typename P> typename P::res_type invoke() {
    return this->template invoke<P>(xdr::xdr_void{});
  }

  template<typename P> typename P::res_type
  invoke(const typename P::arg_wire_type &a) {
    std::uint32_t xid = ++xid_;
    rpc_msg hdr;
    hdr.xid = xid;
    hdr.body.cbody().rpcvers = 2;
    hdr.body.cbody().prog = P::version_type::program;
    hdr.body.cbody().prog = P::version_type::version;
    hdr.body.cbody().prog = P::proc;
    hdr.body.cbody().cred.flavor = AUTH_NONE;
    hdr.body.cbody().verf.flavor = AUTH_NONE;

    msg_ptr m = xdr_to_msg(hdr, a);
    write_message(fd_, m);

    m = read_message(fd_);
    xdr_get g(m);
    archive(g, hdr);
    if (hdr.xid != xid || hdr.body.mtype() != REPLY)
      throw xdr_runtime_error("synchronous_client: unexpected message");
    if (hdr.body.rbody().stat() != MSG_ACCEPTED)
      throw xdr_runtime_error(xdr_to_string(hdr.body.rbody().rreply(),
					    "rejected_reply"));
    if (hdr.body.rbody().areply().reply_data.stat() != SUCCESS)
      throw xdr_runtime_error
	(xdr_to_string(hdr.body.rbody().areply().reply_data, "reply_data"));

    typename P::res_type r;
    archive(g, r);
    if (g.p_ != g.e_)
      throw xdr_runtime_error("synchronous_client: "
			      "did not consume whole message");
    return moveret(r);
  }
};

}
