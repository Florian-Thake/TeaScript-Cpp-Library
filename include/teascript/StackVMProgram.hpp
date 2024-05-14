/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "ConfigEnums.hpp"
#include "StackVMInstructions.hpp"
#include "SourceLocation.hpp"

#include <vector>
#include <span>
#include <map>
#include <memory>
#include <filesystem>
#include <cstdio>


namespace teascript {

namespace StackVM {

using InstrContainer = std::vector< Instruction >;

using DebugInfo      = std::map<std::size_t, SourceLocation>;

class Program;
using ProgramPtr = std::shared_ptr< Program >;


/// A Program for the TeaStackVM
class Program
{
    std::string  mName;
    eOptimize    mUsedOptimization;
    unsigned int mCompilerVersion;

    InstrContainer            mInstructions;
    DebugInfo                 mDebugInfo;

public:
    /// Constructs the program with given name and instructions.
    Program( std::string const &rName, eOptimize const opt, unsigned int compiler_version, InstrContainer &&instr, DebugInfo &&d )
        : mName( rName )
        , mUsedOptimization( opt )
        , mCompilerVersion( compiler_version )
        , mInstructions( std::move(instr) )
        , mDebugInfo( std::move(d) )
    {
    }

    /// \returns the name of the program.
    std::string const &GetName() const
    {
        return mName;
    }

    /// \returns the used optimizatin level for compiling the program.
    eOptimize GetUsedOptimization() const
    {
        return mUsedOptimization;
    }

    /// \returns the (combined) version numer of the compiler (i.e. the TeaScript version). \see version.h
    unsigned int GetCompilerVersion() const
    {
        return mCompilerVersion;
    }

    /// \returns the container with the instructions of the program.
    InstrContainer const &GetInstructions() const
    {
        return mInstructions;
    }

    /// \returns whether there is debug info at all.
    bool IsDebugInfoPresent() const noexcept
    {
        return not mDebugInfo.empty();
    }

    /// \returns whether a debug info for the exact given instruction number is present.
    bool HasDebugInfoFor( size_t const instr ) const noexcept
    {
        return mDebugInfo.contains( instr );
    }

    /// \returns the SourceLocation of the exact given instruction number or an empty one if not exist.
    SourceLocation GetSourceLocationFor( size_t const instr ) const noexcept
    {
        auto const it = mDebugInfo.find( instr );
        return it != mDebugInfo.end() ? it->second : SourceLocation();
    }

    /// \tries to return the best matching debug info for a given instruction number.
    SourceLocation GetBestMatchingSourceLocationFor( size_t const instr ) const noexcept
    {
        if( instr > mInstructions.size() ) { // == is program end, > is illegal address.
            return SourceLocation();
        }
        auto  it = mDebugInfo.lower_bound( instr );
        if( it == mDebugInfo.end() ) {
            if( mDebugInfo.empty() ) {
                return SourceLocation();
            } else {
                return mDebugInfo.rbegin()->second; // most likely this is wrong, but it might be a hint at least.
            }
        }
        if( it->first == instr ) { // bulls eye!
            return it->second;
        }
        // try one prior it.
        // FIXME: depending on the instruction it might be better to return the current one or maybe even the next.
        if( it != mDebugInfo.begin() ) {
            --it;
            return it->second;
        }
        return it->second; // better than nothing.
    }

    /// loads a TeaStackVM Program (usually a .tsb file) from disk. The returned pointer is always valid.
    /// Use header_only = true for only read the header information.
    /// \throws exception::runtime_error or exceptio::load_file_error on error.
    static ProgramPtr Load( std::filesystem::path const &rPathAndName, bool const header_only = false )
    {
        auto const filename = std::filesystem::absolute( rPathAndName );
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996) // if you get an error here use /w34996 or disable sdl checks
#endif
        auto fp = fopen( filename.string().c_str(), "rb" );
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
        if( !fp ) {
            throw exception::load_file_error( filename.string() );
        }

        struct FClose
        {
            FILE *mfp;
            FClose( FILE *fp ) : mfp( fp ) {}
            ~FClose() { fclose( mfp ); }
        };
        FClose const f( fp );

        fseek( fp, 0, SEEK_END ); // seek to end
        size_t const size = ftell( fp );
        fseek( fp, 0, SEEK_SET ); // seek to start

        if( header_only ) {
            // basic header                      .tsb        magic            compiler version         opt level
            constexpr size_t basic_header_size = 4 + sizeof( unsigned int ) + sizeof( unsigned int ) + sizeof( eOptimize );
            // + name_len
            constexpr size_t header_and_name_len = basic_header_size + sizeof( size_t );
            if( size >= header_and_name_len ) {
                // first we read up to the name_len
                unsigned char tmp[header_and_name_len] = {0};
                if( fread( &tmp, 1, header_and_name_len, fp ) == header_and_name_len ) {
                    size_t name_len = 0;
                    ::memcpy( &name_len, &tmp[basic_header_size], sizeof( name_len ) );
                    // sanity for too big values.
                    if( name_len > SHRT_MAX ) {
                        throw std::runtime_error( "Program::Load(): unexpected big value for program name length!" );
                    }
                    std::vector<unsigned char> buf( header_and_name_len + name_len );
                    ::memcpy( buf.data(), &tmp, header_and_name_len );
                    // then read the complete name.
                    if( fread( buf.data()+header_and_name_len, 1, name_len, fp) == name_len ) {
                        return Load( std::span( buf.data(), buf.size() ), true );
                    }
                }

                // on any unexpected behavior we must seek to beginning again!
                fseek( fp, 0, SEEK_SET ); // seek to start
            } // else: just go the normal path below (which will throw a correct errror, because file is too small)
        }

        std::vector<unsigned char> buf( size );
        auto const read = fread( buf.data(), 1, size, fp );
        if( read != size ) {
            throw std::runtime_error( "Program::Load(): error during read file!" );
        }
        return Load( std::span(buf.data(), buf.size()), header_only );
    }


    /// Interprets the given data as a program for the TeaStackVM. The returned pointer is always valid.
    /// Use header_only = true for only read the header information.
    /// \throws exception::runtime_error on error.
    static ProgramPtr Load( std::span<unsigned char> const &content, bool const header_only = false )
    {
        unsigned char const *p = content.data();
        unsigned char const *const end = p + content.size();
        
        bool ok = true;
        // header with magic number.         .tsb        magic            compiler version         opt level
        constexpr size_t basic_header_size = 4 + sizeof( unsigned int ) + sizeof( unsigned int ) + sizeof( eOptimize );

        if( p + basic_header_size > end ) {
            throw std::runtime_error( "Program::Load(): file too small for header!" );
        }
        
        if( p[0] != '.' || p[1] != 't' || p[2] != 's' || p[3] != 'b' ) {
            throw std::runtime_error( "Program::Load(): not a .tsb file!" );
        }
        unsigned int magic = 0;
        ::memcpy( &magic, &p[4], sizeof( magic ) );
        if( magic != 0xcafe07ea ) {
            throw std::runtime_error( "Program::Load(): wrong magic number!" );
        }

        unsigned int version = 0;
        ::memcpy( &version, &p[4+sizeof(unsigned int)], sizeof(version));
        // we don't check the version here intentionally (this allows (to at least try) loading of any produced .tsb although it might be impossible to execute).

        eOptimize opt_level = eOptimize::O0;
        ::memcpy( &opt_level, &p[4 + sizeof( unsigned int ) + sizeof( unsigned int )], sizeof( opt_level ) );

        if( opt_level > eOptimize::O2 ) {
            throw std::runtime_error( "Program::Load(): unknown optimization level!" );
        }

        p += basic_header_size;

        // name
        size_t name_len = 0;
        ok = ok && p + sizeof( name_len ) <= end;
        if( ok ) {
            ::memcpy( &name_len, p, sizeof( name_len ) );
            p += sizeof( name_len );
            // sanity for too big values.
            if( name_len > SHRT_MAX ) {
                throw std::runtime_error( "Program::Load(): unexpected big value for program name length!" );
            }
        }
        
        std::string name( name_len, '\0' );
        ok = ok && p + name_len <= end;
        if( ok ) {
            ::memcpy( name.data(), p, name_len );
            p += name_len;
        }

        if( not ok ) {
            throw std::runtime_error( "Program::Load(): file too small for name!" );
        }

        DebugInfo      debug_info;
        InstrContainer instructions;

        if( header_only ) {
            return std::make_shared<Program>( name, opt_level, version, std::move(instructions), std::move(debug_info) );
        }

        size_t need = 0;
        ok = ok && p + sizeof( need ) <= end;
        if( ok ) {
            ::memcpy( &need, p, sizeof( need ) );
            p += sizeof( need );
            // sanity for too big values (300 MiB / 112 Bytes)
            if( need > ((300ull << 20) / sizeof( Instruction )) ) { // Question from the author in the year 2024: Will we ever see a TeaScript binary bigger than 300 MiB ???
                throw std::runtime_error( "Program::Load(): unexpected big value for program instruction count!" );
            }
        }

        constexpr size_t it_size = sizeof( eTSVM_Instr ) + sizeof( ValueObject::eType );
        size_t count = 0;
        while( ok && count < need && p < end ) {

            if( p + it_size > end ) {
                ok = false;
                break;
            }

            eTSVM_Instr ins = eTSVM_Instr::HALT;
            ::memcpy( &ins, p, sizeof( eTSVM_Instr ) );

            ValueObject::eType type = ValueObject::TypeNaV;
            ::memcpy( &type, &p[sizeof( eTSVM_Instr )], sizeof(type) );

            p += it_size;

            switch( type ) {
            case ValueObject::TypeNaV:
                instructions.emplace_back( ins, ValueObject() );
                break;
            case ValueObject::TypeBool:
                {
                    Bool v = false;
                    ok = ok && p + sizeof( v ) <= end;
                    if( ok ) {
                        ::memcpy( &v, p, sizeof( v ) );
                        p += sizeof( v );
                        instructions.emplace_back( ins, ValueObject( v ) );
                    }
                }
                break;
            case ValueObject::TypeU8:
                {
                    U8 v = 0;
                    ok = ok && p + sizeof( v ) <= end;
                    if( ok ) {
                        ::memcpy( &v, p, sizeof( v ) );
                        p += sizeof( v );
                        instructions.emplace_back( ins, ValueObject( v ) );
                    }
                }
                break;
            case ValueObject::TypeI64:
                {
                    I64 v = 0;
                    ok = ok && p + sizeof( v ) <= end;
                    if( ok ) {
                        ::memcpy( &v, p, sizeof( v ) );
                        p += sizeof( v );
                        instructions.emplace_back( ins, ValueObject( v ) );
                    }
                }
                break;
            case ValueObject::TypeU64:
                {
                    U64 v = 0;
                    ok = ok && p + sizeof( v ) <= end;
                    if( ok ) {
                        ::memcpy( &v, p, sizeof( v ) );
                        p += sizeof( v );
                        instructions.emplace_back( ins, ValueObject( v ) );
                    }
                }
                break;
            case ValueObject::TypeF64:
                {
                    F64 v = 0.0;
                    ok = ok && p + sizeof( v ) <= end;
                    if( ok ) {
                        ::memcpy( &v, p, sizeof( v ) );
                        p += sizeof( v );
                        instructions.emplace_back( ins, ValueObject( v ) );
                    }
                }
                break;
            case ValueObject::TypeString:
                {
                    size_t len = 0;
                    ok = ok && p + sizeof( len ) <= end;
                    if( ok ) {
                        ::memcpy( &len, p, sizeof( len ) );
                        p += sizeof( len );
                    }
                    // sanity, for now we reject on > 10 MiB strings!
                    if( len > (10ull << 20) ) {
                        throw std::runtime_error( "Program::Load(): unexpected big value for payload string length!" );
                    }
                    std::string str( len, '\0' );
                    ok = ok && p + len <= end;
                    if( ok ) {
                        ::memcpy( str.data(), p, len );
                        p += len;
                        instructions.emplace_back( ins, ValueObject( std::move( str ) ) );
                    }
                }
                break;
            default:
                ok = false;
            }

            ++count;
        }

        if( not ok || count != need || p != end ) {
            throw std::runtime_error( "Program::Load(): malformed or read error!" );
        }

        return std::make_shared<Program>( name, opt_level, version, std::move( instructions ), std::move( debug_info ) );
    }

    /// Saves the TeaStackVM Program as file.
    /// \note The saved TeaScript Binary (.tsb) is not meant to be used on other systems.
    ///       It is only valid on the same system with the same version of TeaScript!
    bool Save( std::filesystem::path const &rPathAndName )
    {
        auto const filename = std::filesystem::absolute( rPathAndName );
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996) // if you get an error here use /w34996 or disable sdl checks
#endif
        auto fp = fopen( filename.string().c_str(), "wb" ); // we always overwrite!
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
        if( !fp ) {
            return false;
        }

        // we are always writing in host byte order (.tsb files are not meant to be interchangeable across systems.)

        bool ok = true;
        // header with magic number.
        ok = ok && fwrite( ".tsb", 1, 4, fp ) == 4;
        unsigned int const magic = 0xcafe07ea;
        ok = ok && fwrite( &magic, sizeof(magic), 1, fp ) == 1;

        ok = ok && fwrite( &mCompilerVersion, sizeof(mCompilerVersion), 1, fp ) == 1;
        ok = ok && fwrite( &mUsedOptimization, sizeof(mUsedOptimization), 1, fp ) == 1;
        auto const name_len = mName.size();
        ok = ok && fwrite( &name_len, sizeof( name_len ), 1, fp ) == 1;
        ok = ok && fwrite( mName.data(), 1, name_len, fp) == name_len;

        // header done, now the instructions.
        // for now we always write instr, type, value, except for type==NaV where value is omitted.

        // first the amount
        auto const s = mInstructions.size();
        ok = ok && fwrite( &s, sizeof( s ), 1, fp ) == 1;

        for( auto const &cur : mInstructions ) {
            if( not ok ) break;

            ok = ok && fwrite( &cur.instr, sizeof( cur.instr ), 1, fp ) == 1;
            auto const type = cur.payload.InternalType();
            ok = ok && fwrite( &type, sizeof( type ), 1, fp ) == 1;
            switch( type ) {
            case ValueObject::TypeNaV: break;
            case ValueObject::TypeBool:
                {
                    Bool const v = cur.payload.GetValue<Bool>();
                    ok = ok && fwrite( &v, sizeof( v ), 1, fp ) == 1;
                }
                break;
            case ValueObject::TypeU8:
                {
                    U8 const v = cur.payload.GetValue<U8>();
                    ok = ok && fwrite( &v, sizeof( v ), 1, fp ) == 1;
                }
                break;
            case ValueObject::TypeI64:
                {
                    I64 const v = cur.payload.GetValue<I64>();
                    ok = ok && fwrite( &v, sizeof( v ), 1, fp ) == 1;
                }
                break;
            case ValueObject::TypeU64:
                {
                    U64 const v = cur.payload.GetValue<U64>();
                    ok = ok && fwrite( &v, sizeof( v ), 1, fp ) == 1;
                }
                break;
            case ValueObject::TypeF64:
                {
                    F64 const v = cur.payload.GetValue<F64>();
                    ok = ok && fwrite( &v, sizeof( v ), 1, fp ) == 1;
                }
                break;
            case ValueObject::TypeString:
                {
                    String const &str = cur.payload.GetValue<String>();
                    auto const str_len = str.size();
                    ok = ok && fwrite( &str_len, sizeof( str_len ), 1, fp ) == 1;
                    ok = ok && fwrite( str.data(), 1, str_len, fp ) == str_len;
                }
                break;
            default:
                ok = false;
            }
        }

        // done (debug infos are not saved (in same file))

        if( not ok ) {
            fclose( fp );
            ::remove( filename.string().c_str() );
        }

        return ok;
    }

};

} // namespace StackVM

} // namesapce teascript

