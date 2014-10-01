// -*- C++ -*-

#ifndef _XDRPP_EXCEPTION_H_HEADER_INCLUDED_
#define _XDRPP_EXCEPTION_H_HEADER_INCLUDED_ 1

#include <xdrpp/rpc_msg.hh>
#include <system_error>

namespace std {

template<> struct is_error_code_enum<xdr::accept_stat> : true_type {};
template<> struct is_error_code_enum<xdr::auth_stat> : true_type {};

}

namespace xdr {

const std::error_category &accept_stat_category();
const std::error_category &auth_stat_category();

inline std::error_code
make_error_code(accept_stat s)
{
  return std::error_code(s, accept_stat_category());
}

inline std::error_code
make_error_code(auth_stat s)
{
  return std::error_code(s, auth_stat_category());
}

}

#endif // !_XDRPP_EXCEPTION_H_HEADER_INCLUDED_
