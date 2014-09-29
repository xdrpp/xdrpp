// -*- C++ -*-

//! \file srpc.h Simple synchronous RPC functions.

#include <xdrc/marshal.h>
#include <xdrc/rpc_msg.hh>
#include <xdrc/printer.h>
//#include <mutex>
//#include <condition_variable>
#include <map>

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
    hdr.body.cbody().prog = P::interface_type::program;
    hdr.body.cbody().prog = P::interface_type::version;
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

//! Closes file descriptor when done.
class server_fd {
  const int fd_;
  std::map<uint32_t, std::map<uint32_t, std::unique_ptr<server_base>>> servers_;

  void dispatch(msg_ptr m);
  void register_server_base(server_base *s);

public:
  server_fd(int fd) : fd_(fd) {}
  ~server_fd() { close(fd_); }

  template<typename T> void register_server(T &t) {
    register_server_base(new synchronous_server<T>(t));
  }
  void run();
};

}
