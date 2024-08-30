/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
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
#include "IntegerSequence.hpp"
#include "Exception.hpp"
#include "SourceLocation.hpp"
#include "Print.hpp"  // we need to know if fmt is in use for double -> string conversion


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
class bad_value_cast : public runtime_error
{
    std::string mText;
public:
    bad_value_cast( SourceLocation const &rLoc = {} ) : runtime_error( rLoc, "Bad ValueObject cast" ) {}
    bad_value_cast( std::string const &rText, SourceLocation const &rLoc = {} )
        : runtime_error( rLoc, rText )
    {
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
/// \note only the public getters are considered stable.
class ValueObject final
{
public:
    // reflects the index of std::variant
    enum eType : std::size_t
    {
        TypeNaV = 0,
        TypeBool,
        TypeU8,
        TypeI64,
        TypeLongLong = TypeI64, // alias, deprecated, use TypeI64
        TypeU64,
        TypeF64,
        TypeDouble   = TypeF64, // alias, deprecated, use TypeF64
        TypeString,
        TypeTuple,
        TypeBuffer,
        TypeIntSeq,   // IntegerSequence
        TypeFunction,
        TypeAny,

        TypeLast = TypeAny,
        TypeFirst = TypeNaV
    };

private:

    //TODO [ITEM 98]: Think of a better storage layout. sizeof 64 would be great! Maybe we can get rid of the nested variant and also store any always as shared_ptr?
    // sizeof BareTypes == 72 (std::any == 64 + 8 for std::variant)
    using BareTypes = std::variant< NotAValue, Bool, U8, I64, U64, F64, String, Tuple, Buffer, IntegerSequence, FunctionPtr, std::any >;
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

    // need this object for get a valid pointer for moved out objects. otherwise their pointer might dangle!
    inline static ValueVariant const shared_nav   = ValueVariant( std::in_place_index_t<1>(), std::make_shared<BareTypes>( NotAValue{} ) );
    inline static BareTypes   *const p_shared_nav = std::get<1>( shared_nav ).get();

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
        } else if constexpr( std::is_same_v<T, U8> ) {
            return std::get_if<U8>( mpValue );
        } else if constexpr( std::is_same_v<T, I64> ) {
            return std::get_if<I64>( mpValue );
        } else if constexpr( std::is_same_v<T, U64> ) {
            return std::get_if<U64>( mpValue );
        } else if constexpr( std::is_same_v<T, F64> ) {
            return std::get_if<F64>( mpValue );
        } else if constexpr( std::is_same_v<T, std::string> ) {
            return std::get_if<std::string>( mpValue );
        } else if constexpr( std::is_same_v<T, Tuple> ) {
            return std::get_if<Tuple>( mpValue );
        } else if constexpr( std::is_same_v<T, Buffer> ) {
            return std::get_if<Buffer>( mpValue );
        } else if constexpr( std::is_same_v<T, IntegerSequence> ) {
            return std::get_if<IntegerSequence>( mpValue );
        } else if constexpr( std::is_same_v<T, FunctionPtr> ) {
            return std::get_if<FunctionPtr>( mpValue );
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

    static std::string print_tuple( Tuple const &rTuple, int level = 1 )
    {
        std::string res = "(";
        for( std::size_t i = 0; i < rTuple.Size(); ++i ) {
            if( i > 0 ) {
                res += ", ";
            }
            // NOTE: must avoid cyclic reference endless recursion!
            auto const &val = rTuple.GetValueByIdx( i );
            if( val.GetTypeInfo()->IsSame<Tuple>() ) {
                if( level < 6 ) {
                    res += print_tuple( val.GetValue<Tuple>(), level + 1 );
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

    static std::string print_buffer( Buffer const &rBuffer, size_t max_count )
    {
        std::string res = "[";
        for( std::size_t i = 0; i < rBuffer.size() && i < max_count; ++i ) {
            if( i > 0 ) {
                res += ", ";
            }
            res += std::to_string( rBuffer[i] );
        }
        if( max_count < rBuffer.size() ) {
            res += ",...";
        }
        res += "]";
        return res;
    }

    // internal (for now) number to string converter, especially for floating point types.
    template< ArithmeticNumber T>
    inline static std::string to_string( T const v )
    {
        // 2 cases: 1.) fmt is in use (mandatory for old g++/clang, otheriwse optional).
        //          2.) otherise std::format is implemented already and can be used. (fmt is preferred)
#if TEASCRIPT_FMTFORMAT   // 1.
        auto res = fmt::to_string( v );
#else // 2.
        auto res = std::format( "{}", v );
#endif
        // append at least a .0 to signal it is a floating point value.
        if constexpr( std::is_floating_point_v<T> ) {
            if( not res.empty() && res.find( '.' ) == std::string::npos && res.find( 'e' ) == std::string::npos ) {
                if( std::isfinite( v ) ) {
                    res += ".0";
                }
            }
        }
        return res;
    }


public:
    inline
    ValueObject() noexcept // doesn't throw b/c no heap alloc done.
        : mValue()
        , mpValue( &std::get<0>( mValue ) )
        , mpType( &teascript::TypeNaV )
        , mProps()
    {

    }

    inline
    explicit ValueObject( NotAValue, ValueConfig const &cfg = {} )
        : mValue( create_helper( cfg.IsShared(), BareTypes( NaV ) ) )
        , mpValue( cfg.IsShared() ? std::get<1>( mValue ).get() : &std::get<0>( mValue ) )
        , mpType( &teascript::TypeNaV )
        , mProps( cfg.IsConst() )
    {

    }

    inline
    explicit ValueObject( bool const b, ValueConfig const &cfg = {} )
        : mValue( create_helper( cfg.IsShared(), BareTypes( b ) ) )
        , mpValue( cfg.IsShared() ? std::get<1>( mValue ).get() : &std::get<0>( mValue ) )
        , mpType( &teascript::TypeBool )
        , mProps( cfg.IsConst() )
    {

    }

    inline
    explicit ValueObject( U8 const u, ValueConfig const &cfg = {} )
        : mValue( create_helper( cfg.IsShared(), BareTypes( u ) ) )
        , mpValue( cfg.IsShared() ? std::get<1>( mValue ).get() : &std::get<0>( mValue ) )
        , mpType( &teascript::TypeU8 )
        , mProps( cfg.IsConst() )
    {

    }

    inline
    explicit ValueObject( I64 const i, ValueConfig const &cfg = {} )
        : mValue( create_helper( cfg.IsShared(), BareTypes( i ) ) )
        , mpValue( cfg.IsShared() ? std::get<1>( mValue ).get() : &std::get<0>( mValue ) )
        , mpType( &teascript::TypeLongLong )
        , mProps( cfg.IsConst() )
    {

    }

    inline
    explicit ValueObject( U64 const u, ValueConfig const &cfg = {} )
        : mValue( create_helper( cfg.IsShared(), BareTypes( u ) ) )
        , mpValue( cfg.IsShared() ? std::get<1>( mValue ).get() : &std::get<0>( mValue ) )
        , mpType( &teascript::TypeU64 )
        , mProps( cfg.IsConst() )
    {

    }

    inline
    explicit ValueObject( F64 const d, ValueConfig const &cfg = {} )
        : mValue( create_helper( cfg.IsShared(), BareTypes( d ) ) )
        , mpValue( cfg.IsShared() ? std::get<1>( mValue ).get() : &std::get<0>( mValue ) )
        , mpType( &teascript::TypeDouble )
        , mProps( cfg.IsConst() )
    {

    }

    inline
    explicit ValueObject( std::string const &rStr, ValueConfig const &cfg = {} )
        : mValue( create_helper( cfg.IsShared(), BareTypes( rStr ) ) )
        , mpValue( cfg.IsShared() ? std::get<1>( mValue ).get() : &std::get<0>( mValue ) )
        , mpType( &teascript::TypeString )
        , mProps( cfg.IsConst() )
    {

    }

    inline
    explicit ValueObject( std::string &&rStr, ValueConfig const &cfg = {} )
        : mValue( create_helper( cfg.IsShared(), BareTypes( std::move( rStr ) ) ) )
        , mpValue( cfg.IsShared() ? std::get<1>( mValue ).get() : &std::get<0>( mValue ) )
        , mpType( &teascript::TypeString )
        , mProps( cfg.IsConst() )
    {

    }

    inline
    explicit ValueObject( Buffer &&rBuffer, ValueConfig const &cfg = {} )
        : mValue( create_helper( true, BareTypes( std::move( rBuffer ) ) ) )
        , mpValue( std::get<1>( mValue ).get() )
        , mpType( &teascript::TypeBuffer )
        , mProps( cfg.IsConst() )
    {

    }

    inline
    explicit ValueObject( FunctionPtr &&rFunc, ValueConfig const &cfg )
        : mValue( create_helper( cfg.IsShared(), BareTypes(std::move(rFunc)) ) ) 
        , mpValue( cfg.IsShared() ? std::get<1>( mValue ).get() : &std::get<0>( mValue ) )
        , mpType( cfg.mpTypeSystem != nullptr 
                  ? cfg.mpTypeSystem->Find<FunctionPtr>()
                  : cfg.mpTypeInfo )
        , mProps( cfg.IsConst() )
    {
        // not allow nullptr and wrong type for this usage. also we don't want manually alloc a type here.
        if( nullptr == mpType || not mpType->IsSame<FunctionPtr>() ) {
            throw exception::runtime_error( "Usage Error! No TypeSystem or wrong TypeInfo for ValueObject FunctionPtr constructor!" );
        }
    }

    inline
    explicit ValueObject( std::vector<ValueObject> &&rVals, ValueConfig const &cfg )
        : mValue( create_helper( true, BareTypes( std::move( rVals ) ) ) )
        , mpValue( std::get<1>( mValue ).get() )
        , mpType( cfg.mpTypeSystem != nullptr 
                  ? cfg.mpTypeSystem->Find< std::vector<ValueObject> >()
                  : cfg.mpTypeInfo )
        , mProps( cfg.IsConst() )
    {
        // not allow nullptr and wrong type for this usage. also we don't want manually alloc a type here.
        if( nullptr == mpType || not mpType->IsSame<std::vector<ValueObject>>() ) {
            throw exception::runtime_error( "Usage Error! No TypeSystem or wron TypeInfo for ValueObject vector<ValueObject> constructor!" );
        }
    }

    inline
    explicit ValueObject( Tuple &&rVals, ValueConfig const &cfg )
        : mValue( create_helper( true, BareTypes( std::move( rVals ) ) ) )
        , mpValue( std::get<1>( mValue ).get() )
        , mpType( cfg.mpTypeSystem != nullptr
                  ? cfg.mpTypeSystem->Find< Tuple >()
                  : cfg.mpTypeInfo )
        , mProps( cfg.IsConst() )
    {
        // not allow nullptr and wrong type for this usage. also we don't want manually alloc a type here.
        if( nullptr == mpType || not mpType->IsSame<Tuple>() ) {
            throw exception::runtime_error( "Usage Error! No TypeSystem or wrong TypeInfo for ValueObject Tuple constructor!" );
        }
    }

    inline
    explicit ValueObject( IntegerSequence && seq, ValueConfig const &cfg )
        : mValue( create_helper( true, BareTypes( std::move( seq ) ) ) )
        , mpValue( std::get<1>( mValue ).get() )
        , mpType( &TypeIntegerSequence )
        , mProps( cfg.IsConst() )
    {

    }

    inline
    explicit ValueObject( TypeInfo const & rTypeInfo, ValueConfig const &cfg )
        : mValue( create_helper( true, BareTypes( rTypeInfo ) ) )
        , mpValue( std::get<1>( mValue ).get() )
        , mpType( &TypeTypeInfo )
        , mProps( cfg.IsConst() )
    {

    }

    inline
    explicit ValueObject( Passthrough /*tag*/, std::any &&any, ValueConfig const &cfg = {} )
        : mValue( create_helper( true, BareTypes(std::move(any) ) ) )
        , mpValue( std::get<1>( mValue ).get() )
        , mpType( &TypePassthrough )
        , mProps( cfg.IsConst() )
    {
    }

    /// Factory for create a ValueObject containing arbitrary passthrough data.
    static ValueObject CreatePassthrough( std::any &&any )
    {
        return ValueObject( Passthrough{}, std::move( any ) );
    }

    ~ValueObject()
    {
        // NOTE: actually there is not any build-in type which is allocated
        if( mProps.IsTypeAllocated() ) [[unlikely]] {
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
        if( rOther.mProps.IsTypeAllocated() ) [[unlikely]] {
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
            if( rOther.mProps.IsTypeAllocated() ) [[unlikely]] {
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
        if( rOther.mProps.IsTypeAllocated() ) [[unlikely]] { // must move out!
            rOther.mProps.SetTypeAllocated( false );
        }
        // rOther mValue is moved out and the mpValue may or may not be invalid. So, set it to a static NaV.
        rOther.mpValue = p_shared_nav;
    }

    inline
    ValueObject &operator=( ValueObject &&rOther ) noexcept(noexcept(std::declval<ValueVariant &>() = std::declval<ValueVariant &&>())) //FIXME: check this noexcept spec!
    {
        if( this != &rOther ) {
            mValue = std::move( rOther.mValue );
            mpValue = mValue.index() == 1 ? rOther.mpValue : &std::get<0>( mValue );
            mpType = rOther.mpType;
            mProps = rOther.mProps;
            if( rOther.mProps.IsTypeAllocated() ) [[unlikely]] { // must move out!
                rOther.mProps.SetTypeAllocated( false );
            }
            // rOther mValue is moved out and the mpValue may or may not be invalid. So, set it to a static NaV.
            rOther.mpValue = p_shared_nav;
        }
        return *this;
    }

    /// \returns the TypeInfo for the stored value.
    inline TypeInfo const *GetTypeInfo() const noexcept
    {
        return mpType;
    }

    /// \retunrs the inner type of the variant. Can be used in a switch.
    inline eType InternalType() const noexcept
    {
        return static_cast<eType>(mpValue->index());
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
            if( ShareCount() < 2 ) { // TODO: THREAD!
                // nothing to do if 'this' is the only one...
            } else if( GetTypeInfo()->IsSame<Tuple>() ) {
                auto new_val = tuple::deep_copy( *this, keep_const );
                mValue = new_val.mValue;
                mpValue = std::get<1>( mValue ).get();
            } else if( mpValue->index() == TypeAny ) {
                // costruct a new shared again to avoid extra copies for classes without moves when calling MakeShared() as the next step!!
                auto s = std::make_shared<BareTypes>( *mpValue ); // new copy with new (unshared) shared_ptr
                mValue = std::move( s );
                mpValue = std::get<1>( mValue ).get();
            } else {
                auto  copy = *mpValue; // first make a copy to not shoot in our own foot.
                mValue = std::move( copy );
                mpValue = &std::get<0>( mValue );
            }
        }
        // do this always!
        if( IsConst() && not keep_const ) {
            mProps.MakeMutable();
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

    /// Makes this instance const for the stored value.
    /// \note Other instances to the same value may still change the value when they are mutable!
    ValueObject &MakeConst()
    {
        if( mProps.IsMutable() ) {
            mProps.MakeConst();
        }
        return *this;
    }

    /// Convenience, assigns a Bool value. Types must match.
    /// \note: explicit named with Bool because many types will convert to bool by accident, e.g., char *
    /// \throw May throw exception::type_missmatch/const_assign
    void AssignBoolValue( Bool const b, SourceLocation const &rLoc = {} )
    {
        if( not mpType->IsSame<Bool>() ) {
            throw exception::type_mismatch( rLoc );
        }
        if( mProps.IsConst() ) {
            throw exception::const_assign( rLoc );
        }
        *mpValue = b;
    }

    /// Convenience, assigns an Integer value. Types must match.
    /// \throw May throw exception::type_missmatch/const_assign
    void AssignValue( Integer const i, SourceLocation const &rLoc = {} )
    {
        if( not mpType->IsSame<Integer>() ) {
            throw exception::type_mismatch( rLoc );
        }
        if( mProps.IsConst() ) {
            throw exception::const_assign( rLoc );
        }
        *mpValue = i;
    }

    /// Convenience, assigns an U64 value. Types must match.
    /// \throw May throw exception::type_missmatch/const_assign
    void AssignValue( U64 const u, SourceLocation const &rLoc = {} )
    {
        if( not mpType->IsSame<U64>() ) {
            throw exception::type_mismatch( rLoc );
        }
        if( mProps.IsConst() ) {
            throw exception::const_assign( rLoc );
        }
        *mpValue = u;
    }

    /// Convenience, assigns an U8 value. Types must match.
    /// \throw May throw exception::type_missmatch/const_assign
    void AssignValue( U8 const u, SourceLocation const &rLoc = {} )
    {
        if( not mpType->IsSame<U8>() ) {
            throw exception::type_mismatch( rLoc );
        }
        if( mProps.IsConst() ) {
            throw exception::const_assign( rLoc );
        }
        *mpValue = u;
    }

    /// Convenience, assigns a Decimal value. Types must match.
    /// \throw May throw exception::type_missmatch/const_assign
    void AssignValue( Decimal const d, SourceLocation const &rLoc = {} )
    {
        if( not mpType->IsSame<Decimal>() ) {
            throw exception::type_mismatch( rLoc );
        }
        if( mProps.IsConst() ) {
            throw exception::const_assign( rLoc );
        }
        *mpValue = d;
    }

    /// Convenience, assigns a String value. Types must match.
    /// \throw May throw exception::type_missmatch/const_assign
    void AssignValue( String const &s, SourceLocation const &rLoc = {} )
    {
        if( not mpType->IsSame<String>() ) {
            throw exception::type_mismatch( rLoc );
        }
        if( mProps.IsConst() ) {
            throw exception::const_assign( rLoc );
        }
        *mpValue = s;
    }

    /// Convenience, assigns a String value. Types must match.
    /// \throw May throw exception::type_missmatch/const_assign
    void AssignValue( String  && s, SourceLocation const &rLoc = {} )
    {
        if( not mpType->IsSame<String>() ) {
            throw exception::type_mismatch( rLoc );
        }
        if( mProps.IsConst() ) {
            throw exception::const_assign( rLoc );
        }
        *mpValue = std::move(s);
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
        if( pT1->IsSame<Tuple>() && rOther.ShareCount() > 1 ) {
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
            return mpType->IsSame<TypeInfo>();
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
            []( NotAValue ) -> bool { throw exception::bad_value_cast( "ValueObject is NaV (Not A Value)!" ); },
            []( bool b ) { return b; },
            []( U8 const u ) { return static_cast<bool>(u); },
            []( I64 const i ) { return static_cast<bool>(i); },
            []( U64 const u ) { return static_cast<bool>(u); },
            []( F64 const d ) { return static_cast<bool>(d); },
            []( std::string const &rStr ) { return !rStr.empty(); },
            []( Tuple const &rTuple ) { return rTuple.Size() > 0u; },
            []( Buffer const &rBuffer ) { return not rBuffer.empty(); }
        }, *mpValue );
    }

    /// Converts the value to Integer if possible, otherwise throws exception::bad_value_cast
    Integer GetAsInteger() const
    {
        return std::visit( overloaded{
            []( auto ) -> Integer { throw exception::bad_value_cast( "ValueObject not convertible to Integer!" ); },
            []( NotAValue ) -> Integer { throw exception::bad_value_cast( "ValueObject is NaV (Not A Value)!" ); },
            []( bool const b ) { return static_cast<Integer>(b); },
            []( U8 const u ) { return static_cast<Integer>(u); },
            []( I64 const i ) { return static_cast<Integer>(i); },
            []( U64 const u ) { return u <= static_cast<U64>(std::numeric_limits<I64>::max()) ? static_cast<Integer>(u) : (throw exception::bad_value_cast( "ValueObject with u64 not convertible to Integer!" ), Integer{}); },
            []( F64 const d ) { return std::isfinite( d ) ? static_cast<Integer>(d) : (throw exception::bad_value_cast( "ValueObject with f64 not convertible to Integer!" ), Integer{}); },
            []( std::string const &rStr ) { try { return static_cast<Integer>(std::stoll( rStr )); } catch( ... ) { throw exception::bad_value_cast( "ValueObject with String not convertible to Integer!" ); } }
        }, *mpValue );
    }

    /// Converts the value to std::string if possible, otherwise throws exception::bad_value_cast
    /// \note unlike \see PrintValue the function will throw if value is NaV or any!
    std::string GetAsString() const
    {
        return std::visit( overloaded{
            []( auto ) -> std::string { throw exception::bad_value_cast( "ValueObject not convertible to string!" ); },
            []( std::any const &a ) {
                if( auto const *p_type = std::any_cast<TypeInfo>(&a); p_type != nullptr ) {
                    return p_type->GetName();
                } else throw exception::bad_value_cast( "ValueObject not convertible to string!" ); },
            []( NotAValue ) -> std::string { throw exception::bad_value_cast( "ValueObject is NaV (Not A Value)!" ); },
            []( bool const b ) { return std::string( b ? "true" : "false" ); },
            []( U8 const u ) { return to_string( u ); },
            []( I64 const i ) { return to_string( i ); },
            []( U64 const u ) { return to_string( u ); },
            []( F64 const d ) { return to_string( d ); },
            []( std::string const &rStr ) { return rStr; },
            []( Tuple const &rTuple ) { return print_tuple( rTuple ); },
            []( Buffer const &rBuffer ) { return print_buffer( rBuffer, rBuffer.size() ); },
            []( IntegerSequence const &rSeq ) { return seq::print( rSeq ); } // TODO: check if a Sequence should really be convertible to String? Remove (PrintValue is sufficiend then).
        }, *mpValue );
    }

    /// Returns a std::string for printing information of the value.
    /// \note in contrast to \see GetAsString this function will always produce a printable result (if the std functions dont throw).
    std::string PrintValue() const
    {
        return std::visit( overloaded{
            []( auto ) { return std::string("<not printable>"); },
            []( std::any const &a ) { 
                if( auto const *p_type = std::any_cast<TypeInfo>(&a); p_type != nullptr ) {
                    return p_type->GetName();
                } else return std::string( "<not printable>" ); },
            []( NotAValue ) { return std::string( "NaV (Not A Value)" ); },
            []( bool const b ) { return std::string( b ? "true" : "false" ); },
            []( U8 const u ) { return to_string( u ); },
            []( I64 const i ) { return to_string( i ); },
            []( U64 const u ) { return to_string( u ); },
            []( F64 const d ) { return to_string( d ); },
            []( std::string const &rStr ) { return "\"" + rStr + "\""; },
            []( Tuple const &rTuple ) { return print_tuple( rTuple ); },
            []( Buffer const &rBuffer ) { return print_buffer( rBuffer, 100 ); },
            []( IntegerSequence const &rSeq ) { return seq::print( rSeq ); }
        }, *mpValue );
    }


    /// Visit function for obtaining the inner value by a visitor.
    template< typename Visitor >
    auto Visit( Visitor &&v ) const
    {
        return std::visit( std::forward<Visitor>( v ), *mpValue );
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
};

} // namespace teascript


#include "Number.hpp"

namespace teascript {


/// Returns the order of the 2 ValueObjects.
inline std::strong_ordering operator<=>( teascript::ValueObject const &lhs, teascript::ValueObject const &rhs )
{
    using namespace teascript;

    if( lhs.GetTypeInfo()->IsNaV() && rhs.GetTypeInfo()->IsNaV() ) { // NOTE: Don't use InternalType() here. It might be set to NaV while the set TypeInfo is a different one.
        return std::strong_ordering::equal;
    }

    // compare if rhs is NaV
    if( lhs.GetTypeInfo()->IsNaV() ) {
        if( rhs.InternalType() == ValueObject::TypeNaV ) {
            return std::strong_ordering::equal;
        } else { // NaV is always smaller
            return std::strong_ordering::less;
        }
    }

    // compare if lhs is NaV
    if( rhs.GetTypeInfo()->IsNaV() ) {
        if( lhs.InternalType() == ValueObject::TypeNaV ) {
            return std::strong_ordering::equal;
        } else { // NaV is always smaller
            return std::strong_ordering::greater;
        }
    }

    // same types but one (or both) is NaV?
    if( lhs.GetTypeInfo()->IsSame( *rhs.GetTypeInfo() ) ) {
        bool const lnav = lhs.InternalType() == ValueObject::TypeNaV;
        bool const rnav = rhs.InternalType() == ValueObject::TypeNaV;
        if( lnav && rnav ) {
            return std::strong_ordering::equal;
        } else if( lnav ) {
            return std::strong_ordering::less;
        } else if( rnav ) {
            return std::strong_ordering::greater;
        }
    }

    if( lhs.InternalType() == ValueObject::TypeF64 || rhs.InternalType() == ValueObject::TypeF64 ) {
        F64 const *pd_lhs = lhs.GetValuePtr<F64>();
        F64 const *pd_rhs = rhs.GetValuePtr<F64>();
        F64 const d1 = pd_lhs != nullptr ? *pd_lhs : util::ArithmeticFactory::Convert<F64>( lhs ).GetValue<F64>();
        //F64 const d1 = pd_lhs != nullptr ? *pd_lhs : static_cast<F64>( lhs.GetAsInteger() );
        F64 const d2 = pd_rhs != nullptr ? *pd_rhs : util::ArithmeticFactory::Convert<F64>( rhs ).GetValue<F64>();
        //F64 const d2 = pd_rhs != nullptr ? *pd_rhs : static_cast<F64>(rhs.GetAsInteger());
#if _MSC_VER || _LIBCPP_VERSION || (!defined(__clang__) && __GNUC__ >= 13 ) // libc++14 works here, libstdc++ of g++11 not, but g++13
        return std::strong_order( d1, d2 ); //TODO: check this! we could bail out on e.g., NaN instead or change return value to partial_odering.
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

    // the values will be converted to an arithmetic value for comparison if any of the values is an arithmetic value.
    // NOTE: In C++ bool is also converted to an integer when the bool is compared with an integer!
    if( lhs.GetTypeInfo()->IsArithmetic() || rhs.GetTypeInfo()->IsArithmetic() ) {
        return util::ArithmeticFactory::Compare( lhs, rhs );
    }

    // otherwise the values will be converted to bool if any of the values is a bool.
    if( lhs.InternalType() == ValueObject::TypeBool || rhs.InternalType() == ValueObject::TypeBool ) {
        return lhs.GetAsBool() <=> rhs.GetAsBool();
    }

    // otherwise the values will be converted to a string if any of the values is a string.
    if( lhs.InternalType() == ValueObject::TypeString || rhs.InternalType() == ValueObject::TypeString ) {
#if !_LIBCPP_VERSION // libc++14 fails here
        return lhs.GetAsString() <=> rhs.GetAsString();
#else
        auto const res = lhs.GetAsString().compare( rhs.GetAsString() );
        return res < 0 ? std::strong_ordering::less : res > 0 ? std::strong_ordering::greater : std::strong_ordering::equal;
#endif
    }

    if( lhs.GetTypeInfo()->IsSame<Tuple>() && rhs.GetTypeInfo()->IsSame<Tuple>() ) {
        return tuple::compare_values( lhs.GetValue<Tuple>(), rhs.GetValue<Tuple>() );
    }

    if( lhs.GetTypeInfo()->IsSame<Buffer>() && rhs.GetTypeInfo()->IsSame<Buffer>() ) {
#if !_LIBCPP_VERSION // libc++14 fails here
        return lhs.GetValue<Buffer>() <=> rhs.GetValue<Buffer>();
#else
        auto const &buf1 = lhs.GetValue<Buffer>();
        auto const &buf2 = rhs.GetValue<Buffer>();
        if( buf1.size() != buf2.size() ) {
            return buf1.size() <=> buf2.size();
        } else {
            auto const res = ::memcmp( buf1.data(), buf2.data(), buf1.size() );
            return res < 0 ? std::strong_ordering::less : res > 0 ? std::strong_ordering::greater : std::strong_ordering::equal;
        }
#endif 
    }

    if( lhs.GetTypeInfo()->IsSame<TypeInfo>() && rhs.GetTypeInfo()->IsSame<TypeInfo>() ) {
#if !_LIBCPP_VERSION // libc++14 fails here
        return lhs.GetValue<TypeInfo>().ToTypeIndex() <=> rhs.GetValue<TypeInfo>().ToTypeIndex();
#else
        auto const i1 = lhs.GetValue<TypeInfo>().ToTypeIndex();
        auto const i2 = rhs.GetValue<TypeInfo>().ToTypeIndex();
        return i1 < i2 ? std::strong_ordering::less : i2 < i1 ? std::strong_ordering::greater : std::strong_ordering::equal;
#endif
    }

    // unequal types are usually not comparable.
    throw exception::bad_value_cast( "types do not match for comparison!" );
}

/// Returns whether the 2 ValueObjects are equal.
inline bool operator==( teascript::ValueObject const &lhs, teascript::ValueObject const &rhs )
{
    auto const res = lhs <=> rhs;
    return res == std::strong_ordering::equal || res == std::strong_ordering::equivalent;
}

/// Returns whether the 2 ValueObjects are unequal.
inline bool operator!=( teascript::ValueObject const &lhs, teascript::ValueObject const &rhs )
{
    return !(lhs == rhs);
}

} // namespace teascript

#include "TupleUtil.hpp"
