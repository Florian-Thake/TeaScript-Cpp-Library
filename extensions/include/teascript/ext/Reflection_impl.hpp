/* === Part of TeaScript C++ Library Extension ===
 * SPDX-FileCopyrightText:  Copyright (C) 2026 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

// we use Warning Level 4, but reflectcpp has warings there, so disable them...
#if defined(_MSC_VER )
# pragma warning( push )
# pragma warning( disable: 4324 )
#endif

#include "rfl.hpp"

#if defined(_MSC_VER )
# pragma warning( pop )
#endif

#include "teascript/ValueObject.hpp"

namespace teascript {
namespace reflection {


class Writer
{
    ValueObject &root;
public:

    using OutputArrayType = ValueObject;
    using OutputObjectType = ValueObject;
    using OutputVarType = ValueObject;

    Writer( ValueObject &v ) : root( v ) {}
    ~Writer() = default;

    OutputVarType null_as_root() const
    {
        root = ValueObject( NotAValue{}, ValueConfig( true ) );
        return root;
    }

    OutputArrayType array_as_root( const size_t ) const
    {
        Tuple  tuple;
        // special case for mark an empty toml/json array. //FIXME: need one place in code for this special handling!
        tuple.AppendValue( ValueObject( Buffer(), ValueConfig( false ) ) ); // TODO: check shared!
        root = ValueObject( std::move( tuple ), ValueConfig( true ) );
        return root;
    }

    OutputObjectType object_as_root( const size_t ) const
    {
        Tuple  tuple;
        root = ValueObject( std::move( tuple ), ValueConfig( true ) );
        return root;
    }

    template<class T>
    OutputVarType value_as_root( const T &var ) const
    {
        return from_basic_type( var );
    }

    OutputArrayType add_array_to_array( const size_t, OutputArrayType *parent ) const
    {
        Tuple  arr;
        // special case for mark an empty toml/json array. //FIXME: need one place in code for this special handling!
        arr.AppendValue( ValueObject( Buffer(), ValueConfig( false ) ) ); // TODO: check shared!
        auto arr_val = ValueObject( std::move( arr ), ValueConfig( true ) );

        Tuple &parent_arr = parent->GetMutableValue<Tuple>();
        tuple::TomlJsonUtil::ArrayAppend( parent_arr, arr_val );
        return arr_val;
    }

    OutputArrayType add_array_to_object( const std::string_view &name, const size_t, OutputObjectType *parent ) const
    {
        Tuple  arr;
        // special case for mark an empty toml/json array. //FIXME: need one place in code for this special handling!
        arr.AppendValue( ValueObject( Buffer(), ValueConfig( false ) ) ); // TODO: check shared!
        auto arr_val = ValueObject( std::move( arr ), ValueConfig( true ) );

        Tuple &parent_obj = parent->GetMutableValue<Tuple>();
        parent_obj.AppendKeyValue( std::string( name.data(), name.size() ), arr_val );

        return arr_val;
    }

    OutputObjectType add_object_to_array( const size_t, OutputArrayType *parent ) const
    {
        Tuple obj;
        auto obj_val = ValueObject( std::move( obj ), ValueConfig( true ) );

        Tuple &parent_arr = parent->GetMutableValue<Tuple>();
        tuple::TomlJsonUtil::ArrayAppend( parent_arr, obj_val );

        return obj_val;
    }

    OutputObjectType add_object_to_object( const std::string_view &name, const size_t, OutputObjectType *parent ) const
    {
        Tuple obj;
        auto obj_val = ValueObject( std::move( obj ), ValueConfig( true ) );

        Tuple &parent_obj = parent->GetMutableValue<Tuple>();
        parent_obj.AppendKeyValue( std::string( name.data(), name.size() ), obj_val );

        return obj_val;
    }
    template<class T>
    OutputVarType add_value_to_array( const T &var, OutputArrayType *parent ) const
    {
        auto obj_val = from_basic_type( var );
        Tuple &parent_arr = parent->GetMutableValue<Tuple>();
        tuple::TomlJsonUtil::ArrayAppend( parent_arr, obj_val );
        return obj_val;
    }

    template<class T>
    OutputVarType add_value_to_object( const std::string_view &name, const T &var, OutputObjectType *parent ) const
    {
        auto obj_val = from_basic_type( var );
        Tuple &parent_obj = parent->GetMutableValue<Tuple>();
        parent_obj.AppendKeyValue( std::string( name.data(), name.size() ), obj_val );
        return obj_val;
    }

    OutputVarType add_null_to_array( OutputArrayType *parent ) const
    {
        auto obj_val = ValueObject( NotAValue{}, ValueConfig( true ) );
        Tuple &parent_arr = parent->GetMutableValue<Tuple>();
        tuple::TomlJsonUtil::ArrayAppend( parent_arr, obj_val );
        return obj_val;
    }

    OutputVarType add_null_to_object( const std::string_view &name, OutputObjectType *parent ) const
    {
        auto obj_val = ValueObject( NotAValue{}, ValueConfig( true ) );
        Tuple &parent_obj = parent->GetMutableValue<Tuple>();
        parent_obj.AppendKeyValue( std::string( name.data(), name.size() ), obj_val );
        return obj_val;
    }

    void end_array( OutputArrayType * ) const
    {
    }

    void end_object( OutputObjectType * ) const
    {
    }


    template <class T>
    OutputVarType from_basic_type( const T &var ) const noexcept
    {
        if constexpr( std::is_same<std::remove_cvref_t<T>, std::string>() ) {
            return ValueObject( var, ValueConfig( true ) );
        } else if constexpr( std::is_same<std::remove_cvref_t<T>, bool>() ) {
            return ValueObject( var, ValueConfig( true ) );
        } else if constexpr( std::is_floating_point<std::remove_cvref_t<T>>() ) {
            return ValueObject( static_cast<F64>(var), ValueConfig( true ) );
        } else if constexpr( std::is_unsigned<std::remove_cvref_t<T>>() ) {
            return ValueObject( static_cast<U64>(var), ValueConfig( true ) );
        } else if constexpr( std::is_integral<std::remove_cvref_t<T>>() ) {
            return ValueObject( static_cast<I64>(var), ValueConfig( true ) );
        } else {
            static_assert(rfl::always_false_v<T>, "Unsupported type.");
        }
    }


};

class Reader
{
public:
    using InputArrayType = ValueObject;
    using InputObjectType = ValueObject;
    using InputVarType = ValueObject;

    template <class T>
    static constexpr bool has_custom_constructor = false;

    rfl::Result<InputVarType> get_field_from_array( const size_t idx, const InputArrayType &arr ) const noexcept
    {
        if( arr.IsSubscriptable() ) {
            //rfl::error( "Index " + std::to_string( idx ) + " of of bounds." );
            return arr[idx];
        }
        return rfl::error( "wrong type!" );
    }

    rfl::Result<InputVarType> get_field_from_object( const std::string &name, const InputObjectType &obj ) const noexcept
    {
        if( obj.IsSubscriptable() ) {
            //return rfl::error( "Object contains no field named '" + name + "'." );
            return obj[name];
        }
        return rfl::error( "wrong type!" );
    }

    bool is_empty( const InputVarType &var ) const noexcept
    {
        return not var.HasValue();
    }

    template <class T>
    rfl::Result<T> to_basic_type( const InputVarType &var ) const noexcept
    {
        if constexpr( std::is_same<std::remove_cvref_t<T>, std::string>() ) {
            return var.GetAsString();
        } else if constexpr( std::is_same<std::remove_cvref_t<T>, bool>() ) {
            return var.GetAsBool();
        } else if constexpr( util::is_arithmetic_v<std::remove_cvref_t<T>> ) {
            return util::ArithmeticFactory::ConvertRaw< std::remove_cvref_t<T> >( var );
        } else {
            static_assert(rfl::always_false_v<T>, "Unsupported type.");
        }
    }

    rfl::Result<InputArrayType> to_array( const InputVarType &var ) const noexcept
    {
        return var;
    }

    template <class ArrayReader>
    std::optional<rfl::Error> read_array( const ArrayReader &array_reader, const InputArrayType &arr ) const noexcept
    {
        if( !tuple::TomlJsonUtil::IsAnArray( arr ) ) {
            return rfl::Error( "not an array!" );
        }
        Tuple const &tup = arr.GetConstValue<Tuple>();
        if( tuple::TomlJsonUtil::IsArrayEmpty( tup ) ) {
            return {};
        }
        for( auto const &[_, v] : tup ) {
            const auto err = array_reader.read( v );
            if( err ) {
                return err;
            }
        }
        return {};
    }

    template <class ObjectReader>
    std::optional<rfl::Error> read_object( const ObjectReader &object_reader, const InputObjectType &obj ) const noexcept
    {
        if( obj.InternalType() != ValueObject::TypeTuple || tuple::TomlJsonUtil::IsAnArray( obj ) ) {
            return rfl::Error( "not an array!" );
        }

        Tuple const &tup = obj.GetConstValue<Tuple>();
        for( auto const &[k, v] : tup ) {
            object_reader.read( std::string_view( k ), v );
        }

        return {};
    }

    rfl::Result<InputObjectType> to_object( const InputVarType &var ) const noexcept
    {
        return var;
    }

    template <class T>
    rfl::Result<T> use_custom_constructor( const InputVarType & /*var*/ ) const noexcept
    {
        return rfl::error( "not implemented!" );
    }
};

template <class T, class ProcessorsType>
using Parser = rfl::parsing::Parser<Reader, Writer, T, ProcessorsType>;


void write_tuple( const auto &obj, teascript::ValueObject &v )
{
    using T = std::remove_cvref_t<decltype(obj)>;
    using ParentType = rfl::parsing::Parent<teascript::reflection::Writer>;
    auto w = teascript::reflection::Writer( v );
    using ProcessorsType = rfl::Processors<>;
    teascript::reflection::Parser<T, ProcessorsType>::write( w, obj, typename ParentType::Root{} );
}

template <class T, class... Ps>
auto read_tuple( teascript::ValueObject const &v )
{
    const auto r = teascript::reflection::Reader();
    using ProcessorsType = rfl::Processors<Ps...>;
    return teascript::reflection::Parser<T, ProcessorsType>::read( r, v );
}

} // namespace reflection
} // namespace teascript
