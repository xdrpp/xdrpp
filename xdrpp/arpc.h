// -*- C++ -*-

//! \file arpc.h Asynchronous RPC interface.

#include <xdrpp/exception.h>
#include <xdrpp/server.h>

namespace xdr {

template<typename P, typename T = typename P::res_type> class reply_cb;

namespace detail {

class reply_cb_impl {
  template<typename P, typename T> friend class xdr::reply_cb;
  using cb_t = service_base::cb_t;
  uint32_t xid_;
  unsigned refcount_{0};
  cb_t cb_;

  template<typename CB> reply_cb_impl(uint32_t xid, CB &&cb)
    : xid_(xid), cb_(std::forward<CB>(cb)) {}
  reply_cb_impl(const reply_cb_impl &rcb) = delete;
  reply_cb_impl &operator=(const reply_cb_impl &rcb) = delete;
  ~reply_cb_impl() { if (cb_) reject(PROC_UNAVAIL); }

  void ref() { refcount_++; }
  void unref() { if (!--refcount_) delete this; }

  void send_reply_msg(msg_ptr &&b) {
    assert(cb_);		// If this fails you replied twice
    cb_(std::move(b));
    cb_ = nullptr;
  }

  template<typename P> void send_reply(const typename P::res_wire_type &t) {
    if (xdr_trace_server) {
      std::string s = "REPLY ";
      s += P::proc_name;
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

// Because before C++14 it is a pain to move values into closures, we
// implement \c reply_cb as a reference-counted type that can be
// copied.  Note, however, that it is not thread-safe, as the
// reference count is not protected with a lock.  This means you
// shouldn't attempt to use a \c reply_cb in a different thread (at
// least without doing some fairly convoluted synchronization to make
// sure the copy in the parent is not destroyed simultaneously with
// the one in the child thread).
template<typename P, typename T> class reply_cb {
  using impl_t = detail::reply_cb_impl;
public:
  using type = T;
  impl_t *impl_;

  reply_cb() : impl_(nullptr) {}
  template<typename CB> reply_cb(uint32_t xid, CB &&cb)
    : impl_(new impl_t(xid, std::forward<CB>(cb))) { impl_->ref(); }
  reply_cb(const reply_cb &&rcb) : impl_(rcb.impl_) {
    impl_->ref();
    return *this;
  }
  ~reply_cb() { if(impl_) impl_->unref(); }
  reply_cb &operator=(const reply_cb &rcb) {
    if (rcb.impl_)
      rcb.impl_->ref();
    if (impl_)
      impl_->unref();
    impl_ = rcb.impl_;
  }

  void operator()(const type &t) { impl_->template send_reply<P>(t); }
  void reject(accept_stat stat) { impl_->reject(stat); }
  void reject(auth_stat stat) { impl_->reject(stat); }
};
template<typename P> class reply_cb<P, void> : public reply_cb<P, xdr_void> {
public:
  using type = void;
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
			       hdr.xid, std::move(reply)});
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
  void receive(msg_sock *ms, msg_ptr buf);
};


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
};

class arpc_sock {
public:
  template<typename P> using call_cb_t =
    std::function<void(call_result<typename P::res_type>)>;
  using client_cb_t = std::function<void(rpc_msg &, xdr_get &)>;
  using server_cb_t = std::function<void(rpc_msg &, xdr_get &, arpc_sock *)>;

  arpc_sock(pollset &ps, int fd);

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

  std::unique_ptr<msg_sock> ms_;
  uint32_t xid_counter_{0};
  std::map<uint32_t, std::unique_ptr<call_state_base>> calls_;
  std::map<uint32_t, std::map<uint32_t, server_cb_t>> services_;

  void prepare_call(rpc_msg &hdr, uint32_t prog,
		    uint32_t vers, uint32_t proc);
  void receive(msg_ptr buf);

};

#if 0
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

