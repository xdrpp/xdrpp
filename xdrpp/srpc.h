// -*- C++ -*-

#ifndef _XDRPP_SRPC_H_HEADER_INCLUDED_
#define _XDRPP_SRPC_H_HEADER_INCLUDED_ 1

//! \file srpc.h Simple synchronous RPC functions.

#include <xdrpp/exception.h>
#include <xdrpp/server.h>

namespace xdr {

extern bool xdr_trace_client;

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

  static void moveret(std::unique_ptr<xdr_void> &) {}
  template<typename T> static T &&moveret(T &t) { return std::move(t); }

public:
  synchronous_client_base(int fd) : fd_(fd) {}
  synchronous_client_base(const synchronous_client_base &c) : fd_(c.fd_) {}

  template<typename P> typename P::res_type invoke() {
    return this->template invoke<P>(xdr::xdr_void{});
  }

  template<typename P> typename std::conditional<
    std::is_void<typename P::res_type>::value, void,
    std::unique_ptr<typename P::res_type>>::type
  invoke(const typename P::arg_wire_type &a) {
    rpc_msg hdr;
    prepare_call<P>(hdr);
    uint32_t xid = hdr.xid;

    if (xdr_trace_client) {
      std::string s = "CALL ";
      s += P::proc_name;
      s += " -> [xid " + std::to_string(xid) + "]";
      std::clog << xdr_to_string(a, s.c_str());
    }
    write_message(fd_, xdr_to_msg(hdr, a));
    msg_ptr m = read_message(fd_);

    xdr_get g(m);
    archive(g, hdr);
    check_call_hdr(hdr);
    if (hdr.xid != xid)
      throw xdr_runtime_error("synchronous_client: unexpected xid");

    std::unique_ptr<typename P::res_wire_type> r{new typename P::res_wire_type};
    archive(g, *r);
    if (g.p_ != g.e_)
      throw xdr_bad_message_size("synchronous_client: "
				 "did not consume whole message");
    if (xdr_trace_client) {
      std::string s = "REPLY ";
      s += P::proc_name;
      s += " <- [xid " + std::to_string(xid) + "]";
      std::clog << xdr_to_string(*r, s.c_str());
    }
    return moveret(r);
  }
};

//! Create an RPC client from an interface type and connected stream
//! socket.  Note that the file descriptor is not closed afterwards
//! (as you may wish to use different interfaces over the same file
//! descriptor).  A simple example looks like this:
//!
//! \code
//!    unique_fd fd = tcp_connect_rpc(argc > 2 ? argv[2] : nullptr,
//!                                   MyProg1::program, MyProg1::version);
//!    srpc_client<MyProg1> c{fd.get()};
//!    unique_ptr<big_string> result = c.hello(5);
//! \endcode
template<typename T> using srpc_client =
  typename T::template client<synchronous_client_base>;


//! Attach a RPC services to a single, connected stream socket.  No
//! procedures will be implemented by the RPC server until interface
//! objects are reigstered with \c register_server.
class srpc_server : public rpc_server_base {
  const int fd_;
  bool close_on_destruction_;

public:
  srpc_server(int fd, bool close_on_destruction = true)
    : fd_(fd), close_on_destruction_(close_on_destruction) {}
  ~srpc_server() { if (close_on_destruction_) close(fd_); }

  //! Add objects implementing RPC program interfaces to the server.
  template<typename T> void register_service(T &t) {
    register_service_base(new synchronous_server<T>(t));
  }

  //! Start serving requests.  (Loops until an exception.)
  void run();
};

}

#endif // !_XDRPP_SRPC_H_HEADER_INCLUDED_
