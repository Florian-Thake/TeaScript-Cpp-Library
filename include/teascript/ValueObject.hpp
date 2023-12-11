/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2023 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, version 3.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
 * You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>
 */
#pragma once

#include <any>
#include <variant>
#include <optional>
#include <string>
#include <iostream> // for << operator
#include <cmath> // isfinite

#include "Type.hpp"
#include "FunctionBase.hpp"
#include "Collection.hpp"
#include "Exception.hpp"
#include "SourceLocation.hpp"


namespace teascript {

// forward declare
class ValueObject;
class ValueConfig;
using Tuple = Collection<ValueObject>;
namespace tuple {
namespace {
std::strong_ordering compare_values( Tuple const &, Tuple const & );
ValueObject deep_copy( ValueObject const &, bool const );
void deep_copy( Tuple &, Tuple const &, bool const );
}// namespace {
}// namespace tuple


namespace exception {

/// Exception thrown when a ValueObject could not be converted to some specific type.
class bad_value_cast : public std::bad_any_cast
{
    std::string mText;
public:
    bad_value_cast() = default;
    bad_value_cast( std::string const &rText ) 
        : std::bad_any_cast()
        , mText( rText )
    {
    }

    char const * what() const noexcept override
    {
        return mText.empty() ? "Bad ValueObject cast" : mText.c_str();
    }
};

} // namespace exception


/// NaV - Not A Value
inline static const NotAValue NaV;


// NOTE: the following config classes and enums are considered preliminary for now. 

enum eShared
{
    ValueShared,
    ValueUnshared,
};
enum eConst
{
    ValueConst,
    ValueMutable,
};
class ValueConfig
{
    eShared const mShared;
    eConst  const mConst;
    
public:
    //FIXME: Make better solution, but std::optional<std::reference_wrapper< TypeSystem const >> is too boilerplate in use, e.g. mTS.has_value() ? mTs.value().get().XXX ....
    TypeSystem const *const mpTypeSystem = nullptr;
    TypeInfo   const *const mpTypeInfo   = nullptr;
public:
    inline ValueConfig( eShared const s, eConst const c ) noexcept
        : mShared( s ), mConst( c ), mpTypeSystem( nullptr )
    {
    }

    inline ValueConfig( eShared const s, eConst const c, TypeSystem const &rSys ) noexcept
        : mShared( s ), mConst( c ), mpTypeSystem( &rSys )
    {
    }

    inline ValueConfig( eShared const s, eConst const c, TypeInfo const *pTypeInfo ) noexcept
        : mShared( s ), mConst( c ), mpTypeInfo( pTypeInfo )
    {
    }

    inline ValueConfig( bool const shared = false ) noexcept //not explicit for old code... TRANSITON! remove later!
        : ValueConfig( shared ? ValueShared : ValueUnshared, ValueMutable )
    {
    }

    inline bool IsShared() const noexcept
    {
        return mShared == ValueShared;
    }
    inline bool IsConst() const noexcept
    {
        return mConst == ValueConst;
    }
};

/// The ValueObject class is the common value object for TeaScript.
/// It serves as a variant like type - holding variables, functions, types, ...
/// \note the class layout / data members are considered unstable and may change often.
/// \note only the public getters are considered semi-stable.
class ValueObject final
{
    // reflects the index of std::variant
    enum eType : std::size_t
    {
        TypeNaV = 0,
        TypeBool,
        TypeLongLong,
        TypeDouble,
        TypeString,
        TypeAny,

        TypeLast = TypeAny,
        TypeFirst = TypeNaV
    };

    //TODO [ITEM 98]: Think of a better storage layout. sizeof 64 would be great! Maybe we can get rid of the nested variant and also store any always as shared_ptr?
    // sizeof BareTypes == 72 (std::any == 64 + 8 for std::variant)
    using BareTypes = std::variant< NotAValue, Bool, I64, F64, String, std::any >;
    // sizeof ValueVariant == 80 (BarTypes == 72 + 8 for std::variant)
    using ValueVariant = std::variant< BareTypes, std::shared_ptr<BareTypes> >;

    /// helper function for create the desired ValueVariant (shared VS unshared). 
    /// \note: needed because in_place_index_t cannot use in ? : operator _and_ std::any and std::shared_ptr<BareTypes> are ambiguous.
    inline static
    ValueVariant create_helper( bool const shared, BareTypes &&bare )
    {
        if( shared ) {
            return ValueVariant( std::in_place_index_t<1>(), std::make_shared<BareTypes>( std::move( bare ) ) );
        } else {
            return ValueVariant( std::in_place_index_t<0>(), std::move( bare ) );
        }
    }

    ValueVariant    mValue;
    BareTypes      *mpValue;
    TypeInfo const *mpType;  //TODO: Use something more smart to manage the pointer!
    TypeProperties  mProps;

    // overloaded idiom for variant visitor. inherit from each 'functor/lambda' so that the derived class contains an operator( T ) for each type which shall be dispatchable.
    template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
#if defined( __clang__ ) // clang needs a little help...
    template<class... Ts> overloaded( Ts... ) -> overloaded<Ts...>;
#endif

    template< typename T >
    T const *get_impl() const noexcept
    {
        // NOTE: This constexpr cascade is because GCC bug (see below)!
        if constexpr( std::is_same_v<T, bool> ) {
            return std::get_if<bool>( mpValue );
        } else if constexpr( std::is_same_v<T, I64> ) {
            return std::get_if<I64>( mpValue );
        } else if constexpr( std::is_same_v<T, F64> ) {
            return std::get_if<F64>( mpValue );
        } else if constexpr( std::is_same_v<T, std::string> ) {
            return std::get_if<std::string>( mpValue );
        } else {
            std::any *any = std::get_if<std::any>( mpValue );
            if( any ) {
                return std::any_cast<T>(any);
            }
            return nullptr;
        }
    }

#if 0 // fails on GCC: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=85282
    template<>
    bool const *get_impl<bool>() const noexcept
    {
        return std::get_if<bool>( mpValue );
    }

    template<>
    I64 const *get_impl<I64>() const noexcept
    {
        return std::get_if<I64>( mpValue );
    }

    template<>
    F64 const *get_impl<F64>() const noexcept
    {
        return std::get_if<F64>( mpValue );
    }

    template<>
    std::string const *get_impl<std::string>() const noexcept
    {
        return std::get_if<std::string>( mpValue );
    }
#endif 

#if 0 // Possible Future extension....
    template<std::integral T> //FIXME: matches also char, wchar_t, char16_t, etc.
    T const *get_impl() const noexcept
    {
        long long const *const pLL = get_impl<long long>();
        //TODO: check overflow/underflow!
        return pLL ? static_cast<T const *>(pLL) : nullptr;
    }
#endif

    static std::string print_tuple( Collection<ValueObject> const &rTuple, int level = 1 )
    {
        std::string res = "(";
        for( std::size_t i = 0; i < rTuple.Size(); ++i ) {
            if( i > 0 ) {
                res += ", ";
            }
            // NOTE: must avoid cyclic reference endless recursion!
            auto const &val = rTuple.GetValueByIdx( i );
            if( val.GetTypeInfo()->IsSame<Collection<ValueObject>>() ) {
                if( level < 6 ) {
                    res += print_tuple( val.GetValue< Collection<ValueObject> >(), level + 1 );
                } else {
                    res += "(...)";
                }
            } else {
                res += val.PrintValue();
            }
        }
        res += ")";
        return res;
    }


public:
    inline
    ValueObject() noexcept // the non shared variant doesn't throw b/c no heap alloc done.
        : ValueObject( NotAValue{} )
    {

    }

    inline
    ValueObject( NotAValue, ValueConfig const &cfg = {} )
        : mValue( create_helper( cfg.IsShared(), BareTypes( NaV ) ) )
        , mpValue( cfg.IsShared() ? std::get<1>( mValue ).get() : &std::get<0>( mValue ) )
        , mpType( &teascript::TypeNaV )
        , mProps( cfg.IsConst() )
    {

    }

    // TODO: Decide if explicit should be removed for ease of use ?
    inline
    explicit ValueObject( bool const b, ValueConfig const &cfg = {} )
        : mValue( create_helper( cfg.IsShared(), BareTypes( b ) ) )
        , mpValue( cfg.IsShared() ? std::get<1>( mValue ).get() : &std::get<0>( mValue ) )
        , mpType( &teascript::TypeBool )
        , mProps( cfg.IsConst() )
    {

    }

    // TODO: Make Constructor which takes ArithemticNumber as type!
    // TODO: Decide if explicit should be removed for ease of use ?
    inline
    explicit ValueObject( I64 const i, ValueConfig const &cfg = {} )
        : mValue( create_helper( cfg.IsShared(), BareTypes( i ) ) )
        , mpValue( cfg.IsShared() ? std::get<1>( mValue ).get() : &std::get<0>( mValue ) )
        , mpType( &teascript::TypeLongLong )
        , mProps( cfg.IsConst() )
    {

    }

    // TODO: Make Constructor which takes ArithemticNumber as type!
    // TODO: Decide if explicit should be removed for ease of use ?
    inline
    explicit ValueObject( F64 const d, ValueConfig const &cfg = {} )
        : mValue( create_helper( cfg.IsShared(), BareTypes( d ) ) )
        , mpValue( cfg.IsShared() ? std::get<1>( mValue ).get() : &std::get<0>( mValue ) )
        , mpType( &teascript::TypeDouble )
        , mProps( cfg.IsConst() )
    {

    }

    // TODO: Decide if explicit should be removed for ease of use ?
    inline
    explicit ValueObject( std::string const &rStr, ValueConfig const &cfg = {} )
        : mValue( create_helper( cfg.IsShared(), BareTypes( rStr ) ) )
        , mpValue( cfg.IsShared() ? std::get<1>( mValue ).get() : &std::get<0>( mValue ) )
        , mpType( &teascript::TypeString )
        , mProps( cfg.IsConst() )
    {

    }

    // TODO: Decide if explicit should be removed for ease of use ?
    inline
    explicit ValueObject( std::string &&rStr, ValueConfig const &cfg = {} )
        : mValue( create_helper( cfg.IsShared(), BareTypes( std::move( rStr ) ) ) )
        , mpValue( cfg.IsShared() ? std::get<1>( mValue ).get() : &std::get<0>( mValue ) )
        , mpType( &teascript::TypeString )
        , mProps( cfg.IsConst() )
    {

    }

    inline
    explicit ValueObject( FunctionPtr &&rFunc, ValueConfig const &cfg )
        : mValue( create_helper( cfg.IsShared(), BareTypes(std::move(rFunc)) ) ) 
        , mpValue( std::get<1>( mValue ).get() )
        , mpType( cfg.mpTypeSystem != nullptr 
                  ? cfg.mpTypeSystem->Find<FunctionPtr>()
                  : nullptr )
        , mProps( cfg.IsConst() )
    {
        // not allow nullptr for this usage. also we don't want manually alloc a type here.
        if( nullptr == mpType ) {
            throw exception::runtime_error( "Usage Error! No TypeSystem for ValueObject constructor!" );
        }
    }

    inline
    explicit ValueObject( std::vector<ValueObject> &&rVals, ValueConfig const &cfg )
        : mValue( create_helper( true, BareTypes( std::move( rVals ) ) ) )
        , mpValue( std::get<1>( mValue ).get() )
        , mpType( cfg.mpTypeSystem != nullptr 
                  ? cfg.mpTypeSystem->Find< std::vector<ValueObject>>()
                  : nullptr )
        , mProps( cfg.IsConst() )
    {
        // not allow nullptr for this usage. also we don't want manually alloc a type here.
        if( nullptr == mpType ) {
            throw exception::runtime_error( "Usage Error! No TypeSystem for ValueObject constructor!" );
        }
    }

    inline
    explicit ValueObject( Collection<ValueObject> &&rVals, ValueConfig const &cfg )
        : mValue( create_helper( true, BareTypes( std::move( rVals ) ) ) )
        , mpValue( std::get<1>( mValue ).get() )
        , mpType( cfg.mpTypeSystem != nullptr
                  ? cfg.mpTypeSystem->Find< Collection<ValueObject> >()
                  : cfg.mpTypeInfo )
        , mProps( cfg.IsConst() )
    {
        // not allow nullptr and wrong type for this usage. also we don't want manually alloc a type here.
        if( nullptr == mpType || not mpType->IsSame<Collection<ValueObject>>() ) {
            throw exception::runtime_error( "Usage Error! No TypeSystem or wrong TypeInfo for ValueObject constructor!" );
        }
    }

    inline
    explicit ValueObject( TypeInfo const & rTypeInfo, ValueConfig const &cfg )
        : mValue( create_helper( true, BareTypes( rTypeInfo ) ) )
        , mpValue( std::get<1>( mValue ).get() )
        , mpType( &TypeTypeInfo )
        , mProps( cfg.IsConst() )
    {

    }

    // note: Passthrough data is in experimental state for now
    inline
    explicit ValueObject( Passthrough /*tag*/, std::any &&any, ValueConfig const &cfg = {} )
        : mValue( create_helper( true, BareTypes(std::move(any) ) ) )
        , mpValue( std::get<1>( mValue ).get() )
        , mpType( &TypePassthrough )
        , mProps( cfg.IsConst() )
    {
    }

    /// Factory for create a ValueObject containing arbitrary passthrough data.
    /// \note Passthrough data is in experimental state
    static ValueObject CreatePassthrough( std::any &&any )
    {
        return ValueObject( Passthrough{}, std::move( any ) );
    }

    ~ValueObject()
    {
        if( mProps.IsTypeAllocated() ) {
            delete mpType; // we must manually delete it! //NOTE: a union with one unique_ptr will not help b/c we must call the destructor then manually as well!!!
        }
    }

    inline
    ValueObject( ValueObject const &rOther )
        : mValue( rOther.mValue )
        , mpValue( mValue.index() == 1 ? rOther.mpValue : &std::get<0>( mValue ) )
        , mpType( rOther.mpType )
        , mProps() // later for exception safety (potential double delete of mpType)
    {
        if( rOther.mProps.IsTypeAllocated() ) {
            mpType = new TypeInfo( *rOther.mpType ); // must make a copy
        }
        mProps = rOther.mProps;
    }

    inline
    ValueObject &operator=( ValueObject const &rOther )
    {
        if( this != &rOther && rOther.mpValue != mpValue ) {
            mValue = rOther.mValue;
            mpValue = mValue.index() == 1 ? rOther.mpValue : &std::get<0>( mValue );
            mpType = rOther.mpType; //NOTE: Be careful: assignment operator is able to change type!
            if( rOther.mProps.IsTypeAllocated() ) {
                mpType = new TypeInfo( *rOther.mpType ); // must make a copy
            }
            mProps = rOther.mProps;
        }
        return *this;
    }

    inline
    ValueObject( ValueObject &&rOther ) noexcept(noexcept(ValueVariant( std::move( mValue ) ))) //FIXME: check this noexcept spec!
        : mValue( std::move( rOther.mValue ) )
        , mpValue( mValue.index() == 1 ? rOther.mpValue : &std::get<0>( mValue ) )
        , mpType( rOther.mpType )
        , mProps( rOther.mProps )
    {
        if( rOther.mProps.IsTypeAllocated() ) { // must move out!
            rOther.mProps.SetTypeAllocated( false );
            rOther.mpType = nullptr;
        }
    }

    inline
    ValueObject &operator=( ValueObject &&rOther ) noexcept(noexcept(std::declval<ValueVariant &>() = std::declval<ValueVariant &&>())) //FIXME: check this noexcept spec!
    {
        if( this != &rOther ) {
            mValue = std::move( rOther.mValue );
            mpValue = mValue.index() == 1 ? rOther.mpValue : &std::get<0>( mValue );
            mpType = rOther.mpType;
            mProps = rOther.mProps;
            if( rOther.mProps.IsTypeAllocated() ) { // must move out!
                rOther.mProps.SetTypeAllocated( false );
                rOther.mpType = nullptr;
            }
        }
        return *this;
    }

    /// \warning INTERNAL interface
    inline uintptr_t GetInternalID() const noexcept
    {
        return reinterpret_cast<uintptr_t>(mpValue);
    }

    /// \return whether this instance uses a reference counting mechanism for the stored value.
    inline bool IsShared() const noexcept
    {
        return mValue.index() == 1;
    }

    /// Transforms this instance to use a reference counting mechanism for the stored value (if not already).
    /// \note A std::move is performed on the stored value.
    ValueObject &MakeShared()
    {
        // only if not shared...
        if( mValue.index() == 0 ) {
            auto s = std::make_shared<BareTypes>( std::move( *mpValue ) );
            mValue = std::move( s );
            mpValue = std::get<1>( mValue ).get();
        }
        return *this;
    }

    /// Makes the value of this instance detached from any other instance.
    /// This means a copy of the value is done and the reference counter of the new copy is equal to 1.
    /// \note The new instance is mutable (since it is a new object) or it can be kept const if the original was const as well.
    ValueObject &Detach( bool const keep_const ) //TODO: DetachConfig w. AutoShared, keep_const, TypeSys
    {
        // only if shared...
        if( mValue.index() == 1 ) {
            // costruct a new shared again to avoid extra copies for classes without moves when calling MakeShared() as the next step!!
            if( GetTypeInfo()->GetName() == "Tuple" ) {
                auto new_val = tuple::deep_copy( *this, keep_const );
                mValue = new_val.mValue;
                mpValue = std::get<1>( mValue ).get();
            } else if( mpValue->index() == TypeAny ) {
                auto s = std::make_shared<BareTypes>( *mpValue ); // new copy with new (unshared) shared_ptr
                mValue = std::move( s );
                mpValue = std::get<1>( mValue ).get();
            } else {
                auto  copy = *mpValue; // first make a copy to not shoot in our own foot.
                mValue = std::move( copy );
                mpValue = &std::get<0>( mValue );
            }
            if( IsConst() && not keep_const ) {
                mProps.MakeMutable();
            }
        }
        return *this;
    }

    /// \return whether the stored value is const
    inline bool IsConst() const noexcept
    {
        return mProps.IsConst();
    }

    /// \return whether the stored value is mutable
    inline bool IsMutable() const noexcept
    {
        return not IsConst();
    }

    /// \returns the TypeInfo for the stored value.
    inline TypeInfo const *GetTypeInfo() const noexcept
    {
        return mpType;
    }

    /// Makes this instance const for the stored value.
    /// \note Other instances to the same value may still change the value when they are mutable!
    ValueObject &MakeConst()
    {
        if( mProps.IsMutable() ) {
            mProps.MakeConst();
        }
        return *this;
    }

    /// Assigns a new value to this instance via copy. Types must match.
    /// \throw May throw exception::type_missmatch/const_assign
    void AssignValue( ValueObject const &rOther, SourceLocation const &rLoc = {} )
    {
        TypeInfo const *pT1 = GetTypeInfo();
        TypeInfo const *pT2 = rOther.GetTypeInfo();
        //TODO: Check implicit conversions? (but probably outside)
        if( not pT1->IsSame( *pT2 ) && not pT2->IsSame( teascript::TypeNaV ) ) { // A value can become NaV without lose its real type!
            throw exception::type_mismatch( rLoc );
        }
        if( mProps.IsConst() ) {
            throw exception::const_assign( rLoc );
        }
        if( pT1->GetName() == "Tuple" && rOther.ShareCount() > 1 ) {
            //throw exception::eval_error( "Assign Error! Tuples only support shared assign, use _tuple_copy() instead!" );
            auto new_val = tuple::deep_copy( rOther, false );
            *mpValue = std::move( *new_val.mpValue ); // cannot assign to *this since it might be shared
        } else {
            *mpValue = *rOther.mpValue;
        }
    }

    /// Shares assign a new value by using the same value via a reference counted mechanism. Types must match.
    /// \throw May throw exception::type_missmatch/const_assign/const_shared_assign
    void SharedAssignValue( ValueObject const &rOther, SourceLocation const &rLoc = {} )
    {
        TypeInfo const *pT1 = GetTypeInfo();
        TypeInfo const *pT2 = rOther.GetTypeInfo();
        //TODO: Check implicit conversions? (but probably outside)
        if( not pT1->IsSame( *pT2 ) && not pT2->IsSame( teascript::TypeNaV) ) { // A value can become NaV without lose its real type!
            throw exception::type_mismatch( rLoc );
        }
        if( mProps.IsConst() ) {
            throw exception::const_assign( rLoc );
        }
        if( rOther.mProps.IsConst() ) {
            throw exception::const_shared_assign( rLoc );
        }
        mValue = rOther.mValue;
        // be aware of rOther is not shared.
        mpValue = mValue.index() == 1 ? rOther.mpValue : &std::get<0>( mValue );
    }

    /// returns whether the given instance shares the same value as this instance.
    bool IsSharedWith( ValueObject const &rOther ) const noexcept
    {
        if( IsShared() && rOther.IsShared() ) {
            return mpValue == rOther.mpValue;
        }
        return this == &rOther;
    }

    /// \returns the count of the value is shared.
    long long ShareCount() const noexcept
    {
        if( IsShared() ) {
            return static_cast<long long>(std::get<1>( mValue ).use_count());
        }
        return 0LL; // not shared at all
    }


    /// \returns whether this instance holds a value or is NaV - Not A Value
    bool HasValue() const noexcept
    {
        //return mValue.index() != TypeNaV;
        return mpValue->index() > 0;
    }

    /// \returns if the value can be printed as text/string.
    bool HasPrintableValue() const noexcept
    {
        //return mValue.index() != TypeNaV && mValue.index() != TypeAny;
        //return mpValue->index() > TypeFirst && mpValue->index() < TypeLast;
        if( mpValue->index() == TypeAny ) {
            return get_impl<Collection<ValueObject>>() != nullptr;
        }
        return mpValue->index() > TypeFirst;
    }

    /// \return the stored value as T & or throws exception::bad_value_cast
    template< typename T >
    T & GetValue()
    {
        // one of the rare unevil const_cast: first make this const to can re-use the GetValue code, then remove the const again from result.
        return const_cast<T&>( const_cast<ValueObject const &>(*this).GetValue<T>() );
    }

    /// \return the stored value as T const & or throws exception::bad_value_cast
    template< typename T >
    T const &GetValue() const
    {
        T const * const  p = get_impl<T>();
        if( p != nullptr ) {
            return *p;
        }
        throw exception::bad_value_cast();
    }

    /// \return the stored value as T const * or nullptr
    template< typename T >
    T const *GetValuePtr() const noexcept
    {
        return get_impl<T>();
    }

    /// \returns the stored passthrough data. \throws exception::bad_value_cast.
    std::any const &GetPassthroughData() const
    {
        if( not mpType->IsSame<Passthrough>() ) {
            throw exception::bad_value_cast();
        }
        return std::get<std::any>( *mpValue );
    }

    /// \returns the stored passthrough data. \throws exception::bad_value_cast.
    std::any &GetPassthroughData()
    {
        return const_cast<std::any &>(const_cast<ValueObject const &>(*this).GetPassthroughData());
    }

    /// Converts the value to bool if possible, otherwise throws exception::bad_value_cast
    bool GetAsBool() const
    {
        return std::visit( overloaded{
            []( auto ) -> bool { throw exception::bad_value_cast("ValueObject not convertible to bool!"); },
            []( std::any &a ) {
                if( auto const *p_tuple = std::any_cast<Collection<ValueObject>>(&a); p_tuple != nullptr ) {
                    return p_tuple->Size() > 0u;
                } else throw exception::bad_value_cast( "ValueObject not convertible to bool!" ); },
            []( NotAValue ) -> bool { throw exception::bad_value_cast( "ValueObject is NaV (Not A Value)!" ); },
            []( bool b ) { return b; },
            []( I64 const i ) { return static_cast<bool>(i); },
            []( F64 const d ) { return static_cast<bool>(d); },
            []( std::string const &rStr ) { return !rStr.empty(); }
        }, *mpValue );
    }

    /// Converts the value to Integer if possible, otherwise throws exception::bad_value_cast
    Integer GetAsInteger() const
    {
        return std::visit( overloaded{
            []( auto ) -> Integer { throw exception::bad_value_cast( "ValueObject not convertible to Integer!" ); },
            []( NotAValue ) -> Integer { throw exception::bad_value_cast( "ValueObject is NaV (Not A Value)!" ); },
            []( bool const b ) { return static_cast<Integer>(b); },
            []( I64 const i ) { return static_cast<Integer>(i); },
            []( F64 const d ) { return std::isfinite( d ) ? static_cast<Integer>(d) : (throw exception::bad_value_cast( "ValueObject with double not convertible to Integer!" ), Integer{}); },
            []( std::string const &rStr ) { return static_cast<Integer>(std::stoll( rStr )); }
        }, *mpValue );
    }

    /// Converts the value to std::string if possible, otherwise throws exception::bad_value_cast
    /// \note unlike \see PrintValue the method will throw if value is NaV or any!
    std::string GetAsString() const
    {
        return std::visit( overloaded{
            []( auto ) -> std::string { throw exception::bad_value_cast( "ValueObject not convertible to string!" ); },
            []( std::any &a ) {
                if( auto const *p_tuple = std::any_cast<Collection<ValueObject>>(&a); p_tuple != nullptr ) {
                    return print_tuple( *p_tuple );
                } else throw exception::bad_value_cast( "ValueObject not convertible to string!" ); },
            []( NotAValue ) -> std::string { throw exception::bad_value_cast( "ValueObject is NaV (Not A Value)!" ); },
            []( bool const b ) { return std::string( b ? "true" : "false" ); },
            []( I64 const i ) { return std::to_string( i ); },
            []( F64 const d ) { return std::to_string( d ); }, //FIXME: use better way, this will lose precision for small numbers!
            []( std::string const &rStr ) { return rStr; }
        }, *mpValue );
    }

    /// Returns a std::string for printing information of the value.
    /// \note in contrast to \see GetAsString this method will always produce a printable result (if the std functions dont throw).
    std::string PrintValue() const
    {
        return std::visit( overloaded{
            []( auto ) { return std::string("<not printable>"); },
            []( std::any &a ) { 
                if( auto const *p_tuple = std::any_cast<Collection<ValueObject>>(&a); p_tuple != nullptr ) {
                    return print_tuple( *p_tuple );
                } else return std::string( "<not printable>" ); },
            []( NotAValue ) { return std::string( "NaV (Not A Value)" ); },
            []( bool const b ) { return std::string( b ? "true" : "false"); },
            []( I64 const i ) { return std::to_string( i ); },
            []( F64 const d ) { return std::to_string( d ); }, //FIXME: use better way, this will lose precision for small numbers!
            []( std::string const &rStr ) { return "\"" + rStr + "\""; }
        }, *mpValue );
    }

    /// ostream operator support
    friend std::ostream &operator<<( std::ostream &os, ValueObject const &rObj )
    {
        os << rObj.PrintValue();
        return os;
    }


    /// \return whether the operator [] has a usefull meaning, e.g. this object might have nested child objects via a Tuple or another container.
    inline
    bool IsSubscriptable() const noexcept
    {
        return mpType->IsSame<Tuple>();
    }

    /// Subscript operator [] for index based access of nested child objects, e.g. inside a Tuple.
    /// Will throw if this object is not subscriptable or index is out of range.
    ValueObject const &operator[]( std::size_t const idx ) const
    {
        if( IsSubscriptable() ) {
            return GetValue<Tuple>().GetValueByIdx( idx );
        }
        throw exception::bad_value_cast( "Object is not subscriptable!" );
    }

    /// Subscript operator [] for index based access of nested child objects, e.g. inside a Tuple. 
    /// Will throw if this object is not subscriptable or index is out of range.
    ValueObject &operator[]( std::size_t const idx )
    {
        if( IsSubscriptable() ) {
            return GetValue<Tuple>().GetValueByIdx( idx );
        }
        throw exception::bad_value_cast( "Object is not subscriptable!" );
    }

    /// Subscript operator [] for key based access of nested child objects, e.g. inside a Tuple.
    /// Will throw if this object is not subscriptable or key is not present.
    /// Unlike std::map this operator will _not_ create a missing key / value!
    ValueObject const &operator[]( String const &rKey ) const
    {
        if( IsSubscriptable() ) {
            return GetValue<Tuple>().GetValueByKey( rKey );
        }
        throw exception::bad_value_cast( "Object is not subscriptable!" );
    }

    /// Subscript operator [] for key based access of nested child objects, e.g. inside a Tuple.
    /// Will throw if this object is not subscriptable or key is not present.
    /// Unlike std::map this operator will _not_ create a missing key / value!
    ValueObject &operator[]( String const &rKey )
    {
        if( IsSubscriptable() ) {
            return GetValue<Tuple>().GetValueByKey( rKey );
        }
        throw exception::bad_value_cast( "Object is not subscriptable!" );
    }


    //FIXME: strong_ordering is not ok for floating point and special custom types!
    /// Returns the order of the 2 ValueObjects. long long is preferred over bool. bool is preferred over string.
    friend std::strong_ordering operator<=>( ValueObject const &lhs, ValueObject const &rhs )
    {
        //FIXME: use Number helper class for arithmetic conversions.
        if( lhs.get_impl<F64>() != nullptr || rhs.get_impl<F64>() != nullptr ) {
            F64 const *pd_lhs = lhs.get_impl<F64>();
            F64 const *pd_rhs = rhs.get_impl<F64>();
            F64 const d1 = pd_lhs != nullptr ? *pd_lhs : static_cast<F64>(lhs.GetAsInteger());
            F64 const d2 = pd_rhs != nullptr ? *pd_rhs : static_cast<F64>(rhs.GetAsInteger());
#if _MSC_VER || _LIBCPP_VERSION // libc++14 works here, libstdc++ of g++11 not.
            return std::strong_order(d1, d2); //TODO: check this! we could bail out on e.g., NaN instead or change return value to partial_odering.
#else
            auto const c = d1 <=> d2;
            if( c == std::partial_ordering::unordered ) {
                throw exception::runtime_error( "Strong ordering of floating point not implemented on this platform!" );
            } else if( c == std::partial_ordering::less ) {
                return std::strong_ordering::less;
            } else if( c == std::partial_ordering::equivalent ) {
                return std::strong_ordering::equal;
            } else {
                return std::strong_ordering::greater;
            }
#endif
        }

        // bool, string and any will be converted to long long for comparison if any of the values is a long long
        // NOTE: In C++ bool is also converted to long long when the bool is compared with a long long!
        if( lhs.get_impl<I64>() != nullptr || rhs.get_impl<I64>() != nullptr ) {
            return lhs.GetAsInteger() <=> rhs.GetAsInteger();
        }
        // otherwise string and any will be converted to bool if any of the values is a bool
        if( lhs.get_impl<bool>() != nullptr || rhs.get_impl<bool>() != nullptr ) {
            return lhs.GetAsBool() <=> rhs.GetAsBool();
        }
        
        //FIXME: implement clean conversion rules!
        if( lhs.get_impl<std::string>() != nullptr && (lhs.get_impl<std::string>() != nullptr || /*convertible*/ false) ) {
#if !_LIBCPP_VERSION // libc++14 fails here
            return lhs.GetAsString() <=> rhs.GetAsString();
#else
            auto const res = lhs.GetAsString().compare( rhs.GetAsString() );
            return res < 0 ? std::strong_ordering::less : res > 0 ? std::strong_ordering::greater : std::strong_ordering::equal;
#endif
        }

        if( lhs.get_impl<TypeInfo>() != nullptr && rhs.get_impl<TypeInfo>() != nullptr ) {
#if !_LIBCPP_VERSION // libc++14 fails here
            return lhs.GetValue<TypeInfo>().ToTypeIndex() <=> rhs.GetValue<TypeInfo>().ToTypeIndex();
#else
            auto const i1 = lhs.GetValue<TypeInfo>().ToTypeIndex();
            auto const i2 = rhs.GetValue<TypeInfo>().ToTypeIndex();
            return i1 < i2 ? std::strong_ordering::less : i2 < i1 ? std::strong_ordering::greater : std::strong_ordering::equal;
#endif
        }

        if( lhs.GetTypeInfo()->GetName() == "Tuple" && rhs.GetTypeInfo()->GetName() == "Tuple" ) {
            return tuple::compare_values( lhs.GetValue<Tuple>(), rhs.GetValue<Tuple>() );
        }

        //FIXME: Now need type info for check equal types or conversion
        throw exception::bad_value_cast( "types do not match for comparison!" );
    }

    /// Returns whether the 2 ValueObjects are equal.
    friend bool operator==( ValueObject const &lhs, ValueObject const &rhs )
    {
        auto const res = lhs <=> rhs;
        return res == std::strong_ordering::equal || res == std::strong_ordering::equivalent;
    }

    /// Returns whether the 2 ValueObjects are unequal.
    friend bool operator!=( ValueObject const &lhs, ValueObject const &rhs )
    {
        return !(lhs == rhs);
    }
};

} // namespace teascript

#include "TupleUtil.hpp"
