// -*- C++ -*-

#ifndef _XDRPP_EXCEPTION_H_HEADER_INCLUDED_
#define _XDRPP_EXCEPTION_H_HEADER_INCLUDED_ 1

#include <xdrpp/rpc_msg.hh>
#include <cerrno>
#include <cstring>

namespace xdr {

const char *rpc_errmsg(accept_stat ev);
const char *rpc_errmsg(auth_stat ev);
const char *rpc_errmsg(reject_stat ev);

//! Exception representing errors returned by the server
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

//! Check that a header has a result.  \throws xdr_call_error if the
//! reply does not contain a response.
void check_call_hdr(const rpc_msg &hdr);

struct xdr_system_error : xdr_runtime_error {
  xdr_system_error(const char *what, int no = errno)
    : xdr_runtime_error(std::string(what) + std::strerror(no)) {}
};

}

#endif // !_XDRPP_EXCEPTION_H_HEADER_INCLUDED_
