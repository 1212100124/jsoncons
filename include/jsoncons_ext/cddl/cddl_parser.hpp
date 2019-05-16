// Copyright 2019 Daniel Parker
// Distributed under the Boost license, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// See https://github.com/danielaparker/jsoncons for latest version

#ifndef JSONCONS_CDDL_CDDL_PARSER_HPP
#define JSONCONS_CDDL_CDDL_PARSER_HPP

#include <jsoncons/json.hpp>
#include <jsoncons_ext/cddl/cddl_error.hpp>

namespace jsoncons { namespace cddl {

template <class CharT>
class basic_cddl_specification
{
public:
    typedef CharT char_type;

    static basic_cddl_specification parse(const std::basic_string<CharT>& s);
};

enum class cddl_state : uint8_t
{
    expect_rule,
    id,
    memberkey,
    expect_assign,
    expect_colon,
    expect_groupent,
    expect_value,
    expect_memberkey,
    expect_memberkey_or_right_bracket,
    expect_memberkey_or_right_brace,
    value,
    array_definition,
    array_definition2,
    map_definition,
    map_definition2,
    groupent,
    expect_comma_or_end
};

struct state_item
{
    cddl_state state;

    state_item(cddl_state state)
        : state(state)
    {
    }
};

template <class CharT>
class basic_cddl_parser
{
public:
    typedef CharT char_type;
private:
    const char_type* p_;
    const char_type* endp_; 
    size_t line_;
    size_t column_;
public:
    basic_cddl_parser()
        : line_(1), column_(1)
    {
    }
    basic_cddl_specification<char_type> parse(const std::basic_string<char_type>& s)
    {
        std::vector<state_item> state_stack;

        state_stack.emplace_back(cddl_state::expect_rule);

        std::basic_string<char_type> buffer;

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
                                buffer.push_back(*p_);
                                state_stack.push_back(cddl_state::id);
                                ++p_;
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
                            state_stack.back() = cddl_state::expect_groupent;
                            ++p_;
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
                        default:
                            buffer.clear();
                            state_stack.back() = cddl_state::value;
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
                            buffer.clear();
                            state_stack.back() = cddl_state::memberkey;
                        break;
                    }
                    break;
                }
                case cddl_state::expect_memberkey_or_right_bracket:
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
                            state_stack.pop_back();
                            break;
                        default:
                            buffer.clear();
                            state_stack.back() = cddl_state::memberkey;
                        break;
                    }
                    break;
                }
                case cddl_state::expect_memberkey_or_right_brace:
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
                            state_stack.pop_back();
                            break;
                        default:
                            buffer.clear();
                            state_stack.back() = cddl_state::memberkey;
                        break;
                    }
                    break;
                }
                case cddl_state::expect_colon:
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
                            state_stack.back() = cddl_state::expect_value;
                            ++p_;
                            break;
                        default:
                            throw ser_error(cddl_errc::expected_assign,line_,column_);
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
                            ++p_;
                            state_stack.pop_back();
                            break;
                        case '(':
                            ++p_;
                            state_stack.push_back(cddl_state::groupent);
                            break;
                        default:
                            buffer.clear();
                            buffer.push_back(*p_);
                            state_stack.back() = cddl_state::array_definition2;
                            state_stack.push_back(cddl_state::memberkey);
                            ++p_;
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
                            state_stack.pop_back();
                            ++p_;
                            break;
                        case ',':
                            buffer.clear();
                            state_stack.push_back(cddl_state::expect_memberkey_or_right_bracket);
                            ++p_;
                            break;
                        case '(':
                            ++p_;
                            state_stack.push_back(cddl_state::groupent);
                            break;
                        default:
                            throw ser_error(cddl_errc::expected_comma_or_left_paren_or_right_bracket,line_,column_);
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
                            state_stack.pop_back();
                            break;
                        case '(':
                            ++p_;
                            state_stack.push_back(cddl_state::groupent);
                            break;
                        default:
                            buffer.clear();
                            buffer.push_back(*p_);
                            state_stack.back() = cddl_state::map_definition2;
                            state_stack.push_back(cddl_state::memberkey);
                            ++p_;
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
                            buffer.clear();
                            state_stack.push_back(cddl_state::expect_memberkey_or_right_brace);
                            ++p_;
                            break;
                        case '}':
                            ++p_;
                            state_stack.pop_back();
                            break;
                        case '(':
                            ++p_;
                            state_stack.push_back(cddl_state::groupent);
                            break;
                        default:
                            throw ser_error(cddl_errc::expected_comma_or_left_paren_or_right_brace,line_,column_);
                            break;
                    }
                    break;
                }
                case cddl_state::expect_comma_or_end: 
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
                            state_stack.pop_back();
                            ++p_;
                            break;
                        default:
                            state_stack.pop_back();
                            //++p_;
                            break;
                    }
                    break;
                }
                case cddl_state::groupent: 
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
                            state_stack.pop_back();
                            break;
                        default:
                            buffer.clear();
                            buffer.push_back(*p_);
                            state_stack.push_back(cddl_state::memberkey);
                            ++p_;
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
                            state_stack.back() = cddl_state::array_definition;
                            break;
                        case '{':
                            ++p_;
                            state_stack.back() = cddl_state::map_definition;
                            break;
                        case '(':
                            ++p_;
                            state_stack.back() = cddl_state::expect_comma_or_end;
                            state_stack.push_back(cddl_state::groupent);
                            break;
                        default:
                            throw ser_error(cddl_errc::expected_groupent,line_,column_);
                            break;
                    }
                    break;
                }
                case cddl_state::id: 
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':case '=':
                            if (is_hyphen_or_dot(buffer.back()))
                            {
                                throw ser_error(cddl_errc::invalid_id,line_,column_);
                            }
                            state_stack.back() = cddl_state::expect_assign;
                            std::cout << "id: " << buffer << "\n";
                            break;
                        default:
                            if (is_ealpha(*p_) || is_digit(*p_) || is_hyphen_or_dot(*p_))
                            {
                                buffer.push_back(*p_);
                                ++p_;
                            }
                            else
                            {
                                throw ser_error(cddl_errc::invalid_id,line_,column_);
                            }
                            break;
                    }
                    break;
                }
                case cddl_state::memberkey: 
                {
                    switch (*p_)
                    {
                        case ' ': case '\t': case '\r': case '\n':
                            std::cout << "memberkey: " << buffer << "\n";
                            advance_past_space_character();
                            state_stack.back() = cddl_state::expect_colon;
                            break;
                        case ';':
                            skip_to_end_of_line();
                            break;
                        case ':':
                            std::cout << "memberkey: " << buffer << "\n";
                            state_stack.back() = cddl_state::expect_colon;
                            break;
                        default:
                            buffer.push_back(*p_);
                            ++p_;
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
                            buffer.push_back(*p_);
                            ++p_;
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

        return basic_cddl_specification<CharT>();
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

    bool is_hyphen_or_dot(char_type c)
    {
        return c == '-' || c == '.';
    }

    bool is_digit(char_type c)
    {
        return (c >= 0x30 && c <= 0x39);
    }

    bool is_alpha(char_type c)
    {
        return (c >= 0x41 && c <= 0x5A) || (c >= 0x61 && c <= 0x7A);
    }

    bool is_ealpha(char_type c)
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
                    break;
            }
        }
    }
};

template <class CharT>
basic_cddl_specification<CharT> basic_cddl_specification<CharT>::parse(const std::basic_string<CharT>& s)
{
    basic_cddl_parser<CharT> parser;
    return parser.parse(s);
}

typedef basic_cddl_specification<char> cddl_specification;

}}

#endif
