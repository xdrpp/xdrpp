// -*- C++ -*-

//! \file srpc.h Simple synchronous RPC functions.

#include <xdrc/marshal.h>
#include <xdrc/printer.h>
#include <xdrc/rpc_msg.hh>
#include <map>

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

struct server_base {
  const uint32_t prog_;
  const uint32_t vers_;

  server_base(uint32_t prog, uint32_t vers) : prog_(prog), vers_(vers) {}
  virtual ~server_base() {}
  virtual msg_ptr process(const rpc_msg &hdr, xdr_get &g) = 0;
};

template<typename T> struct synchronous_server : server_base {
  using interface = typename T::rpc_interface_type;
  T &server_;

  synchronous_server(T &server)
    : server_base(interface::program, interface::version), server_(server) {}

  msg_ptr process(const rpc_msg &chdr, xdr_get &g) override {
    if (chdr.body.mtype() != CALL
	|| chdr.body.cbody().rpcvers != 2
	|| chdr.body.cbody().prog != prog_
	|| chdr.body.cbody().vers != vers_)
      return nullptr;

    rpc_msg rhdr;
    rhdr.xid = chdr.xid;
    rhdr.body.mtype(REPLY).rbody().stat(MSG_ACCEPTED)
      .areply().reply_data.stat(SUCCESS);

    msg_ptr ret;
    if (!interface::call_dispatch(*this, chdr.body.cbody().proc,
				  g, rhdr, ret)) {
      rhdr.body.rbody().areply().reply_data.stat(PROC_UNAVAIL);
      ret = xdr_to_msg(rhdr);
    }
    return ret;
  }

  template<typename P> typename std::enable_if<
    !std::is_same<void, typename P::res_type>::value>::type
  dispatch(xdr_get &g, rpc_msg rhdr, msg_ptr &ret) {
    std::unique_ptr<typename P::arg_wire_type>
      arg(new typename P::arg_wire_type);
    archive(g, *arg);
    if (g.p_ != g.e_)
      throw xdr_bad_message_size("synchronous_server did not consume"
				 " whole message");
    std::unique_ptr<typename P::res_type> res =
      P::dispatch_dropvoid(server_, std::move(arg));
    ret = xdr_to_msg(rhdr, *res);
  }
  template<typename P> typename std::enable_if<
    std::is_same<void, typename P::res_type>::value>::type
  dispatch(xdr_get &g, rpc_msg rhdr, msg_ptr &ret) {
    std::unique_ptr<typename P::arg_wire_type>
      arg(new typename P::arg_wire_type);
    archive(g, *arg);
    if (g.p_ != g.e_)
      throw xdr_bad_message_size("synchronous_server did not consume"
				 " whole message");
    P::dispatch_dropvoid(server_, std::move(arg));
    ret = xdr_to_msg(rhdr);
  }
};

class rpc_server {
  std::map<uint32_t, std::map<uint32_t, std::unique_ptr<server_base>>> servers_;
  void register_server_base(server_base *s);

public:
  //! Add objects implementing RPC program interfaces to the server.
  template<typename T> void register_service(T &t) {
    register_server_base(new synchronous_server<T>(t));
  }
  msg_ptr dispatch(msg_ptr m);
};

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
