// -*- C++ -*-

//! \file arpc.h Asynchronous RPC interface.

#include <map>
#include <mutex>
#include <xdrpp/exception.h>
#include <xdrpp/marshal.h>
#include <xdrpp/msgsock.h>

namespace xdr {

class reply_cb_base {
  msg_sock *ms_;
  std::shared_ptr<const bool> ms_destroyed_;
  rpc_msg hdr_;

protected:
  reply_cb_base(msg_sock *ms, std::uint32_t xid)
    : ms_(ms), ms_destroyed_(ms->destroyed_ptr()), hdr_(xid, REPLY) {}
  reply_cb_base(reply_cb_base &&rcb)
    : ms_(rcb.ms_), ms_destroyed_(std::move(rcb.ms_destroyed_)),
      hdr_(std::move(rcb.hdr_)) { rcb.ms_ = nullptr; }
  ~reply_cb_base() { if (ms_) reject(PROC_UNAVAIL); }
  template<typename T> void send_reply(const T &t) {
    assert(ms_);
    if (*ms_destroyed_)
      return;
    ms_->putmsg(xdr_to_msg(hdr_, t));
    ms_ = nullptr;
  }

public:
  opaque_auth &reply_verf() {
    assert(ms_);
    return hdr_.body.rbody().areply().verf;
  }
  void reject(accept_stat stat);
  void reject(auth_stat stat);
};

template<typename T> class reply_cb : public reply_cb_base {
  using type = T;
  using reply_cb_base::reply_cb_base;
  void operator()(const T &t) { send_reply(t); }
};

//! A \c unique_ptr to a call result, or NULL if the call failed (in
//! which case \c message returns an error message).
template<typename T> struct call_result : std::unique_ptr<T> {
  rpc_call_stat stat_;
  using std::unique_ptr<T>::unique_ptr;
  call_result(const rpc_call_stat &stat) : stat_(stat) {}
  const char *message() const { return *this ? nullptr : stat_.message(); }
};

class arpc_sock {
public:
  template<typename P> using call_cb_t =
    std::function<void(call_result<typename P::res_wire_type>)>;

  using client_cb_t = std::function<void(rpc_msg &, xdr_get &)>;
  using server_cb_t = std::function<void(rpc_msg &, xdr_get &, arpc_sock *)>;

  arpc_sock(pollset &ps, int fd);

  template<typename P> inline void
  invoke(const typename P::arg_wire_type &arg, call_cb_t<P> cb);

private:
  struct call_state_base {
    virtual ~call_state_base() {}
    virtual void get_reply(xdr_get &g) = 0;
    virtual void get_error(const rpc_call_stat &stat) = 0;
  };

  template<typename P> class call_state : call_state_base {
    call_cb_t<P> cb_;
    bool used_ {false};
  public:
    template<typename F> call_state(F &&f) : cb_(std::forward<F>(f)) {}
    ~call_state() { get_error(rpc_call_stat::NETWORK_ERROR); }

    void get_reply(xdr_get &g) override {
      if (used_)
	return;

      call_result<typename P::res_wire_type> r{new typename P::res_wire_type};
      try { archive(g, *r); }
      catch (const xdr_runtime_error &) {
	get_error(rpc_call_stat::GARBAGE_RES);
	return;
      }
      catch (const std::bad_alloc &) {
	get_error(rpc_call_stat::BAD_ALLOC);
	return;
      }
      used_ = true;
      cb_(r);
    }

    void get_error(const rpc_call_stat &stat) override {
      if (used_)
	return;
      used_ = true;
      cb_(stat);
    }
  };

  std::unique_ptr<msg_sock> ms_;
  std::uint32_t xid_counter_{0};
  std::map<uint32_t, std::unique_ptr<call_state_base>> calls_;
  std::map<uint32_t, std::map<uint32_t, server_cb_t>> services_;

  void prepare_call(rpc_msg &hdr, std::uint32_t prog,
		    std::uint32_t vers, std::uint32_t proc);
  void receive(msg_ptr buf);

};

template<typename P> inline void
arpc_sock::invoke(const typename P::arg_wire_type &arg, call_cb_t<P> cb)
{
  rpc_msg hdr;
  prepare_call(hdr, P::interface_type::program,
	       P::interface_type::version, P::proc);
  ms_->putmsg(xdr_to_msg(hdr, arg));
  calls_.emplace(hdr.xid, new call_state<P>(cb));
}

}

