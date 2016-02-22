// Copyright 201r3 Daniel Parker
// Distributed under the Boost license, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// See https://github.com/danielaparker/jsoncons for latest version

#ifndef JSONCONS_JCR_JCR_HPP
#define JSONCONS_JCR_JCR_HPP

#include <limits>
#include <string>
#include <vector>
#include <exception>
#include <cstdlib>
#include <cstring>
#include <ostream>
#include <memory>
#include <typeinfo>
#include "jsoncons/json_structures.hpp"
#include "jsoncons/jsoncons.hpp"
#include "jsoncons/json_output_handler.hpp"
#include "jsoncons/output_format.hpp"
#include "jsoncons/json_serializer.hpp"
#include "jsoncons/json_deserializer.hpp"
#include "jsoncons/json_reader.hpp"
#include "jsoncons/json_type_traits.hpp"
#include "jsoncons_ext/jcr/jcr_deserializer.hpp"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
#endif

namespace jsoncons { namespace jcr {

template <typename CharT,class T> inline
void serialize(basic_json_output_handler<CharT>& os, const T&)
{
    os.value(null_type());
}

template <typename StringT, class Alloc>
class basic_json_schema;

template <typename CharT>
class basic_parse_error_handler;

enum class value_types : uint8_t 
{
    // Simple types
    empty_object_t,
    double_t,
    integer_t,
    uinteger_t,
    bool_t,
    null_t,
    // Non simple types
    string_t,
    object_t,
    array_t
};

inline
bool is_simple(value_types type)
{
    return type < value_types::string_t;
}

template <typename StringT, typename Alloc = std::allocator<typename StringT::value_type>>
class basic_json_schema
{
public:

    typedef Alloc allocator_type;

    typedef typename StringT::value_type char_type;
    typedef typename StringT::traits_type char_traits_type;

    typedef typename StringT::allocator_type string_allocator;
    typedef StringT string_type;
    typedef basic_json_schema<StringT,Alloc> value_type;
    typedef name_value_pair<string_type,value_type> member_type;

    typedef typename std::allocator_traits<Alloc>:: template rebind_alloc<basic_json_schema<StringT,Alloc>> array_allocator;

    typedef typename std::allocator_traits<Alloc>:: template rebind_alloc<member_type> object_allocator;

    typedef json_array<basic_json_schema<StringT,Alloc>,array_allocator> array;
    typedef json_object<string_type,basic_json_schema<StringT,Alloc>,object_allocator>  object;

    typedef jsoncons::null_type null_type;

    typedef typename object::iterator object_iterator;
    typedef typename object::const_iterator const_object_iterator;
    typedef typename array::iterator array_iterator;
    typedef typename array::const_iterator const_array_iterator;

    template <typename IteratorT>
    class range 
    {
        IteratorT first_;
        IteratorT last_;
    public:
        range(const IteratorT& first, const IteratorT& last)
            : first_(first), last_(last)
        {
        }

    public:
        friend class basic_json_schema<StringT, Alloc>;

        IteratorT begin()
        {
            return first_;
        }
        IteratorT end()
        {
            return last_;
        }
    };

    typedef range<object_iterator> object_range;
    typedef range<const_object_iterator> const_object_range;
    typedef range<array_iterator> array_range;
    typedef range<const_array_iterator> const_array_range;

    struct variant
    {
        struct string_data : public string_allocator
        {
            const char_type* c_str() const { return p_; }
            const char_type* data() const { return p_; }
            size_t length() const { return length_; }
            string_allocator get_allocator() const
            {
                return *this;
            }

            bool operator==(const string_data& rhs) const
            {
                return length() == rhs.length() ? std::char_traits<char_type>::compare(data(), rhs.data(), length()) == 0 : false;
            }

            string_data(const string_allocator& allocator)
                : string_allocator(allocator), p_(nullptr), length_(0)
            {
            }

            char_type* p_;
            size_t length_;
        private:
            string_data(const string_data&);
            string_data& operator=(const string_data&);
        };

        struct string_dataA
        {
            string_data data;
            char_type c[1];
        };
        typedef typename std::aligned_storage<sizeof(string_dataA), JSONCONS_ALIGNOF(string_dataA)>::type storage_type;

        static size_t aligned_size(size_t n)
        {
            return sizeof(storage_type) + n;
        }

        string_data* create_string_data(const char_type* s, size_t length, const string_allocator& allocator)
        {
            size_t mem_size = aligned_size(length*sizeof(char));

            typename std::allocator_traits<string_allocator>:: template rebind_alloc<char> alloc(allocator);

            char* storage = alloc.allocate(mem_size);
            string_data* ps = new(storage)string_data(allocator);
            auto psa = reinterpret_cast<string_dataA*>(storage); 

            ps->p_ = new(&psa->c)char_type[length + 1];
            memcpy(ps->p_, s, length*sizeof(char_type));
            ps->p_[length] = 0;
            ps->length_ = length;
            return ps;
        }

        void destroy_string_data(const string_allocator& allocator, string_data* p)
        {
            size_t mem_size = aligned_size(p->length_*sizeof(char_type));
            typename std::allocator_traits<string_allocator>:: template rebind_alloc<char> alloc(allocator);
            alloc.deallocate(reinterpret_cast<char*>(p),mem_size);
        }

        static const size_t small_string_capacity = (sizeof(int64_t)/sizeof(char_type)) - 1;

        variant()
            : type_(value_types::empty_object_t)
        {
        }

        variant(const Alloc& a)
            : type_(value_types::object_t)
        {
            value_.object_val_ = create_impl<object>(a, object_allocator(a));
        }

        variant(std::initializer_list<value_type> init,
                const Alloc& a)
            : type_(value_types::array_t)
        {
            value_.array_val_ = create_impl<array>(a, std::move(init), array_allocator(a));
        }

        explicit variant(variant&& var)
            : type_(value_types::null_t)
        {
            swap(var);
        }
        
        explicit variant(variant&& var, const Alloc& a)
            : type_(value_types::null_t)
        {
            swap(var);
        }

        explicit variant(const variant& var)
        {
            init_variant(var);
        }
        explicit variant(const variant& var, const Alloc& a)
            : type_(var.type_)
        {
            init_variant(var);
        }

        variant(const object & val)
            : type_(value_types::object_t)
        {
            value_.object_val_ = create_impl<object>(val.get_allocator(), val) ;
        }

        variant(const object & val, const Alloc& a)
            : type_(value_types::object_t)
        {
            value_.object_val_ = create_impl<object>(a, val, object_allocator(a)) ;
        }

        variant(object&& val)
            : type_(value_types::object_t)
        {
            value_.object_val_ = create_impl<object>(val.get_allocator(), std::move(val));
        }

        variant(object&& val, const Alloc& a)
            : type_(value_types::object_t)
        {
            value_.object_val_ = create_impl<object>(a, std::move(val), object_allocator(a));
        }

        variant(const array& val)
            : type_(value_types::array_t)
        {
            value_.array_val_ = create_impl<array>(val.get_allocator(), val);
        }

        variant(const array& val, const Alloc& a)
            : type_(value_types::array_t)
        {
            value_.array_val_ = create_impl<array>(a, val, array_allocator(a));
        }

        variant(array&& val)
            : type_(value_types::array_t)
        {
            value_.array_val_ = create_impl<array>(val.get_allocator(), std::move(val));
        }

        variant(array&& val, const Alloc& a)
            : type_(value_types::array_t)
        {
            value_.array_val_ = create_impl<array>(a, std::move(val), array_allocator(a));
        }

        explicit variant(null_type)
            : type_(value_types::null_t)
        {
        }

        explicit variant(bool val)
            : type_(value_types::bool_t)
        {
            value_.bool_val_ = val;
        }

        explicit variant(double val, uint8_t precision)
            : type_(value_types::double_t), length_or_precision_(precision)
        {
            value_.double_val_ = val;
        }

        explicit variant(int64_t val)
            : type_(value_types::integer_t)
        {
            value_.integer_val_ = val;
        }

        explicit variant(uint64_t val)
            : type_(value_types::uinteger_t)
        {
            value_.uinteger_val_ = val;
        }

        explicit variant(const string_type& s, const Alloc& a)
        {
            type_ = value_types::string_t;
            //value_.string_val_ = create_impl<string_type>(a, s, string_allocator(a));
            value_.string_val_ = create_string_data(s.data(), s.length(), string_allocator(a));
        }

        explicit variant(const char_type* s, const Alloc& a)
        {
            size_t length = std::char_traits<char_type>::length(s);
            type_ = value_types::string_t;
            //value_.string_val_ = create_impl<string_type>(a, s, string_allocator(a));
            value_.string_val_ = create_string_data(s, length, string_allocator(a));
        }

        explicit variant(const char_type* s, size_t length, const Alloc& a)
        {
            type_ = value_types::string_t;
            //value_.string_val_ = create_impl<string_type>(a, s, length, string_allocator(a));
            value_.string_val_ = create_string_data(s, length, string_allocator(a));
        }

        template<class InputIterator>
        variant(InputIterator first, InputIterator last, const Alloc& a)
            : type_(value_types::array_t)
        {
            value_.array_val_ = create_impl<array>(a, first, last, array_allocator(a));
        }

        void init_variant(const variant& var)
        {
            type_ = var.type_;
            switch (type_)
            {
            case value_types::null_t:
            case value_types::empty_object_t:
                break;
            case value_types::double_t:
                length_or_precision_ = 0;
                value_.double_val_ = var.value_.double_val_;
                break;
            case value_types::integer_t:
                value_.integer_val_ = var.value_.integer_val_;
                break;
            case value_types::uinteger_t:
                value_.uinteger_val_ = var.value_.uinteger_val_;
                break;
            case value_types::bool_t:
                value_.bool_val_ = var.value_.bool_val_;
                break;
            case value_types::string_t:
                //value_.string_val_ = create_impl<string_type>(var.value_.string_val_->get_allocator(), *(var.value_.string_val_), string_allocator(var.value_.string_val_->get_allocator()));
                value_.string_val_ = create_string_data(var.value_.string_val_->data(), var.value_.string_val_->length(), string_allocator(var.value_.string_val_->get_allocator()));
                break;
            case value_types::array_t:
                value_.array_val_ = create_impl<array>(var.value_.array_val_->get_allocator(), *(var.value_.array_val_), array_allocator(var.value_.array_val_->get_allocator()));
                break;
            case value_types::object_t:
                value_.object_val_ = create_impl<object>(var.value_.object_val_->get_allocator(), *(var.value_.object_val_), object_allocator(var.value_.object_val_->get_allocator()));
                break;
            default:
                break;
            }
        }

        ~variant()
        {
            destroy_variant();
        }

        void destroy_variant()
        {
            switch (type_)
            {
            case value_types::string_t:
                //destroy_impl(value_.string_val_->get_allocator(), value_.string_val_);
                destroy_string_data(value_.string_val_->get_allocator(), value_.string_val_);
                break;
            case value_types::array_t:
                destroy_impl(value_.array_val_->get_allocator(), value_.array_val_);
                break;
            case value_types::object_t:
                destroy_impl(value_.object_val_->get_allocator(), value_.object_val_);
                break;
            default:
                break; 
            }
        }

        variant& operator=(const variant& val)
        {
            if (this != &val)
            {
                if (is_simple(type_))
                {
                    if (is_simple(val.type_))
                    {
                        type_ = val.type_;
                        length_or_precision_ = val.length_or_precision_;
                        value_ = val.value_;
                    }
                    else
                    {
                        init_variant(val);
                    }
                }
                else
                {
                    destroy_variant();
                    init_variant(val);
                }
            }
            return *this;
        }

        variant& operator=(variant&& val)
        {
            if (this != &val)
            {
                val.swap(*this);
            }
            return *this;
        }

        void assign(const object & val)
        {
            destroy_variant();
            type_ = value_types::object_t;
            value_.object_val_ = create_impl<object>(val.get_allocator(), val, object_allocator(val.get_allocator()));
        }

        void assign(object && val)
        {
            switch (type_)
            {
            case value_types::object_t:
                value_.object_val_->swap(val);
                break;
            default:
                destroy_variant();
                type_ = value_types::object_t;
                value_.object_val_ = create_impl<object>(val.get_allocator(), std::move(val), object_allocator(val.get_allocator()));
                break;
            }
        }

        void assign(const array& val)
        {
            destroy_variant();
            type_ = value_types::array_t;
            value_.array_val_ = create_impl<array>(val.get_allocator(), val, array_allocator(val.get_allocator())) ;
        }

        void assign(array&& val)
        {
            switch (type_)
            {
            case value_types::array_t:
                value_.array_val_->swap(val);
                break;
            default:
                destroy_variant();
                type_ = value_types::array_t;
                value_.array_val_ = create_impl<array>(val.get_allocator(), std::move(val), array_allocator(val.get_allocator()));
                break;
            }
        }

        void assign(const string_type& s)
        {
            destroy_variant();
            type_ = value_types::string_t;
            //value_.string_val_ = create_impl<string_type>(s.get_allocator(), s, string_allocator(s.get_allocator()));
            value_.string_val_ = create_string_data(s.data(), s.length(), string_allocator(s.get_allocator()));
        }

        void assign_string(const char_type* s, size_t length, const Alloc& allocator = Alloc())
        {
            destroy_variant();
            type_ = value_types::string_t;
            //value_.string_val_ = create_impl<string_type>(allocator, s, length, string_allocator(allocator));
            value_.string_val_ = create_string_data(s, length, string_allocator(allocator));
        }

        void assign(int64_t val)
        {
            destroy_variant();
            type_ = value_types::integer_t;
            value_.integer_val_ = val;
        }

        void assign(uint64_t val)
        {
            destroy_variant();
            type_ = value_types::uinteger_t;
            value_.uinteger_val_ = val;
        }

        void assign(double val, uint8_t precision = 0)
        {
            destroy_variant();
            type_ = value_types::double_t;
            length_or_precision_ = precision;
            value_.double_val_ = val;
        }

        void assign(bool val)
        {
            destroy_variant();
            type_ = value_types::bool_t;
            value_.bool_val_ = val;
        }

        void assign(null_type)
        {
            destroy_variant();
            type_ = value_types::null_t;
        }

        bool operator!=(const variant& rhs) const
        {
            return !(*this == rhs);
        }

        bool operator==(const variant& rhs) const
        {
            if (is_number() & rhs.is_number())
            {
                switch (type_)
                {
                case value_types::integer_t:
                    switch (rhs.type_)
                    {
                    case value_types::integer_t:
                        return value_.integer_val_ == rhs.value_.integer_val_;
                    case value_types::uinteger_t:
                        return value_.integer_val_ == rhs.value_.uinteger_val_;
                    case value_types::double_t:
                        return value_.integer_val_ == rhs.value_.double_val_;
                    default:
                        break;
                    }
                    break;
                case value_types::uinteger_t:
                    switch (rhs.type_)
                    {
                    case value_types::integer_t:
                        return value_.uinteger_val_ == rhs.value_.integer_val_;
                    case value_types::uinteger_t:
                        return value_.uinteger_val_ == rhs.value_.uinteger_val_;
                    case value_types::double_t:
                        return value_.uinteger_val_ == rhs.value_.double_val_;
                    default:
                        break;
                    }
                    break;
                case value_types::double_t:
                    switch (rhs.type_)
                    {
                    case value_types::integer_t:
                        return value_.double_val_ == rhs.value_.integer_val_;
                    case value_types::uinteger_t:
                        return value_.double_val_ == rhs.value_.uinteger_val_;
                    case value_types::double_t:
                        return value_.double_val_ == rhs.value_.double_val_;
                    default:
                        break;
                    }
                    break;
                default:
                    break;
                }
            }

            if (rhs.type_ != type_)
            {
                return false;
            }
            switch (type_)
            {
            case value_types::bool_t:
                return value_.bool_val_ == rhs.value_.bool_val_;
            case value_types::null_t:
            case value_types::empty_object_t:
                return true;
            case value_types::string_t:
                return *(value_.string_val_) == *(rhs.value_.string_val_);
            case value_types::array_t:
                return *(value_.array_val_) == *(rhs.value_.array_val_);
                break;
            case value_types::object_t:
                return *(value_.object_val_) == *(rhs.value_.object_val_);
                break;
            default:
                // throw
                break;
            }
            return false;
        }

        bool validate(const basic_json<StringT,Alloc>& val) const
        {
            if (is_number() & val.is_number())
            {
                switch (type_)
                {
                case value_types::integer_t:
                    return value_.integer_val_ == val.as_integer();
                case value_types::uinteger_t:
                    return value_.uinteger_val_ == val.as_uinteger();
                case value_types::double_t:
                    return value_.double_val_ == val.as_double();
                default:
                    break;
                }
            }

            switch (type_)
            {
            case value_types::bool_t:
                return value_.bool_val_ == val.as_bool();
            case value_types::null_t:
                return val.is_null();
            case value_types::empty_object_t:
                return val.is_object() && val.size() == 0;
            case value_types::string_t:
                return *(value_.string_val_) == val.as_string();
            case value_types::array_t:
                return *(value_.array_val_) == val.array_value();
                break;
            case value_types::object_t:
                return *(value_.object_val_) == val.object_value();
                break;
            default:
                // throw
                break;
            }
            return false;
        }

        bool is_null() const JSONCONS_NOEXCEPT
        {
            return type_ == value_types::null_t;
        }

        bool is_bool() const JSONCONS_NOEXCEPT
        {
            return type_ == value_types::bool_t;
        }

        bool empty() const JSONCONS_NOEXCEPT
        {
            switch (type_)
            {
            case value_types::string_t:
                return value_.string_val_->length() == 0;
            case value_types::array_t:
                return value_.array_val_->size() == 0;
            case value_types::empty_object_t:
                return true;
            case value_types::object_t:
                return value_.object_val_->size() == 0;
            default:
                return false;
            }
        }

        bool is_string() const JSONCONS_NOEXCEPT
        {
            return (type_ == value_types::string_t);
        }

        bool is_number() const JSONCONS_NOEXCEPT
        {
            return type_ == value_types::double_t || type_ == value_types::integer_t || type_ == value_types::uinteger_t;
        }

        void swap(variant& rhs)
        {
            using std::swap;
            if (this == &rhs)
            {
                // same object, do nothing
            }
            else
            {
                swap(type_, rhs.type_);
                swap(length_or_precision_, rhs.length_or_precision_);
                swap(value_, rhs.value_);
            }
        }

        value_types type_;
        uint8_t length_or_precision_;
        union
        {
            double double_val_;
            int64_t integer_val_;
            uint64_t uinteger_val_;
            bool bool_val_;
            object* object_val_;
            array* array_val_;
            string_data* string_val_;
            char_type small_string_val_[sizeof(int64_t)/sizeof(char_type)];
        } value_;
    };

    static basic_json_schema parse_stream(std::basic_istream<char_type>& is);
    static basic_json_schema parse_stream(std::basic_istream<char_type>& is, basic_parse_error_handler<char_type>& err_handler);

    static basic_json_schema parse(const string_type& s)
    {
        basic_json_deserializer<basic_json_schema<StringT, Alloc>> handler;
        basic_json_parser<char_type> parser(handler);
        parser.begin_parse();
        parser.parse(s.data(),0,s.length());
        parser.end_parse();
        parser.check_done(s.data(),parser.index(),s.length());
        if (!handler.is_valid())
        {
            JSONCONS_THROW_EXCEPTION(std::runtime_error,"Failed to parse json string");
        }
        return handler.get_result();
    }

    static basic_json_schema parse(const string_type& s, basic_parse_error_handler<char_type>& err_handler)
    {
        basic_json_deserializer<basic_json_schema<StringT, Alloc>> handler;
        basic_json_parser<char_type> parser(handler,err_handler);
        parser.begin_parse();
        parser.parse(s.data(),0,s.length());
        parser.end_parse();
        parser.check_done(s.data(),parser.index(),s.length());
        if (!handler.is_valid())
        {
            JSONCONS_THROW_EXCEPTION(std::runtime_error,"Failed to parse json string");
        }
        return handler.get_result();
    }

    static basic_json_schema parse_file(const std::string& s);

    static basic_json_schema parse_file(const std::string& s, basic_parse_error_handler<char_type>& err_handler);

    static basic_json_schema make_array()
    {
        return basic_json_schema::array();
    }

    static basic_json_schema make_array(size_t n, const array_allocator& allocator = array_allocator())
    {
        return basic_json_schema::array(n,allocator);
    }

    template <class T>
    static basic_json_schema make_array(size_t n, const T& val, const array_allocator& allocator = array_allocator())
    {
        return basic_json_schema::array(n, val,allocator);
    }

    template <size_t dim>
    static typename std::enable_if<dim==1,basic_json_schema>::type make_array(size_t n)
    {
        return array(n);
    }

    template <size_t dim, class T>
    static typename std::enable_if<dim==1,basic_json_schema>::type make_array(size_t n, const T& val, const Alloc& allocator = Alloc())
    {
        return array(n,val,allocator);
    }

    template <size_t dim, typename... Args>
    static typename std::enable_if<(dim>1),basic_json_schema>::type make_array(size_t n, Args... args)
    {
        const size_t dim1 = dim - 1;

        basic_json_schema val = make_array<dim1>(args...);
        val.resize(n);
        for (size_t i = 0; i < n; ++i)
        {
            val[i] = make_array<dim1>(args...);
        }
        return val;
    }

    variant var_;

    basic_json_schema() 
        : var_()
    {
    }

    basic_json_schema(const Alloc& allocator) 
        : var_(allocator)
    {
    }

    basic_json_schema(std::initializer_list<value_type> init,
               const Alloc& allocator = Alloc()) 
        : var_(std::move(init), allocator)
    {
    }

    basic_json_schema(const basic_json_schema<StringT, Alloc>& val)
        : var_(val.var_)
    {
    }

    basic_json_schema(const basic_json_schema<StringT, Alloc>& val, const Alloc& allocator)
        : var_(val.var_,allocator)
    {
    }

    basic_json_schema(basic_json_schema<StringT,Alloc>&& other)
        : var_(std::move(other.var_))
    {
    }

    basic_json_schema(basic_json_schema<StringT,Alloc>&& other, const Alloc& allocator)
        : var_(std::move(other.var_),allocator)
    {
    }

    basic_json_schema(const array& val)
        : var_(val)
    {
    }

    basic_json_schema(array&& other)
        : var_(std::move(other))
    {
    }

    basic_json_schema(const object& other)
        : var_(other)
    {
    }

    basic_json_schema(object&& other)
        : var_(std::move(other))
    {
    }

    template <typename T>
    basic_json_schema(T val)
        : var_(null_type())
    {
        json_type_traits<value_type,T>::assign(*this,val);
    }

    basic_json_schema(double val, uint8_t precision)
        : var_(val,precision)
    {
    }

    template <typename T>
    basic_json_schema(T val, const Alloc& allocator)
        : var_(allocator)
    {
        json_type_traits<value_type,T>::assign(*this,val);
    }

    basic_json_schema(const char_type *s, size_t length, const Alloc& allocator = Alloc())
        : var_(s, length, allocator)
    {
    }
    template<class InputIterator>
    basic_json_schema(InputIterator first, InputIterator last, const Alloc& allocator = Alloc())
        : var_(first,last,allocator)
    {
    }

    ~basic_json_schema()
    {
    }

    basic_json_schema& operator=(const basic_json_schema<StringT,Alloc>& rhs)
    {
        var_ = rhs.var_;
        return *this;
    }

    basic_json_schema& operator=(basic_json_schema<StringT,Alloc>&& rhs)
    {
        if (this != &rhs)
        {
            var_ = std::move(rhs.var_);
        }
        return *this;
    }

    template <class T>
    basic_json_schema<StringT, Alloc>& operator=(T val)
    {
        json_type_traits<value_type,T>::assign(*this,val);
        return *this;
    }

    bool operator!=(const basic_json_schema<StringT,Alloc>& rhs) const;

    bool operator==(const basic_json_schema<StringT,Alloc>& rhs) const;

    size_t size() const JSONCONS_NOEXCEPT
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            return 0;
        case value_types::object_t:
            return var_.value_.object_val_->size();
        case value_types::array_t:
            return var_.value_.array_val_->size();
        default:
            return 0;
        }
    }

    string_type to_string(const string_allocator& allocator=string_allocator()) const JSONCONS_NOEXCEPT
    {
        string_type s(allocator);
        std::basic_ostringstream<char_type,char_traits_type,string_allocator> os(s);
        {
            basic_json_serializer<char_type> serializer(os);
            to_stream(serializer);
        }
        return os.str();
    }

    string_type to_string(const basic_output_format<char_type>& format,
                          const string_allocator& allocator=string_allocator()) const
    {
        string_type s(allocator);
        std::basic_ostringstream<char_type> os(s);
        {
            basic_json_serializer<char_type> serializer(os, format);
            to_stream(serializer);
        }
        return os.str();
    }

    void to_stream(basic_json_output_handler<char_type>& handler) const
    {
        switch (var_.type_)
        {
        case value_types::string_t:
            handler.value(var_.value_.string_val_->data(),var_.value_.string_val_->length());
            break;
        case value_types::double_t:
            handler.value(var_.value_.double_val_, var_.length_or_precision_);
            break;
        case value_types::integer_t:
            handler.value(var_.value_.integer_val_);
            break;
        case value_types::uinteger_t:
            handler.value(var_.value_.uinteger_val_);
            break;
        case value_types::bool_t:
            handler.value(var_.value_.bool_val_);
            break;
        case value_types::null_t:
            handler.value(null_type());
            break;
        case value_types::empty_object_t:
            handler.begin_object();
            handler.end_object();
            break;
        case value_types::object_t:
            {
                handler.begin_object();
                object* o = var_.value_.object_val_;
                for (const_object_iterator it = o->begin(); it != o->end(); ++it)
                {
                    handler.name((it->name()).data(),it->name().length());
                    it->value().to_stream(handler);
                }
                handler.end_object();
            }
            break;
        case value_types::array_t:
            {
                handler.begin_array();
                array *o = var_.value_.array_val_;
                for (const_array_iterator it = o->begin(); it != o->end(); ++it)
                {
                    it->to_stream(handler);
                }
                handler.end_array();
            }
            break;
        default:
            break;
        }
    }

    void to_stream(std::basic_ostream<char_type>& os) const
    {
        basic_json_serializer<char_type> serializer(os);
        to_stream(serializer);
    }

    void to_stream(std::basic_ostream<char_type>& os, const basic_output_format<char_type>& format) const
    {
        basic_json_serializer<char_type> serializer(os, format);
        to_stream(serializer);
    }

    void to_stream(std::basic_ostream<char_type>& os, const basic_output_format<char_type>& format, bool indenting) const
    {
        basic_json_serializer<char_type> serializer(os, format, indenting);
        to_stream(serializer);
    }

    bool is_null() const JSONCONS_NOEXCEPT
    {
        return var_.is_null();
    }

    size_t count(const string_type& name) const
    {
        switch (var_.type_)
        {
        case value_types::object_t:
            {
                auto it = var_.value_.object_val_->find(name);
                if (it == members().end())
                {
                    return 0;
                }
                size_t count = 0;
                while (it != members().end() && it->name() == name)
                {
                    ++count;
                    ++it;
                }
                return count;
            }
            break;
        default:
            return 0;
        }
    }

    template<typename T>
    bool is() const
    {
        return json_type_traits<value_type,T>::is(*this);
    }

    bool is_string() const JSONCONS_NOEXCEPT
    {
        return var_.is_string();
    }


    bool is_bool() const JSONCONS_NOEXCEPT
    {
        return var_.is_bool();
    }

    bool is_object() const JSONCONS_NOEXCEPT
    {
        return var_.type_ == value_types::object_t || var_.type_ == value_types::empty_object_t;
    }

    bool is_array() const JSONCONS_NOEXCEPT
    {
        return var_.type_ == value_types::array_t;
    }

    bool is_integer() const JSONCONS_NOEXCEPT
    {
        return var_.type_ == value_types::integer_t || (var_.type_ == value_types::uinteger_t && (as_uinteger() <= static_cast<unsigned long long>(std::numeric_limits<long long>::max JSONCONS_NO_MACRO_EXP())));
    }

    bool is_uinteger() const JSONCONS_NOEXCEPT
    {
        return var_.type_ == value_types::uinteger_t || (var_.type_ == value_types::integer_t && as_integer() >= 0);
    }

    bool is_double() const JSONCONS_NOEXCEPT
    {
        return var_.type_ == value_types::double_t;
    }

    bool is_number() const JSONCONS_NOEXCEPT
    {
        return var_.is_number();
    }

    bool empty() const JSONCONS_NOEXCEPT
    {
        return var_.empty();
    }

    size_t capacity() const
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            return var_.value_.array_val_->capacity();
        case value_types::object_t:
            return var_.value_.object_val_->capacity();
        default:
            return 0;
        }
    }

    template<class U=Alloc,
         typename std::enable_if<std::is_default_constructible<U>::value
            >::type* = nullptr>
    void create_object_implicitly()
    {
        var_.type_ = value_types::object_t;
        var_.value_.object_val_ = create_impl<object>(Alloc(),object_allocator(Alloc()));
    }

    template<class U=Alloc,
         typename std::enable_if<!std::is_default_constructible<U>::value
            >::type* = nullptr>
    void create_object_implicitly() const
    {
        JSONCONS_THROW_EXCEPTION(std::runtime_error,"Cannot create_impl object implicitly - allocator is not default constructible.");
    }

    void reserve(size_t n)
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            var_.value_.array_val_->reserve(n);
            break;
        case value_types::empty_object_t:
        {
            create_object_implicitly();
            var_.value_.object_val_->reserve(n);
        }
        break;
        case value_types::object_t:
        {
            var_.value_.object_val_->reserve(n);
        }
            break;
        default:
            break;
        }
    }

    void resize(size_t n)
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            var_.value_.array_val_->resize(n);
            break;
        default:
            break;
        }
    }

    template <typename T>
    void resize(size_t n, T val)
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            var_.value_.array_val_->resize(n, val);
            break;
        default:
            break;
        }
    }

    template<typename T>
    T as() const
    {
        return json_type_traits<value_type,T>::as(*this);
    }

    template<typename T>
    typename std::enable_if<std::is_same<string_type,T>::value>::type as(const string_allocator& allocator) const
    {
        return json_type_traits<value_type,T>::as(*this,allocator);
    }

    bool as_bool() const JSONCONS_NOEXCEPT
    {
        switch (var_.type_)
        {
        case value_types::null_t:
        case value_types::empty_object_t:
            return false;
        case value_types::bool_t:
            return var_.value_.bool_val_;
        case value_types::double_t:
            return var_.value_.double_val_ != 0.0;
        case value_types::integer_t:
            return var_.value_.integer_val_ != 0;
        case value_types::uinteger_t:
            return var_.value_.uinteger_val_ != 0;
        case value_types::string_t:
            return var_.value_.string_val_->length() != 0;
        case value_types::array_t:
            return var_.value_.array_val_->size() != 0;
        case value_types::object_t:
            return var_.value_.object_val_->size() != 0;
        default:
            return false;
        }
    }

    int64_t as_integer() const
    {
        switch (var_.type_)
        {
        case value_types::double_t:
            return static_cast<int64_t>(var_.value_.double_val_);
        case value_types::integer_t:
            return static_cast<int64_t>(var_.value_.integer_val_);
        case value_types::uinteger_t:
            return static_cast<int64_t>(var_.value_.uinteger_val_);
        case value_types::bool_t:
            return var_.value_.bool_val_ ? 1 : 0;
        default:
            JSONCONS_THROW_EXCEPTION(std::runtime_error,"Not an integer");
        }
    }

    uint64_t as_uinteger() const
    {
        switch (var_.type_)
        {
        case value_types::double_t:
            return static_cast<uint64_t>(var_.value_.double_val_);
        case value_types::integer_t:
            return static_cast<uint64_t>(var_.value_.integer_val_);
        case value_types::uinteger_t:
            return static_cast<uint64_t>(var_.value_.uinteger_val_);
        case value_types::bool_t:
            return var_.value_.bool_val_ ? 1 : 0;
        default:
            JSONCONS_THROW_EXCEPTION(std::runtime_error,"Not an unsigned integer");
        }
    }

    double as_double() const
    {
        switch (var_.type_)
        {
        case value_types::double_t:
            return var_.value_.double_val_;
        case value_types::integer_t:
            return static_cast<double>(var_.value_.integer_val_);
        case value_types::uinteger_t:
            return static_cast<double>(var_.value_.uinteger_val_);
        case value_types::null_t:
            return std::numeric_limits<double>::quiet_NaN();
        default:
            JSONCONS_THROW_EXCEPTION(std::runtime_error,"Not a double");
        }
    }

    string_type as_string() const JSONCONS_NOEXCEPT
    {
        switch (var_.type_)
        {
        case value_types::string_t:
            return string_type(var_.value_.string_val_->data(),var_.value_.string_val_->length(),var_.value_.string_val_->get_allocator());
        default:
            return to_string();
        }
    }

    string_type as_string(const string_allocator& allocator) const JSONCONS_NOEXCEPT
    {
        switch (var_.type_)
        {
        case value_types::string_t:
            return string_type(var_.value_.string_val_->data(),var_.value_.string_val_->length(),allocator);
        default:
            return to_string(allocator);
        }
    }

    string_type as_string(const basic_output_format<char_type>& format) const 
    {
        switch (var_.type_)
        {
        case value_types::string_t:
            return string_type(var_.value_.string_val_->data(),var_.value_.string_val_->length(),var_.value_.string_val_->get_allocator());
        default:
            return to_string(format);
        }
    }

    string_type as_string(const basic_output_format<char_type>& format,
                          const string_allocator& allocator) const 
    {
        switch (var_.type_)
        {
        case value_types::string_t:
            return string_type(var_.value_.string_val_->data(),var_.value_.string_val_->length(),allocator);
        default:
            return to_string(format,allocator);
        }
    }

    const char_type* as_cstring() const
    {
        switch (var_.type_)
        {
        case value_types::string_t:
            return var_.value_.string_val_->c_str();
        default:
            JSONCONS_THROW_EXCEPTION(std::runtime_error,"Not a cstring");
        }
    }

    basic_json_schema<StringT, Alloc>& at(const string_type& name)
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            JSONCONS_THROW_EXCEPTION_1(std::out_of_range,"%s not found", name);
        case value_types::object_t:
            {
                auto it = var_.value_.object_val_->find(name);
                if (it == members().end())
                {
                    JSONCONS_THROW_EXCEPTION_1(std::out_of_range, "%s not found", name);
                }
                return it->value();
            }
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Attempting to get %s from a value that is not an object", name);
            }
        }
    }

    basic_json_schema<StringT, Alloc>& evaluate() 
    {
        return *this;
    }

    basic_json_schema<StringT, Alloc>& evaluate_with_default() 
    {
        return *this;
    }

    const basic_json_schema<StringT, Alloc>& evaluate() const
    {
        return *this;
    }

    basic_json_schema<StringT, Alloc>& evaluate(size_t i) 
    {
        return at(i);
    }

    const basic_json_schema<StringT, Alloc>& evaluate(size_t i) const
    {
        return at(i);
    }

    basic_json_schema<StringT, Alloc>& evaluate(const string_type& name) 
    {
        return at(name);
    }

    const basic_json_schema<StringT, Alloc>& evaluate(const string_type& name) const
    {
        return at(name);
    }

    const basic_json_schema<StringT, Alloc>& at(const string_type& name) const
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            JSONCONS_THROW_EXCEPTION_1(std::out_of_range,"%s not found", name);
        case value_types::object_t:
            {
                auto it = var_.value_.object_val_->find(name);
                if (it == members().end())
                {
                    JSONCONS_THROW_EXCEPTION_1(std::out_of_range, "%s not found", name);
                }
                return it->value();
            }
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Attempting to get %s from a value that is not an object", name);
            }
        }
    }

    basic_json_schema<StringT, Alloc>& at(size_t i)
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            if (i >= var_.value_.array_val_->size())
            {
                JSONCONS_THROW_EXCEPTION(std::out_of_range,"Invalid array subscript");
            }
            return var_.value_.array_val_->operator[](i);
        default:
            JSONCONS_THROW_EXCEPTION(std::runtime_error,"Index on non-array value not supported");
        }
    }

    const basic_json_schema<StringT, Alloc>& at(size_t i) const
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            if (i >= var_.value_.array_val_->size())
            {
                JSONCONS_THROW_EXCEPTION(std::out_of_range,"Invalid array subscript");
            }
            return var_.value_.array_val_->operator[](i);
        default:
            JSONCONS_THROW_EXCEPTION(std::runtime_error,"Index on non-array value not supported");
        }
    }

    object_iterator find(const string_type& name)
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            return members().end();
        case value_types::object_t:
            return var_.value_.object_val_->find(name);
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Attempting to get %s from a value that is not an object", name);
            }
        }
    }

    const_object_iterator find(const string_type& name) const
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            return members().end();
        case value_types::object_t:
            return var_.value_.object_val_->find(name);
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Attempting to get %s from a value that is not an object", name);
            }
        }
    }

    object_iterator find(const char_type* name)
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            return members().end();
        case value_types::object_t:
            return var_.value_.object_val_->find(name);
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Attempting to get %s from a value that is not an object", name);
            }
        }
    }

    const_object_iterator find(const char_type* name) const
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            return members().end();
        case value_types::object_t:
            return var_.value_.object_val_->find(name);
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Attempting to get %s from a value that is not an object", name);
            }
        }
    }

    template<typename T>
    basic_json_schema<StringT, Alloc> get(const string_type& name, T&& default_val) const
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            {
                return basic_json_schema<StringT,Alloc>(std::forward<T>(default_val));
            }
        case value_types::object_t:
            {
                const_object_iterator it = var_.value_.object_val_->find(name);
                if (it != members().end())
                {
                    return it->value();
                }
                else
                {
                    return basic_json_schema<StringT,Alloc>(std::forward<T>(default_val));
                }
            }
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Attempting to get %s from a value that is not an object", name);
            }
        }
    }

    // Modifiers

    void shrink_to_fit()
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            var_.value_.array_val_->shrink_to_fit();
            break;
        case value_types::object_t:
            var_.value_.object_val_->shrink_to_fit();
            break;
        default:
            break;
        }
    }

    void clear()
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            var_.value_.array_val_->clear();
            break;
        case value_types::object_t:
            var_.value_.object_val_->clear();
            break;
        default:
            break;
        }
    }

    void erase(object_iterator first, object_iterator last)
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            break;
        case value_types::object_t:
            var_.value_.object_val_->erase(first, last);
            break;
        default:
            JSONCONS_THROW_EXCEPTION(std::runtime_error,"Not an object");
            break;
        }
    }

    void erase(array_iterator first, array_iterator last)
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            var_.value_.array_val_->erase(first, last);
            break;
        default:
            JSONCONS_THROW_EXCEPTION(std::runtime_error,"Not an array");
            break;
        }
    }

    // Removes all elements from an array value whose index is between from_index, inclusive, and to_index, exclusive.

    void erase(const string_type& name)
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            break;
        case value_types::object_t:
            var_.value_.object_val_->erase(name);
            break;
        default:
            JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Attempting to set %s on a value that is not an object", name);
            break;
        }
    }

    void set(const string_type& name, const basic_json_schema<StringT, Alloc>& value)
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            create_object_implicitly();
        case value_types::object_t:
            var_.value_.object_val_->set(name, value);
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Attempting to set %s on a value that is not an object", name);
            }
        }
    }

    void set(string_type&& name, const basic_json_schema<StringT, Alloc>& value){
        switch (var_.type_){
        case value_types::empty_object_t:
            create_object_implicitly();
        case value_types::object_t:
            var_.value_.object_val_->set(std::move(name),value);
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Attempting to set %s on a value that is not an object",name);
            }
        }
    }

    void set(const string_type& name, basic_json_schema<StringT, Alloc>&& value){
        switch (var_.type_){
        case value_types::empty_object_t:
            create_object_implicitly();
        case value_types::object_t:
            var_.value_.object_val_->set(name,std::move(value));
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Attempting to set %s on a value that is not an object",name);
            }
        }
    }

    void set(string_type&& name, basic_json_schema<StringT, Alloc>&& value)
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            create_object_implicitly();
        case value_types::object_t:
            var_.value_.object_val_->set(std::move(name),std::move(value));
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Attempting to set %s on a value that is not an object",name);
            }
        }
    }

    object_iterator set(object_iterator hint, const string_type& name, const basic_json_schema<StringT, Alloc>& value)
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            create_object_implicitly();
        case value_types::object_t:
            return var_.value_.object_val_->set(hint, name, value);
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Attempting to set %s on a value that is not an object", name);
            }
        }
    }

    object_iterator set(object_iterator hint, string_type&& name, const basic_json_schema<StringT, Alloc>& value){
        switch (var_.type_){
        case value_types::empty_object_t:
            create_object_implicitly();
        case value_types::object_t:
            return var_.value_.object_val_->set(hint, std::move(name),value);
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Attempting to set %s on a value that is not an object",name);
            }
        }
    }

    object_iterator set(object_iterator hint, const string_type& name, basic_json_schema<StringT, Alloc>&& value){
        switch (var_.type_){
        case value_types::empty_object_t:
            create_object_implicitly();
        case value_types::object_t:
            return var_.value_.object_val_->set(hint, name,std::move(value));
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Attempting to set %s on a value that is not an object",name);
            }
        }
    } 

    object_iterator set(object_iterator hint, string_type&& name, basic_json_schema<StringT, Alloc>&& value){
        switch (var_.type_){
        case value_types::empty_object_t:
            create_object_implicitly();
        case value_types::object_t:
            return var_.value_.object_val_->set(hint, std::move(name),std::move(value));
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Attempting to set %s on a value that is not an object",name);
            }
        }
    }

    void add(const basic_json_schema<StringT, Alloc>& value)
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            var_.value_.array_val_->push_back(value);
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION(std::runtime_error,"Attempting to insert into a value that is not an array");
            }
        }
    }

    void add(basic_json_schema<StringT, Alloc>&& value){
        switch (var_.type_){
        case value_types::array_t:
            var_.value_.array_val_->push_back(std::move(value));
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION(std::runtime_error,"Attempting to insert into a value that is not an array");
            }
        }
    }

    array_iterator add(const_array_iterator pos, const basic_json_schema<StringT, Alloc>& value)
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            return var_.value_.array_val_->add(pos, value);
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION(std::runtime_error,"Attempting to insert into a value that is not an array");
            }
        }
    }

    array_iterator add(const_array_iterator pos, basic_json_schema<StringT, Alloc>&& value){
        switch (var_.type_){
        case value_types::array_t:
            return var_.value_.array_val_->add(pos, std::move(value));
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION(std::runtime_error,"Attempting to insert into a value that is not an array");
            }
        }
    }

    value_types type() const
    {
        return var_.type_;
    }

    uint8_t length_or_precision() const
    {
        return var_.length_or_precision_;
    }

    void swap(basic_json_schema<StringT,Alloc>& b)
    {
        var_.swap(b.var_);
    }

    template <class T>
    std::vector<T> as_vector() const
    {
        std::vector<T> v(size());
        for (size_t i = 0; i < v.size(); ++i)
        {
            v[i] = json_type_traits<value_type,T>::as(at(i));
        }
        return v;
    }

    friend void swap(basic_json_schema<StringT,Alloc>& a, basic_json_schema<StringT,Alloc>& b)
    {
        a.swap(b);
    }

    void assign_string(const string_type& rhs)
    {
        var_.assign(rhs);
    }

    void assign_string(const char_type* rhs, size_t length)
    {
        var_.assign_string(rhs,length);
    }

    void assign_bool(bool rhs)
    {
        var_.assign(rhs);
    }

    void assign_object(const object & rhs)
    {
        var_.assign(rhs);
    }

    void assign_array(const array& rhs)
    {
        var_.assign(rhs);
    }

    void assign_null()
    {
        var_.assign(null_type());
    }

    void assign_integer(int64_t rhs)
    {
        var_.assign(rhs);
    }

    void assign_uinteger(uint64_t rhs)
    {
        var_.assign(rhs);
    }

    void assign_double(double rhs, uint8_t precision = 0)
    {
        var_.assign(rhs,precision);
    }

    static basic_json_schema make_2d_array(size_t m, size_t n);

    template <typename T>
    static basic_json_schema make_2d_array(size_t m, size_t n, T val);

    static basic_json_schema make_3d_array(size_t m, size_t n, size_t k);

    template <typename T>
    static basic_json_schema make_3d_array(size_t m, size_t n, size_t k, T val);

    object_range members()
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            return object_range(object_iterator(true),object_iterator(true));
        case value_types::object_t:
            return object_range(object_value().begin(),object_value().end());
        default:
            JSONCONS_THROW_EXCEPTION(std::runtime_error,"Not an object");
        }
    }

    const_object_range members() const
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            return const_object_range(const_object_iterator(true),const_object_iterator(true));
        case value_types::object_t:
            return const_object_range(object_value().begin(),object_value().end());
        default:
            JSONCONS_THROW_EXCEPTION(std::runtime_error,"Not an object");
        }
    }

    array_range elements()
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            return array_range(array_value().begin(),array_value().end());
        default:
            JSONCONS_THROW_EXCEPTION(std::runtime_error,"Not an array");
        }
    }

    const_array_range elements() const
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            return const_array_range(array_value().begin(),array_value().end());
        default:
            JSONCONS_THROW_EXCEPTION(std::runtime_error,"Not an array");
        }
    }

    array& array_value() 
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            return *(var_.value_.array_val_);
        default:
            JSONCONS_THROW_EXCEPTION(std::runtime_error,"Bad array cast");
            break;
        }
    }

    const array& array_value() const
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            return *(var_.value_.array_val_);
        default:
            JSONCONS_THROW_EXCEPTION(std::runtime_error,"Bad array cast");
            break;
        }
    }

    object& object_value()
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            create_object_implicitly();
        case value_types::object_t:
            return *(var_.value_.object_val_);
        default:
            JSONCONS_THROW_EXCEPTION(std::runtime_error,"Bad object cast");
            break;
        }
    }

    const object& object_value() const
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            const_cast<value_type*>(this)->create_object_implicitly(); // HERE
        case value_types::object_t:
            return *(var_.value_.object_val_);
        default:
            JSONCONS_THROW_EXCEPTION(std::runtime_error,"Bad object cast");
            break;
        }
    }

    bool validate(const basic_json<StringT,Alloc>& val) const
    {
        return var_.validate(val);
    }

private:

    friend std::basic_ostream<typename StringT::value_type>& operator<<(std::basic_ostream<typename StringT::value_type>& os, const basic_json_schema<StringT, Alloc>& o)
    {
        o.to_stream(os);
        return os;
    }

    friend std::basic_istream<typename StringT::value_type>& operator<<(std::basic_istream<typename StringT::value_type>& is, basic_json_schema<StringT, Alloc>& o)
    {
        basic_json_deserializer<basic_json_schema<StringT, Alloc>> handler;
        basic_json_reader<typename StringT::value_type> reader(is, handler);
        reader.read_next();
        reader.check_done();
        if (!handler.is_valid())
        {
            JSONCONS_THROW_EXCEPTION(std::runtime_error,"Failed to parse json stream");
        }
        o = handler.get_result();
        return is;
    }
};

template <class JsonT>
void swap(typename JsonT::member_type& a, typename JsonT::member_type& b)
{
    a.swap(b);
}

template<typename StringT, typename Alloc>
bool basic_json_schema<StringT, Alloc>::operator!=(const basic_json_schema<StringT, Alloc>& rhs) const
{
    return !(*this == rhs);
}

template<typename StringT, typename Alloc>
bool basic_json_schema<StringT, Alloc>::operator==(const basic_json_schema<StringT, Alloc>& rhs) const
{
    return var_ == rhs.var_;
}

template<typename StringT, typename Alloc>
basic_json_schema<StringT, Alloc> basic_json_schema<StringT, Alloc>::make_2d_array(size_t m, size_t n)
{
    basic_json_schema<StringT, Alloc> a = basic_json_schema<StringT, Alloc>::array();
    a.resize(m);
    for (size_t i = 0; i < a.size(); ++i)
    {
        a[i] = basic_json_schema<StringT, Alloc>::make_array(n);
    }
    return a;
}

template<typename StringT, typename Alloc>
template<typename T>
basic_json_schema<StringT, Alloc> basic_json_schema<StringT, Alloc>::make_2d_array(size_t m, size_t n, T val)
{
    basic_json_schema<StringT, Alloc> v;
    v = val;
    basic_json_schema<StringT, Alloc> a = make_array(m);
    for (size_t i = 0; i < a.size(); ++i)
    {
        a[i] = basic_json_schema<StringT, Alloc>::make_array(n, v);
    }
    return a;
}

template<typename StringT, typename Alloc>
basic_json_schema<StringT, Alloc> basic_json_schema<StringT, Alloc>::make_3d_array(size_t m, size_t n, size_t k)
{
    basic_json_schema<StringT, Alloc> a = basic_json_schema<StringT, Alloc>::array();
    a.resize(m);
    for (size_t i = 0; i < a.size(); ++i)
    {
        a[i] = basic_json_schema<StringT, Alloc>::make_2d_array(n, k);
    }
    return a;
}

template<typename StringT, typename Alloc>
template<typename T>
basic_json_schema<StringT, Alloc> basic_json_schema<StringT, Alloc>::make_3d_array(size_t m, size_t n, size_t k, T val)
{
    basic_json_schema<StringT, Alloc> v;
    v = val;
    basic_json_schema<StringT, Alloc> a = make_array(m);
    for (size_t i = 0; i < a.size(); ++i)
    {
        a[i] = basic_json_schema<StringT, Alloc>::make_2d_array(n, k, v);
    }
    return a;
}

template<typename StringT, typename Alloc>
basic_json_schema<StringT, Alloc> basic_json_schema<StringT, Alloc>::parse_stream(std::basic_istream<char_type>& is)
{
    basic_json_deserializer<basic_json_schema<StringT, Alloc>> handler;
    basic_json_reader<char_type> reader(is, handler);
    reader.read_next();
    reader.check_done();
    if (!handler.is_valid())
    {
        JSONCONS_THROW_EXCEPTION(std::runtime_error,"Failed to parse json stream");
    }
    return handler.get_result();
}

template<typename StringT, typename Alloc>
basic_json_schema<StringT, Alloc> basic_json_schema<StringT, Alloc>::parse_stream(std::basic_istream<char_type>& is, 
                                                              basic_parse_error_handler<char_type>& err_handler)
{
    basic_json_deserializer<basic_json_schema<StringT, Alloc>> handler;
    basic_json_reader<char_type> reader(is, handler, err_handler);
    reader.read_next();
    reader.check_done();
    if (!handler.is_valid())
    {
        JSONCONS_THROW_EXCEPTION(std::runtime_error,"Failed to parse json stream");
    }
    return handler.get_result();
}

template<typename StringT, typename Alloc>
basic_json_schema<StringT, Alloc> basic_json_schema<StringT, Alloc>::parse_file(const std::string& filename)
{
    FILE* fp;

#if defined(JSONCONS_HAS_FOPEN_S)
    errno_t err = fopen_s(&fp, filename.c_str(), "rb");
    if (err != 0) 
    {
        JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Cannot open file %s", filename);
    }
#else
    fp = std::fopen(filename.c_str(), "rb");
    if (fp == nullptr)
    {
        JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Cannot open file %s", filename);
    }
#endif
    basic_json_deserializer<basic_json_schema<StringT, Alloc>> handler;
    try
    {
        // obtain file size:
        std::fseek (fp , 0 , SEEK_END);
        long size = std::ftell (fp);
        std::rewind(fp);

        if (size > 0)
        {
            std::vector<char_type> buffer(size);

            // copy the file into the buffer:
            size_t result = std::fread (buffer.data(),1,size,fp);
            if (result != static_cast<unsigned long long>(size))
            {
                JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Error reading file %s", filename);
            }

            basic_json_parser<char_type> parser(handler);
            parser.begin_parse();
            parser.parse(buffer.data(),0,buffer.size());
            parser.end_parse();
            parser.check_done(buffer.data(),parser.index(),buffer.size());
        }

        std::fclose (fp);
    }
    catch (...)
    {
        std::fclose (fp);
        throw;
    }
    if (!handler.is_valid())
    {
        JSONCONS_THROW_EXCEPTION(std::runtime_error,"Failed to parse json file");
    }
    return handler.get_result();
}

template<typename StringT, typename Alloc>
basic_json_schema<StringT, Alloc> basic_json_schema<StringT, Alloc>::parse_file(const std::string& filename, 
                                                            basic_parse_error_handler<char_type>& err_handler)
{
    FILE* fp;

#if !defined(JSONCONS_HAS_FOPEN_S)
    fp = std::fopen(filename.c_str(), "rb");
    if (fp == nullptr)
    {
        JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Cannot open file %s", filename);
    }
#else
    errno_t err = fopen_s(&fp, filename.c_str(), "rb");
    if (err != 0) 
    {
        JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Cannot open file %s", filename);
    }
#endif

    basic_json_deserializer<basic_json_schema<StringT, Alloc>> handler;
    try
    {
        // obtain file size:
        std::fseek (fp , 0 , SEEK_END);
        long size = std::ftell (fp);
        std::rewind(fp);

        if (size > 0)
        {
            std::vector<char_type> buffer(size);

            // copy the file into the buffer:
            size_t result = std::fread (buffer.data(),1,size,fp);
            if (result != static_cast<unsigned long long>(size))
            {
                JSONCONS_THROW_EXCEPTION_1(std::runtime_error,"Error reading file %s", filename);
            }

            basic_json_parser<char_type> parser(handler,err_handler);
            parser.begin_parse();
            parser.parse(buffer.data(),0,buffer.size());
            parser.end_parse();
            parser.check_done(buffer.data(),parser.index(),buffer.size());
        }

        std::fclose (fp);
    }
    catch (...)
    {
        std::fclose (fp);
        throw;
    }
    if (!handler.is_valid())
    {
        JSONCONS_THROW_EXCEPTION(std::runtime_error,"Failed to parse json file");
    }
    return handler.get_result();
}

template <typename JsonT>
std::basic_istream<typename JsonT::char_type>& operator>>(std::basic_istream<typename JsonT::char_type>& is, JsonT& o)
{
    basic_json_deserializer<JsonT> handler;
    basic_json_reader<typename JsonT::char_type> reader(is, handler);
    reader.read_next();
    reader.check_done();
    if (!handler.is_valid())
    {
        JSONCONS_THROW_EXCEPTION(std::runtime_error,"Failed to parse json stream");
    }
    o = handler.get_result();
    return is;
}

template<typename JsonT>
class json_printable
{
public:
    typedef typename JsonT::char_type char_type;

    json_printable(const JsonT& o,
                   bool is_pretty_print)
       : o_(&o), is_pretty_print_(is_pretty_print)
    {
    }

    json_printable(const JsonT& o,
                   bool is_pretty_print,
                   const basic_output_format<char_type>& format)
       : o_(&o), is_pretty_print_(is_pretty_print), format_(format)
    {
        ;
    }

    void to_stream(std::basic_ostream<char_type>& os) const
    {
        o_->to_stream(os, format_, is_pretty_print_);
    }

    friend std::basic_ostream<char_type>& operator<<(std::basic_ostream<char_type>& os, const json_printable<JsonT>& o)
    {
        o.to_stream(os);
        return os;
    }

    const JsonT *o_;
    bool is_pretty_print_;
    basic_output_format<char_type> format_;
private:
    json_printable();
};

template<typename JsonT>
json_printable<JsonT> print(const JsonT& val)
{
    return json_printable<JsonT>(val,false);
}

template<class JsonT>
json_printable<JsonT> print(const JsonT& val,
                            const basic_output_format<typename JsonT::char_type>& format)
{
    return json_printable<JsonT>(val, false, format);
}

template<class JsonT>
json_printable<JsonT> pretty_print(const JsonT& val)
{
    return json_printable<JsonT>(val,true);
}

template<typename JsonT>
json_printable<JsonT> pretty_print(const JsonT& val,
                                   const basic_output_format<typename JsonT::char_type>& format)
{
    return json_printable<JsonT>(val, true, format);
}

typedef basic_json_schema<std::string,std::allocator<char>> json_schema;
typedef basic_json_schema<std::wstring,std::allocator<wchar_t>> wjson_schema;

typedef basic_jcr_deserializer<json_schema> jcr_deserializer;
typedef basic_jcr_deserializer<wjson_schema> wjcr_deserializer;

}}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif
