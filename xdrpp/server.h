// -*- C++ -*-

//! \file server.h Classes for implementing RPC servers.

#ifndef _XDRPP_SERVER_H_HEADER_INCLUDED_
#define _XDRPP_SERVER_H_HEADER_INCLUDED_ 1

#include <iostream>
#include <xdrpp/marshal.h>
#include <xdrpp/printer.h>
#include <xdrpp/msgsock.h>
#include <xdrpp/socket.h>
#include <xdrpp/rpc_msg.hh>
#include <map>

namespace xdr {

extern bool xdr_trace_server;

//! Structure that gets marshalled as an RPC success header.
struct rpc_success_hdr {
  uint32_t xid;
  explicit constexpr rpc_success_hdr(uint32_t x) : xid(x) {}
};
template<> struct xdr_traits<rpc_success_hdr> : xdr_traits_base {
  static constexpr bool valid = true;
  static constexpr bool is_class = true;
  static constexpr bool is_struct = true;
  static constexpr bool has_fixed_size = true;
  static constexpr std::size_t fixed_size = 24;
  static constexpr std::size_t serial_size(const rpc_success_hdr &) {
    return fixed_size;
  }
  template<typename Archive> static void save(Archive &a,
					      const rpc_success_hdr &t) {
    archive(a, t.xid, "xid");
    archive(a, REPLY, "mtype");
    archive(a, MSG_ACCEPTED, "stat");
    archive(a, AUTH_NONE, "flavor");
    archive(a, uint32_t(0), "body");
    archive(a, SUCCESS, "stat");
  }
};

msg_ptr rpc_accepted_error_msg(uint32_t xid, accept_stat stat);
msg_ptr rpc_prog_mismatch_msg(uint32_t xid, uint32_t low, uint32_t high);
msg_ptr rpc_rpc_mismatch_msg(uint32_t xid);

struct service_base {
  using cb_t = std::function<void(msg_ptr)>;

  const uint32_t prog_;
  const uint32_t vers_;

  service_base(uint32_t prog, uint32_t vers) : prog_(prog), vers_(vers) {}
  virtual ~service_base() {}
  virtual void process(rpc_msg &hdr, xdr_get &g, cb_t reply) = 0;

  bool check_call(const rpc_msg &hdr) {
    return hdr.body.mtype() == CALL
      && hdr.body.cbody().rpcvers == 2
      && hdr.body.cbody().prog == prog_
      && hdr.body.cbody().vers == vers_;
  }

  template<typename T> static bool decode_arg(xdr_get &g, T &arg) {
    try {
      archive(g, arg);
      g.done();
      return true;
    }
    catch (const xdr_runtime_error &) {
      return false;
    }
  }
};

template<typename T> struct synchronous_service : service_base {
  using interface = typename T::rpc_interface_type;
  T &server_;

  synchronous_service(T &server)
    : service_base(interface::program, interface::version), server_(server) {}

  void process(rpc_msg &hdr, xdr_get &g, cb_t reply) override {
    if (!check_call(hdr))
      reply(nullptr);
    if (!interface::call_dispatch(*this, hdr.body.cbody().proc, hdr, g,
				  std::move(reply)))
      reply(rpc_accepted_error_msg(hdr.xid, PROC_UNAVAIL));
  }

  template<typename P> typename std::enable_if<
    !std::is_same<void, typename P::res_type>::value>::type
  dispatch(rpc_msg &hdr, xdr_get &g, cb_t reply) {
    pointer<typename P::arg_wire_type> arg;
    if (!decode_arg(g, arg.activate()))
      return reply(rpc_accepted_error_msg(hdr.xid, GARBAGE_ARGS));
    
    if (xdr_trace_server) {
      std::string s = "CALL ";
      s += P::proc_name;
      s += " <- [xid " + std::to_string(hdr.xid) + "]";
      std::clog << xdr_to_string(*arg, s.c_str());
    }

    std::unique_ptr<typename P::res_type> res =
      P::dispatch_dropvoid(server_, std::move(arg));

    if (xdr_trace_server) {
      std::string s = "REPLY ";
      s += P::proc_name;
      s += " -> [xid " + std::to_string(hdr.xid) + "]";
      std::clog << xdr_to_string(*res, s.c_str());
    }

    reply(xdr_to_msg(rpc_success_hdr(hdr.xid), *res));
  }

  template<typename P> typename std::enable_if<
    std::is_same<void, typename P::res_type>::value>::type
  dispatch(rpc_msg &hdr, xdr_get &g, cb_t reply) {
    pointer<typename P::arg_wire_type> arg;
    if (!decode_arg(g, arg.activate()))
      return reply(rpc_accepted_error_msg(hdr.xid, GARBAGE_ARGS));
    
    if (xdr_trace_server) {
      std::string s = "CALL ";
      s += P::proc_name;
      s += " <- [xid " + std::to_string(hdr.xid) + "]";
      std::clog << xdr_to_string(*arg, s.c_str());
    }

    P::dispatch_dropvoid(server_, std::move(arg));

    if (xdr_trace_server) {
      std::string s = "REPLY ";
      s += P::proc_name;
      s += " -> [xid " + std::to_string(hdr.xid) + "]\n";
      std::clog <<  s;
    }

    reply(xdr_to_msg(rpc_success_hdr(hdr.xid)));
  }
};

class rpc_server_base {
  std::map<uint32_t,
	   std::map<uint32_t, std::unique_ptr<service_base>>> servers_;
protected:
  void register_service_base(service_base *s);
public:
  void dispatch(msg_ptr m, service_base::cb_t reply);
};

//! Listens for connections on a TCP socket (optionally registering
//! the socket with \c rpcbind), and then serves one or more
//! program/version interfaces to accepted connections.
class rpc_tcp_listener : rpc_server_base {
  pollset ps_;
  unique_fd listen_fd_;
  const bool use_rpcbind_;

  void accept_cb();
  void receive_cb(msg_sock *ms, msg_ptr mp);

public:
  rpc_tcp_listener(unique_fd &&fd, bool use_rpcbind = false);
  rpc_tcp_listener() : rpc_tcp_listener(unique_fd(-1), true) {}
  virtual ~rpc_tcp_listener();

  //! Add objects implementing RPC program interfaces to the server.
  template<typename T> void register_service(T &t) {
    register_service_base(new synchronous_service<T>(t));
    if(use_rpcbind_)
      rpcbind_register(listen_fd_.get(), T::rpc_interface_type::program,
		       T::rpc_interface_type::version);
  }
  void run();
};

}

#endif // !_XDRPP_SERVER_H_HEADER_INCLUDED_
