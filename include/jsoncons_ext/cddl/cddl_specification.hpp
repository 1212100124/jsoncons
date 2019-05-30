// Copyright 2019
// Distributed under the Boost license, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// See https://github.com/danielaparker/jsoncons for latest version

#ifndef JSONCONS_CDDL_CDDL_SPECIFICATION_HPP
#define JSONCONS_CDDL_CDDL_SPECIFICATION_HPP

#include <jsoncons/json.hpp>
#include <jsoncons_ext/cddl/cddl_error.hpp>
#include <memory>

namespace jsoncons { namespace cddl {

class cddl_specification
{
public:
    cddl_specification() = default;
    cddl_specification(const cddl_specification&) = default;
    cddl_specification(cddl_specification&&) = default;
    cddl_specification& operator=(const cddl_specification&) = default;
    cddl_specification& operator=(cddl_specification&&) = default;

    virtual ~cddl_specification() = default;

    virtual void validate(staj_reader&)
    {
        throw std::runtime_error("Invalid specification");
    }

    static std::unique_ptr<cddl_specification> parse(const std::string& s);
};

class memberkey_rule
{
public:
    memberkey_rule() = default;
    memberkey_rule(const memberkey_rule&) = default;
    memberkey_rule(memberkey_rule&&) = default;
    memberkey_rule& operator=(const memberkey_rule&) = default;
    memberkey_rule& operator=(memberkey_rule&&) = default;

    std::string name_;
    std::unique_ptr<cddl_specification> rule_; 
};

class array_rule : public cddl_specification
{
    std::vector<memberkey_rule> memberkey_rules_;
public:
    array_rule() = default;
    array_rule(const array_rule&) = default;
    array_rule(array_rule&&) = default;
    array_rule& operator=(const array_rule&) = default;
    array_rule& operator=(array_rule&&) = default;

    virtual void validate(staj_reader& reader)
    {
        const staj_event& event = reader.current();

        switch (event.event_type())
        {
            case staj_event_type::begin_array:
                break;
            default:
                throw std::runtime_error("Expected array");
                break;
        }
    }
};

enum class cddl_state : uint8_t
{
    expect_rule,
    id,
    expect_assign,
    expect_colon_or_comma_or_delimiter,
    expect_groupent,
    expect_value,
    expect_occur_or_memberkey,
    expect_occur_or_value,
    occur,
    expect_memberkey,
    value,
    minus,
    digit1,
    hex_number_value,
    zero_digit,
    fraction,
    plus_minus_exponent,
    exponent,
    quoted_value,
    expect_rangeop_or_slash_or_comma_or_delimiter,
    expect_rangeop,
    expect_exclusive_or_inclusive_rangeop,
    array_definition,
    array_definition2,
    map_definition,
    map_definition2,
    group,
    group2,
    after_value
};

class cddl_parser
{
public:
private:
    const char* p_;
    const char* endp_; 
    size_t line_;
    size_t column_;

    struct state_item
    {
        cddl_state state;
        char delimiter;

        explicit state_item(cddl_state state)
            : state(state), delimiter(0)
        {
        }

        state_item(cddl_state state, char delimiter)
            : state(state), delimiter(delimiter)
        {
        }

        state_item(cddl_state state, const state_item& parent)
            : state(state), delimiter(parent.delimiter)
        {
        }
    };
public:
    cddl_parser()
        : line_(1), column_(1)
    {
    }

    std::unique_ptr<cddl_specification> parse(const std::string& s)
    {
        std::vector<std::unique_ptr<cddl_specification>> rule_stack;

        std::vector<state_item> state_stack;

        state_stack.emplace_back(cddl_state::expect_rule);

        std::string buffer;

        p_ = s.data();
        endp_ = s.data() + s.length(); 

        while (p_ < endp_)
        {
            switch (state_stack.back().state)
            {
                case cddl_state::expect_rule: 
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            advance_past_space_character();
                            break;
                        case ';':
                            skip_to_end_of_line();
                            break;
                        default:
                            if (is_ealpha(*p_))
                            {
                                buffer.clear();
                                buffer.push_back(*p_);
                                state_stack.emplace_back(cddl_state::expect_assign);
                                state_stack.emplace_back(cddl_state::id,'=');
                                ++p_;
                                ++column_;
                            }
                            else
                            {
                                throw ser_error(cddl_errc::expected_id,line_,column_);
                            }
                            break;
                    }
                    break;
                }
                case cddl_state::expect_assign:
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            advance_past_space_character();
                            break;
                        case ';':
                            skip_to_end_of_line();
                            break;
                        case '=':
                            state_stack.back().state = cddl_state::expect_groupent;
                            ++p_;
                            ++column_;
                            break;
                        default:
                            throw ser_error(cddl_errc::expected_assign,line_,column_);
                            break;
                    }
                    break;
                }
                case cddl_state::expect_value: 
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            advance_past_space_character();
                            break;
                        case ';':
                            skip_to_end_of_line();
                            break;
                        case '\"':
                            buffer.clear();
                            state_stack.back().state = cddl_state::quoted_value;
                            ++p_;
                            ++column_;
                            break;
                        case '-':
                            buffer.clear();
                            buffer.push_back(*p_);
                            state_stack.back().state = cddl_state::minus;
                            ++p_;
                            ++column_;
                            break;
                        case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                            buffer.clear();
                            buffer.push_back(*p_);
                            state_stack.back().state = cddl_state::digit1;
                            ++p_;
                            ++column_;
                            break;
                        case '0': 
                            buffer.clear();
                            buffer.push_back(*p_);
                            state_stack.back().state = cddl_state::zero_digit;
                            ++p_;
                            ++column_;
                            break;
                        default:
                            buffer.clear();
                            state_stack.back().state = cddl_state::value;
                            break;
                    }
                    break;
                }
                case cddl_state::minus: 
                {
                    switch (*p_)
                    {
                        case '0': 
                            buffer.clear();
                            buffer.push_back(*p_);
                            state_stack.back().state = cddl_state::zero_digit;
                            ++p_;
                            ++column_;
                            break;
                        case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                            buffer.clear();
                            buffer.push_back(*p_);
                            state_stack.back().state = cddl_state::digit1;
                            ++p_;
                            ++column_;
                            break;
                        default:
                            throw ser_error(cddl_errc::invalid_number,line_,column_);
                            break;
                    }
                    break;
                }
                case cddl_state::expect_occur_or_memberkey:
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            advance_past_space_character();
                            break;
                        case ';':
                            skip_to_end_of_line();
                            break;
                        case '?':
                        case '*':
                        case '+':
                            buffer.clear();
                            state_stack.back().state = cddl_state::occur;
                            ++p_;
                            ++column_;
                            break;
                        case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                            buffer.clear();
                            state_stack.back().state = cddl_state::expect_occur_or_value;
                            break;
                        default:
                            state_stack.back().state = cddl_state::expect_memberkey;
                            break;
                    }
                    break;
                }
                case cddl_state::expect_occur_or_value:
                {
                    switch (*p_)
                    {
                        case '?':
                        case '*':
                        case '+':
                            std::cout << "x: " << buffer << "\n"; 
                            buffer.clear();
                            state_stack.back().state = cddl_state::occur;
                            ++p_;
                            ++column_;
                            break;
                        case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                            buffer.push_back(*p_);
                            ++p_;
                            ++column_;
                            break;
                        default:
                            state_stack.back().state = cddl_state::expect_colon_or_comma_or_delimiter;
                            state_stack.emplace_back(cddl_state::digit1, state_stack.back());
                            break;
                    }
                    break;
                }
                case cddl_state::occur:
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            advance_past_space_character();
                            state_stack.back().state = cddl_state::expect_memberkey;
                            break;
                        case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                            buffer.push_back(*p_);
                            ++p_;
                            ++column_;
                            break;
                        default:
                            throw ser_error(cddl_errc::expected_uint_or_space,line_,column_);
                            break;
                    }
                    break;
                }
                case cddl_state::expect_memberkey:
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            advance_past_space_character();
                            break;
                        case ';':
                            skip_to_end_of_line();
                            break;
                        default:
                            if (*p_ == state_stack.back().delimiter)
                            {
                                state_stack.pop_back();
                            }
                            else
                            {
                                buffer.clear();
                                state_stack.back().state = cddl_state::expect_colon_or_comma_or_delimiter;
                                if (is_ealpha(*p_))
                                {
                                    state_stack.emplace_back(cddl_state::id, state_stack.back());
                                }
                                else
                                {
                                    state_stack.emplace_back(cddl_state::expect_value, state_stack.back());
                                }
                            }
                            break;
                    }
                    break;
                }
                case cddl_state::expect_colon_or_comma_or_delimiter:
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            advance_past_space_character();
                            break;
                        case ';':
                            skip_to_end_of_line();
                            break;
                        case ':':
                            state_stack.back().state = cddl_state::expect_rangeop_or_slash_or_comma_or_delimiter;
                            state_stack.emplace_back(cddl_state::expect_value, state_stack.back());
                            ++p_;
                            ++column_;
                            break;
                        case ',':
                            state_stack.pop_back();
                            break;
                        default:
                            if (*p_ == state_stack.back().delimiter)
                            {
                                state_stack.pop_back();
                            }
                            else
                            {
                                throw ser_error(cddl_errc::expected_assign,line_,column_);
                            }
                            break;
                    }
                    break;
                }
                case cddl_state::array_definition: 
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            advance_past_space_character();
                            break;
                        case ';':
                            skip_to_end_of_line();
                            break;
                        case ']':
                            state_stack.back().state = cddl_state::array_definition2;
                            break;
                        case '(':
                            ++p_;
                            ++column_;
                            state_stack.emplace_back(cddl_state::group, ')');
                            break;
                        default:
                            state_stack.back().state = cddl_state::array_definition2;
                            state_stack.emplace_back(cddl_state::expect_occur_or_memberkey, ']');
                            break;
                    }
                    break;
                }
                case cddl_state::array_definition2: 
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            advance_past_space_character();
                            break;
                        case ';':
                            skip_to_end_of_line();
                            break;
                        case ']':
                            rule_stack.emplace_back(new array_rule());
                            state_stack.pop_back();
                            ++p_;
                            ++column_;
                            break;
                        case ',':
                            state_stack.emplace_back(cddl_state::expect_occur_or_memberkey,']');
                            ++p_;
                            ++column_;
                            break;
                        case '(':
                            ++p_;
                            ++column_;
                            state_stack.emplace_back(cddl_state::group, ')');
                            break;
                        default:
                            throw ser_error(cddl_errc::expected_comma_or_left_par_or_right_sqbracket,line_,column_);
                            break;
                    }
                    break;
                }
                case cddl_state::map_definition: 
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            advance_past_space_character();
                            break;
                        case ';':
                            skip_to_end_of_line();
                            break;
                        case '}':
                            ++p_;
                            ++column_;
                            state_stack.pop_back();
                            break;
                        case '(':
                            ++p_;
                            ++column_;
                            state_stack.emplace_back(cddl_state::group, ')');
                            break;
                        default:
                            state_stack.back().state = cddl_state::map_definition2;
                            state_stack.emplace_back(cddl_state::expect_occur_or_memberkey, '}');
                            break;
                    }
                    break;
                }
                case cddl_state::map_definition2: 
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            advance_past_space_character();
                            break;
                        case ';':
                            skip_to_end_of_line();
                            break;
                        case ',':
                            state_stack.emplace_back(cddl_state::expect_occur_or_memberkey, '}');
                            ++p_;
                            ++column_;
                            break;
                        case '}':
                            ++p_;
                            ++column_;
                            state_stack.pop_back();
                            break;
                        case '(':
                            ++p_;
                            ++column_;
                            state_stack.emplace_back(cddl_state::group, ')');
                            break;
                        default:
                            throw ser_error(cddl_errc::expected_comma_or_left_par_or_right_curbracket,line_,column_);
                            break;
                    }
                    break;
                }
                case cddl_state::group: 
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            advance_past_space_character();
                            break;
                        case ';':
                            skip_to_end_of_line();
                            break;
                        case ')':
                            ++p_;
                            ++column_;
                            state_stack.pop_back();
                            break;
                        default:
                            state_stack.back().state = cddl_state::group2;
                            state_stack.emplace_back(cddl_state::expect_occur_or_memberkey, ')');
                            break;
                    }
                    break;
                }
                case cddl_state::group2: 
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            advance_past_space_character();
                            break;
                        case ';':
                            skip_to_end_of_line();
                            break;
                        case ',':
                            state_stack.emplace_back(cddl_state::expect_occur_or_memberkey, ')');
                            ++p_;
                            ++column_;
                            break;
                        case ')':
                            ++p_;
                            ++column_;
                            state_stack.pop_back();
                            break;
                        default:
                            throw ser_error(cddl_errc::expected_comma_or_right_par,line_,column_);
                            break;
                    }
                    break;
                }
                case cddl_state::expect_groupent: 
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            advance_past_space_character();
                            break;
                        case ';':
                            skip_to_end_of_line();
                            break;
                        case '[':
                            ++p_;
                            ++column_;
                            state_stack.back().state = cddl_state::array_definition;
                            break;
                        case '{':
                            ++p_;
                            ++column_;
                            state_stack.back().state = cddl_state::map_definition;
                            break;
                        case '(':
                            ++p_;
                            ++column_;
                            state_stack.back().state = cddl_state::group;
                            break;
                        default:
                            buffer.clear();
                            state_stack.back().state = cddl_state::expect_rangeop_or_slash_or_comma_or_delimiter;
                            state_stack.emplace_back(cddl_state::expect_value, state_stack.back());
                            break;
                    }
                    break;
                }

                case cddl_state::id: 
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            if (is_hyphen_or_dot(buffer.back()))
                            {
                                throw ser_error(cddl_errc::invalid_id,line_,column_);
                            }
                            state_stack.pop_back();
                            std::cout << "id: " << buffer << "\n";
                            break;
                        default:
                            if (*p_ == state_stack.back().delimiter)
                            {
                                if (is_hyphen_or_dot(buffer.back()))
                                {
                                    throw ser_error(cddl_errc::invalid_id,line_,column_);
                                }
                                state_stack.pop_back();
                                std::cout << "id: " << buffer << "\n";
                            }
                            else if (is_ealpha(*p_) || is_digit(*p_) || is_hyphen_or_dot(*p_))
                            {
                                buffer.push_back(*p_);
                                ++p_;
                                ++column_;
                            }
                            else
                            {
                                if (is_hyphen_or_dot(buffer.back()))
                                {
                                    throw ser_error(cddl_errc::invalid_id,line_,column_);
                                }
                                state_stack.pop_back();
                            }
                            break;
                    }
                    break;
                }
                case cddl_state::value: 
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            std::cout << "value: " << buffer << "\n";
                            advance_past_space_character();
                            state_stack.pop_back();
                            break;
                        case ',':
                            std::cout << "value: " << buffer << "\n";
                            state_stack.pop_back();
                            break;
                        default:
                            if (*p_ == state_stack.back().delimiter)
                            {
                                std::cout << "value: " << buffer << "\n";
                                state_stack.pop_back();
                            }
                            else
                            {
                                buffer.push_back(*p_);
                                ++p_;
                                ++column_;
                            }
                            break;
                    }
                    break;
                }
                case cddl_state::digit1: 
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            std::cout << "number: " << buffer << "\n";
                            advance_past_space_character();
                            state_stack.pop_back();
                            break;
                        case ',':
                            std::cout << "number: " << buffer << "\n";
                            state_stack.pop_back();
                            break;
                        case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                            buffer.push_back(*p_);
                            ++p_;
                            ++column_;
                            break;
                        case '.':
                            state_stack.back().state = cddl_state::fraction;
                            buffer.push_back(*p_);
                            ++p_;
                            ++column_;
                            break;
                        case 'e':
                            state_stack.back().state = cddl_state::plus_minus_exponent;
                            buffer.push_back(*p_);
                            ++p_;
                            ++column_;
                            break;
                        default:
                            if (*p_ == state_stack.back().delimiter)
                            {
                                std::cout << "value: " << buffer << "\n";
                                state_stack.pop_back();
                            }
                            else
                            {
                                throw ser_error(cddl_errc::invalid_id,line_,column_);
                            }
                            break;
                    }
                    break;
                }
                case cddl_state::plus_minus_exponent: 
                {
                    switch (*p_)
                    {
                        case '+':
                        case '-':
                        case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                            buffer.push_back(*p_);
                            ++p_;
                            ++column_;
                            state_stack.back().state = cddl_state::exponent;
                            break;
                        default:
                            if (*p_ == state_stack.back().delimiter)
                            {
                                std::cout << "value: " << buffer << "\n";
                                state_stack.pop_back();
                            }
                            else
                            {
                                throw ser_error(cddl_errc::invalid_id,line_,column_);
                            }
                            break;
                    }
                    break;
                }
                case cddl_state::exponent: 
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            std::cout << "number: " << buffer << "\n";
                            advance_past_space_character();
                            state_stack.pop_back();
                            break;
                        case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                            buffer.push_back(*p_);
                            ++p_;
                            ++column_;
                            break;
                        default:
                            if (*p_ == state_stack.back().delimiter)
                            {
                                std::cout << "value: " << buffer << "\n";
                                state_stack.pop_back();
                            }
                            else
                            {
                                throw ser_error(cddl_errc::invalid_id,line_,column_);
                            }
                            break;
                    }
                    break;
                }
                case cddl_state::fraction: 
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            std::cout << "number: " << buffer << "\n";
                            advance_past_space_character();
                            state_stack.pop_back();
                            break;
                        case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                            buffer.push_back(*p_);
                            ++p_;
                            ++column_;
                            break;
                        case 'e':
                            state_stack.back().state = cddl_state::plus_minus_exponent;
                            buffer.push_back(*p_);
                            ++p_;
                            ++column_;
                            break;
                        default:
                            if (*p_ == state_stack.back().delimiter)
                            {
                                std::cout << "value: " << buffer << "\n";
                                state_stack.pop_back();
                            }
                            else
                            {
                                throw ser_error(cddl_errc::invalid_id,line_,column_);
                            }
                            break;
                    }
                    break;
                }
                case cddl_state::hex_number_value: 
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            std::cout << "number: " << buffer << "\n";
                            advance_past_space_character();
                            state_stack.pop_back();
                            break;
                        case ',':
                            std::cout << "number: " << buffer << "\n";
                            state_stack.pop_back();
                            break;
                        default:
                            if (*p_ == state_stack.back().delimiter)
                            {
                                std::cout << "value: " << buffer << "\n";
                                state_stack.pop_back();
                            }
                            else
                            {
                                buffer.push_back(*p_);
                                ++p_;
                                ++column_;
                            }
                            break;
                    }
                    break;
                }
                case cddl_state::zero_digit: 
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            std::cout << "number: " << buffer << "\n";
                            advance_past_space_character();
                            state_stack.pop_back();
                            break;
                        case ',':
                            std::cout << "number: " << buffer << "\n";
                            state_stack.pop_back();
                            break;
                        case 'x':
                            buffer.push_back(*p_);
                            state_stack.back().state = cddl_state::hex_number_value;
                            ++p_;
                            ++column_;
                            break;
                        default:
                            buffer.push_back(*p_);
                            state_stack.back().state = cddl_state::digit1;
                            ++p_;
                            ++column_;
                            break;
                    }
                    break;
                }
                case cddl_state::expect_rangeop_or_slash_or_comma_or_delimiter: 
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            advance_past_space_character();
                            break;
                        case ',':
                            state_stack.pop_back();
                            break;
                        case '/':
                            state_stack.emplace_back(cddl_state::expect_value, state_stack.back());
                            ++p_;
                            ++column_;
                            break;
                        case '.':
                            state_stack.back().state = cddl_state::expect_rangeop;
                            ++p_;
                            ++column_;
                            break;
                        default:
                            switch (state_stack.back().delimiter)
                            {
                                case 0:
                                    state_stack.pop_back();
                                    break;
                                default:
                                    if (*p_ == state_stack.back().delimiter)
                                    {
                                        state_stack.pop_back();
                                    }
                                    else
                                    {
                                        throw ser_error(cddl_errc::expected_rangeop_or_slash_or_comma_or_right_bracket,line_,column_);
                                    }
                                    break;
                            }
                            break;
                    }
                    break;
                }
                case cddl_state::expect_rangeop: 
                {
                    switch (*p_)
                    {
                        case '.':
                            state_stack.back().state = cddl_state::expect_exclusive_or_inclusive_rangeop;
                            ++p_;
                            ++column_;
                            break;
                        default:
                            throw ser_error(cddl_errc::expected_rangeop_or_slash_or_comma_or_right_bracket,line_,column_);
                            break;
                    }
                    break;
                }
                case cddl_state::expect_exclusive_or_inclusive_rangeop: 
                {
                    switch (*p_)
                    {
                        case '.': // inclusive
                            state_stack.back().state = cddl_state::expect_rangeop_or_slash_or_comma_or_delimiter;
                            state_stack.emplace_back(cddl_state::expect_value, state_stack.back());
                            ++p_;
                            ++column_;
                            break;
                        default:
                            state_stack.back().state = cddl_state::expect_rangeop_or_slash_or_comma_or_delimiter;
                            state_stack.emplace_back(cddl_state::expect_value, state_stack.back());
                            break;
                    }
                    break;
                }
                case cddl_state::quoted_value: 
                {
                    switch (*p_)
                    {
                        case '\"':
                            std::cout << "value: " << buffer << "\n";
                            state_stack.pop_back();
                            ++p_;
                            ++column_;
                            break;
                        default:
                            buffer.push_back(*p_);
                            ++p_;
                            ++column_;
                            break;
                    }
                    break;
                }
                default:
                {
                    JSONCONS_UNREACHABLE();
                }
            }
        }

        std::cout << "stack size: " << state_stack.size() << "\n";
        for (auto item : state_stack)
        {
            std::cout << (int)item.state << "\n";
        }

        JSONCONS_ASSERT(rule_stack.size() != 0);
        return std::move(rule_stack.front());
    }
private:

    void advance_past_space_character()
    {
        switch (*p_)
        {
            case ' ':case '\t':
                ++p_;
                ++column_;
                break;
            case '\r':
                if (p_+1 < endp_ && *(p_+1) == '\n')
                    ++p_;
                ++line_;
                column_ = 1;
                ++p_;
                break;
            case '\n':
                ++line_;
                column_ = 1;
                ++p_;
                break;
            default:
                break;
        }
    }

    bool is_hyphen_or_dot(char c)
    {
        return c == '-' || c == '.';
    }

    bool is_digit(char c)
    {
        return (c >= 0x30 && c <= 0x39);
    }

    bool is_alpha(char c)
    {
        return (c >= 0x41 && c <= 0x5A) || (c >= 0x61 && c <= 0x7A);
    }

    bool is_ealpha(char c)
    {
        switch (c)
        {
            case '@':
            case '_':
            case '$':
                return true;
            default:
                return is_alpha(c);
        }
    }

    void skip_to_end_of_line()
    {
        while (p_ < endp_)
        {
            switch (*p_)
            {
                case '\r':
                case '\n':
                    advance_past_space_character();
                    return;
                default:
                    ++p_;
                    ++column_;
                    break;
            }
        }
    }
};

std::unique_ptr<cddl_specification> cddl_specification::parse(const std::string& s)
{
    cddl_parser parser;
    return parser.parse(s);
}

}}

#endif
