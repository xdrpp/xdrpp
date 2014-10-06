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
  xdr_call_error stat;
};

class rpc_sock {
public:
  using client_cb_t = std::function<void(rpc_msg &, xdr_get &)>;
  using server_cb_t = std::function<void(rpc_msg &, xdr_get &, rpc_sock *)>;

  rpc_sock(pollset &ps, int fd);


private:
  std::unique_ptr<msg_sock> ms_;
  std::uint32_t next_xid_{1};
  std::map<uint32_t, client_cb_t> calls_;
  std::map<uint32_t, std::map<uint32_t, server_cb_t>> services_;

  void receive(msg_ptr buf);
};

}

