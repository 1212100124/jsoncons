### jsoncons::json_content_handler

```c++
typedef basic_json_content_handler<char> json_content_handler
```

Defines an interface for receiving JSON events. The `json_content_handler` class is an instantiation of the `basic_json_content_handler` class template that uses `char` as the character type. 

#### Header
```c++
#include <jsoncons/json_content_handler.hpp>
```
#### Member types

Member type                         |Definition
------------------------------------|------------------------------
`string_view_type`|A non-owning view of a string, holds a pointer to character data and length. Supports conversion to and from strings. Will be typedefed to the C++ 17 [string view](http://en.cppreference.com/w/cpp/string/basic_string_view) if `JSONCONS_HAS_STRING_VIEW` is defined in `jsoncons_config.hpp`, otherwise proxied.  

#### Public producer interface

    bool begin_object()
    bool begin_object(const serializing_context& context)
Indicates the begining of an object. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter.
Returns `true` if the producer should continue streaming events, `false` otherwise.

    bool end_object()
    bool end_object(const serializing_context& context)
Indicates the end of an object. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    bool begin_array()
    bool begin_array(const serializing_context& context)
Indicates the beginning of an array. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    bool begin_array(size_t length)
    bool begin_array(size_t length, const serializing_context& context)
Indicates the beginning of an array of known length. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    bool end_array()
    bool end_array(const serializing_context& context)
Indicates the end of an array. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    bool name(const string_view_type& name)
    bool name(const string_view_type& name, const serializing_context& context)
Writes the name part of a name-value pair inside an object. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter.  
Returns `true` if the producer should continue streaming events, `false` otherwise.

    bool string_value(const string_view_type& value, 
                      semantic_tag_type tag = semantic_tag_type::none, 
                      const serializing_context& context=null_serializing_context());
Writes a string value. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    bool byte_string_value(const uint8_t* data, size_t length, 
                           semantic_tag_type tag=semantic_tag_type::none, 
                           const serializing_context& context=null_serializing_context()); 
Writes a byte string value. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    bool byte_string_value(const byte_string_view& v, 
                           semantic_tag_type tag=semantic_tag_type::none, 
                           const serializing_context& context=null_serializing_context());
Writes a byte string value. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    bool bignum_value(const string_view_type& s) 
    bool bignum_value(const string_view_type& s, const serializing_context& context) 
Writes a bignum using the decimal string representation of a bignum. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    bool decimal_value(const string_view_type& s) 
    bool decimal_value(const string_view_type& s, const serializing_context& context) 
Writes a decimal value using the decimal string representation. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    bool date_time_value(const string_view_type& s) 
    bool date_time_value(const string_view_type& s, const serializing_context& context) 
Writes a date-time value using the string representation. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    bool epoch_time_value(int64_t val) 
    bool epoch_time_value(int64_t val, const serializing_context& context) 
Writes an epoch time value using the integer representation. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    bool int64_value(int64_t value, 
                     semantic_tag_type tag = semantic_tag_type::none, 
                     const serializing_context& context=null_serializing_context());
Writes a signed integer value. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    bool uint64_value(uint64_t value, 
                      semantic_tag_type tag = semantic_tag_type::none, 
                      const serializing_context& context=null_serializing_context())
Writes a non-negative integer value. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    bool double_value(double value, 
                      const floating_point_options& fmt = floating_point_options(), 
                      semantic_tag_type tag = semantic_tag_type::none, 
                      const serializing_context& context=null_serializing_context())
Writes a floating point value with default precision (`std::numeric_limits<double>::digits10`.) Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    bool double_value(double value, uint8_t precision) 
    bool double_value(double value, uint8_t precision, const serializing_context& context)
Writes a floating point value with specified precision. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    bool bool_value(bool value, 
                    const serializing_context& context=null_serializing_context());
Writes a boolean value. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    bool null_value(const serializing_context& context=null_serializing_context());
Writes a null value. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    void flush()
Flushes whatever is buffered to the destination.

#### Private virtual consumer interface

    virtual bool do_begin_object(semantic_tag_type tag, const serializing_context& context) = 0;
Handles the beginning of an object. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    virtual bool do_end_object(const serializing_context& context) = 0;
Handles the end of an object. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    virtual bool do_begin_array(semantic_tag_type tag, const serializing_context& context) = 0;
Handles the beginning of an array. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    virtual bool do_begin_array(size_t length, const serializing_context& context);
Handles the beginning of an array of known length. Defaults to calling `do_begin_array(semantic_tag_type, const serializing_context& context)`. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    virtual bool do_end_array(const serializing_context& context) = 0;
Handles the end of an array. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    virtual bool do_name(const string_view_type& name, 
                         const serializing_context& context) = 0;
Handles the name part of a name-value pair inside an object. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter.  
Returns `true` if the producer should continue streaming events, `false` otherwise.

    virtual bool do_string_value(const string_view_type& val, 
                                 semantic_tag_type tag,
                                 const serializing_context& context) = 0;
Handles a string value. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    virtual bool do_byte_string_value(const byte_string_view& b, semantic_tag_type tag,
                                      const serializing_context& context) = 0;
Handles a byte string value. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    virtual bool do_int64_value(int64_t value, 
                                semantic_tag_type tag, 
                                const serializing_context& context) = 0;
Handles a signed integer value. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    virtual bool do_uint64_value(uint64_t value, 
                                 semantic_tag_type tag, 
                             const serializing_context& context) = 0;
Handles a non-negative integer value. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    virtual bool do_double_value(double value, 
                                 const floating_point_options& fmt, 
                                 semantic_tag_type tag, 
                                 const serializing_context& context) = 0;
Handles a floating point value. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    virtual bool do_bool_value(bool value, semantic_tag_type tag, const serializing_context& context) = 0;
Handles a boolean value. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    virtual bool do_null_value(semantic_tag_type tag, const serializing_context& context) = 0;
Handles a null value. Contextual information including
line and column number is provided in the [context](serializing_context.md) parameter. 
Returns `true` if the producer should continue streaming events, `false` otherwise.

    virtual void do_flush() = 0;
Allows producers of json events to flush whatever they've buffered.

#### See also

- [semantic_tag_type](../semantic_tag_type.md)

