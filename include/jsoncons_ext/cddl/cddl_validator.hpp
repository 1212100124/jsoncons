// Copyright 2019
// Distributed under the Boost license, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// See https://github.com/danielaparker/jsoncons for latest version

#ifndef JSONCONS_CDDL_CDDL_VALIDATOR_HPP
#define JSONCONS_CDDL_CDDL_VALIDATOR_HPP

#include <jsoncons/json_content_handler.hpp>
#include <jsoncons/staj_reader.hpp>
#include <jsoncons_ext/cddl/cddl_error.hpp>
#include <jsoncons_ext/cddl/cddl_rule.hpp>
#include <jsoncons_ext/cddl/cddl_specification.hpp>
#include <memory>

namespace jsoncons { namespace cddl {

class cddl_validator : public json_content_handler
{
public:
    typedef std::string string_type;
private:
    struct stack_item
    {
        string_type s_;
        staj_event_type event_type;

        stack_item(staj_event_type event_type)
            : event_type(event_type)
        {
        }

        stack_item(const stack_item&) = default;
        stack_item(stack_item&&) = default;
        stack_item& operator=(const stack_item&) = default;
        stack_item& operator=(stack_item&&) = default;
    };
    cddl_specification spec_;
    std::vector<stack_item> content_stack_;
    std::vector<group_entry_rule> spec_stack_;
public:
    cddl_validator(cddl_specification&& spec)
        : spec_(std::move(spec))
    {
        spec_stack_.emplace_back(spec.root());
    }
    cddl_validator(const cddl_validator&) = delete;
    cddl_validator(cddl_validator&&) = default;
    cddl_validator& operator=(const cddl_validator&) = delete;
    cddl_validator& operator=(cddl_validator&&) = default;
private:
    void do_flush() override
    {
    }

    bool do_begin_object(semantic_tag, const ser_context&) override
    {
        return true;
    }

    bool do_end_object(const ser_context&) override
    {
        return true;
    }

    bool do_begin_array(semantic_tag tag, const ser_context&) override
    {
        std::cout << "do_begin_array\n";
        if (!spec_stack_.back().rule->matches_event(staj_event(staj_event_type::begin_array,tag)))
        {
            throw std::runtime_error("Not an array");
        }
        content_stack_.emplace_back(staj_event_type::begin_array);
        return true;
    }

    bool do_end_array(const ser_context&) override
    {
        JSONCONS_ASSERT(!content_stack_.empty());
        if (content_stack_.back().event_type != staj_event_type::begin_array)
        {
            throw std::runtime_error("Invalid content stack");
        }
        content_stack_.pop_back();
        return true;
    }

    bool do_name(const string_view_type&, const ser_context&) override
    {
        return true;
    }

    bool do_null_value(semantic_tag, const ser_context&) override
    {
        return true;
    }

    bool do_string_value(const string_view_type&, semantic_tag, const ser_context&) override
    {
        return true;
    }

    bool do_byte_string_value(const byte_string_view&,
                              semantic_tag, 
                              const ser_context&) override
    {
        return true;
    }

    bool do_int64_value(int64_t, 
                        semantic_tag, 
                        const ser_context&) override
    {
        return true;
    }

    bool do_uint64_value(uint64_t, 
                         semantic_tag, 
                         const ser_context&) override
    {
        return true;
    }

    bool do_double_value(double, 
                         semantic_tag,
                         const ser_context&) override
    {
        return true;
    }

    bool do_bool_value(bool, semantic_tag, const ser_context&) override
    {
        return true;
    }
};

}}

#endif
