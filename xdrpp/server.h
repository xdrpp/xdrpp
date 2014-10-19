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
msg_ptr rpc_auth_error_msg(uint32_t xid, auth_stat stat);
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

class rpc_server_base {
  std::map<uint32_t,
	   std::map<uint32_t, std::unique_ptr<service_base>>> servers_;
protected:
  void register_service_base(service_base *s);
public:
  void dispatch(msg_ptr m, service_base::cb_t reply);
};

}

#endif // !_XDRPP_SERVER_H_HEADER_INCLUDED_
