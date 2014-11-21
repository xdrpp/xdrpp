// -*- C++ -*-

//! \file arpc.h Asynchronous RPC interface.

#ifndef _XDRPP_ARPC_H_HEADER_INCLUDED_
#define _XDRPP_ARPC_H_HEADER_INCLUDED_ 1

#include <xdrpp/exception.h>
#include <xdrpp/server.h>
#include <xdrpp/srpc.h>	     // XXX prepare_call

namespace xdr {

template<typename T> class reply_cb;

namespace detail {

class reply_cb_impl {
  template<typename T> friend class xdr::reply_cb;
  using cb_t = service_base::cb_t;
  uint32_t xid_;
  cb_t cb_;
  const char *const proc_name_;

  template<typename CB> reply_cb_impl(uint32_t xid, CB &&cb, const char *name)
    : xid_(xid), cb_(std::forward<CB>(cb)), proc_name_(name) {}
  reply_cb_impl(const reply_cb_impl &rcb) = delete;
  reply_cb_impl &operator=(const reply_cb_impl &rcb) = delete;
  ~reply_cb_impl() { if (cb_) reject(PROC_UNAVAIL); }

  void send_reply_msg(msg_ptr &&b) {
    assert(cb_);		// If this fails you replied twice
    cb_(std::move(b));
    cb_ = nullptr;
  }

  template<typename T> void send_reply(const T &t) {
    if (xdr_trace_server) {
      std::string s = "REPLY ";
      s += proc_name_;
      s += " -> [xid " + std::to_string(xid_) + "]";
      std::clog << xdr_to_string(t, s.c_str());
    }
    send_reply_msg(xdr_to_msg(rpc_success_hdr(xid_), t));
  }

  void reject(accept_stat stat) {
    send_reply_msg(rpc_accepted_error_msg(xid_, stat));
  }
  void reject(auth_stat stat) {
    send_reply_msg(rpc_auth_error_msg(xid_, stat));
  }
};

} // namespace detail

template<typename T> class reply_cb {
  using impl_t = detail::reply_cb_impl;
public:
  using type = T;
  std::unique_ptr<impl_t> impl_;

  reply_cb() {}
  template<typename CB> reply_cb(uint32_t xid, CB &&cb, const char *name)
    : impl_(new impl_t(xid, std::forward<CB>(cb), name)) {}

  void operator()(const type &t) { impl_->send_reply(t); }
  void reject(accept_stat stat) { impl_->reject(stat); }
  void reject(auth_stat stat) { impl_->reject(stat); }
};
template<> class reply_cb<void> : public reply_cb<xdr_void> {
public:
  using type = void;
  using reply_cb<xdr_void>::reply_cb;
  using reply_cb<xdr_void>::operator();
  void operator()() { this->operator()(xdr_void{}); }
};

template<typename T, typename Session, typename Interface>
class arpc_service : public service_base {
  T &server_;

public:
  void process(void *session, rpc_msg &hdr, xdr_get &g, cb_t reply) override {
    if (!check_call(hdr))
      reply(nullptr);
    if (!Interface::call_dispatch(*this, hdr.body.cbody().proc,
				  static_cast<Session *>(session),
				  hdr, g, std::move(reply)))
      reply(rpc_accepted_error_msg(hdr.xid, PROC_UNAVAIL));
  }

  template<typename P>
  void dispatch(Session *session, rpc_msg &hdr, xdr_get &g, cb_t reply) {
    wrap_transparent_ptr<typename P::arg_tuple_type> arg;
    if (!decode_arg(g, arg))
      return reply(rpc_accepted_error_msg(hdr.xid, GARBAGE_ARGS));
    
    if (xdr_trace_server) {
      std::string s = "CALL ";
      s += P::proc_name;
      s += " <- [xid " + std::to_string(hdr.xid) + "]";
      std::clog << xdr_to_string(arg, s.c_str());
    }

    dispatch_with_session<P>(server_, session, std::move(arg),
			     reply_cb<typename P::res_type>{
			       hdr.xid, std::move(reply), P::proc_name});
  }

  arpc_service(T &server)
    : service_base(Interface::program, Interface::version),
      server_(server) {}
};

class arpc_server : public rpc_server_base {
public:
  template<typename T, typename Interface = typename T::rpc_interface_type>
  void register_service(T &t) {
    register_service_base(new arpc_service<T, void, Interface>(t));
  }
  void receive(rpc_sock *ms, msg_ptr buf);
};

template<typename Session = void,
	 typename SessionAllocator = session_allocator<Session>>
using arpc_tcp_listener =
  generic_rpc_tcp_listener<arpc_service, Session, SessionAllocator>;


//! A \c unique_ptr to a call result, or NULL if the call failed (in
//! which case \c message returns an error message).
template<typename T> struct call_result : std::unique_ptr<T> {
  rpc_call_stat stat_;
  using std::unique_ptr<T>::unique_ptr;
  call_result(const rpc_call_stat &stat) : stat_(stat) {}
  const char *message() const { return *this ? nullptr : stat_.message(); }
};
template<> struct call_result<void> {
  rpc_call_stat stat_;
  call_result(const rpc_call_stat &stat) : stat_(stat) {}
  const char *message() const { return stat_ ? nullptr : stat_.message(); }
  explicit operator bool() const { return bool(stat_); }
};


class asynchronous_client_base {
  rpc_sock &s_;
public:
  asynchronous_client_base(rpc_sock &s) : s_(s) {}
  asynchronous_client_base(asynchronous_client_base &c) : s_(c.s_) {}

  template<typename P, typename CB, typename...A>
  void invoke(const A &...a, CB &&cb) {
  }

  asynchronous_client_base *operator->() { return this; }
};

#if 0
class arpc_sock {
public:
  template<typename P> using call_cb_t =
    std::function<void(call_result<typename P::res_type>)>;
  using client_cb_t = std::function<void(rpc_msg &, xdr_get &)>;
  using server_cb_t = std::function<void(rpc_msg &, xdr_get &, arpc_sock *)>;

  arpc_sock(pollset &ps, sock_t s);

  template<typename P, typename...A> inline void
  invoke(const A &...a, call_cb_t<P> cb);

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

  std::unique_ptr<rpc_sock> ms_;
  uint32_t xid_counter_{0};
  std::map<uint32_t, std::unique_ptr<call_state_base>> calls_;
  std::map<uint32_t, std::map<uint32_t, server_cb_t>> services_;

  void prepare_call(rpc_msg &hdr, uint32_t prog,
		    uint32_t vers, uint32_t proc);
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
#endif

}

#endif // !_XDRPP_ARPC_H_HEADER_INCLUDED_
