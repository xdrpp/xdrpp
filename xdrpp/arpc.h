// -*- C++ -*-

//! \file arpc.h Asynchronous RPC interface.

#include <map>
#include <mutex>
#include <xdrpp/exception.h>
#include <xdrpp/marshal.h>
#include <xdrpp/msgsock.h>

namespace xdr {

//! A pointer to a call result
template<typename T> struct call_result : std::unique_ptr<T> {
  using std::unique_ptr<T>::unique_ptr;
  rpc_call_stat stat;
};

class rpc_sock {
public:
  template<typename P> using call_cb_t =
    std::function<void(call_result<typename P::res_wire_type>)>;

  using client_cb_t = std::function<void(rpc_msg &, xdr_get &)>;
  using server_cb_t = std::function<void(rpc_msg &, xdr_get &, rpc_sock *)>;

  rpc_sock(pollset &ps, int fd);

  template<typename P> void
  invoke(const typename P::arg_wire_type &arg, call_cb_t<P> cb) {
    rpc_msg hdr;
    hdr.xid = ++xid_counter_;
    hdr.body.mtype(CALL);
    hdr.body.cbody().rpcvers = 2;
    hdr.body.cbody().prog = P::interface_type::program;
    hdr.body.cbody().vers = P::interface_type::version;
    hdr.body.cbody().proc = P::proc;

  }

private:
  std::unique_ptr<msg_sock> ms_;
  std::uint32_t xid_counter_{0};
  std::map<uint32_t, client_cb_t> calls_;
  std::map<uint32_t, std::map<uint32_t, server_cb_t>> services_;

  void receive(msg_ptr buf);

  template<typename P>
  void process_reply(call_cb_t<P> cb) {
  }
};

}

