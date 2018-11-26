// Copyright 2018 Daniel Parker
// Distributed under the Boost license, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// See https://github.com/danielaparker/jsoncons for latest version

#ifndef JSONCONS_CBOR_CBOR_SERIALIZER_HPP
#define JSONCONS_CBOR_CBOR_SERIALIZER_HPP

#include <string>
#include <sstream>
#include <vector>
#include <istream>
#include <ostream>
#include <cstdlib>
#include <limits> // std::numeric_limits
#include <fstream>
#include <memory>
#include <jsoncons/json_exception.hpp>
#include <jsoncons/jsoncons_utilities.hpp>
#include <jsoncons/json_content_handler.hpp>
#include <jsoncons/config/binary_utilities.hpp>
#include <jsoncons/detail/writer.hpp>
#include <jsoncons/detail/parse_number.hpp>

namespace jsoncons { namespace cbor {

enum class cbor_structure_type {object, indefinite_length_object, array, indefinite_length_array};

template<class CharT,class Writer=jsoncons::detail::stream_byte_writer>
class basic_cbor_serializer final : public basic_json_content_handler<CharT>
{

    enum class decimal_parse_state { start, integer, exp1, exp2, fraction1 };
public:
    using typename basic_json_content_handler<CharT>::string_view_type;
    typedef Writer writer_type;
    typedef typename Writer::output_type output_type;

private:
    struct stack_item
    {
        cbor_structure_type type_;
        size_t count_;

        stack_item(cbor_structure_type type)
           : type_(type), count_(0)
        {
        }

        size_t count() const
        {
            return count_;
        }

        bool is_object() const
        {
            return type_ == cbor_structure_type::object || type_ == cbor_structure_type::indefinite_length_object;
        }

        bool is_indefinite_length() const
        {
            return type_ == cbor_structure_type::indefinite_length_array || type_ == cbor_structure_type::indefinite_length_object;
        }

    };
    std::vector<stack_item> stack_;
    Writer writer_;

    // Noncopyable and nonmoveable
    basic_cbor_serializer(const basic_cbor_serializer&) = delete;
    basic_cbor_serializer& operator=(const basic_cbor_serializer&) = delete;
public:
    basic_cbor_serializer(output_type& os)
       : writer_(os)
    {
    }

    ~basic_cbor_serializer()
    {
        try
        {
            writer_.flush();
        }
        catch (...)
        {
        }
    }

private:
    // Implementing methods

    void do_flush() override
    {
        writer_.flush();
    }

    bool do_begin_object(semantic_tag_type, const serializing_context&) override
    {
        stack_.push_back(stack_item(cbor_structure_type::indefinite_length_object));
        
        writer_.push_back(0xbf);
        return true;
    }

    bool do_begin_object(size_t length, semantic_tag_type, const serializing_context&) override
    {
        stack_.push_back(stack_item(cbor_structure_type::object));

        if (length <= 0x17)
        {
            binary::to_big_endian(static_cast<uint8_t>(0xa0 + length), 
                                  std::back_inserter(writer_));
        } 
        else if (length <= 0xff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0xb8), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint8_t>(length), 
                                  std::back_inserter(writer_));
        } 
        else if (length <= 0xffff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0xb9), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint16_t>(length), 
                                  std::back_inserter(writer_));
        } 
        else if (length <= 0xffffffff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0xba), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint32_t>(length), 
                                  std::back_inserter(writer_));
        } 
        else if (length <= 0xffffffffffffffff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0xbb), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint64_t>(length), 
                                  std::back_inserter(writer_));
        }

        return true;
    }

    bool do_end_object(const serializing_context&) override
    {
        JSONCONS_ASSERT(!stack_.empty());
        if (stack_.back().is_indefinite_length())
        {
            writer_.push_back(0xff);
        }
        stack_.pop_back();

        end_value();
        return true;
    }

    bool do_begin_array(semantic_tag_type, const serializing_context&) override
    {
        stack_.push_back(stack_item(cbor_structure_type::indefinite_length_array));
        writer_.push_back(0x9f);
        return true;
    }

    bool do_begin_array(size_t length, semantic_tag_type tag, const serializing_context&) override
    {
        if (length == 2 && tag == semantic_tag_type::bigfloat)
        {
            writer_.push_back(0xc5);
        }
        stack_.push_back(stack_item(cbor_structure_type::array));
        if (length <= 0x17)
        {
            binary::to_big_endian(static_cast<uint8_t>(0x80 + length), 
                                  std::back_inserter(writer_));
        } 
        else if (length <= 0xff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0x98), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint8_t>(length), 
                                  std::back_inserter(writer_));
        } 
        else if (length <= 0xffff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0x99), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint16_t>(length), 
                                  std::back_inserter(writer_));
        } 
        else if (length <= 0xffffffff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0x9a), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint32_t>(length), 
                                  std::back_inserter(writer_));
        } 
        else if (length <= 0xffffffffffffffff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0x9b), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint64_t>(length), 
                                  std::back_inserter(writer_));
        }
        return true;
    }

    bool do_end_array(const serializing_context&) override
    {
        JSONCONS_ASSERT(!stack_.empty());
        if (stack_.back().is_indefinite_length())
        {
            writer_.push_back(0xff);
        }
        stack_.pop_back();
        end_value();
        return true;
    }

    bool do_name(const string_view_type& name, const serializing_context& context) override
    {
        do_string_value(name, semantic_tag_type::none, context);
        return true;
    }

    bool do_null_value(semantic_tag_type tag, const serializing_context&) override
    {
        if (tag == semantic_tag_type::undefined)
        {
            writer_.push_back(0xf7);
        }
        else
        {
            writer_.push_back(0xf6);
        }

        end_value();
        return true;
    }

    void write_string_value(const string_view_type& sv)
    {
        std::vector<uint8_t> target;
        auto result = unicons::convert(
            sv.begin(), sv.end(), std::back_inserter(target), 
            unicons::conv_flags::strict);
        if (result.ec != unicons::conv_errc())
        {
            JSONCONS_THROW(json_exception_impl<std::runtime_error>("Illegal unicode"));
        }

        const size_t length = target.size();
        if (length <= 0x17)
        {
            // fixstr stores a byte array whose length is upto 31 bytes
            binary::to_big_endian(static_cast<uint8_t>(0x60 + length), 
                                  std::back_inserter(writer_));
        }
        else if (length <= 0xff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0x78), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint8_t>(length), 
                                  std::back_inserter(writer_));
        }
        else if (length <= 0xffff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0x79), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint16_t>(length), 
                                  std::back_inserter(writer_));
        }
        else if (length <= 0xffffffff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0x7a), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint32_t>(length), 
                                  std::back_inserter(writer_));
        }
        else if (length <= 0xffffffffffffffff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0x7b), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint64_t>(length), 
                                  std::back_inserter(writer_));
        }

        for (auto c : target)
        {
            writer_.push_back(c);
        }
    }

    void write_bignum_value(const string_view_type& sv)
    {
        bignum n(sv.data(), sv.length());
        int signum;
        std::vector<uint8_t> data;
        n.dump(signum, data);
        size_t length = data.size();

        if (signum == -1)
        {
            writer_.push_back(0xc3);
        }
        else
        {
            writer_.push_back(0xc2);
        }

        if (length <= 0x17)
        {
            // fixstr stores a byte array whose length is upto 31 bytes
            binary::to_big_endian(static_cast<uint8_t>(0x40 + length), 
                                  std::back_inserter(writer_));
        }
        else if (length <= 0xff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0x58), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint8_t>(length), 
                                  std::back_inserter(writer_));
        }
        else if (length <= 0xffff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0x59), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint16_t>(length), 
                                  std::back_inserter(writer_));
        }
        else if (length <= 0xffffffff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0x5a), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint32_t>(length), 
                                  std::back_inserter(writer_));
        }
        else if (length <= 0xffffffffffffffff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0x5b), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint64_t>(length), 
                                  std::back_inserter(writer_));
        }

        for (auto c : data)
        {
            writer_.push_back(c);
        }
    }

    void write_decimal_value(const string_view_type& sv, const serializing_context& context)
    {
        decimal_parse_state state = decimal_parse_state::start;
        std::basic_string<CharT> s;
        std::basic_string<CharT> exponent;
        int64_t scale = 0;
        for (auto c : sv)
        {
            switch (state)
            {
                case decimal_parse_state::start:
                {
                    switch (c)
                    {
                        case '-':
                            s.push_back(c);
                            state = decimal_parse_state::integer;
                            break;
                        case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                            s.push_back(c);
                            state = decimal_parse_state::integer;
                            break;
                        default:
                            throw std::invalid_argument("Invalid decimal string");
                    }
                    break;
                }
                case decimal_parse_state::integer:
                {
                    switch (c)
                    {
                        case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                            s.push_back(c);
                            state = decimal_parse_state::integer;
                            break;
                        case 'e': case 'E':
                            state = decimal_parse_state::exp1;
                            break;
                        case '.':
                            state = decimal_parse_state::fraction1;
                            break;
                        default:
                            throw std::invalid_argument("Invalid decimal string");
                    }
                    break;
                }
                case decimal_parse_state::exp1:
                {
                    switch (c)
                    {
                        case '+':
                            state = decimal_parse_state::exp2;
                            break;
                        case '-':
                            exponent.push_back(c);
                            state = decimal_parse_state::exp2;
                            break;
                        case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                            exponent.push_back(c);
                            state = decimal_parse_state::exp2;
                            break;
                        default:
                            throw std::invalid_argument("Invalid decimal string");
                    }
                    break;
                }
                case decimal_parse_state::exp2:
                {
                    switch (c)
                    {
                        case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                            exponent.push_back(c);
                            break;
                        default:
                            throw std::invalid_argument("Invalid decimal string");
                    }
                    break;
                }
                case decimal_parse_state::fraction1:
                {
                    switch (c)
                    {
                        case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                            s.push_back(c);
                            --scale;
                            break;
                        default:
                            throw std::invalid_argument("Invalid decimal string");
                    }
                    break;
                }
            }
        }

        writer_.push_back(0xc4);
        do_begin_array((size_t)2, semantic_tag_type::none, context);
        if (exponent.length() > 0)
        {
            auto result = jsoncons::detail::to_integer<int64_t>(exponent.data(), exponent.length());
            if (result.overflow)
            {
                throw std::invalid_argument("Invalid decimal string");
            }
            scale += result.value;
        }
        do_int64_value(scale, semantic_tag_type::none, context);

        auto result = jsoncons::detail::to_integer<int64_t>(s.data(),s.length());
        if (!result.overflow)
        {
            do_int64_value(result.value, semantic_tag_type::none, context);
        }
        else
        {
            write_bignum_value(s);
        }
        do_end_array(context);
    }

    bool do_string_value(const string_view_type& sv, semantic_tag_type tag, const serializing_context& context) override
    {
        switch (tag)
        {
            case semantic_tag_type::bignum:
            {
                write_bignum_value(sv);
                break;
            }
            case semantic_tag_type::decimal_fraction:
            {
                write_decimal_value(sv, context);
                break;
            }
            case semantic_tag_type::date_time:
            {
                writer_.push_back(0xc0);
                write_string_value(sv);
                break;
            }
            default:
            {
                write_string_value(sv);
                break;
            }
        }

        end_value();
        return true;
    }

    bool do_byte_string_value(const byte_string_view& b, 
                              byte_string_chars_format encoding_hint,
                              semantic_tag_type, 
                              const serializing_context&) override
    {
        switch (encoding_hint)
        {
            case byte_string_chars_format::base64url:
                writer_.push_back(0xd5);
                break;
            case byte_string_chars_format::base64:
                writer_.push_back(0xd6);
                break;
            case byte_string_chars_format::base16:
                writer_.push_back(0xd7);
                break;
            default:
                break;
        }
        if (b.length() <= 0x17)
        {
            // fixstr stores a byte array whose length is upto 31 bytes
            binary::to_big_endian(static_cast<uint8_t>(0x40 + b.length()), 
                                  std::back_inserter(writer_));
        }
        else if (b.length() <= 0xff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0x58), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint8_t>(b.length()), 
                                  std::back_inserter(writer_));
        }
        else if (b.length() <= 0xffff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0x59), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint16_t>(b.length()), 
                                  std::back_inserter(writer_));
        }
        else if (b.length() <= 0xffffffff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0x5a), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint32_t>(b.length()), 
                                  std::back_inserter(writer_));
        }
        else if (b.length() <= 0xffffffffffffffff)
        {
            binary::to_big_endian(static_cast<uint8_t>(0x5b), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint64_t>(b.length()), 
                                  std::back_inserter(writer_));
        }

        for (auto c : b)
        {
            writer_.push_back(c);
        }

        end_value();
        return true;
    }

    bool do_double_value(double val, 
                         const floating_point_options&, 
                         semantic_tag_type tag,
                         const serializing_context&) override
    {
        if (tag == semantic_tag_type::epoch_time)
        {
            writer_.push_back(0xc1);
        }

        float valf = (float)val;
        if ((double)valf == val)
        {
            binary::to_big_endian(static_cast<uint8_t>(0xfa), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(valf, std::back_inserter(writer_));
        }
        else
        {
            binary::to_big_endian(static_cast<uint8_t>(0xfb), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(val, std::back_inserter(writer_));
        }

        // write double

        end_value();
        return true;
    }

    bool do_int64_value(int64_t value, 
                        semantic_tag_type tag, 
                        const serializing_context&) override
    {
        if (tag == semantic_tag_type::epoch_time)
        {
            writer_.push_back(0xc1);
        }
        if (value >= 0)
        {
            if (value <= 0x17)
            {
                binary::to_big_endian(static_cast<uint8_t>(value), 
                                  std::back_inserter(writer_));
            } 
            else if (value <= (std::numeric_limits<uint8_t>::max)())
            {
                binary::to_big_endian(static_cast<uint8_t>(0x18), 
                                  std::back_inserter(writer_));
                binary::to_big_endian(static_cast<uint8_t>(value), 
                                  std::back_inserter(writer_));
            } 
            else if (value <= (std::numeric_limits<uint16_t>::max)())
            {
                binary::to_big_endian(static_cast<uint8_t>(0x19), 
                                  std::back_inserter(writer_));
                binary::to_big_endian(static_cast<uint16_t>(value), 
                                  std::back_inserter(writer_));
            } 
            else if (value <= (std::numeric_limits<uint32_t>::max)())
            {
                binary::to_big_endian(static_cast<uint8_t>(0x1a), 
                                  std::back_inserter(writer_));
                binary::to_big_endian(static_cast<uint32_t>(value), 
                                  std::back_inserter(writer_));
            } 
            else if (value <= (std::numeric_limits<int64_t>::max)())
            {
                binary::to_big_endian(static_cast<uint8_t>(0x1b), 
                                  std::back_inserter(writer_));
                binary::to_big_endian(static_cast<int64_t>(value), 
                                  std::back_inserter(writer_));
            }
        } else
        {
            const auto posnum = -1 - value;
            if (value >= -24)
            {
                binary::to_big_endian(static_cast<uint8_t>(0x20 + posnum), 
                                  std::back_inserter(writer_));
            } 
            else if (posnum <= (std::numeric_limits<uint8_t>::max)())
            {
                binary::to_big_endian(static_cast<uint8_t>(0x38), 
                                  std::back_inserter(writer_));
                binary::to_big_endian(static_cast<uint8_t>(posnum), 
                                  std::back_inserter(writer_));
            } 
            else if (posnum <= (std::numeric_limits<uint16_t>::max)())
            {
                binary::to_big_endian(static_cast<uint8_t>(0x39), 
                                  std::back_inserter(writer_));
                binary::to_big_endian(static_cast<uint16_t>(posnum), 
                                  std::back_inserter(writer_));
            } 
            else if (posnum <= (std::numeric_limits<uint32_t>::max)())
            {
                binary::to_big_endian(static_cast<uint8_t>(0x3a), 
                                  std::back_inserter(writer_));
                binary::to_big_endian(static_cast<uint32_t>(posnum), 
                                  std::back_inserter(writer_));
            } 
            else if (posnum <= (std::numeric_limits<int64_t>::max)())
            {
                binary::to_big_endian(static_cast<uint8_t>(0x3b), 
                                  std::back_inserter(writer_));
                binary::to_big_endian(static_cast<int64_t>(posnum), 
                                  std::back_inserter(writer_));
            }
        }
        end_value();
        return true;
    }

    bool do_uint64_value(uint64_t value, 
                         semantic_tag_type tag, 
                         const serializing_context&) override
    {
        if (tag == semantic_tag_type::epoch_time)
        {
            writer_.push_back(0xc1);
        }

        if (value <= 0x17)
        {
            binary::to_big_endian(static_cast<uint8_t>(value), 
                                  std::back_inserter(writer_));
        } 
        else if (value <=(std::numeric_limits<uint8_t>::max)())
        {
            binary::to_big_endian(static_cast<uint8_t>(0x18), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint8_t>(value), 
                                  std::back_inserter(writer_));
        } 
        else if (value <=(std::numeric_limits<uint16_t>::max)())
        {
            binary::to_big_endian(static_cast<uint8_t>(0x19), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint16_t>(value), 
                                  std::back_inserter(writer_));
        } 
        else if (value <=(std::numeric_limits<uint32_t>::max)())
        {
            binary::to_big_endian(static_cast<uint8_t>(0x1a), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint32_t>(value), 
                                  std::back_inserter(writer_));
        } 
        else if (value <=(std::numeric_limits<uint64_t>::max)())
        {
            binary::to_big_endian(static_cast<uint8_t>(0x1b), 
                                  std::back_inserter(writer_));
            binary::to_big_endian(static_cast<uint64_t>(value), 
                                  std::back_inserter(writer_));
        }
        end_value();
        return true;
    }

    bool do_bool_value(bool value, semantic_tag_type, const serializing_context&) override
    {
        if (value)
        {
            writer_.push_back(0xf5);
        }
        else
        {
            writer_.push_back(0xf4);
        }

        end_value();
        return true;
    }

    void end_value()
    {
        if (!stack_.empty())
        {
            ++stack_.back().count_;
        }
    }
};

typedef basic_cbor_serializer<char,jsoncons::detail::stream_byte_writer> cbor_serializer;

typedef basic_cbor_serializer<char,jsoncons::detail::bytes_writer> cbor_bytes_serializer;

}}
#endif
