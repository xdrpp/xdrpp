// -*- C++ -*-

#ifndef _POLLSET_H_
#define _POLLSET_H_ 1

/** \file pollset.h Asynchronous I/O and event harness. */

#include <csignal>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <poll.h>

namespace xdr {

//! Set the \c O_NONBLOCK flag on a socket.  \throws std::system_error
//! on failure.
void set_nonblock(int fd);

//! Set the close-on-exec flag of a file descriptor.  \throws
//! std::system_error on failure.
void set_close_on_exec(int fd);

//! Keep closing a file descriptor until you don't get \c EINTR.
void really_close(int fd);

class pollset {
public:
  using cb_t = std::function<void()>;
private:
  static constexpr int kReadFlag = 0x1;
  static constexpr int kWriteFlag = 0x2;
  static constexpr int kOnceFlag = 0x4;
public:
  enum op_t {
    //! Specify interest in read-ready condition
    Read = kReadFlag,
    //! Specify interest in write-ready condition
    Write = kWriteFlag,		
    //! Valid only when removing callbacks.
    ReadWrite = kReadFlag | kWriteFlag,
    //! Like \c Read, but the callback only executes once.
    ReadOnce = kReadFlag | kOnceFlag,
    //! Like \c Write, but the callback only executes once.
    WriteOnce = kWriteFlag | kOnceFlag
  };

private:
  // File descriptor callback information
  struct fd_state {
    cb_t rcb;
    cb_t wcb;
    int idx {-1};		// Index in pollfds_
    bool roneshot;
    bool woneshot;
    ~fd_state();			// Sanity check no active callbacks
  };

  enum class wake_type : std::uint8_t {
    Normal = 0,
    Signal = 1
  };
  
  // State for asynchronous tasks
  template<typename R> struct async_task {
    pollset *ps_;
    std::function<R()> work_;
    std::function<void(R)> cb_;
    std::unique_ptr<R> rp_;

    void start() {
      rp_.reset(new R { work_() });
      ps_->inject_cb(std::bind(&async_task::done, this));
    }
    void done() {
      std::unique_ptr<async_task> self {this};
      ps_->nasync_--;
      cb_(std::move(*rp_));
    }
  };

  // Self-pipe used to wake up poll from signal handlers and other threads
  int selfpipe_[2];

  // File descriptor callback state
  std::vector<pollfd> pollfds_;
  std::unordered_map<int, fd_state> state_;

  // Timeout callback state
  std::multimap<std::int64_t, cb_t> time_cbs_;

  // Asynchronous events enqueued from other threads
  std::mutex async_cbs_lock_;
  std::vector<cb_t> async_cbs_;
  bool async_pending_{false};
  size_t nasync_{0};

  // Signal callback state
  static constexpr int num_sig = 32;
  static std::mutex signal_owners_lock;
  static pollset *signal_owners[num_sig];
  static volatile std::sig_atomic_t signal_flags[num_sig];
  bool signal_pending_{false};
  std::map<int, cb_t> signal_cbs_;

  void wake(wake_type wt);
  void run_pending_asyncs();
  void inject_cb_vec(std::vector<cb_t>::iterator b,
		     std::vector<cb_t>::iterator e);
  int next_timeout(int ms);
  void run_timeouts();
  void run_signal_handlers();
  void consolidate();
  cb_t &fd_cb_helper(int fd, op_t op);
  static void signal_handler(int);
  static void erase_signal_cb(int);

public:
  pollset();
  ~pollset();

  //! Go through one round of checking all file descriptors.  \arg \c
  //! timeout is a timeout in milliseconds (or -1 to wait forever).
  //! 
  //! Typical usage:  \code
  //!   PollSet ps;
  //!   // ... register some callbacks or asynchronous events ...
  //!   while(ps.pending())
  //!     ps.poll();
  //! \endcode
  void poll(int timeout = -1);

  //! Returns \c false if no file descriptor callbacks are registered
  //! and no timeouts or asynchronous events are pending.  If it
  //! returns \c false, then PollSet::poll will pause forever in the
  //! absence of a signal or a call to PollSet::inject_cb in a
  //! different thread.
  bool pending() const;

  //! Cause PollSet::poll to return if it is sleeping.  Unlike most
  //! other methods, \c wake is safe to call from a signal handler or
  //! a different thread.
  void wake() { wake(wake_type::Normal); }

  //! Set a read or write callback on a particular file descriptor.
  //! \arg \c fd is the file descriptor.  \arg \c op specifies the
  //! condition on which to invoke the callback.  Only one \c Read and
  //! one \c Write callback are permitted per file descriptor.  E.g.,
  //! calling \c set_cb with \c ReadOnce overwrites a previous \c Read
  //! callback on the same file descriptor.  The value \c ReadWrite is
  //! illegal when adding a callback (you must set \c Read and \c
  //! Write callbacks separately).  \arg \c cb is the callback, which
  //! must be convertible to PollSet::cb_t.
  template<typename CB> void fd_cb(int fd, op_t op, CB &&cb) {
    if (!(fd_cb_helper(fd, op) = std::forward<CB>(cb)))
      fd_cb(fd, op);
  }

  //! Remove a callback on a file descriptor.  If \c op is \c
  //! ReadWrite, removes both read and write callbacks on the
  //! descriptor.
  void fd_cb(int fd, op_t op, std::nullptr_t = nullptr);

  //! Inject a callback to run immediately.  Unlike most methods, it
  //! is safe to call this function from another thread.  Being
  //! thread-safe adds extra overhead, so it does not make sense to
  //! call this function from the same thread as PollSet::poll.  Note
  //! that \c inject_cb acquires a lock and definitely must <i>not</i>
  //! be called from a signal handler (or deadlock could ensue).
  template<typename CB> void inject_cb(CB &&cb) {
    std::lock_guard<std::mutex> lk(async_cbs_lock_);
    async_cbs_.emplace_back(std::forward<CB>(cb));
    if (!async_pending_) {
      async_pending_ = true;
      wake();
    }
  }

  //! Execute a task asynchonously in another thread, then run
  //! callback on the task's result in the main thread.  \arg \c work
  //! is a task to perform asynchronously in another thread, and must
  //! be convertible to std::function<R()> for some type \c R.  \arg
  //! \c cb is the callback that processes the result in the main
  //! thread, and must be convertible to std::function<void(R)> for
  //! the same type \c R.
  template<typename Work, typename CB> void async(Work &&work, CB &&cb) {
    using R = decltype(work());
    async_task<R> *a = new async_task<R> {
      this, std::forward<Work>(work), std::forward<CB>(cb), nullptr
    };
    ++nasync_;
    std::thread(&async_task<R>::start, a).detach();
  }

  //! Number of milliseconds since an arbitrary but fixed time, used
  //! as the basis of all timeouts.  Time zero is
  //! std::chrono::steady_clock's epoch, which in some implementations
  //! is the time a machine was booted.
  static std::int64_t now_ms();

  //! Abstract class used to represent a pending timeout.
  class Timeout {
    using iterator = decltype(time_cbs_)::iterator;
    iterator i_;
    Timeout(iterator i) : i_(i) {}
    Timeout &operator=(iterator i) { i_ = i; return *this; }
    friend class pollset;
  };

  //! Set a callback to run a certain number of milliseconds from now.
  //! \arg \c ms is the delay in milliseconds before running the
  //! callback.  \arg \c cb must be convertible to PollSet::cb_t.
  //! \returns an object on which you can call the method
  //! PollSet::timeout_cancel to cancel the timeout.
  template<typename CB> Timeout timeout(std::int64_t ms, CB &&cb) {
    return timeout_at(now_ms() + ms, std::forward<CB>(cb));
  }
  //! Set a callback to run at a specific time (as returned by
  //! PollSet::now_ms()).
  template<typename CB> Timeout timeout_at(std::int64_t ms, CB &&cb) {
    return time_cbs_.emplace(ms, std::forward<CB>(cb));
  }

  //! An invalid timeout, useful for initializing PollSet::Timeout
  //! values before a timeout has been scheduled.
  Timeout timeout_null() { return time_cbs_.end(); }

  //! Returns \b true if a timeout is not equal to
  //! PollSet::timeout_invalid().  Note that a PollSet::Timeout is
  //! invalid but not null after firing.  Hence, additional care must
  //! be taken by the programmer to avoid canceling already executed
  //! `Timeout`s.
  bool timeout_is_not_null(Timeout t) { return t.i_ != timeout_null().i_; }

  //! Cancel a pending timeout.  Sets the PollSet::Timeout argument \c
  //! t to PollSet::timeout_null(), but obviously does not not affect
  //! other copies of \c t that will now be invalid.
  void timeout_cancel(Timeout &t);

  //! Returns the absolute time (in milliseconds) at which a timeout
  //! will run.
  std::int64_t timeout_time(Timeout t) const { return t.i_->first; }

  //! Reschedule a timeout to run at a specific time.  Updates the
  //! argument \c t, but invalidates any other copies of \c t.
  void timeout_reschedule_at(Timeout &t, std::int64_t ms);
  //! Reschedule a timeout some number of milliseconds in the future.
  void timeout_reschedule(Timeout &t, std::int64_t ms) {
    timeout_reschedule_at(t, now_ms() + ms);
  }

  //! Add a callback for a particular signal.  Note that only one
  //! callback can be added for a particular signal across all
  //! `PollSet`s in a single process.  Hence, calling this function
  //! may "steal" a signal from a different `PollSet` (erasing
  //! whatever callback the other `PollSet` had for the signal).  Such
  //! callback stealing is atomic, allowing one to steal a `PollSet`s
  //! signals before deleting it with no risk of signals going
  //! uncaught.
  void signal_cb(int sig, cb_t cb);

  //! Remove any previously added callback for a particular signal.
  //! Because signal callbacks are process-wide, this static method
  //! will affect whatever `PollSet` currently owns the signal.
  static void signal_cb(int sig, std::nullptr_t = nullptr);
};

}

#endif /* !_POLLSET_H_ */
