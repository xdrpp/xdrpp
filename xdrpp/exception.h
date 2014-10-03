// -*- C++ -*-

/** \file exception.h Exceptions raised by RPC calls.  These depend on
 * the RPC message format.  Other exceptions may be found in \c
 * types.h.*/

#ifndef _XDRPP_EXCEPTION_H_HEADER_INCLUDED_
#define _XDRPP_EXCEPTION_H_HEADER_INCLUDED_ 1

#include <xdrpp/rpc_msg.hh>
#include <cerrno>
#include <cstring>

namespace xdr {

//! Translate one of the conditions in
//! [RFC5531](https://tools.ietf.org/html/rfc5531) for an unexecuted
//! call into a string.
const char *rpc_errmsg(accept_stat ev);
//! Translate one of the conditions in
//! [RFC5531](https://tools.ietf.org/html/rfc5531) for an unexecuted
//! call into a string.
const char *rpc_errmsg(auth_stat ev);
//! Return <tt>"rpcvers field mismatch"</tt> in response to constant
//! \c RPC_MISMATCH.
const char *rpc_errmsg(reject_stat ev);

//! This is the exception raised in an RPC client when it reaches the
//! server and transmits a call, with no connection or communication
//! errors, but the server replies with an RPC-level message header
//! refusing to execute the call.
struct xdr_call_error : xdr_runtime_error {
  union {
    accept_stat accept_;
    auth_stat auth_;
    reject_stat reject_;
  };
  enum { ACCEPT_STAT, AUTH_STAT, REJECT_STAT } type_;
  xdr_call_error(accept_stat);
  xdr_call_error(auth_stat);
  xdr_call_error(reject_stat);
};

//! Check that an RPC header precedes a result.  \throws
//! xdr_call_error if the reply does not contain a response.
void check_call_hdr(const rpc_msg &hdr);

//! This exception represents a system error encountered while
//! attempting to send RPC messages over a socket.
struct xdr_system_error : xdr_runtime_error {
  xdr_system_error(const char *what, int no = errno)
    : xdr_runtime_error(std::string(what) + ": " + std::strerror(no)) {}
};

}

#endif // !_XDRPP_EXCEPTION_H_HEADER_INCLUDED_
