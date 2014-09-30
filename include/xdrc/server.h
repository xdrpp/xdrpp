// -*- C++ -*-

#ifndef _XDRC_SERVER_H_HEADER_INCLUDED_
#define _XDRC_SERVER_H_HEADER_INCLUDED_ 1

#include <xdrc/marshal.h>
#include <xdrc/printer.h>
#include <xdrc/rpc_msg.hh>
#include <map>

namespace xdr {

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

}

#endif // !_XDRC_SERVER_H_HEADER_INCLUDED_
