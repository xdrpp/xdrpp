// -*- C++ -*-

#ifndef _XDRC_SRPC_H_HEADER_INCLUDED_
#define _XDRC_SRPC_H_HEADER_INCLUDED_ 1

//! \file srpc.h Simple synchronous RPC functions.

#include <xdrc/server.h>

namespace xdr {

msg_ptr read_message(int fd);
void write_message(int fd, const msg_ptr &m);

void prepare_call(uint32_t prog, uint32_t vers, uint32_t proc, rpc_msg &hdr);
template<typename P> inline void
prepare_call(rpc_msg &hdr)
{
  prepare_call(P::interface_type::program, P::interface_type::version,
	       P::proc, hdr);
}


//! Synchronous file descriptor demultiplexer.
class synchronous_client_base {
  const int fd_;
  std::uint32_t xid_{0};

  static void moveret(std::unique_ptr<xdr_void> &) {}
  template<typename T> static T &&moveret(T &t) { return std::move(t); }

public:
  synchronous_client_base(int fd) : fd_(fd) {}

  template<typename P> typename P::res_type invoke() {
    return this->template invoke<P>(xdr::xdr_void{});
  }

  template<typename P> typename std::conditional<
    std::is_void<typename P::res_type>::value, void,
    std::unique_ptr<typename P::res_type>>::type
  invoke(const typename P::arg_wire_type &a) {
    std::uint32_t xid = ++xid_;
    rpc_msg hdr;
    prepare_call<P>(hdr);

    write_message(fd_, xdr_to_msg(hdr, a));
    msg_ptr m = read_message(fd_);

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

    std::unique_ptr<typename P::res_type> r (new typename P::res_type);
    archive(g, *r);
    if (g.p_ != g.e_)
      throw xdr_runtime_error("synchronous_client: "
			      "did not consume whole message");
    return moveret(r);
  }
};

template<typename T> using srpc_client =
  typename T::template client<synchronous_client_base>;


//! Attach an RPC server to a stream socket.  No procedures will be
//! implemented by the RPC server until interface objects are
//! reigstered with \c register_server.
class srpc_server : public rpc_server {
  const int fd_;
  bool close_on_destruction_;

public:
  srpc_server(int fd, bool close_on_destruction = true)
    : fd_(fd), close_on_destruction_(close_on_destruction) {}
  ~srpc_server() { if (close_on_destruction_) close(fd_); }

  //! Start serving requests.  (Loops until an exception.)
  void run();
};

}

#endif // !_XDRC_SRPC_H_HEADER_INCLUDED_
