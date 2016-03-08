// Copyright 2015 Daniel Parker
// Distributed under the Boost license, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// See https://github.com/danielaparker/jsoncons for latest version

#ifndef JSONCONS_JCR_JCR_PARSER_HPP
#define JSONCONS_JCR_JCR_PARSER_HPP

#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include <istream>
#include <cstdlib>
#include <stdexcept>
#include <system_error>
#include "jsoncons/json.hpp"
#include "jsoncons/parse_error_handler.hpp"
#include "jcr_input_handler.hpp"
#include "jcr_error_category.hpp"
#include "jcr_rules.hpp"

namespace jsoncons { namespace jcr {

template <typename char_type>
struct jcr_char_traits
{
};

template <>
struct jcr_char_traits<char>
{
    static std::pair<const char*,size_t> integer_literal() 
    {
        static const char* value = "integer";
        return std::pair<const char*,size_t>(value,7);
    }
    static std::pair<const char*,size_t> string_literal() 
    {
        static const char* value = "string";
        return std::pair<const char*,size_t>(value,6);
    }
};

template <>
struct jcr_char_traits<wchar_t>
{

    static std::pair<const wchar_t*,size_t> integer_literal() 
    {
        static const wchar_t* value = L"integer";
        return std::pair<const wchar_t*,size_t>(value,7);
    }
    static std::pair<const wchar_t*,size_t> string_literal() 
    {
        static const wchar_t* value = L"string";
        return std::pair<const wchar_t*,size_t>(value,6);
    }
};

enum class value_types
{
    none_t,
    integer_t,
    uinteger_t,
    double_t
};

enum class states 
{
    root,
    start, 
    slash,  
    slash_slash, 
    slash_star, 
    slash_star_star,
    expect_comma_or_end,  
    object,
    expect_member_name, 
    expect_colon,
    expect_value,
    array, 
    string,
    escape, 
    u1, 
    u2, 
    u3, 
    u4, 
    expect_surrogate_pair1, 
    expect_surrogate_pair2, 
    u6, 
    u7, 
    u8, 
    u9, 
    minus, 
    zero,  
    integer,
    dot,
    dot_dot,
    fraction,
    exp1,
    exp2,
    exp3,
    n,
    t,  
    f,  
    any_integer,
    any_string,
    rule_name,
    expect_rule,
    cr,
    lf,
    expect_named_rule,
    expect_rule_value,
    member_name,
    quoted_string_value,
    target_rule_name
    done
};

template<typename JsonT>
class basic_jcr_parser : private basic_parsing_context<typename JsonT::char_type>
{
    typedef typename rule<JsonT> rule_type;
    typedef typename JsonT::char_type char_type;
    typedef typename JsonT::string_type string_type;

    static const int default_initial_stack_capacity = 100;

    std::vector<states> stack_;
    basic_jcr_input_handler<rule_type> *handler_;
    basic_parse_error_handler<char_type> *err_handler_;
    size_t column_;
    size_t line_;
    uint32_t cp_;
    uint32_t cp2_;
    std::basic_string<char_type> string_buffer_;
    std::basic_string<char> number_buffer_;
    bool is_negative_;
    size_t index_;
    int initial_stack_capacity_;
    int max_depth_;
    int nesting_depth_;
    float_reader float_reader_;
    const char_type* begin_input_;
    const char_type* end_input_;
    const char_type* p_;
    uint8_t precision_;
    std::pair<const char_type*,size_t> literal_;
    size_t literal_index_;

    string_type member_name_;

public:
    basic_jcr_parser(basic_jcr_input_handler<rule_type>& handler)
       : handler_(std::addressof(handler)),
         err_handler_(std::addressof(basic_default_parse_error_handler<char_type>::instance())),
         column_(0),
         line_(0),
         cp_(0),
         is_negative_(false),
         index_(0),
         initial_stack_capacity_(default_initial_stack_capacity)
    {
        max_depth_ = std::numeric_limits<int>::max JSONCONS_NO_MACRO_EXP();
    }

    basic_jcr_parser(basic_jcr_input_handler<rule_type>& handler,
                      basic_parse_error_handler<char_type>& err_handler)
       : handler_(std::addressof(handler)),
         err_handler_(std::addressof(err_handler)),
         column_(0),
         line_(0),
         cp_(0),
         is_negative_(false),
         index_(0),
         initial_stack_capacity_(default_initial_stack_capacity)

    {
        max_depth_ = std::numeric_limits<int>::max JSONCONS_NO_MACRO_EXP();
    }

    const basic_parsing_context<char_type>& parsing_context() const
    {
        return *this;
    }

    ~basic_jcr_parser()
    {
    }

    size_t max_nesting_depth() const
    {
        return static_cast<size_t>(max_depth_);
    }

    void max_nesting_depth(size_t max_nesting_depth)
    {
        max_depth_ = static_cast<int>(std::min(max_nesting_depth,static_cast<size_t>(std::numeric_limits<int>::max JSONCONS_NO_MACRO_EXP())));
    }

    bool done() const
    {
        return stack_.back() == states::done;
    }

    void begin_parse()
    {
        stack_.clear();
        stack_.reserve(initial_stack_capacity_);
        stack_.push_back(states::root);
        stack_.push_back(states::start);
        line_ = 1;
        column_ = 1;
        nesting_depth_ = 0;
    }

    void check_done(const char_type* input, size_t start, size_t length)
    {
        index_ = start;
        for (; index_ < length; ++index_)
        {
            char_type curr_char_ = input[index_];
            switch (curr_char_)
            {
            case '\n':
            case '\r':
            case '\t':
            case ' ':
                break;
            default:
                err_handler_->error(std::error_code(json_parser_errc::extra_character, json_error_category()), *this);
                break;
            }
        }
    }

    void parse_string()
    {
        const char_type* sb = p_;
        bool done = false;
        while (!done && p_ < end_input_)
        {
            switch (*p_)
            {
            case 0x00:case 0x01:case 0x02:case 0x03:case 0x04:case 0x05:case 0x06:case 0x07:case 0x08:case 0x0b:
            case 0x0c:case 0x0e:case 0x0f:case 0x10:case 0x11:case 0x12:case 0x13:case 0x14:case 0x15:case 0x16:
            case 0x17:case 0x18:case 0x19:case 0x1a:case 0x1b:case 0x1c:case 0x1d:case 0x1e:case 0x1f:
                string_buffer_.append(sb,p_-sb);
                column_ += (p_ - sb + 1);
                err_handler_->error(std::error_code(json_parser_errc::illegal_control_character, json_error_category()), *this);
                // recovery - skip
                done = true;
                ++p_;
                break;
            case '\r':
                {
                    column_ += (p_ - sb + 1);
                    err_handler_->error(std::error_code(json_parser_errc::illegal_character_in_string, json_error_category()), *this);
                    // recovery - keep
                    string_buffer_.append(sb, p_ - sb + 1);
                    stack_.push_back(states::cr);
                    done = true;
                    ++p_;
                }
                break;
            case '\n':
                {
                    column_ += (p_ - sb + 1);
                    err_handler_->error(std::error_code(json_parser_errc::illegal_character_in_string, json_error_category()), *this);
                    // recovery - keep
                    string_buffer_.append(sb, p_ - sb + 1);
                    stack_.push_back(states::lf);
                    done = true;
                    ++p_;
                }
                break;
            case '\t':
                {
                    column_ += (p_ - sb + 1);
                    err_handler_->error(std::error_code(json_parser_errc::illegal_character_in_string, json_error_category()), *this);
                    // recovery - keep
                    string_buffer_.append(sb, p_ - sb + 1);
                    done = true;
                    ++p_;
                }
                break;
            case '\\': 
                string_buffer_.append(sb,p_-sb);
                column_ += (p_ - sb + 1);
                stack_.back() = states::escape;
                done = true;
                ++p_;
                break;
            case '\"':
                if (string_buffer_.length() == 0)
                {
                    end_string_value(sb,p_-sb);
                }
                else
                {
                    string_buffer_.append(sb,p_-sb);
                    end_string_value(string_buffer_.data(),string_buffer_.length());
                    string_buffer_.clear();
                }
                column_ += (p_ - sb + 1);
                done = true;
                ++p_;
                break;
            default:
                ++p_;
                break;
            }
        }
        if (!done)
        {
            string_buffer_.append(sb,p_-sb);
            column_ += (p_ - sb + 1);
        }
    }

    void parse(const char_type* const input, size_t start, size_t length)
    {
        begin_input_ = input + start;
        end_input_ = input + length;
        p_ = begin_input_;

        index_ = start;
        while ((p_ < end_input_) && (stack_.back() != states::done))
        {
            switch (*p_)
            {
            case 0x00:case 0x01:case 0x02:case 0x03:case 0x04:case 0x05:case 0x06:case 0x07:case 0x08:case 0x0b:
            case 0x0c:case 0x0e:case 0x0f:case 0x10:case 0x11:case 0x12:case 0x13:case 0x14:case 0x15:case 0x16:
            case 0x17:case 0x18:case 0x19:case 0x1a:case 0x1b:case 0x1c:case 0x1d:case 0x1e:case 0x1f:
                err_handler_->error(std::error_code(json_parser_errc::illegal_control_character, json_error_category()), *this);
                break;
            default:
                break;
            }

            switch (stack_.back())
            {
            case states::cr:
                ++line_;
                column_ = 1;
                switch (*p_)
                {
                case '\n':
                    JSONCONS_ASSERT(!stack_.empty())
                    stack_.pop_back();
                    ++p_;
                    break;
                default:
                    JSONCONS_ASSERT(!stack_.empty())
                    stack_.pop_back();
                    break;
                }
                break;
            case states::lf:
                ++line_;
                column_ = 1;
                JSONCONS_ASSERT(!stack_.empty())
                stack_.pop_back();
                break;
            case states::start: 
                {
                    switch (*p_)
                    {
                        case '\r':
                            stack_.push_back(states::cr);
                            break;
                        case '\n':
                            stack_.push_back(states::lf);
                            break;
                        case ' ':case '\t':
                        {
                            bool done = false;
                            while (!done && (p_ + 1) < end_input_)
                            {
                                switch (*(p_ + 1))
                                {
                                case ' ':case '\t':
                                    ++p_;
                                    ++column_;
                                    break;
                                default:
                                    done = true;
                                    break;
                                }
                            }
                        }
                        break; 
                    case '{':
                        if (++nesting_depth_ >= max_depth_)
                        {
                            err_handler_->error(std::error_code(json_parser_errc::max_depth_exceeded, json_error_category()), *this);
                        }
                        handler_->begin_json();
                        stack_.back() = states::object;
                        stack_.push_back(states::object);
                        handler_->begin_object(*this);
                        break;
                    case '[':
                        if (++nesting_depth_ >= max_depth_)
                        {
                            err_handler_->error(std::error_code(json_parser_errc::max_depth_exceeded, json_error_category()), *this);
                        }
                        handler_->begin_json();
                        stack_.back() = states::array;
                        stack_.push_back(states::array);
                        handler_->begin_array(*this);
                        break;
                    case '\"':
                        handler_->begin_json();
                        stack_.back() = states::string;
                        break;
                    case '-':
                        handler_->begin_json();
                        is_negative_ = true;
                        stack_.back() = states::minus;
                        break;
                    case '0': 
                        handler_->begin_json();
                        number_buffer_.push_back(static_cast<char>(*p_));
                        stack_.back() = states::zero;
                        break;
                    case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                        handler_->begin_json();
                        number_buffer_.push_back(static_cast<char>(*p_));
                        stack_.back() = states::integer;
                        break;
                    case 'f':
                        handler_->begin_json();
                        stack_.back() = states::f;
                        literal_ = json_literals<char_type>::false_literal();
                        literal_index_ = 1;
                        break;
                    case 'n':
                        handler_->begin_json();
                        stack_.back() = states::n;
                        literal_ = json_literals<char_type>::null_literal();
                        literal_index_ = 1;
                        break;
                    case 't':
                        handler_->begin_json();
                        stack_.back() = states::t;
                        literal_ = json_literals<char_type>::true_literal();
                        literal_index_ = 1;
                        break;
                    case '/':
                        stack_.push_back(states::slash);
                        break;
                    case '}':
                        err_handler_->fatal_error(std::error_code(json_parser_errc::unexpected_right_brace, json_error_category()), *this);
                        break;
                    case ']':
                        err_handler_->fatal_error(std::error_code(json_parser_errc::unexpected_right_bracket, json_error_category()), *this);
                        break;
                    default:
                        err_handler_->fatal_error(std::error_code(json_parser_errc::invalid_json_text, json_error_category()), *this);
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;

            case states::expect_comma_or_end: 
                {
                    switch (*p_)
                    {
                    case '\r':
                        stack_.push_back(states::cr);
                        break;
                    case '\n':
                        stack_.push_back(states::lf);
                        break;
                    case ' ':case '\t':
                        {
                            bool done = false;
                            while (!done && (p_ + 1) < end_input_)
                            {
                                switch (*(p_ + 1))
                                {
                                case ' ':case '\t':
                                    ++p_;
                                    ++column_;
                                    break;
                                default:
                                    done = true;
                                    break;
                                }
                            }
                        }
                        break; 
                    case '}':
                        --nesting_depth_;
                        JSONCONS_ASSERT(!stack_.empty())
                        stack_.pop_back();
                        if (stack_.back() == states::object)
                        {
                            handler_->end_object(*this);
                        }
                        else if (stack_.back() == states::array)
                        {
                            err_handler_->fatal_error(std::error_code(json_parser_errc::expected_comma_or_right_bracket, json_error_category()), *this);
                        }
                        else
                        {
                            err_handler_->fatal_error(std::error_code(json_parser_errc::unexpected_right_brace, json_error_category()), *this);
                        }

                        JSONCONS_ASSERT(stack_.size() >= 2);
                        if (stack_[stack_.size()-2] == states::root)
                        {
                            stack_.back() = states::done;
                            handler_->end_json();
                        }
                        else
                        {
                            stack_.back() = states::expect_comma_or_end;
                        }
                        break;
                    case ']':
                        --nesting_depth_;
                        JSONCONS_ASSERT(!stack_.empty())
                        stack_.pop_back();
                        if (stack_.back() == states::array)
                        {
                            handler_->end_array(*this);
                        }
                        else if (stack_.back() == states::object)
                        {
                            err_handler_->fatal_error(std::error_code(json_parser_errc::expected_comma_or_right_brace, json_error_category()), *this);
                        }
                        else
                        {
                            err_handler_->fatal_error(std::error_code(json_parser_errc::unexpected_right_bracket, json_error_category()), *this);
                        }
                        JSONCONS_ASSERT(stack_.size() >= 2);
                        if (stack_[stack_.size()-2] == states::root)
                        {
                            stack_.back() = states::done;
                            handler_->end_json();
                        }
                        else
                        {
                            stack_.back() = states::expect_comma_or_end;
                        }
                        break;
                    case ',':
                        begin_member_or_element();
                        break;
                    case '/':
                        stack_.push_back(states::slash);
                        break;
                    default:
                        JSONCONS_ASSERT(stack_.size() >= 2);
                        if (stack_[stack_.size()-2] == states::array)
                        {
                            err_handler_->error(std::error_code(json_parser_errc::expected_comma_or_right_bracket, json_error_category()), *this);
                        }
                        else if (stack_[stack_.size()-2] == states::object)
                        {
                            err_handler_->error(std::error_code(json_parser_errc::expected_comma_or_right_brace, json_error_category()), *this);
                        }
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;
            case states::object: 
                {
                    switch (*p_)
                    {
                    case '\r':
                        stack_.push_back(states::cr);
                        break;
                    case '\n':
                        stack_.push_back(states::lf);
                        break;
                    case ' ':case '\t':
                        {
                            bool done = false;
                            while (!done && (p_ + 1) < end_input_)
                            {
                                switch (*(p_ + 1))
                                {
                                case ' ':case '\t':
                                    ++p_;
                                    ++column_;
                                    break;
                                default:
                                    done = true;
                                    break;
                                }
                            }
                        }
                        break;
                    case '}':
                        --nesting_depth_;
                        JSONCONS_ASSERT(!stack_.empty())
                        stack_.pop_back();
                        if (stack_.back() != states::object)
                        {
                            err_handler_->error(std::error_code(json_parser_errc::invalid_json_text, json_error_category()), *this);
                        }
                        handler_->end_object(*this);
                        JSONCONS_ASSERT(stack_.size() >= 2);
                        if (stack_[stack_.size()-2] == states::root)
                        {
                            stack_.back() = states::done;
                            handler_->end_json();
                        }
                        else
                        {
                            stack_.back() = states::expect_comma_or_end;
                        }
                        break;
                    case '\"':
                        stack_.back() = states::member_name;
                        stack_.push_back(states::string);
                        break;
                    case '/':
                        stack_.push_back(states::slash);
                        break;
                    case '\'':
                        err_handler_->error(std::error_code(json_parser_errc::single_quote, json_error_category()), *this);
                        break;
                    default:
                        err_handler_->error(std::error_code(json_parser_errc::expected_name, json_error_category()), *this);
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;
            case states::expect_member_name: 
                {
                    switch (*p_)
                    {
                    case '\r':
                        stack_.push_back(states::cr);
                        break;
                    case '\n':
                        stack_.push_back(states::lf);
                        break;
                    case ' ':case '\t':
                        {
                            bool done = false;
                            while (!done && (p_ + 1) < end_input_)
                            {
                                switch (*(p_ + 1))
                                {
                                case ' ':case '\t':
                                    ++p_;
                                    ++column_;
                                    break;
                                default:
                                    done = true;
                                    break;
                                }
                            }
                        }
                        break;
                    case '\"':
                        //stack_.back() = states::string;
                        stack_.back() = states::member_name;
                        stack_.push_back(states::string);
                        break;
                    case '/':
                        stack_.push_back(states::slash);
                        break;
                    case '}':
                        --nesting_depth_;
                        err_handler_->error(std::error_code(json_parser_errc::extra_comma, json_error_category()), *this);
                        break;
                    case '\'':
                        err_handler_->error(std::error_code(json_parser_errc::single_quote, json_error_category()), *this);
                        break;
                    default:
                        err_handler_->error(std::error_code(json_parser_errc::expected_name, json_error_category()), *this);
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;
            case states::expect_colon: 
                {
                    switch (*p_)
                    {
                    case '\r':
                        stack_.push_back(states::cr);
                        break;
                    case '\n':
                        stack_.push_back(states::lf);
                        break;
                    case ' ':case '\t':
                        {
                            bool done = false;
                            while (!done && (p_ + 1) < end_input_)
                            {
                                switch (*(p_ + 1))
                                {
                                case ' ':case '\t':
                                    ++p_;
                                    ++column_;
                                    break;
                                default:
                                    done = true;
                                    break;
                                }
                            }
                        }
                        break;
                    case ':':
                        stack_.back() = states::expect_value;
                        break;
                    case '/':
                        stack_.push_back(states::slash);
                        break;
                    default:
                        err_handler_->error(std::error_code(json_parser_errc::expected_colon, json_error_category()), *this);
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;
            case states::expect_value: 
                {
                    switch (*p_)
                    {
                    case '\r':
                        stack_.push_back(states::cr);
                        break;
                    case '\n':
                        stack_.push_back(states::lf);
                        break;
                    case ' ':case '\t':
                        {
                            bool done = false;
                            while (!done && (p_ + 1) < end_input_)
                            {
                                switch (*(p_ + 1))
                                {
                                case ' ':case '\t':
                                    ++p_;
                                    ++column_;
                                    break;
                                default:
                                    done = true;
                                    break;
                                }
                            }
                        }
                        break;
                    case '{':
                        if (++nesting_depth_ >= max_depth_)
                        {
                            err_handler_->error(std::error_code(json_parser_errc::max_depth_exceeded, json_error_category()), *this);
                        }
                        stack_.back() = states::object;
                        stack_.push_back(states::object);
                        handler_->begin_object(*this);
                        break;
                    case '[':
                        if (++nesting_depth_ >= max_depth_)
                        {
                            err_handler_->error(std::error_code(json_parser_errc::max_depth_exceeded, json_error_category()), *this);
                        }
                        stack_.back() = states::array;
                        stack_.push_back(states::array);
                        handler_->begin_array(*this);
                        break;
                    case '/':
                        stack_.push_back(states::slash);
                        break;

                    case '\"':
                        stack_.back() = states::string;
                        break;
                    case '-':
                        is_negative_ = true;
                        stack_.back() = states::minus;
                        break;
                    case '0': 
                        number_buffer_.push_back(static_cast<char>(*p_));
                        stack_.back() = states::zero;
                        break;
                    case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                        number_buffer_.push_back(static_cast<char>(*p_));
                        stack_.back() = states::integer;
                        break;
                    case 'f':
                        stack_.back() = states::f;
                        literal_ = json_literals<char_type>::false_literal();
                        literal_index_ = 1;
                        break;
                    case 'n':
                        stack_.back() = states::n;
                        literal_ = json_literals<char_type>::null_literal();
                        literal_index_ = 1;
                        break;
                    case 't':
                        stack_.back() = states::t;
                        literal_ = json_literals<char_type>::true_literal();
                        literal_index_ = 1;
                        break;
                    case ']':
                        JSONCONS_ASSERT(stack_.size() >= 2);
                        if (stack_[stack_.size()-2] == states::array)
                        {
                            err_handler_->error(std::error_code(json_parser_errc::extra_comma, json_error_category()), *this);
                        }
                        else
                        {
                            err_handler_->error(std::error_code(json_parser_errc::expected_value, json_error_category()), *this);
                        }
                        break;
                    case '\'':
                        err_handler_->error(std::error_code(json_parser_errc::single_quote, json_error_category()), *this);
                        break;
                    default:
                        err_handler_->error(std::error_code(json_parser_errc::expected_value, json_error_category()), *this);
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;
            case states::array: 
                {
                    switch (*p_)
                    {
                    case '\r':
                        stack_.push_back(states::cr);
                        break;
                    case '\n':
                        stack_.push_back(states::lf);
                        break;
                    case ' ':case '\t':
                        {
                            bool done = false;
                            while (!done && (p_ + 1) < end_input_)
                            {
                                switch (*(p_ + 1))
                                {
                                case ' ':case '\t':
                                    ++p_;
                                    ++column_;
                                    break;
                                default:
                                    done = true;
                                    break;
                                }
                            }
                        }
                        break;
                    case '{':
                        if (++nesting_depth_ >= max_depth_)
                        {
                            err_handler_->error(std::error_code(json_parser_errc::max_depth_exceeded, json_error_category()), *this);
                        }
                        stack_.back() = states::object;
                        stack_.push_back(states::object);
                        handler_->begin_object(*this);
                        break;
                    case '[':
                        if (++nesting_depth_ >= max_depth_)
                        {
                            err_handler_->error(std::error_code(json_parser_errc::max_depth_exceeded, json_error_category()), *this);
                        }
                        stack_.back() = states::array;
                        stack_.push_back(states::array);
                        handler_->begin_array(*this);
                        break;
                    case ']':
                        --nesting_depth_;
                        JSONCONS_ASSERT(!stack_.empty())
                        stack_.pop_back();
                        if (stack_.back() != states::array)
                        {
                            err_handler_->error(std::error_code(json_parser_errc::invalid_json_text, json_error_category()), *this);
                        }
                        handler_->end_array(*this);
                        JSONCONS_ASSERT(stack_.size() >= 2);
                        if (stack_[stack_.size()-2] == states::root)
                        {
                            stack_.back() = states::done;
                            handler_->end_json();
                        }
                        else
                        {
                            stack_.back() = states::expect_comma_or_end;
                        }
                        break;
                    case '\"':
                        stack_.back() = states::string;
                        break;
                    case '/':
                        stack_.push_back(states::slash);
                        break;
                    case '-':
                        is_negative_ = true;
                        stack_.back() = states::minus;
                        break;
                    case '0': 
                        number_buffer_.push_back(static_cast<char>(*p_));
                        stack_.back() = states::zero;
                        break;
                    case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                        number_buffer_.push_back(static_cast<char>(*p_));
                        stack_.back() = states::integer;
                        break;
                    case 'f':
                        stack_.back() = states::f;
                        literal_ = json_literals<char_type>::false_literal();
                        literal_index_ = 1;
                        break;
                    case 'n':
                        stack_.back() = states::n;
                        literal_ = json_literals<char_type>::null_literal();
                        literal_index_ = 1;
                        break;
                    case 't':
                        stack_.back() = states::t;
                        literal_ = json_literals<char_type>::true_literal();
                        literal_index_ = 1;
                        break;
                    case '\'':
                        err_handler_->error(std::error_code(json_parser_errc::single_quote, json_error_category()), *this);
                        break;
                    default:
                        err_handler_->error(std::error_code(json_parser_errc::expected_value, json_error_category()), *this);
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;
            case states::string: 
                parse_string();
                break;
            case states::escape: 
                {
                    escape_next_char(*p_);
                }
                ++p_;
                ++column_;
                break;
            case states::u1: 
                {
                    append_codepoint(*p_);
                    stack_.back() = states::u2;
                }
                ++p_;
                ++column_;
                break;
            case states::u2: 
                {
                    append_codepoint(*p_);
                    stack_.back() = states::u3;
                }
                ++p_;
                ++column_;
                break;
            case states::u3: 
                {
                    append_codepoint(*p_);
                    stack_.back() = states::u4;
                }
                ++p_;
                ++column_;
                break;
            case states::u4: 
                {
                    append_codepoint(*p_);
                    if (cp_ >= min_lead_surrogate && cp_ <= max_lead_surrogate)
                    {
                        stack_.back() = states::expect_surrogate_pair1;
                    }
                    else
                    {
                        json_char_traits<char_type, sizeof(char_type)>::append_codepoint_to_string(cp_, string_buffer_);
                        stack_.back() = states::string;
                    }
                }
                ++p_;
                ++column_;
                break;
            case states::expect_surrogate_pair1: 
                {
                    switch (*p_)
                    {
                    case '\\': 
                        cp2_ = 0;
                        stack_.back() = states::expect_surrogate_pair2;
                        break;
                    default:
                        err_handler_->error(std::error_code(json_parser_errc::expected_codepoint_surrogate_pair, json_error_category()), *this);
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;
            case states::expect_surrogate_pair2: 
                {
                    switch (*p_)
                    {
                    case 'u':
                        stack_.back() = states::u6;
                        break;
                    default:
                        err_handler_->error(std::error_code(json_parser_errc::expected_codepoint_surrogate_pair, json_error_category()), *this);
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;
            case states::u6:
                {
                    append_second_codepoint(*p_);
                    stack_.back() = states::u7;
                }
                ++p_;
                ++column_;
                break;
            case states::u7: 
                {
                    append_second_codepoint(*p_);
                    stack_.back() = states::u8;
                }
                ++p_;
                ++column_;
                break;
            case states::u8: 
                {
                    append_second_codepoint(*p_);
                    stack_.back() = states::u9;
                }
                ++p_;
                ++column_;
                break;
            case states::u9: 
                {
                    append_second_codepoint(*p_);
                    uint32_t cp = 0x10000 + ((cp_ & 0x3FF) << 10) + (cp2_ & 0x3FF);
                    json_char_traits<char_type, sizeof(char_type)>::append_codepoint_to_string(cp, string_buffer_);
                    stack_.back() = states::string;
                }
                ++p_;
                ++column_;
                break;
            case states::minus:  
                {
                    switch (*p_)
                    {
                    case '0': 
                        number_buffer_.push_back(static_cast<char>(*p_));
                        stack_.back() = states::zero;
                        break;
                    case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                        number_buffer_.push_back(static_cast<char>(*p_));
                        stack_.back() = states::integer;
                        break;
                    default:
                        err_handler_->error(std::error_code(json_parser_errc::expected_value, json_error_category()), *this);
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;
            case states::zero:  
                {
                    switch (*p_)
                    {
                    case '\r':
                        stack_.push_back(states::cr);
                        break;
                    case '\n':
                        stack_.push_back(states::lf);
                        break;
                    case ' ':case '\t':
                        {
                            bool done = false;
                            while (!done && (p_ + 1) < end_input_)
                            {
                                switch (*(p_ + 1))
                                {
                                case ' ':case '\t':
                                    ++p_;
                                    ++column_;
                                    break;
                                default:
                                    done = true;
                                    break;
                                }
                            }
                        }
                        end_integer_value();
                        break; // No change
                    case '}':
                        --nesting_depth_;
                        end_integer_value();
                        JSONCONS_ASSERT(!stack_.empty())
                        stack_.pop_back();
                        if (stack_.back() != states::object)
                        {
                            err_handler_->error(std::error_code(json_parser_errc::invalid_json_text, json_error_category()), *this);
                        }
                        handler_->end_object(*this);
                        JSONCONS_ASSERT(stack_.size() >= 2);
                        if (stack_[stack_.size()-2] == states::root)
                        {
                            stack_.back() = states::done;
                            handler_->end_json();
                        }
                        else
                        {
                            stack_.back() = states::expect_comma_or_end;
                        }
                        break;
                    case ']':
                        end_integer_value();
                        --nesting_depth_;
                        JSONCONS_ASSERT(!stack_.empty())
                        stack_.pop_back();
                        if (stack_.back() != states::array)
                        {
                            err_handler_->error(std::error_code(json_parser_errc::invalid_json_text, json_error_category()), *this);
                        }
                        handler_->end_array(*this);
                        JSONCONS_ASSERT(stack_.size() >= 2);
                        if (stack_[stack_.size()-2] == states::root)
                        {
                            stack_.back() = states::done;
                            handler_->end_json();
                        }
                        else
                        {
                            stack_.back() = states::expect_comma_or_end;
                        }
                        break;
                    case '.':
                        precision_ = static_cast<uint8_t>(number_buffer_.length());
                        number_buffer_.push_back(static_cast<char>(*p_));
                        stack_.back() = states::fraction;
                        break;
                    case ',':
                        end_integer_value();
                        begin_member_or_element();
                        break;
                    case '0': case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                        err_handler_->error(std::error_code(json_parser_errc::leading_zero, json_error_category()), *this);
                        break;
                    default:
                        err_handler_->error(std::error_code(json_parser_errc::invalid_number, json_error_category()), *this);
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;
            case states::integer: 
                {
                    switch (*p_)
                    {
                    case '\r':
                        stack_.push_back(states::cr);
                        break;
                    case '\n':
                        stack_.push_back(states::lf);
                        break;
                    case ' ':case '\t':
                        {
                            bool done = false;
                            while (!done && (p_ + 1) < end_input_)
                            {
                                switch (*(p_ + 1))
                                {
                                case ' ':case '\t':
                                    ++p_;
                                    ++column_;
                                    break;
                                default:
                                    done = true;
                                    break;
                                }
                            }
                        }
                        end_integer_value();
                        break; 
                    case '}':
                        --nesting_depth_;
                        end_integer_value();
                        JSONCONS_ASSERT(!stack_.empty())
                        stack_.pop_back();
                        if (stack_.back() != states::object)
                        {
                            err_handler_->error(std::error_code(json_parser_errc::invalid_json_text, json_error_category()), *this);
                        }
                        handler_->end_object(*this);
                        JSONCONS_ASSERT(stack_.size() >= 2);
                        if (stack_[stack_.size()-2] == states::root)
                        {
                            stack_.back() = states::done;
                            handler_->end_json();
                        }
                        else
                        {
                            stack_.back() = states::expect_comma_or_end;
                        }
                        break;
                    case ']':
                        end_integer_value();
                        --nesting_depth_;
                        JSONCONS_ASSERT(!stack_.empty())
                        stack_.pop_back();
                        if (stack_.back() != states::array)
                        {
                            err_handler_->error(std::error_code(json_parser_errc::invalid_json_text, json_error_category()), *this);
                        }
                        handler_->end_array(*this);
                        JSONCONS_ASSERT(stack_.size() >= 2);
                        if (stack_[stack_.size()-2] == states::root)
                        {
                            stack_.back() = states::done;
                            handler_->end_json();
                        }
                        else
                        {
                            stack_.back() = states::expect_comma_or_end;
                        }
                        break;
                    case '0': 
                    case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                        number_buffer_.push_back(static_cast<char>(*p_));
                        stack_.back() = states::integer;
                        break;
                    case '.':
                        precision_ = static_cast<uint8_t>(number_buffer_.length());
                        number_buffer_.push_back(static_cast<char>(*p_));
                        stack_.back() = states::fraction;
                        break;
                    case ',':
                        end_integer_value();
                        begin_member_or_element();
                        break;
                    case 'e':case 'E':
                        number_buffer_.push_back(static_cast<char>(*p_));
                        stack_.back() = states::exp1;
                        break;
                    default:
                        err_handler_->error(std::error_code(json_parser_errc::invalid_number, json_error_category()), *this);
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;
            case states::fraction: 
                {
                    switch (*p_)
                    {
                    case '\r':
                        stack_.push_back(states::cr);
                        break;
                    case '\n':
                        stack_.push_back(states::lf);
                        break;
                    case ' ':case '\t':
                        {
                            bool done = false;
                            while (!done && (p_ + 1) < end_input_)
                            {
                                switch (*(p_ + 1))
                                {
                                case ' ':case '\t':
                                    ++p_;
                                    ++column_;
                                    break;
                                default:
                                    done = true;
                                    break;
                                }
                            }
                        }
                        end_fraction_value();
                        break; 
                    case '}':
                        --nesting_depth_;
                        end_fraction_value();
                        JSONCONS_ASSERT(!stack_.empty())
                        stack_.pop_back();
                        if (stack_.back() != states::object)
                        {
                            err_handler_->error(std::error_code(json_parser_errc::invalid_json_text, json_error_category()), *this);
                        }
                        handler_->end_object(*this);
                        JSONCONS_ASSERT(stack_.size() >= 2);
                        if (stack_[stack_.size()-2] == states::root)
                        {
                            stack_.back() = states::done;
                            handler_->end_json();
                        }
                        else
                        {
                            stack_.back() = states::expect_comma_or_end;
                        }
                        break;
                    case ']':
                        end_fraction_value();
                        --nesting_depth_;
                        JSONCONS_ASSERT(!stack_.empty())
                        stack_.pop_back();
                        if (stack_.back() != states::array)
                        {
                            err_handler_->error(std::error_code(json_parser_errc::invalid_json_text, json_error_category()), *this);
                        }
                        handler_->end_array(*this);
                        JSONCONS_ASSERT(stack_.size() >= 2);
                        if (stack_[stack_.size()-2] == states::root)
                        {
                            stack_.back() = states::done;
                            handler_->end_json();
                        }
                        else
                        {
                            stack_.back() = states::expect_comma_or_end;
                        }
                        break;
                    case '0': 
                    case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                        ++precision_;
                        number_buffer_.push_back(static_cast<char>(*p_));
                        stack_.back() = states::fraction;
                        break;
                    case ',':
                        end_fraction_value();
                        begin_member_or_element();
                        break;
                    case 'e':case 'E':
                        number_buffer_.push_back(static_cast<char>(*p_));
                        stack_.back() = states::exp1;
                        break;
                    default:
                        err_handler_->error(std::error_code(json_parser_errc::invalid_number, json_error_category()), *this);
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;
            case states::exp1: 
                {
                    switch (*p_)
                    {
                    case '+':
                        stack_.back() = states::exp2;
                        break;
                    case '-':
                        number_buffer_.push_back(static_cast<char>(*p_));
                        stack_.back() = states::exp2;
                        break;
                    case '0': 
                    case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                        number_buffer_.push_back(static_cast<char>(*p_));
                        stack_.back() = states::exp3;
                        break;
                    default:
                        err_handler_->error(std::error_code(json_parser_errc::expected_value, json_error_category()), *this);
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;
            case states::exp2:  
                {
                    switch (*p_)
                    {
                    case '0': 
                    case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                        number_buffer_.push_back(static_cast<char>(*p_));
                        stack_.back() = states::exp3;
                        break;
                    default:
                        err_handler_->error(std::error_code(json_parser_errc::expected_value, json_error_category()), *this);
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;
            case states::exp3: 
                {
                    switch (*p_)
                    {
                    case '\r':
                        stack_.push_back(states::cr);
                        break;
                    case '\n':
                        stack_.push_back(states::lf);
                        break;
                    case ' ':case '\t':
                        {
                            bool done = false;
                            while (!done && (p_ + 1) < end_input_)
                            {
                                switch (*(p_ + 1))
                                {
                                case ' ':case '\t':
                                    ++p_;
                                    ++column_;
                                    break;
                                default:
                                    done = true;
                                    break;
                                }
                            }
                        }
                        end_fraction_value();
                        break; 
                    case '}':
                        --nesting_depth_;
                        end_fraction_value();
                        JSONCONS_ASSERT(!stack_.empty())
                        stack_.pop_back();
                        if (stack_.back() != states::object)
                        {
                            err_handler_->error(std::error_code(json_parser_errc::invalid_json_text, json_error_category()), *this);
                        }
                        handler_->end_object(*this);
                        JSONCONS_ASSERT(stack_.size() >= 2);
                        if (stack_[stack_.size()-2] == states::root)
                        {
                            stack_.back() = states::done;
                            handler_->end_json();
                        }
                        else
                        {
                            stack_.back() = states::expect_comma_or_end;
                        }
                        break;
                    case ']':
                        end_fraction_value();
                        --nesting_depth_;
                        JSONCONS_ASSERT(!stack_.empty())
                        stack_.pop_back();
                        if (stack_.back() != states::array)
                        {
                            err_handler_->error(std::error_code(json_parser_errc::invalid_json_text, json_error_category()), *this);
                        }
                        handler_->end_array(*this);
                        JSONCONS_ASSERT(stack_.size() >= 2);
                        if (stack_[stack_.size()-2] == states::root)
                        {
                            stack_.back() = states::done;
                            handler_->end_json();
                        }
                        else
                        {
                            stack_.back() = states::expect_comma_or_end;
                        }
                        break;
                    case ',':
                        end_fraction_value();
                        begin_member_or_element();
                        break;
                    case '0': 
                    case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                        number_buffer_.push_back(static_cast<char>(*p_));
                        stack_.back() = states::exp3;
                        break;
                    default:
                        err_handler_->error(std::error_code(json_parser_errc::invalid_number, json_error_category()), *this);
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;
            case states::t: 
                while (p_ < end_input_ && literal_index_ < literal_.second)
                {
                    if (*p_ != literal_.first[literal_index_])
                    {
                        err_handler_->error(std::error_code(json_parser_errc::invalid_value, json_error_category()), *this);
                    }
                    ++p_;
                    ++literal_index_;
                    ++column_;
                }
                if (literal_index_ == literal_.second)
                {
                    handler_->value(true, *this);
                    JSONCONS_ASSERT(stack_.size() >= 2);
                    if (stack_[stack_.size()-2] == states::root)
                    {
                        stack_.back() = states::done;
                        handler_->end_json();
                    }
                    else
                    {
                        stack_.back() = states::expect_comma_or_end;
                    }
                }
                break;
            case states::f:  
                while (p_ < end_input_ && literal_index_ < literal_.second)
                {
                    if (*p_ != literal_.first[literal_index_])
                    {
                        err_handler_->error(std::error_code(json_parser_errc::invalid_value, json_error_category()), *this);
                    }
                    ++p_;
                    ++literal_index_;
                    ++column_;
                }
                if (literal_index_ == literal_.second)
                {
                    handler_->value(false, *this);
                    JSONCONS_ASSERT(stack_.size() >= 2);
                    if (stack_[stack_.size()-2] == states::root)
                    {
                        stack_.back() = states::done;
                        handler_->end_json();
                    }
                    else
                    {
                        stack_.back() = states::expect_comma_or_end;
                    }
                }
                break;
            case states::n: 
                while (p_ < end_input_ && literal_index_ < literal_.second)
                {
                    if (*p_ != literal_.first[literal_index_])
                    {
                        err_handler_->error(std::error_code(json_parser_errc::invalid_value, json_error_category()), *this);
                    }
                    ++p_;
                    ++literal_index_;
                    ++column_;
                }
                if (literal_index_ == literal_.second)
                {
                    handler_->value(null_type(), *this);
                    JSONCONS_ASSERT(stack_.size() >= 2);
                    if (stack_[stack_.size()-2] == states::root)
                    {
                        stack_.back() = states::done;
                        handler_->end_json();
                    }
                    else
                    {
                        stack_.back() = states::expect_comma_or_end;
                    }
                }
                break;
            case states::slash: 
                {
                    switch (*p_)
                    {
                    case '*':
                        stack_.back() = states::slash_star;
                        break;
                    case '/':
                        stack_.back() = states::slash_slash;
                        break;
                    default:    
                        err_handler_->error(std::error_code(json_parser_errc::invalid_json_text, json_error_category()), *this);
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;
            case states::slash_star:  
                {
                    switch (*p_)
                    {
                    case '\r':
                        stack_.push_back(states::cr);
                        break;
                    case '\n':
                        stack_.push_back(states::lf);
                        break;
                    case '*':
                        stack_.back() = states::slash_star_star;
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;
            case states::slash_slash: 
                {
                    switch (*p_)
                    {
                    case '\r':
                        stack_.back() = states::cr;
                        break;
                    case '\n':
                        stack_.back() = states::lf;
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;
            case states::slash_star_star: 
                {
                    switch (*p_)
                    {
                    case '/':
                        JSONCONS_ASSERT(!stack_.empty())
                        stack_.pop_back();
                        break;
                    default:    
                        stack_.back() = states::slash_star;
                        break;
                    }
                }
                ++p_;
                ++column_;
                break;
            default:
                JSONCONS_THROW_EXCEPTION(std::runtime_error,"Bad parser state");
                break;
            }
        }
        index_ += (p_-begin_input_);
    }

    void end_parse()
    {
        JSONCONS_ASSERT(stack_.size() >= 2);
        if (stack_[stack_.size()-2] == states::root)
        {
            switch (stack_.back())
            {
            case states::zero:  
            case states::integer:
                end_integer_value();
                break;
            case states::fraction:
            case states::exp3:
                end_fraction_value();
                break;
            default:
                break;
            }
        }
        if (stack_.back() != states::done)
        {
            err_handler_->error(std::error_code(json_parser_errc::unexpected_eof, json_error_category()), *this);
        }
    }

    states state() const
    {
        return stack_.back();
    }

    size_t index() const
    {
        return index_;
    }
private:
    void end_fraction_value()
    {
        try
        {
            double d = float_reader_.read(number_buffer_.data(), precision_);
            if (is_negative_)
                d = -d;
            handler_->value(d, static_cast<uint8_t>(precision_), *this);
        }
        catch (...)
        {
            err_handler_->error(std::error_code(json_parser_errc::invalid_number, json_error_category()), *this);
            handler_->value(null_type(), *this); // recovery
        }
        number_buffer_.clear();
        is_negative_ = false;

        JSONCONS_ASSERT(stack_.size() >= 2);
        switch (stack_[stack_.size()-2])
        {
        case states::array:
        case states::object:
            stack_.back() = states::expect_comma_or_end;
            break;
        case states::root:
            stack_.back() = states::done;
            handler_->end_json();
            break;
        default:
            err_handler_->error(std::error_code(json_parser_errc::invalid_json_text, json_error_category()), *this);
            break;
        }
    }

    void end_integer_value()
    {
        if (is_negative_)
        {
            try
            {
                int64_t d = string_to_integer(is_negative_, number_buffer_.data(), number_buffer_.length());
                handler_->value(d, *this);
            }
            catch (const std::exception&)
            {
                try
                {
                    double d = float_reader_.read(number_buffer_.data(), number_buffer_.length());
                    handler_->value(-d, static_cast<uint8_t>(number_buffer_.length()), *this);
                }
                catch (...)
                {
                    err_handler_->error(std::error_code(json_parser_errc::invalid_number, json_error_category()), *this);
                    handler_->value(null_type(), *this);
                }
            }
        }
        else
        {
            try
            {
                uint64_t d = string_to_uinteger(number_buffer_.data(), number_buffer_.length());
                handler_->value(d, *this);
            }
            catch (const std::exception&)
            {
                try
                {
                    double d = float_reader_.read(number_buffer_.data(),number_buffer_.length());
                    handler_->value(d, static_cast<uint8_t>(number_buffer_.length()), *this);
                }
                catch (...)
                {
                    err_handler_->error(std::error_code(json_parser_errc::invalid_number, json_error_category()), *this);
                    handler_->value(null_type(), *this);
                }
            }
        }

        JSONCONS_ASSERT(stack_.size() >= 2);
        switch (stack_[stack_.size()-2])
        {
        case states::array:
        case states::object:
            stack_.back() = states::expect_comma_or_end;
            break;
        case states::root:
            stack_.back() = states::done;
            handler_->end_json();
            break;
        default:
            err_handler_->error(std::error_code(json_parser_errc::invalid_json_text, json_error_category()), *this);
            break;
        }
        number_buffer_.clear();
        is_negative_ = false;
    }

    void append_codepoint(int c)
    {
        switch (c)
        {
        case '0': case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
        case 'a':case 'b':case 'c':case 'd':case 'e':case 'f':
        case 'A':case 'B':case 'C':case 'D':case 'E':case 'F':
            cp_ = append_to_codepoint(cp_, c);
            break;
        default:
            err_handler_->error(std::error_code(json_parser_errc::expected_value, json_error_category()), *this);
            break;
        }
    }

    void append_second_codepoint(int c)
    {
        switch (c)
        {
        case '0': 
        case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
        case 'a':case 'b':case 'c':case 'd':case 'e':case 'f':
        case 'A':case 'B':case 'C':case 'D':case 'E':case 'F':
            cp2_ = append_to_codepoint(cp2_, c);
            break;
        default:
            err_handler_->error(std::error_code(json_parser_errc::expected_value, json_error_category()), *this);
            break;
        }
    }

    void escape_next_char(int next_input)
    {
        switch (next_input)
        {
        case '\"':
            string_buffer_.push_back('\"');
            stack_.back() = states::string;
            break;
        case '\\': 
            string_buffer_.push_back('\\');
            stack_.back() = states::string;
            break;
        case '/':
            string_buffer_.push_back('/');
            stack_.back() = states::string;
            break;
        case 'b':
            string_buffer_.push_back('\b');
            stack_.back() = states::string;
            break;
        case 'f':  
            string_buffer_.push_back('\f');
            stack_.back() = states::string;
            break;
        case 'n':
            string_buffer_.push_back('\n');
            stack_.back() = states::string;
            break;
        case 'r':
            string_buffer_.push_back('\r');
            stack_.back() = states::string;
            break;
        case 't':
            string_buffer_.push_back('\t');
            stack_.back() = states::string;
            break;
        case 'u':
            cp_ = 0;
            stack_.back() = states::u1;
            break;
        default:    
            err_handler_->error(std::error_code(json_parser_errc::illegal_escaped_character, json_error_category()), *this);
            break;
        }
    }

    void end_string_value(const char_type* s, size_t length) 
    {
        JSONCONS_ASSERT(stack_.size() >= 2);
        switch (stack_[stack_.size()-2])
        {
        case states::member_name:
            member_name_ = string_type(s,length);
            handler_->name(s, length, *this);
            stack_.pop_back();
            stack_.back() = states::expect_colon;
            break;
        case states::object:
        case states::array:
            handler_->value(s, length, *this);
            stack_.back() = states::expect_comma_or_end;
            break;
        case states::root:
            handler_->value(s, length, *this);
            stack_.back() = states::done;
            handler_->end_json();
            break;
        default:
            err_handler_->error(std::error_code(json_parser_errc::invalid_json_text, json_error_category()), *this);
            break;
        }
    }

    void begin_member_or_element() 
    {
        JSONCONS_ASSERT(stack_.size() >= 2);
        switch (stack_[stack_.size()-2])
        {
        case states::object:
            stack_.back() = states::expect_member_name;
            break;
        case states::array:
            stack_.back() = states::expect_value;
            break;
        case states::root:
            break;
        default:
            err_handler_->error(std::error_code(json_parser_errc::invalid_json_text, json_error_category()), *this);
            break;
        }
    }
 
    uint32_t append_to_codepoint(uint32_t cp, int c)
    {
        cp *= 16;
        if (c >= '0'  &&  c <= '9')
        {
            cp += c - '0';
        }
        else if (c >= 'a'  &&  c <= 'f')
        {
            cp += c - 'a' + 10;
        }
        else if (c >= 'A'  &&  c <= 'F')
        {
            cp += c - 'A' + 10;
        }
        else
        {
            err_handler_->error(std::error_code(json_parser_errc::invalid_hex_escape_sequence, json_error_category()), *this);
        }
        return cp;
    }

    size_t do_line_number() const override
    {
        return line_;
    }

    size_t do_column_number() const override
    {
        return column_;
    }

    char_type do_current_char() const override
    {
        return p_ < end_input_? *p_ : 0;
    }
};

typedef basic_jcr_parser<json> jcr_parser;
typedef basic_jcr_parser<wjson> wjcr_parser;

}}

#endif

