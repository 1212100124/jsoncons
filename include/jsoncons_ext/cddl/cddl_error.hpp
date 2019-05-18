/// Copyright 2018 Daniel Parker
// Distributed under the Boost license, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// See https://github.com/danielaparker/jsoncons for latest version

#ifndef JSONCONS_CDDL_CDDL_ERROR_HPP
#define JSONCONS_CDDL_CDDL_ERROR_HPP

#include <system_error>
#include <jsoncons/config/jsoncons_config.hpp>

namespace jsoncons { namespace cddl {

enum class cddl_errc
{
    unexpected_eof = 1,
    expected_assign,
    expected_id,
    invalid_id,
    expected_groupent,
    expected_comma_or_left_par_or_right_sqbracket,
    expected_comma_or_left_par_or_right_curbracket,
    expected_comma_or_right_par,
    expected_slash_or_comma_or_right_bracket,
    invalid_number
};

class cddl_error_category_impl
   : public std::error_category
{
public:
    const char* name() const noexcept override
    {
        return "jsoncons/cddl";
    }
    std::string message(int ev) const override
    {
        switch (static_cast<cddl_errc>(ev))
        {
            case cddl_errc::unexpected_eof:
                return "Unexpected end of file";
            case cddl_errc::expected_assign:
                return "Expected =";
            case cddl_errc::expected_id:
                return "Expected a rule id";
            case cddl_errc::invalid_id:
                return "Invalid rule id";
            case cddl_errc::expected_groupent:
                return "Expected [ or { or (";
            case cddl_errc::expected_comma_or_left_par_or_right_sqbracket:
                return "Expected , or ( or ]";
            case cddl_errc::expected_comma_or_left_par_or_right_curbracket:
                return "Expected , or ( or }";
            case cddl_errc::expected_comma_or_right_par:
                return "Expected , or )";
            case cddl_errc::expected_slash_or_comma_or_right_bracket:
                return "Expected / or , or right bracket";
            case cddl_errc::invalid_number:
                return "Invalid number";
            default:
                return "Unknown CDDL parser error";
        }
    }
};

inline
const std::error_category& cddl_error_category()
{
  static cddl_error_category_impl instance;
  return instance;
}

inline 
std::error_code make_error_code(cddl_errc result)
{
    return std::error_code(static_cast<int>(result),cddl_error_category());
}


}}

namespace std {
    template<>
    struct is_error_code_enum<jsoncons::cddl::cddl_errc> : public true_type
    {
    };
}

#endif
