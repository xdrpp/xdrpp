// -*- C++ -*-

/** \file depth_checker.h Support for checking XDR recursion depth. */

#ifndef _XDRPP_DEPTH_CHECKER_H_HEADER_INCLUDED_
#define _XDRPP_DEPTH_CHECKER_H_HEADER_INCLUDED_ 1

#include <xdrpp/types.h>

namespace xdr
{

//! Archive for checking if XDR recursion depth exceeds a limit.
//! Returns false if the depth limit is exceeded, true otherwise.
struct depth_checker
{
    std::uint32_t depth_limit_;
    std::uint32_t current_depth_ = 0;
    bool within_limit_ = true;

    explicit depth_checker(std::uint32_t limit) : depth_limit_(limit)
    {
    }

    // Handle basic numeric types
    template <typename T>
    std::enable_if_t<
        std::is_same_v<std::uint32_t, typename xdr_traits<T>::uint_type> ||
        std::is_same_v<std::uint64_t, typename xdr_traits<T>::uint_type>>
    operator()(const T&)
    {
        // Basic types don't increase recursion depth
    }

    // Handle bytes types (strings, opaque arrays/vectors)
    template <typename T>
    std::enable_if_t<xdr_traits<T>::is_bytes>
    operator()(const T&)
    {
        // Bytes types don't increase recursion depth
    }

    // Handle recursive types (classes and containers)
    template <typename T>
    std::enable_if_t<xdr_traits<T>::is_class || xdr_traits<T>::is_container>
    operator()(const T& t)
    {
        if (!within_limit_)
            return;

        ++current_depth_;

        if (current_depth_ > depth_limit_)
        {
            within_limit_ = false;
            return;
        }

        xdr_traits<T>::save(*this, t);
        --current_depth_;
    }

    bool
    result() const
    {
        return within_limit_;
    }
};

//! Check if the recursion depth of an XDR type exceeds a given limit.
//! Returns true if the type is within the depth limit, false otherwise.
template <typename T>
bool
check_xdr_depth(const T& t, std::uint32_t depth_limit)
{
    depth_checker checker(depth_limit);
    archive(checker, t);
    return checker.result();
}

} // namespace xdr

#endif // !_XDRPP_DEPTH_CHECKER_H_HEADER_INCLUDED_