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
#include "Context.hpp"
#include "Type.hpp"
#include "Error.hpp"
#include "TupleUtil.hpp"
#include "IntegerSequence.hpp"
#include "TomlSupport.hpp"
#include "JsonSupport.hpp"
#include "Print.hpp"
#include "Parser.hpp"
#include "StackVMCompiler.hpp"
#include "StackMachine.hpp"
#include "LibraryFunctions.hpp"
#include "version.h"


#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <random>
#include <thread>
#include <algorithm>


namespace teascript {


/// The CoreLibrary of TeaScript providing core functionality for the scripts.
class CoreLibrary
{
    // The Core Library C++ API - now public
public:
    // API History:
    // 0   - (since 0.6  ): without Error
    // 1   - (since 0.16 ): changed error return from Bool(false) to Error for all functions which don't return a Bool on success
    static constexpr long long API_Version = 1LL;

    static void PrintVersion()
    {
        TEASCRIPT_PRINT( "{} {}.{}.{}\n", self_name_str(), version::get_major(), version::get_minor(), version::get_patch() );
    }

    static void PrintCopyright()
    {
        TEASCRIPT_PRINT( "{}\n", copyright_info() );
    }

    static void PrintStdOut( std::string const &rStr )
    {
        TEASCRIPT_PRINT( "{}", rStr );
    }

    static void PrintStdError( std::string const &rStr )
    {
        TEASCRIPT_ERROR( "{}", rStr );
    }

    // colored printing is only possible when libfmt is used.
#if TEASCRIPT_FMTFORMAT
    static void PrintColored( long long const rgb, ValueObject const &rStr )
    {
        if( rgb < 0 || rgb > 0xFF'FF'FF ) {
            throw exception::out_of_range( "PrintColored: rgb < 0 || rgb > 0xFF'FF'FF" );
        }
        fmt::print( fmt::fg( static_cast<fmt::color>(static_cast<unsigned int>(rgb)) ), "{}", rStr.GetAsString() );
    }
#endif

    static std::string ReadLine()
    {
        std::string line;
        std::getline( std::cin, line );
        if( line.ends_with( '\r' ) ) { // temporary fix for Windows Unicode support hook up.
            line.erase( line.size() - 1 );
        }
        return line;
    }

    static Error MakeRuntimeError( std::string const &rMessage )
    {
        return Error( eError::RuntimeError, rMessage );
    }

    static long long ErrorGetCode( Error const &rError )
    {
        return static_cast<long long>(rError.Code());
    }

    static std::string ErrorGetName( Error const &rError )
    {
        return rError.Name();
    }

    static std::string ErrorGetMessage( Error const &rError )
    {
        return rError.Message();
    }

    static double Sqrt( double const d )
    {
        return std::sqrt( d );
    }

    static double Trunc( double const d )
    {
        return std::trunc( d );
    }

    static ValueObject StrToNum( std::string  const &rStr )
    {
        try {
            return ValueObject( ValueObject( rStr, false ).GetAsInteger() ); // ensure same conversion routine is used.
        } catch( ... ) {
            //return ValueObject( false ); // First attempt of error handling.
            return ValueObject( Error::MakeRuntimeError( "Could not convert to Integer!" ) );
        }
    }

    /// Converts string to either f64, u8, u64 or i64 (or an Error)
    static ValueObject StrToNumEx( std::string  const &rStr )
    {
        try {
            Content content( rStr );
            Parser::SkipWhitespace( content );
            Parser  p;
            if( p.Num( content ) ) {
                auto ast = p.GetLastToplevelASTNode();
                if( ast.get() != nullptr ) {
                    Context dummy;
                    return ast->Eval( dummy );
                }
            }
        } catch( ... ) {
        }
        //return ValueObject( false ); // First attempt of error handling.
        return ValueObject( Error::MakeRuntimeError( "Could not convert to Number!" ) );
    }


    static std::string NumToStr( long long const num )
    {
        return std::to_string( num );
    }

    static long long StrLength( std::string const &rStr ) // in bytes.
    {
        return static_cast<long long>(rStr.length());
    }

    static long long StrUTF8GlyphCount( std::string const &rStr ) // in glyphs
    {
        return static_cast<long long>(util::utf8_string_length( rStr ));
    }

    /// \returns the byte position of glyph \param glyph in rStr. rStr must be a valid UTF-8 encoded string.
    static long long StrUTF8GlyphToBytePos( std::string const &rStr, long long const glyph )
    {
        return static_cast<long long>(util::utf8_glyph_to_byte_pos( rStr, glyph ));
    }

    /// \returns a complete UTF-8 code point as std::string where \param at poitns into or an empty string when out of range.
    static std::string StrAt( std::string const &rStr, long long const at )
    {
        if( at < 0 || static_cast<size_t>(at) >= rStr.length() ) {
            return {};
        }
        // we must always return one complete valid utf-8 code point.
        // NOTE: this algorithm assumes that the string is a valid UTF-8 encoded string.
        auto         start = static_cast<std::size_t>(at);
        std::size_t  count = 1;
        // first we check if we peek in a middle of an utf-8 code point sequecne. Then we must search the start.
        if( (static_cast<unsigned char>(rStr[static_cast<std::size_t>(at)]) & 0xC0) == 0x80 ) {
            // find the start
            while( (static_cast<unsigned char>(rStr[start]) & 0xC0) == 0x80 && start > 0 ) {
                --start;
            }
        }
        // here we have the start
        // do we need to find an end?
        if( static_cast<unsigned char>(rStr[start]) > 0xC1 ) {
            auto cur = start + 1;
            while( cur < rStr.length() && (static_cast<unsigned char>(rStr[cur]) & 0xC0) == 0x80 ) {
                ++count;
                ++cur;
            }
        }

        return rStr.substr( start, count );
    }

    /// \returns a substring of \param rStr for the range [from, from+count). 
    /// If the range does not form a complete UTF-8 range or is out of range an empty string will be returned.
    static std::string SubStr( std::string const &rStr, long long const from, long long const count )
    {
        if( from < 0 || static_cast<size_t>(from) >= rStr.length() || count < -1 ) { // -1 == npos == until end of string
            return {};
        }
        if( not util::is_complete_utf8_range( rStr, static_cast<std::size_t>(from), static_cast<std::size_t>(count) ) ) {
            return {};
        }
        return rStr.substr( static_cast<size_t>(from), static_cast<size_t>(count) );
    }

    static long long StrFind( std::string const &rStr, std::string const &rToFind, long long const off )
    {
        auto const  res = rToFind.empty() ? std::string::npos : rStr.find( rToFind, static_cast<size_t>(off) );
        return res == std::string::npos ? -1LL : static_cast<long long>(res);
    }

    static long long StrReverseFind( std::string const &rStr, std::string const &rToFind, long long const off )
    {
        auto const  res = rToFind.empty() ? std::string::npos : rStr.rfind( rToFind, static_cast<size_t>(off) );
        return res == std::string::npos ? -1LL : static_cast<long long>(res);
    }

    /// replaces the range [start, start+count) in /param rStr with /param rNew.
    /// If the range does not form a complete UTF-8 range or is out of range nothing will happen and the function will return false.
    static bool StrReplacePos( std::string &rStr, long long const start, long long const count, std::string const &rNew )
    {
        if( start < 0 || static_cast<size_t>(start) >= rStr.length() || count < -1 ) { // -1 == npos == until end of string
            return false;
        }
        if( not util::is_complete_utf8_range( rStr, static_cast<std::size_t>(start), static_cast<std::size_t>(count) ) ) {
            return false;
        }
        try {
            rStr.replace( static_cast<size_t>(start), static_cast<size_t>(count), rNew );
            return true;
        } catch( ... ) {
            return false;
        }
    }

    static ValueObject StrFromAscii( ValueObject const &val )
    {
        if( not val.GetTypeInfo()->IsArithmetic() ) {
            return ValueObject( MakeRuntimeError( "Not a number!" ), ValueConfig( ValueUnshared, ValueMutable ) );
        }
        try {
            auto const v = val.GetAsInteger();
            if( v >= 0 && v < 128 ) {
                return ValueObject( std::string( 1, static_cast<char>(v) ) );
            }
        } catch( ... ) {
        }
        return ValueObject( MakeRuntimeError( "Not in ASCII range!" ), ValueConfig( ValueUnshared, ValueMutable ) );
    }

    /// gets the local wall clock time in fractional seconds.
    static double GetLocalTimeInSecs()
    {
#if _MSC_VER || (!defined(__clang__) && __GNUC__ >= 13 ) // TODO: _LIBCPP_VERSION
        std::chrono::zoned_time const  now = {std::chrono::current_zone(), std::chrono::system_clock::now()};
        std::chrono::duration<double> const  timesecs = now.get_local_time() - std::chrono::floor<std::chrono::days>( now.get_local_time() );
        return timesecs.count();
#else
        auto const now = std::chrono::system_clock::now(); // UTC without(!) leap seconds.
        // get the fractional seconds first
        std::chrono::duration<double> const  timesecs = now - std::chrono::floor<std::chrono::days>( now );
        double const frac = timesecs.count() - static_cast<double>(static_cast<long long>(timesecs.count()));

        // convert to local time.
        auto const t = std::chrono::system_clock::to_time_t( now );
        struct tm  tconv {};
        (void) ::localtime_r( &t, &tconv );

        // build seconds
        return static_cast<double>(tconv.tm_hour * 60 * 60 + tconv.tm_min * 60 + tconv.tm_sec) + frac;
#endif
    }

    /// gets the UTC time in fractional seconds.
    static double GetUTCTimeInSecs()
    {
        // NOTE: std::chrono::utc_clock::now() is with(!) leap seconds. If use this for time representation it will be in the 'future' of the system clock!
        auto const now = std::chrono::system_clock::now(); // UTC without(!) leap seconds.
        std::chrono::duration<double> const  timesecs = now - std::chrono::floor<std::chrono::days>( now );
        return timesecs.count();
    }

    /// gets a time stamp since first call to this function in fractional seconds.
    static double GetTimeStamp()
    {
        static auto const start = std::chrono::steady_clock::now(); // start time stamp
        auto const now = std::chrono::steady_clock::now();
        std::chrono::duration<double> const  timesecs = now - start;
        return timesecs.count();
    }

    static void SleepSecs( long long const secs )
    {
        std::this_thread::sleep_for( std::chrono::seconds( secs ) );
    }

    static long long CreateRandomNumber( long long const start, long long const end )
    {
        if( start < 0 || end < 0 || end < start ) {
            return -1LL;
        }
        if( static_cast<long long>(std::numeric_limits<unsigned int>::max()) < start
            || static_cast<long long>(std::numeric_limits<unsigned int>::max()) < end ) {
            return -1LL;
        }
#if defined(_DEBUG) && 0
        static std::mt19937 gen32;
#else
        static std::mt19937 gen32( std::random_device{}() );
        //static std::mt19937 gen32( static_cast<unsigned int>(::time(nullptr)) ); // alternative.
#endif
        //std::uniform_int_distribution<> distrib( start, end ); // info: for a class member with a constant range use this for a complete unfiorm distribution!
        long long const num = static_cast<long long>(gen32());
        return start + (num % (end - start + 1));
    }

    static std::string CurrentPath()
    {
        std::error_code  ec;
        auto cur = std::filesystem::current_path(ec).generic_u8string(); // TODO: THREAD per CoreLib / Context instance? make thread safe.
        if( !cur.empty() && !cur.ends_with( u8'/' ) ) {
            cur.append( 1, u8'/' );
        }
        return std::string( cur.begin(), cur.end() ); // must make a copy... :-(
    }

    static bool ChangeCurrentPath( std::string const &rPath )
    {
        std::error_code ec;
        std::filesystem::current_path( std::filesystem::absolute( util::utf8_path( rPath ) ), ec );
        if( ec ) {
            return false;
        }
        return true;
    }

    static std::string TempPath()
    {
        std::error_code  ec;
        auto cur = std::filesystem::temp_directory_path(ec).generic_u8string();
        if( !cur.empty() && !cur.ends_with(u8'/') ) {
            cur.append( 1, u8'/' );
        }
        return std::string( cur.begin(), cur.end() ); // must make a copy... :-(
    }

    static bool CreateDir( std::string const &rPath, bool recursive )
    {
        auto const  path = std::filesystem::absolute( util::utf8_path( rPath ) );
        std::error_code  ec;
        if( recursive ) {
            return std::filesystem::create_directories( path, ec ) && not ec;
        } else {
            return std::filesystem::create_directory( path, ec ) && not ec;
        }
    }

    // checks whether directory or file exists
    static bool PathExists( std::string const &rPath )
    {
        auto const  path = std::filesystem::absolute( util::utf8_path( rPath ) );
        std::error_code ec;
        bool const res = std::filesystem::exists( path, ec );
        return ec ? false : res; //TODO: better error handling. return either a bool or an error code.
    }

    // get the file size, (only work for files, not directories)
    static long long FileSize( std::string const &rFile )
    {
        auto const  path = std::filesystem::absolute( util::utf8_path( rFile ) );
        std::error_code  ec;
        auto const  size = std::filesystem::file_size( path, ec );
        if( ec ) {
            return -1LL;
        }
        return static_cast<long long>(size);
    }

    // deletes(!) file or (empty) directory
    static bool PathDelete( std::string const &rPath )
    {
        auto const  path = std::filesystem::absolute( util::utf8_path( rPath ) );
        std::error_code  ec;
        return std::filesystem::remove( path, ec ) && not ec; //TODO: better error handling. return either a bool or an error code.
    }

    static bool DoFileCopy( std::string const &rFile, std::string const &rDestDir, std::filesystem::copy_options const options )
    {
        auto const  src_file = std::filesystem::absolute( util::utf8_path( rFile ) );
        auto const  dest_dir = std::filesystem::absolute( util::utf8_path( rDestDir ) );

        auto const  dest_file = dest_dir / src_file.filename();

        std::error_code ec;
        return std::filesystem::copy_file( src_file, dest_file, options, ec ) && not ec; //TODO: better error handling. return either a bool or an error code.
    }

    static bool FileCopy( std::string const &rFile, std::string const &rDestDir, bool overwrite )
    {
        return DoFileCopy( rFile, rDestDir, overwrite ? std::filesystem::copy_options::overwrite_existing : std::filesystem::copy_options::skip_existing );
    }

    static bool FileCopyIfNewer( std::string const &rFile, std::string const &rDestDir )
    {
        return DoFileCopy( rFile, rDestDir, std::filesystem::copy_options::update_existing );
    }

    static std::string LastModifiedToString( std::filesystem::file_time_type const ftime )
    {
#if _MSC_VER || (!defined(__clang__) && __GNUC__ >= 13 ) // TODO: _LIBCPP_VERSION
        auto const tp = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
        // must round down to seconds first, otherwise fractional seconds will be printed.
        auto const tp_secs = std::chrono::floor<std::chrono::seconds>( tp );
        std::chrono::zoned_time const  zoned = {std::chrono::current_zone(), tp_secs};
        auto local_tp = zoned.get_local_time();
        return std::format( "{0:%F} {0:%T}", local_tp ); // with this the last modified is perfectly sortable _and_ readable
#else
        // clock_cast not available yet, but on the other hand MSVC does not offer to_sys! :(
        auto const tp = std::chrono::file_clock::to_sys( ftime );
        // convert to local time. (cast needed for clang w. libc++ )
        auto const t = std::chrono::system_clock::to_time_t( std::chrono::time_point_cast<std::chrono::system_clock::duration>(tp) );
        struct tm  tconv {};
# if defined(_MSC_VER)
        (void) ::localtime_s( &tconv, &t );
# else
        (void) ::localtime_r( &t, &tconv );
# endif
        std::ostringstream stream;
        stream << std::put_time( &tconv, "%F %T" ); // with this the last modified is perfectly sortable _and_ readable
        return stream.str();
#endif
    }

    // retrieves the last modified date/time for files and directories as string (which can be perfectly sorted as well)
    static std::string LastModified( std::string const &rPath )
    {
        auto const  path = std::filesystem::absolute( util::utf8_path( rPath ) );
        std::error_code  ec;
        auto const  lm   = std::filesystem::last_write_time( path, ec );

        if( ec ) {
            return "";
        }

        return LastModifiedToString( lm );
    }

    static ValueObject ReadTextFile( std::string const &rFile )
    {
        // TODO: THREAD path building per CoreLib / Context instance? make thread safe and use the internal current path for make absolute!
        auto const  path = std::filesystem::absolute( util::utf8_path( rFile ) );
        return ReadTextFileEx( path );
    }

    static ValueObject ReadTextFileEx( std::filesystem::path const &rPath )
    {
        std::ifstream  file( rPath, std::ios::binary | std::ios::ate ); // ate jumps to end.
        if( file ) {
            auto size = file.tellg();
            file.seekg( 0 );
            if( size >= 3 ) {
                char  buf[4] = {};
                file.read( buf, 3 );
                if( buf[0] == '\xEF' && buf[1] == '\xBB' && buf[2] == '\xBF' ) {
                    size -= 3; // skip bom
                } else {
                    file.seekg( 0 ); // no bom, start from beginning.
                }
            }
            std::string  buf;
            buf.resize( static_cast<size_t>(size) );
            file.read( buf.data(), size );
            if( !file.good() ) {
                return ValueObject( MakeRuntimeError( "Cannot read file!" ), ValueConfig( ValueUnshared, ValueMutable ) );
            }
#if 1
            auto const span = std::span( buf.data(), buf.size() );  // libc++14 needs the constructor call, std::span( buf ) won't compile!
            if( not util::is_valid_utf8( span, true ) ) {
                return ValueObject( MakeRuntimeError( "Not valid UTF-8 (or ASCII control chars)!" ), ValueConfig( ValueUnshared, ValueMutable ) );
            }
#else // the old check was very relaxed...
            for( size_t idx = 0; idx < buf.size(); ++idx ) {
                unsigned char const  c = static_cast<unsigned char>(buf[idx]);
                if( c < 0x20 ) { // before 'space' ? (ASCII Control)
                    // bail out on uncommon control chars (they are not printable and should not appear in normal (UTF-8) text, although they are valid.)
                    if( c < 0x8 || c > 0xD ) { // '\t' until '\r' are allowed!
                        throw teascript::exception::runtime_error( "ReadTextFile(): Reject content due to uncommon ASCII Control character!" );
                    }
                } else if( c == 0xC0 || c == 0xC1 || c > 0xF4 ) {
                    throw teascript::exception::runtime_error( "ReadTextFile(): Invalid UTF-8 detected (c == 0xC0 || c == 0xC1 || c > 0xF4)!" );
                } else {
                    // simplified check. if we have at least one follow char, that follow char must be in [0x80,0xBF]
                    // this is not complete true for all possible c's, but the range can only be smaller than we test.
                    // so we will not detect ALL possible errors. Also, possible 2nd and 3rd follow chars are unchecked for now.
                    if( c > 0xC1 ) {
                        unsigned char const c1 = static_cast<unsigned char>(buf[idx + 1]);
                        if( c1 < 0x80 || c1 > 0xBF ) {
                            throw teascript::exception::runtime_error( "ReadTextFile(): Invalid UTF-8 detected (broken follow char: c1 < 0x80 || c1 > 0xBF)!" );
                        }
                    } // else: we could check if c is a follow char. but then the check above must be complete _and_ also advance idx.
                }
            }
#endif
            return ValueObject( std::move( buf ), ValueConfig( ValueUnshared, ValueMutable ) );
        }
        return ValueObject( MakeRuntimeError( "Cannot open/read file!" ), ValueConfig( ValueUnshared, ValueMutable ) );
    }

    static bool WriteTextFile( std::string const &rFile, std::string const &rContent, bool const overwrite, bool const bom )
    {
        // TODO: THREAD path building per CoreLib / Context instance? make thread safe and use the internal current path for make absolute!
        auto const  path = std::filesystem::absolute( util::utf8_path( rFile ) );
        return WriteTextFileEx( path, rContent, overwrite, bom );
    }

    static bool WriteTextFileEx( std::filesystem::path const &rPath, std::string const & rContent, bool const overwrite, bool const bom )
    {
        // TODO: error handling! return an Error instead of Bool / throw!
        if( !overwrite ) {
            // must use fopen w. eXclusive mode for ensure file did not exist before.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996) // if you get an error here use /w34996 or disable sdl checks
#endif
            auto fp = fopen( rPath.string().c_str(), "w+x" );
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
            if( !fp ) {
                return false; // file exist already (or access denied)
            }
            fclose( fp );
        }
        std::ofstream  file( rPath, std::ios::binary | std::ios::trunc );
        if( file ) {
            if( bom ) {
                file.write( "\xEF\xBB\xBF", 3 );
            }
            file.write( rContent.data(), static_cast<std::streamsize>(rContent.size()) );
            return file.good();
        }
        return false;
    }

    static ValueObject ReadBinaryFile( std::string const &rFile )
    {
        // TODO: THREAD path building per CoreLib / Context instance? make thread safe and use the internal current path for make absolute!
        auto const  path = std::filesystem::absolute( util::utf8_path( rFile ) );
        return ReadBinaryFileEx( path );
    }

    static ValueObject ReadBinaryFileEx( std::filesystem::path const &rPath )
    {
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996) // if you get an error here use /w34996 or disable sdl checks
#endif
        auto fp = fopen( rPath.string().c_str(), "rb" );
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
        if( !fp ) {
            return ValueObject( MakeRuntimeError( "Cannot open file!" ), ValueConfig( ValueUnshared, ValueMutable ) );
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

        Buffer buf( size );
        auto const read = fread( buf.data(), 1, size, fp );
        if( read == size ) {
            return ValueObject( std::move( buf ), ValueConfig( ValueUnshared, ValueMutable ) );
        } else {
            return ValueObject( MakeRuntimeError( "Cannot read file!" ), ValueConfig( ValueUnshared, ValueMutable ) );
        }
    }

    static bool WriteBinaryFile( std::string const &rFile, Buffer const &rContent, bool const overwrite )
    {
        // TODO: THREAD path building per CoreLib / Context instance? make thread safe and use the internal current path for make absolute!
        auto const  path = std::filesystem::absolute( util::utf8_path( rFile ) );
        return WriteBinaryFileEx( path, rContent, overwrite );
    }

    static bool WriteBinaryFileEx( std::filesystem::path const &rPath, Buffer const &rContent, bool const overwrite )
    {
        // TODO: error handling! return an Error instead of Bool / throw!
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996) // if you get an error here use /w34996 or disable sdl checks
#endif
        auto fp = fopen( rPath.string().c_str(), overwrite ? "wb" : "wbx");
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
        if( !fp ) {
            return false;
        }
        if( rContent.size() != fwrite( rContent.data(), 1, rContent.size(), fp ) ) {
            fclose( fp );
            ::remove( rPath.string().c_str() );
            return false;
        }
        fclose( fp );
        return true;
    }


    // Tuple

    static long long TupleSize( Tuple const & rTuple )
    {
        return static_cast<long long>(rTuple.Size());
    }

    static ValueObject TupleValue( Tuple const &rTuple, long long const idx )
    {
        return rTuple.GetValueByIdx( static_cast<std::size_t>(idx) );
    }

    static ValueObject TupleNamedValue( Tuple const &rTuple, std::string const &rName )
    {
        return rTuple.GetValueByKey( rName );
    }

    static void TupleSetValue( Tuple &rTuple, long long const idx, ValueObject &rVal )
    {
        rTuple.GetValueByIdx( static_cast<std::size_t>(idx) ).AssignValue( rVal.MakeShared() );
    }

    static void TupleSetNamedValue( Tuple &rTuple, std::string const &rName, ValueObject &rVal )
    {
        rTuple.GetValueByKey( rName ).AssignValue( rVal.MakeShared() );
    }

    static void TupleAppend( Tuple &rTuple, ValueObject &rVal )
    {
        rTuple.AppendValue( rVal.MakeShared() );
    }

    static bool TupleNamedAppend( Tuple &rTuple, std::string const &rName, ValueObject &rVal )
    {
        return rTuple.AppendKeyValue( rName, rVal.MakeShared() );
    }

    static void TupleInsert( Tuple &rTuple, long long const idx, ValueObject &rVal )
    {
        rTuple.InsertValue( static_cast<std::size_t>(idx), rVal.MakeShared() );
    }

    static void TupleNamedInsert( Tuple &rTuple, long long const idx, std::string const &rName, ValueObject &rVal )
    {
        rTuple.InsertKeyValue( static_cast<std::size_t>(idx), rName, rVal.MakeShared() );
    }

    static bool TupleRemove( Tuple  &rTuple, long long const idx )
    {
        return rTuple.RemoveValueByIdx( static_cast<std::size_t>(idx) );
    }

    static bool TupleNamedRemove( Tuple &rTuple, std::string const &rName )
    {
        return rTuple.RemoveValueByKey( rName );
    }

    static long long TupleIndexOf( Tuple const &rTuple, std::string const &rName )
    {
        auto const idx = rTuple.IndexOfKey( rName );
        return idx > static_cast<size_t>(std::numeric_limits<long long>::max()) ? -1LL : static_cast<long long>(idx);
    }

    static std::string TupleNameOf( Tuple const &rTuple, long long const idx )
    {
        return rTuple.KeyOfIndex( static_cast<size_t>(idx) );
    }

    static void TupleSwapValues( Tuple &rTuple, long long const idx1, long long const idx2 )
    {
        rTuple.SwapByIdx( static_cast<std::size_t>(idx1), static_cast<std::size_t>(idx2) );
    }

    static bool TupleSameTypes( Tuple const &rTuple1, Tuple const &rTuple2 )
    {
        return tuple::is_same_structure( rTuple1, rTuple2 );
    }

    static void TuplePrint( ValueObject const &rTuple, std::string const &rRootName, long long max_nesting )
    {
        tuple::foreach_named_element( rRootName, rTuple, true, [=]( std::string const &name, ValueObject const &rVal, int level ) -> bool {
            std::string const val_str = rVal.GetTypeInfo()->IsSame<Tuple>() ? "<Tuple>" : rVal.HasPrintableValue() ? rVal.PrintValue() : "<" + rVal.GetTypeInfo()->GetName() + ">";
            std::string const text = name + ": " + val_str + "\n";
            PrintStdOut( text );
            return level < max_nesting;
        } );
    }

    static IntegerSequence MakeSequence( long long start, long long end, long long step )
    {
        return IntegerSequence( start, end, step );
    }


    // Buffer

protected:
    static void CheckValueObjectForBufferPos( ValueObject const &pos )
    {
        if( not pos.GetTypeInfo()->IsArithmetic() ) {
            throw exception::type_mismatch( "Must be an arithmetic type!" );
        }
        if( pos.GetTypeInfo()->IsSigned() ) {
            if( pos.GetAsInteger() < 0 ) {
                throw exception::out_of_range( "Must be a positive integer!" );
            }
        }
    }

    static bool CheckValueObjectForBufferPos_NoThrow( ValueObject const &pos )
    {
        if( not pos.GetTypeInfo()->IsArithmetic() ) {
            return false;
        }
        if( pos.GetTypeInfo()->IsSigned() ) {
            if( pos.GetAsInteger() < 0 ) {
                return false;
            }
        }
        return true;
    }

    static size_t ToSize( ValueObject const &size_or_pos )
    {
        if constexpr( sizeof( size_t ) >= sizeof( U64 ) ) {
            return static_cast<size_t>(util::ArithmeticFactory::Convert<U64>( size_or_pos ).GetValue<U64>());
        } else {
            auto const v = util::ArithmeticFactory::Convert<U64>( size_or_pos ).GetValue<U64>();
            if( std::numeric_limits<size_t>::max() < v ) {
                throw exception::out_of_range( "value too big for size_t!" );
            }
            return static_cast<size_t>(v);
        }
    }

    static bool CheckBufferPosForRead( Buffer const &rBuffer, size_t const pos, size_t const wanted ) noexcept
    {
        if( std::numeric_limits<size_t>::max() - wanted < pos ) { // overflow protection
            return false;
        }
        if( pos + wanted > rBuffer.size() ) {
            return false;
        }
        return true;
    }

    static bool CheckBufferPosForWrite( Buffer  &rBuffer, size_t const pos, size_t const wanted, U8 const val = 0 ) noexcept
    {
        if( pos > rBuffer.size() ) { // == size will append data if enough capacity
            return false;
        }
        if( std::numeric_limits<size_t>::max() - wanted < pos ) { // overflow protection
            return false;
        }
        // our buffers don't grow automatically behind the current allocation!
        if( pos + wanted > rBuffer.capacity() ) {
            return false;
        }
        // grow?
        if( pos + wanted > rBuffer.size() ) {
            if( val != 0 ) {
                rBuffer.resize( pos + wanted, val );
            } else {
                rBuffer.resize( pos + wanted ); // probably faster when use default construct instead of copy from val
            }
        }

        return true;
    }

public:
    static Buffer MakeBuffer( ValueObject const & size )
    {
        CheckValueObjectForBufferPos( size );
        Buffer buf;
        buf.reserve( ToSize( size ) );
        return buf;
    }

    static U64 BufSize( Buffer const &rBuffer )
    {
        return rBuffer.size();
    }

    static U64 BufCapacity( Buffer const &rBuffer )
    {
        return rBuffer.capacity();
    }

    static bool BufResize( Buffer &rBuffer, ValueObject const &size )
    {
        if( not CheckValueObjectForBufferPos_NoThrow( size ) ) {
            return false;
        }
        auto const  s = ToSize( size );
        try {
            rBuffer.resize( s );
            return true;
        } catch( ... ) {
            return false;
        }
    }

    static U8 BufAt( Buffer const &rBuffer, ValueObject const & pos )
    {
        CheckValueObjectForBufferPos( pos );
        auto const idx = ToSize( pos );
        if( idx >= rBuffer.size() ) {
            throw exception::out_of_range( "idx is out of range!" );
        }

        return rBuffer[idx];
    }

    static bool BufFill( Buffer &rBuffer, ValueObject const &pos, ValueObject const &count, U8 const val )
    {
        if( not CheckValueObjectForBufferPos_NoThrow( pos ) ) {
            return false;
        }
        if( not count.GetTypeInfo()->IsArithmetic() ) {
            return false;
        }
        auto const  idx = ToSize( pos );
        auto const  n   = ToSize( count );
        auto const  old = rBuffer.size();
        //                                                                         possible overflow will be caught by idx > size()!
        if( not CheckBufferPosForWrite( rBuffer, idx, n == static_cast<size_t>(-1) ? rBuffer.capacity() - idx : n, val ) ) {
            return false;
        }

        // old data to overwrite?
        if( idx < old ) {
            std::fill_n( rBuffer.begin() + idx, old - idx, val );
        }
        return true;
    }

    static bool BufFill32( Buffer &rBuffer, ValueObject const &pos, ValueObject const &count, U64 const val )
    {
        if( not CheckValueObjectForBufferPos_NoThrow( pos ) ) {
            return false;
        }
        if( not count.GetTypeInfo()->IsArithmetic() ) {
            return false;
        }
        auto const  idx = ToSize( pos );
        if( idx >= rBuffer.capacity() ) {
            return false;
        }
        auto const  n   = ToSize( count );
        auto const  end = n == static_cast<size_t>(-1) ? rBuffer.capacity() - idx : n;
        if( val > std::numeric_limits<std::uint32_t>::max() ) {
            return false;
        }
        auto const  valu32 = static_cast<std::uint32_t>(val);

        if( (end % sizeof( valu32 )) != 0 ) { // not aligned to full value boundary!
            return false;
        }

        if( not CheckBufferPosForWrite( rBuffer, idx, end ) ) {
            return false;
        }

        auto elems = end / sizeof(valu32);
        auto i = idx;
        while( elems >= 16 ) {

            ::memcpy( rBuffer.data() + i + (0 * sizeof( valu32 )), &valu32, sizeof( valu32 ) );
            ::memcpy( rBuffer.data() + i + (1 * sizeof( valu32 )), &valu32, sizeof( valu32 ) );
            ::memcpy( rBuffer.data() + i + (2 * sizeof( valu32 )), &valu32, sizeof( valu32 ) );
            ::memcpy( rBuffer.data() + i + (3 * sizeof( valu32 )), &valu32, sizeof( valu32 ) );

            ::memcpy( rBuffer.data() + i + (4 * sizeof( valu32 )), &valu32, sizeof( valu32 ) );
            ::memcpy( rBuffer.data() + i + (5 * sizeof( valu32 )), &valu32, sizeof( valu32 ) );
            ::memcpy( rBuffer.data() + i + (6 * sizeof( valu32 )), &valu32, sizeof( valu32 ) );
            ::memcpy( rBuffer.data() + i + (7 * sizeof( valu32 )), &valu32, sizeof( valu32 ) );

            ::memcpy( rBuffer.data() + i + (8 * sizeof( valu32 )), &valu32, sizeof( valu32 ) );
            ::memcpy( rBuffer.data() + i + (9 * sizeof( valu32 )), &valu32, sizeof( valu32 ) );
            ::memcpy( rBuffer.data() + i + (10 * sizeof( valu32 )), &valu32, sizeof( valu32 ) );
            ::memcpy( rBuffer.data() + i + (11 * sizeof( valu32 )), &valu32, sizeof( valu32 ) );

            ::memcpy( rBuffer.data() + i + (12 * sizeof( valu32 )), &valu32, sizeof( valu32 ) );
            ::memcpy( rBuffer.data() + i + (13 * sizeof( valu32 )), &valu32, sizeof( valu32 ) );
            ::memcpy( rBuffer.data() + i + (14 * sizeof( valu32 )), &valu32, sizeof( valu32 ) );
            ::memcpy( rBuffer.data() + i + (15 * sizeof( valu32 )), &valu32, sizeof( valu32 ) );

            i += (16 * sizeof( valu32 ));
            elems -= 16;
        }
        // rest
        while( elems > 0 ) {
            ::memcpy( rBuffer.data() + i, &valu32, sizeof( valu32 ) );
            i += sizeof( valu32 );
            --elems;
        }
        return true;
    }

    static bool BufCopy( Buffer &dest, ValueObject const &dst_off, Buffer const &src, ValueObject const &src_off, ValueObject const &len )
    {
        if( not CheckValueObjectForBufferPos_NoThrow( dst_off ) ) {
            return false;
        }
        if( not CheckValueObjectForBufferPos_NoThrow( src_off ) ) {
            return false;
        }
        if( not len.GetTypeInfo()->IsArithmetic() ) {
            return false;
        }
        auto const  dst_idx = ToSize( dst_off );
        if( dst_idx >= dest.capacity() ) {
            return false;
        }

        auto const  src_idx = ToSize( src_off );
        auto const  l       = ToSize( len );
        auto const  end     = l == static_cast<size_t>(-1) ? src.size() - src_idx : l;
        if( not CheckBufferPosForRead( src, src_idx, end ) ) {
            return false;
        }
        if( not CheckBufferPosForWrite( dest, dst_idx, end ) ) {
            return false;
        }

        ::memcpy( dest.data() + dst_idx, src.data() + src_idx, end );
        return true;
    }

    static bool BufSetU8( Buffer  &rBuffer, ValueObject const &pos, U8 const val )
    {
        if( not CheckValueObjectForBufferPos_NoThrow( pos ) ) {
            return false;
        }
        auto const idx = ToSize( pos );
        if( not CheckBufferPosForWrite( rBuffer, idx, sizeof( U8 ) ) ) {
            return false;
        }

        rBuffer[idx] = val;

        return true;
    }

    static bool BufSetI8( Buffer &rBuffer, ValueObject const &pos, I64 const val )
    {
        if( val < std::numeric_limits<signed char>::min() || val > std::numeric_limits<signed char>::max() ) {
            return false;
        }
        auto const valu8 = static_cast<U8>(static_cast<signed char>(val));
        return BufSetU8( rBuffer, pos, valu8 );
    }

    // writing unsigned 16 bit data in host byte order into the buffer.
    static bool BufSetU16( Buffer &rBuffer, ValueObject const &pos, U64 const val )
    {
        if( not CheckValueObjectForBufferPos_NoThrow( pos ) ) {
            return false;
        }

        auto  idx = ToSize( pos );
        if( not CheckBufferPosForWrite( rBuffer, idx, sizeof( std::uint16_t ) ) ) {
            return false;
        }
        if( val > std::numeric_limits<std::uint16_t>::max() ) {
            return false;
        }
        auto const valu16 = static_cast<std::uint16_t>(val);

        // here it is ensured to have valid elements in the buffer already.

        ::memcpy( rBuffer.data() + idx, &valu16, sizeof( valu16 ) );

        return true;
    }

    // writing signed 16 bit data in host byte order into the buffer.
    static bool BufSetI16( Buffer &rBuffer, ValueObject const &pos, I64 const val )
    {
        if( val < std::numeric_limits<std::int16_t>::min() || val > std::numeric_limits<std::int16_t>::max() ) {
            return false;
        }
        auto const valu16 = static_cast<std::uint16_t>(static_cast<std::int16_t>(val));
        return BufSetU16( rBuffer, pos, static_cast<U64>(valu16) );
    }

    // writing unsigned 32 bit data in host byte order into the buffer.
    static bool BufSetU32( Buffer &rBuffer, ValueObject const &pos, U64 const val )
    {
        if( not CheckValueObjectForBufferPos_NoThrow( pos ) ) {
            return false;
        }

        auto  idx = ToSize( pos );
        if( not CheckBufferPosForWrite( rBuffer, idx, sizeof( std::uint32_t ) ) ) {
            return false;
        }
        if( val > std::numeric_limits<std::uint32_t>::max() ) {
            return false;
        }
        auto const valu32 = static_cast<std::uint32_t>(val);

        // here it is ensured to have valid elements in the buffer already.

        ::memcpy( rBuffer.data() + idx, &valu32, sizeof( valu32 ) );

        return true;
    }

    // writing signed 32 bit data in host byte order into the buffer.
    static bool BufSetI32( Buffer &rBuffer, ValueObject const &pos, I64 const val )
    {
        if( val < std::numeric_limits<std::int32_t>::min() || val > std::numeric_limits<std::int32_t>::max() ) {
            return false;
        }
        auto const valu32 = static_cast<std::uint32_t>(static_cast<std::int32_t>(val));
        return BufSetU32( rBuffer, pos, static_cast<U64>(valu32) );
    }

    // writing unsigned 64 bit data in host byte order into the buffer.
    static bool BufSetU64( Buffer &rBuffer, ValueObject const &pos, U64 const val )
    {
        if( not CheckValueObjectForBufferPos_NoThrow( pos ) ) {
            return false;
        }

        auto  idx = ToSize( pos );
        if( not CheckBufferPosForWrite( rBuffer, idx, sizeof( U64 ) ) ) {
            return false;
        }

        // here it is ensured to have valid elements in the buffer already.

        ::memcpy( rBuffer.data() + idx, &val, sizeof( val ) );

        return true;
    }

    // writing signed 64 bit data in host byte order into the buffer.
    static bool BufSetI64( Buffer &rBuffer, ValueObject const &pos, I64 const val )
    {
        return BufSetU64( rBuffer, pos, static_cast<U64>(val) );
    }

    // writing String (_without_ trailing 0 !) into the buffer.
    static bool BufSetString( Buffer &rBuffer, ValueObject const &pos, String const &rStr )
    {
        if( not CheckValueObjectForBufferPos_NoThrow( pos ) ) {
            return false;
        }

        auto const idx = ToSize( pos );
        if( not CheckBufferPosForWrite( rBuffer, idx, rStr.length() ) ) {
            return false;
        }

        // here it is ensured to have valid elements in the buffer already.

        ::memcpy( rBuffer.data() + idx, rStr.data(), rStr.length() );

        return true;
    }

    static ValueObject BufGetU8( Buffer const &rBuffer, ValueObject const &pos )
    {
        if( not CheckValueObjectForBufferPos_NoThrow( pos ) ) {
            return ValueObject( MakeRuntimeError( "Buffer: idx is invalid type or negative!" ), ValueConfig( ValueUnshared, ValueMutable ) );
        }
        auto const idx = ToSize( pos );
        if( not CheckBufferPosForRead( rBuffer, idx, sizeof( U8 ) ) ) {
            return ValueObject( MakeRuntimeError( "Buffer: idx is out-of-range!" ), ValueConfig( ValueUnshared, ValueMutable ) );
        }

        return ValueObject( rBuffer[idx] );
    }

    static ValueObject BufGetI8( Buffer const &rBuffer, ValueObject const &pos )
    {
        if( not CheckValueObjectForBufferPos_NoThrow( pos ) ) {
            return ValueObject( MakeRuntimeError( "Buffer: idx is invalid type or negative!" ), ValueConfig( ValueUnshared, ValueMutable ) );
        }
        auto const idx = ToSize( pos );
        if( not CheckBufferPosForRead( rBuffer, idx, sizeof( signed char ) ) ) {
            return ValueObject( MakeRuntimeError( "Buffer: idx is out-of-range!" ), ValueConfig( ValueUnshared, ValueMutable ) );
        }

        I64 const val = static_cast<I64>(static_cast<signed char>(rBuffer[idx]));
        return ValueObject( val );
    }

    template< std::integral Intermediate, std::integral Result>
    static ValueObject BufGet_T( Buffer const &rBuffer, ValueObject const &pos )
    {
        if( not CheckValueObjectForBufferPos_NoThrow( pos ) ) {
            return ValueObject( MakeRuntimeError( "Buffer: idx is invalid type or negative!" ), ValueConfig( ValueUnshared, ValueMutable ) );
        }
        auto const idx = ToSize( pos );
        if( not CheckBufferPosForRead( rBuffer, idx, sizeof( Intermediate ) ) ) {
            return ValueObject( MakeRuntimeError( "Buffer: idx is out-of-range!" ), ValueConfig( ValueUnshared, ValueMutable ) );
        }

        Intermediate val_i{};
        ::memcpy( &val_i, rBuffer.data() + idx, sizeof( Intermediate ) );
        
        if constexpr( std::is_same_v<Intermediate, Result> ) {
            return ValueObject( val_i );
        } else {
            Result const val = static_cast<Result>(val_i);
            return ValueObject( val );
        }
    }

    static ValueObject BufGetU16( Buffer const &rBuffer, ValueObject const &pos )
    {
        return BufGet_T<std::uint16_t, U64>( rBuffer, pos );
    }

    static ValueObject BufGetI16( Buffer const &rBuffer, ValueObject const &pos )
    {
        return BufGet_T<std::int16_t, I64>( rBuffer, pos );
    }

    static ValueObject BufGetU32( Buffer const &rBuffer, ValueObject const &pos )
    {
        return BufGet_T<std::uint32_t, U64>( rBuffer, pos );
    }

    static ValueObject BufGetI32( Buffer const &rBuffer, ValueObject const &pos )
    {
        return BufGet_T<std::int32_t, I64>( rBuffer, pos );
    }

    static ValueObject BufGetU64( Buffer const &rBuffer, ValueObject const &pos )
    {
        return BufGet_T<U64, U64>( rBuffer, pos );
    }

    static ValueObject BufGetI64( Buffer const &rBuffer, ValueObject const &pos )
    {
        return BufGet_T<I64, I64>( rBuffer, pos );
    }

    static ValueObject BufGetString( Buffer const &rBuffer, ValueObject const &pos, ValueObject const &len )
    {
        if( not CheckValueObjectForBufferPos_NoThrow( pos ) ) {
            return ValueObject( MakeRuntimeError( "Buffer: idx is invalid type or negative!" ), ValueConfig( ValueUnshared, ValueMutable ) );
        }
        if( not CheckValueObjectForBufferPos_NoThrow( len ) ) {
            return ValueObject( MakeRuntimeError( "Buffer: len is invalid type or negative!" ), ValueConfig( ValueUnshared, ValueMutable ) );
        }
        auto const idx = ToSize( pos );
        auto const l   = ToSize( len );
        if( not CheckBufferPosForRead( rBuffer, idx, l ) ) {
            return ValueObject( MakeRuntimeError( "Buffer: idx is out-of-range!" ), ValueConfig( ValueUnshared, ValueMutable ) );
        }

        auto const span = std::span( rBuffer.data() + idx, l );
        if( not util::is_valid_utf8( span ) ) {
            return ValueObject( MakeRuntimeError( "Buffer: no valid UTF-8!" ), ValueConfig( ValueUnshared, ValueMutable ) );
        }

        std::string  res;
        res.assign( reinterpret_cast<char const *>(span.data()), span.size() );
        return ValueObject( std::move( res ), ValueConfig( true ) );
    }

    // extract an ascii string out of the buffer, all values must be in range [0,127]
    static ValueObject BufGetAscii( Buffer const &rBuffer, ValueObject const &pos, ValueObject const &len )
    {
        if( not CheckValueObjectForBufferPos_NoThrow( pos ) ) {
            return ValueObject( MakeRuntimeError( "Buffer: idx is invalid type or negative!" ), ValueConfig( ValueUnshared, ValueMutable ) );
        }
        if( not CheckValueObjectForBufferPos_NoThrow( len ) ) {
            return ValueObject( MakeRuntimeError( "Buffer: len is invalid type or negative!" ), ValueConfig( ValueUnshared, ValueMutable ) );
        }
        auto const idx = ToSize( pos );
        auto const l   = ToSize( len );
        if( not CheckBufferPosForRead( rBuffer, idx, l ) ) {
            return ValueObject( MakeRuntimeError( "Buffer: idx is out-of-range!" ), ValueConfig( ValueUnshared, ValueMutable ) );
        }

        auto const span = std::span( rBuffer.data() + idx, l );
#if !defined(_LIBCPP_VERSION) // libc++14 does not have ranges::all_of yet!
        if( not std::ranges::all_of( span, []( auto const c ) { return c < 128; } ) ) {
            return ValueObject( MakeRuntimeError( "Buffer: no valid ASCII!" ), ValueConfig( ValueUnshared, ValueMutable ) );
        }
#else
        if( not std::all_of( span.begin(), span.end(), [](auto const c) { return c < 128; }) ) {
            return ValueObject( MakeRuntimeError( "Buffer: no valid ASCII!" ), ValueConfig( ValueUnshared, ValueMutable ) );
        }
#endif
        
        std::string  res;
        res.assign( reinterpret_cast<char const *>(span.data()), span.size() );
        return ValueObject( std::move( res ), ValueConfig( true ) );
    }


    static ValueObject ReadDirFirst( Context &rContext, std::string const &rPath )
    {
        std::error_code ec;
        auto const  path = std::filesystem::weakly_canonical( std::filesystem::absolute( util::utf8_path( rPath ) ), ec );
        // build utf-8 filepath again... *grrr*
        auto path_utf8 = path.generic_u8string();
        if( !path_utf8.empty() && !path_utf8.ends_with( u8'/' ) ) {
            path_utf8.append( 1, u8'/' );
        }
        auto const path_str = not ec ? std::string( path_utf8.begin(), path_utf8.end() ) // must make a copy... :-(
                                     : rPath;
        
        auto dir_it = not ec ? std::filesystem::directory_iterator( path, ec ) : std::filesystem::directory_iterator();
        auto const cfg = ValueConfig{ValueShared,ValueMutable,rContext.GetTypeSystem()};
        Tuple res;
        res.Reserve( 8 );
        if( ec || dir_it == std::filesystem::directory_iterator() ) {
            res.AppendKeyValue( "valid", ValueObject( false, cfg ) );
            res.AppendKeyValue( "error", ValueObject( static_cast<long long>(ec.value()), cfg ) );
            res.AppendKeyValue( "path", ValueObject( path_str, cfg ) );
            return ValueObject( std::move( res ), cfg );
        }

        // build utf-8 filename
        auto const name_str = util::utf8_path_to_str( dir_it->path().filename() );

        res.AppendKeyValue( "valid", ValueObject( true, cfg ) );

        res.AppendKeyValue( "name", ValueObject( name_str, cfg ) );
        auto const size = dir_it->is_regular_file() ? static_cast<long long>(dir_it->file_size()) : 0LL; // file_size for dirs produces error on Linux/Libc++
        res.AppendKeyValue( "size", ValueObject( size, cfg ) );
        std::string last_modified = LastModifiedToString( dir_it->last_write_time() );
        res.AppendKeyValue( "last_modified", ValueObject( last_modified, cfg ) );
        res.AppendKeyValue( "is_file", ValueObject( dir_it->is_regular_file(), cfg));
        res.AppendKeyValue( "is_dir", ValueObject( dir_it->is_directory(), cfg));
        res.AppendKeyValue( "path", ValueObject( path_str, cfg ) );

        res.AppendKeyValue( "_handle", ValueObject::CreatePassthrough( dir_it ) );
        return ValueObject( std::move( res ), cfg );
    }

    static ValueObject ReadDirNext( Context &rContext, Tuple &rTuple )
    {
        bool error = false;
        std::filesystem::directory_iterator dir_it;
        try {
            auto handle = rTuple.GetValueByKey( "_handle" );
            dir_it = std::any_cast<std::filesystem::directory_iterator>(handle.GetPassthroughData());
        } catch( std::bad_any_cast const & ) {
            error = true;
        } catch( exception::out_of_range const & ) {
            error = true;
        }

        // advance
        if( not error ) {
            ++dir_it;
        }

        auto const cfg = ValueConfig{ValueShared,ValueMutable,rContext.GetTypeSystem()};
        Tuple res;
        res.Reserve( 8 );
        if( error || dir_it == std::filesystem::directory_iterator() ) {
            std::error_code const ec = error ? std::make_error_code( std::errc::invalid_argument ) : std::error_code();
            res.AppendKeyValue( "valid", ValueObject( false, cfg ) );
            res.AppendKeyValue( "error", ValueObject( static_cast<long long>(ec.value()), cfg ) );
            if( rTuple.ContainsKey( "path" ) ) {
                res.AppendKeyValue( "path", rTuple.GetValueByKey( "path" ) );
            }
            return ValueObject( std::move( res ), cfg );
        }

        // build utf-8 filename
        auto const name_str = util::utf8_path_to_str( dir_it->path().filename() );

        res.AppendKeyValue( "valid", ValueObject( true, cfg ) );
        res.AppendKeyValue( "name", ValueObject( name_str, cfg ) );
        auto const size = dir_it->is_regular_file() ? static_cast<long long>(dir_it->file_size()) : 0LL; // file_size for dirs produces error on Linux/Libc++
        res.AppendKeyValue( "size", ValueObject( size, cfg ) );
        std::string last_modified = LastModifiedToString( dir_it->last_write_time() );
        res.AppendKeyValue( "last_modified", ValueObject( last_modified, cfg ) );
        res.AppendKeyValue( "is_file", ValueObject( dir_it->is_regular_file(), cfg ) );
        res.AppendKeyValue( "is_dir", ValueObject( dir_it->is_directory(), cfg ) );
        if( rTuple.ContainsKey( "path" ) ) {
            res.AppendKeyValue( "path", rTuple.GetValueByKey( "path" ) );
        }

        res.AppendKeyValue( "_handle", ValueObject::CreatePassthrough( dir_it ) );
        return ValueObject( std::move( res ), cfg );
    }

private:
    // as long as we must support _f64toi64 ... 
    static long long f64toi64( double const d )
    {
        return static_cast<long long>(d);
    }

protected:

    virtual void BuildInternals( Context &rTmpContext, config::eConfig const config )
    {
        // using collection directly here for speed up add key value.
        Context::VariableCollection  res;
        res.Reserve( 128 );

        auto tea_add_var = [&res]( std::string const &s, ValueObject const &v ) { res.AppendKeyValue( s, v ); };

#if 0 // Possible way to add teascript code with underscore _name
        {
            Parser p;
            auto func_node_val = p.Parse( "func ( x ) { x * x }", "Core" )->Eval( rTmpContext );
            res.push_back( std::make_pair( "_squared", std::move( func_node_val ) ) );
        }
#endif
        auto const  core_level  = static_cast<unsigned int>(config & config::LevelMask);
        auto const  opt_out     = static_cast<unsigned int>(config & config::FeatureOptOutMask);
        // the standard ValueConfig
        auto const  cfg         = ValueConfig( ValueShared, ValueConst, rTmpContext.GetTypeSystem() );
        auto const  cfg_mutable = ValueConfig( ValueShared, ValueMutable, rTmpContext.GetTypeSystem() );

        // add start time stamp of this CoreLibrary incarnation, the Core Lib config and our copyright information
        {
            tea_add_var( "_init_core_stamp", ValueObject( GetTimeStamp(), cfg ) );
            tea_add_var( "_core_config", ValueObject( static_cast<long long>(static_cast<unsigned long long>(config)), cfg ) ); // config of Core Lib (first cast to ull for avoid - by accident)
            tea_add_var( "__teascript_copyright", ValueObject( std::string(TEASCRIPT_COPYRIGHT), cfg ) );
        }

        // feature tuple (decided to have this always for make it easier to test in scripts)
        // NOTE: The feature tuple only reflects if the library was built with the features available.
        //       It does not show if the functions of the features are loaded or not. This depends
        //       on the loading level, the set opt out bits and also on user code (e.g., undef a function).
        {
            Tuple  feat;
            feat.Reserve( 5 );
            feat.AppendKeyValue( "format", ValueObject( (TEASCRIPT_FMTFORMAT == 1), cfg ) );
            feat.AppendKeyValue( "color", ValueObject( (TEASCRIPT_FMTFORMAT == 1), cfg ) );
            feat.AppendKeyValue( "toml", ValueObject( (TEASCRIPT_TOMLSUPPORT == 1), cfg ) );
            feat.AppendKeyValue( "json", ValueObject( static_cast<I64>(TEASCRIPT_JSONSUPPORT), cfg));
#if TEASCRIPT_JSONSUPPORT
            feat.AppendKeyValue( "json_adapter", ValueObject( std::string(JsonSupport<>::GetAdapterName()), cfg));

#endif
            ValueObject val{std::move( feat ), cfg_mutable};
            tea_add_var( "features", std::move( val ) ); // missing _ is intended for now.
        }

        // Add the basic Types
        {
            tea_add_var( "TypeInfo", ValueObject( TypeTypeInfo, cfg ) );
            tea_add_var( "NaV", ValueObject( TypeNaV, cfg ) );
            tea_add_var( "Bool", ValueObject( TypeBool, cfg ) );
            tea_add_var( "u8", ValueObject( TypeU8, cfg ) );
            tea_add_var( "i64", ValueObject( TypeLongLong, cfg ) );
            tea_add_var( "u64", ValueObject( TypeU64, cfg ) );
            tea_add_var( "f64", ValueObject( TypeDouble, cfg ) );
            tea_add_var( "String", ValueObject( TypeString, cfg ) );
            tea_add_var( "Error", ValueObject( TypeError, cfg ) );
            tea_add_var( "Number", ValueObject( MakeTypeInfo<Number>("Number"), cfg)); // Fake concept for 'Number'
            tea_add_var( "Function", ValueObject( MakeTypeInfo<FunctionPtr>("Function"), cfg));
            tea_add_var( "Tuple", ValueObject( tuple::get_type_info(), cfg));
            tea_add_var( "Const", ValueObject( MakeTypeInfo<Const>( "Const" ), cfg ) ); // Fake concept for 'const'
            tea_add_var( "IntegerSequence", ValueObject( TypeIntegerSequence, cfg ) );
            tea_add_var( "Buffer", ValueObject( TypeBuffer, cfg ) );
        }


        // _version_major | _version_minor | _version_patch | _version_combined_number | _api_version (long long) --> version information
        {
            tea_add_var( "_version_major", ValueObject( static_cast<long long>(version::Major), cfg ) );
            tea_add_var( "_version_minor", ValueObject( static_cast<long long>(version::Minor), cfg ) );
            tea_add_var( "_version_patch", ValueObject( static_cast<long long>(version::Patch), cfg ) );
            tea_add_var( "_version_combined_number", ValueObject( static_cast<long long>(version::combined_number()), cfg ) );
            tea_add_var( "_version_build_date_time", ValueObject( std::string(version::build_date_time_str()), cfg));
            tea_add_var( "_api_version", ValueObject( API_Version, cfg ) ); // Core Lib API version
        }

        // for a minimal core lib this is all already...
        if( core_level == config::LevelMinimal ) {
            rTmpContext.InjectVars( std::move( res ) );
            return;
        }

        // NOTE: since we have an explicit cast (as operator) this is a relict.
        //       But it stays here for a while since it was the only possible way to get an i64 from a f64 for a long time.
        // _f64toi64 : i64 (f64) --> converts a f64 to i64. same effect as trunc() but yields a i64. DEPRECATED, use a cast instead!
        {
            auto func = std::make_shared< LibraryFunction< decltype(f64toi64) > >( &f64toi64 );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_f64toi64", std::move( val ) );
        }

        // _out : void ( String ) --> prints param1 (String) to stdout
        if( not (opt_out & config::NoStdOut) ) {
            auto func = std::make_shared< LibraryFunction< decltype(PrintStdOut) > >( &PrintStdOut );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_out", std::move( val ) );
        }

        // _err : void ( String ) --> prints param1 (String) to stderr
        if( not (opt_out & config::NoStdErr) ) {
            auto func = std::make_shared< LibraryFunction< decltype(PrintStdError) > >( &PrintStdError );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_err", std::move( val ) );
        }

        // readline : String ( void ) --> read line from stdin (and blocks until line finished), returns the read line without line feed.
        if( not (opt_out & config::NoStdIn) ) {
            auto func = std::make_shared< LibraryFunction< decltype(ReadLine) > >( &ReadLine );
            ValueObject val{std::move( func ), cfg_mutable};
            tea_add_var( "readline", std::move( val ) ); // missing _ is intended for now.
        }

        // _exit_failure | _exit_success (long long) --> common exit codes
        {
            tea_add_var( "_exit_failure", ValueObject( static_cast<long long>(EXIT_FAILURE), cfg ) );
            tea_add_var( "_exit_success", ValueObject( static_cast<long long>(EXIT_SUCCESS), cfg ) );
        }

        // _exit : void (any) --> exits the script (with stack unwinding/scope cleanup) with param1 exit value. (this function never returns!)
        {
            Parser p; //FIXME: We need a compiled version of it!!
            auto func_node_val = p.Parse( "func ( val ) { _Exit val }", "Core" )->Eval( rTmpContext );
            tea_add_var( "_exit", std::move( func_node_val ) );
        }

        // error

        // _error_get_code : i64 ( Error ) --> returns the code of the Error as i64.
        {
            auto func = std::make_shared< LibraryFunction< decltype(ErrorGetCode) > >( &ErrorGetCode );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_error_get_code", std::move( val ) );
        }

        // _error_get_name : String ( Error ) --> returns the name of the Error as String.
        {
            auto func = std::make_shared< LibraryFunction< decltype(ErrorGetName) > >( &ErrorGetName );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_error_get_name", std::move( val ) );
        }

        // _error_get_message : String ( Error ) --> returns the message of the Error as String.
        {
            auto func = std::make_shared< LibraryFunction< decltype(ErrorGetMessage) > >( &ErrorGetMessage );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_error_get_message", std::move( val ) );
        }

        // make_runtime_error : Error ( message: String ) --> creates a Runtime Error with given message. alternative for: "str" as Error.
        if( core_level >= config::LevelUtil ) {
            auto func = std::make_shared< LibraryFunction< decltype(MakeRuntimeError) > >( &MakeRuntimeError );
            ValueObject val{std::move( func ), cfg_mutable};
            tea_add_var( "make_runtime_error", std::move( val ) ); // missing _ is intended for now.
        }

        // sequence

        // _seq : Sequence ( start: I64, end: I64, step: I64 ) --> creates an integer sequence from start to end with step step.
        {
            auto func = std::make_shared< LibraryFunction< decltype(MakeSequence) > >( &MakeSequence );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_seq", std::move( val ) );
        }

        // Buffer

        // _buf: Buffer ( size: Number )  --> creates an empty Buffer (size == 0) with capacity 'size'.
        {
            auto func = std::make_shared< LibraryFunction< decltype(MakeBuffer) > >( &MakeBuffer );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_buf", std::move( val ) );
        }

        // _buf_size: U64 ( Buffer )  --> returns the actual amount of used/filled bytes in the buffer.
        {
            auto func = std::make_shared< LibraryFunction< decltype(BufSize) > >( &BufSize );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_buf_size", std::move( val ) );
        }

        // _buf_capacity: U64 ( Buffer )  --> returns the amount of allocated memory in bytes for the buffer.
        {
            auto func = std::make_shared< LibraryFunction< decltype(BufCapacity) > >( &BufCapacity );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_buf_capacity", std::move( val ) );
        }

        // _buf_at: U8 ( Buffer, pos: Number )  --> returns byte at given position, throws on out of range.
        {
            auto func = std::make_shared< LibraryFunction< decltype(BufAt) > >(&BufAt);
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_buf_at", std::move( val ) );
        }

        // _buf_fill: Bool ( Buffer, pos: Number, count: Number, val: U8 )  --> fills the buffer from pos up to pos + count with val.
        {
            auto func = std::make_shared< LibraryFunction< decltype(BufFill) > >( &BufFill );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_buf_fill", std::move( val ) );
        }

        if( core_level >= config::LevelCore ) {
            // _buf_resize: Bool ( Buffer, size: Number )  --> resizes the buffer (shrink or grow). new values are added as zero. see _buf_fill also.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufResize) > >( &BufResize );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_resize", std::move( val ) );
            }

            // _buf_copy: Bool ( dst: Buffer, dst_off: Number, src: Buffer, src_off: Number, len: Number )  --> copies src buffer into dst buffer.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufCopy) > >( &BufCopy );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_copy", std::move( val ) );
            }
        }

        if( core_level >= config::LevelUtil ) {
            // _buf_fill32: Bool ( Buffer, pos: Number, count: Number, val: U64 )  --> fills the buffer from pos up to pos + count with val as u32
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufFill32) > >( &BufFill32 );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_fill32", std::move( val ) );
            }

            // _buf_set_u8: Bool ( Buffer, pos: Number, val: U8 )  --> sets val as U8 in buffer at given position, returns true on success.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufSetU8) > >( &BufSetU8 );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_set_u8", std::move( val ) );
            }

            // _buf_set_i8: Bool ( Buffer, pos: Number, val: I64 )  --> sets val as I8 in buffer at given position, returns true on success.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufSetI8) > >( &BufSetI8 );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_set_i8", std::move( val ) );
            }

            // _buf_set_u16: Bool ( Buffer, pos: Number, val: U64 )  --> sets val as U16 in buffer at given position, returns true on success.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufSetU16) > >( &BufSetU16 );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_set_u16", std::move( val ) );
            }

            // _buf_set_i16: Bool ( Buffer, pos: Number, val: I64 )  --> sets val as I16 in buffer at given position, returns true on success.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufSetI16) > >( &BufSetI16 );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_set_i16", std::move( val ) );
            }

            // _buf_set_u32: Bool ( Buffer, pos: Number, val: U64 )  --> sets val as U32 in buffer at given position, returns true on success.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufSetU32) > >( &BufSetU32 );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_set_u32", std::move( val ) );
            }

            // _buf_set_i32: Bool ( Buffer, pos: Number, val: I64 )  --> sets val as I32 in buffer at given position, returns true on success.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufSetI32) > >( &BufSetI32 );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_set_i32", std::move( val ) );
            }

            // _buf_set_u64: Bool ( Buffer, pos: Number, val: U64 )  --> sets val as U64 in buffer at given position, returns true on success.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufSetU64) > >( &BufSetU64 );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_set_u64", std::move( val ) );
            }

            // _buf_set_i64: Bool ( Buffer, pos: Number, val: I64 )  --> sets val as I64 in buffer at given position, returns true on success.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufSetI64) > >( &BufSetI64 );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_set_i64", std::move( val ) );
            }

            // _buf_set_string: Bool ( Buffer, pos: Number, val: String )  --> writes the String (_without_ trailing 0!) val into the buffer at given position, returns true on success.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufSetString) > >( &BufSetString );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_set_string", std::move( val ) );
            }

            // _buf_get_u8: U8|Bool ( Buffer, pos: Number )  --> gets an U8 from buffer at given position, returns Bool(false) on failure.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufGetU8) > >( &BufGetU8 );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_get_u8", std::move( val ) );
            }

            // _buf_get_i8: I64|Bool ( Buffer, pos: Number )  --> gets an I8 as I64 from buffer at given position, returns Bool(false) on failure.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufGetI8) > >( &BufGetI8 );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_get_i8", std::move( val ) );
            }

            // _buf_get_u16: U64|Bool ( Buffer, pos: Number )  --> gets an U16 as U64 from buffer at given position, returns Bool(false) on failure.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufGetU16) > >( &BufGetU16 );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_get_u16", std::move( val ) );
            }

            // _buf_get_i16: I64|Bool ( Buffer, pos: Number )  --> gets an I16 as I64 from buffer at given position, returns Bool(false) on failure.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufGetI16) > >( &BufGetI16 );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_get_i16", std::move( val ) );
            }

            // _buf_get_u32: U64|Bool ( Buffer, pos: Number )  --> gets an U32 as U64 from buffer at given position, returns Bool(false) on failure.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufGetU32) > >( &BufGetU32 );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_get_u32", std::move( val ) );
            }

            // _buf_get_i32: I64|Bool ( Buffer, pos: Number )  --> gets an I32 as I64 from buffer at given position, returns Bool(false) on failure.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufGetI32) > >( &BufGetI32 );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_get_i32", std::move( val ) );
            }

            // _buf_get_u64: U64|Bool ( Buffer, pos: Number )  --> gets an U64 from buffer at given position, returns Bool(false) on failure.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufGetU64) > >( &BufGetU64 );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_get_u64", std::move( val ) );
            }

            // _buf_get_i64: I64|Bool ( Buffer, pos: Number )  --> gets an I64 from buffer at given position, returns Bool(false) on failure.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufGetI64) > >( &BufGetI64 );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_get_i64", std::move( val ) );
            }

            // _buf_get_string: String|Bool ( Buffer, pos: Number, len: Number )  --> gets a String from buffer at given position, must be valid UTF-8, returns Bool(false) on failure.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufGetString) > >( &BufGetString );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_get_string", std::move( val ) );
            }

            // _buf_get_ascii: String|Bool ( Buffer, pos: Number, len: Number )  --> gets a String from buffer at given position, all values must be in range [0,127], returns Bool(false) on failure.
            {
                auto func = std::make_shared< LibraryFunction< decltype(BufGetAscii) > >( &BufGetAscii );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_buf_get_ascii", std::move( val ) );
            }
        }

        if( core_level >= config::LevelCore ) {
            // _strtonum : i64|Error (String) --> converts a String to i64. Returns Error on error. this works only with real String objects. alternative for '+str'.
            {
                auto func = std::make_shared< LibraryFunction< decltype(StrToNum) > >( &StrToNum );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_strtonum", std::move( val ) );
            }

            // _strtonumex : i64|u8|u64|f64|Error (String) --> converts a String to i64,u8,u64 or f64. Returns Error on error. this works only with real String objects.
            {
                auto func = std::make_shared< LibraryFunction< decltype(StrToNumEx) > >( &StrToNumEx );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_strtonumex", std::move( val ) );
            }

            // _numtostr : String (i64) --> converts a i64 to String. this works only with real i64 objects. alternative for 'num % ""'
            {
                auto func = std::make_shared< LibraryFunction< decltype(NumToStr) > >( &NumToStr );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_numtostr", std::move( val ) );
            }

#if TEASCRIPT_FMTFORMAT
            // format : String ( format: String, ... ) --> formats the string with libfmt the same way as known from C++.
            {
                auto func = std::make_shared< FormatStringFunc >();
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "format", std::move( val ) ); // missing _ is intended for now.
            }
#endif
        }

        
        if( core_level >= config::LevelUtil ) {
#if TEASCRIPT_FMTFORMAT
            if( not (opt_out & config::NoStdOut) ) {
                // cprint : void (i64, String)  --> prints the text in the given rgb color (only available with libfmt)
                auto func = std::make_shared< LibraryFunction< decltype(PrintColored) > >( &PrintColored );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "cprint", std::move( val ) ); // missing _ is intended for now.
            }
#endif

             // _print_version : void ( void ) --> prints TeaScript version to stdout
            if( not (opt_out & config::NoStdOut) ) {
                auto func = std::make_shared< LibraryFunction< decltype(PrintVersion) > >( &PrintVersion );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_print_version", std::move( val ) );
            }

            // _sqrt : f64 (f64) --> calculates square root
            {
                auto func = std::make_shared< LibraryFunction< decltype(Sqrt) > >( &Sqrt );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_sqrt", std::move( val ) );
            }

            // _trunc : f64 (f64) --> rounds the given Number towards zero as f64. e.g. 1.9 will yield 1.0, -2.9 will yield -2.0.
            {
                auto func = std::make_shared< LibraryFunction< decltype(Trunc) > >( &Trunc );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_trunc", std::move( val ) );
            }
        }

        // evaluate and load

        // _eval : Any (String) --> parses and evaluates the String as TeaScript code and returns its result.
        if( not (opt_out & config::NoEval) ) {
            auto func = std::make_shared< EvalFunc >( false );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_eval", std::move( val ) );
        }

        // eval_file : Any (String) --> parses and evaluates the content of the file and returns its result. All defined functions and variables in the top level scope will stay available.
        if( core_level >= config::LevelCore && not (opt_out & config::NoEvalFile) ) {
            auto func = std::make_shared< EvalFunc >( true );
            ValueObject val{std::move( func ), cfg_mutable};
            tea_add_var( "eval_file", std::move( val ) ); // missing _ is intended for now.
        }


        // tuple support

        // _tuple_create : Tuple ( ... ) --> creates a tuple from the passed parameters. parameter count is variable and type Any.
        {
            auto func = std::make_shared< MakeTupleFunc >();
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_tuple_create", std::move( val ) );
        }

        // _tuple_named_create : Tuple ( ... ) --> creates a tuple from the passed parameters. parameter count is variable and MUST be of type Tuple( String, Any )
        {
            auto func = std::make_shared< MakeTupleFunc >( MakeTupleFunc::eFlavor::Dictionary );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_tuple_named_create", std::move( val ) );
        }

        // _tuple_size : i64 ( Tuple ) --> returns the element count of the Tuple
        {
            auto func = std::make_shared< LibraryFunction< decltype(TupleSize) > >( &TupleSize );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_tuple_size", std::move( val ) );
        }

        // _tuple_same_types : Bool ( Tuple, Tuple ) --> checks whether the 2 tuples have the same types in exactly the same order (and with same names).
        {
            auto func = std::make_shared< LibraryFunction< decltype(TupleSameTypes) > >( &TupleSameTypes );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_tuple_same_types", std::move( val ) );
        }

        if( core_level >= config::LevelCore ) {
            // _tuple_val : Any ( Tuple, i64 ) --> returns the value at given index.
            {
                auto func = std::make_shared< LibraryFunction< decltype(TupleValue) > >( &TupleValue );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_tuple_val", std::move( val ) );
            }

            // _tuple_named_val : Any ( Tuple, String ) --> returns the value with given name (or throws)
            {
                auto func = std::make_shared< LibraryFunction< decltype(TupleNamedValue) > >( &TupleNamedValue );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_tuple_named_val", std::move( val ) );
            }

            // _tuple_set : void ( Tuple, i64, Any ) --> sets the value at given index or throws if index not exist.
            {
                auto func = std::make_shared< LibraryFunction< decltype(TupleSetValue) > >( &TupleSetValue );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_tuple_set", std::move( val ) );
            }

            // _tuple_named_set : void ( Tuple, String, Any ) --> set the value with given name (or throws)
            {
                auto func = std::make_shared< LibraryFunction< decltype(TupleSetNamedValue) > >( &TupleSetNamedValue );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_tuple_named_set", std::move( val ) );
            }

            // _tuple_append : void ( Tuple, Any ) --> appends new value to the end as new element
            {
                auto func = std::make_shared< LibraryFunction< decltype(TupleAppend) > >( &TupleAppend );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_tuple_append", std::move( val ) );
            }

            // _tuple_named_append : Bool ( Tuple, String, Any ) --> appends new value with given name to the end as new element if the name not exist yet.
            {
                auto func = std::make_shared< LibraryFunction< decltype(TupleNamedAppend) > >( &TupleNamedAppend );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_tuple_named_append", std::move( val ) );
            }

            // _tuple_insert : void ( Tuple, i64, Any ) --> inserts new value at given index
            {
                auto func = std::make_shared< LibraryFunction< decltype(TupleInsert) > >( &TupleInsert );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_tuple_insert", std::move( val ) );
            }

            // _tuple_named_insert : void ( Tuple, i64, String, Any ) --> inserts a value with given name at given index
            {
                auto func = std::make_shared< LibraryFunction< decltype(TupleNamedInsert) > >( &TupleNamedInsert );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_tuple_named_insert", std::move( val ) );
            }

            // _tuple_remove : Bool ( Tuple, i64 ) --> removes element at given index, returns whether an element has been removed.
            {
                auto func = std::make_shared< LibraryFunction< decltype(TupleRemove) > >( &TupleRemove );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_tuple_remove", std::move( val ) );
            }

            // _tuple_named_remove : Bool ( Tuple, String ) --> removes element with given name, returns whether an element has been removed.
            {
                auto func = std::make_shared< LibraryFunction< decltype(TupleNamedRemove) > >( &TupleNamedRemove );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_tuple_named_remove", std::move( val ) );
            }

            // _tuple_index_of : i64 ( Tuple, String ) --> returns the index of the element with given name or -1
            {
                auto func = std::make_shared< LibraryFunction< decltype(TupleIndexOf) > >( &TupleIndexOf );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_tuple_index_of", std::move( val ) );
            }

            // _tuple_name_of : String ( Tuple, i64 ) --> returns the name of the element with given idx (or throws)
            {
                auto func = std::make_shared< LibraryFunction< decltype(TupleNameOf) > >( &TupleNameOf );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_tuple_name_of", std::move( val ) );
            }

            // _tuple_swap : void ( Tuple, i64, i64 ) --> swaps elements of given indices
            {
                auto func = std::make_shared< LibraryFunction< decltype(TupleSwapValues) > >( &TupleSwapValues );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_tuple_swap", std::move( val ) );
            }

            // tuple_print : void ( Tuple, String, i64 ) --> prints (recursively) all (named) elements, for debugging (String param is the "root" name, last param is for max nesting level)
            if( not (opt_out & config::NoStdOut) ) {
                auto func = std::make_shared< LibraryFunction< decltype(TuplePrint) > >( &TuplePrint );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "tuple_print", std::move( val ) ); // missing _ is intended for now.
            }
        }


        // minimalistic string support

        // _strlen : i64 ( String ) --> returns the length of the string in bytes (excluding the ending 0).
        {
            auto func = std::make_shared< LibraryFunction< decltype(StrLength) > >( &StrLength );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_strlen", std::move( val ) );
        }

        // _strglyphs : i64 ( String ) --> returns the utf-8 (Unicode) glyph count of the string (excluding the ending 0).
        {
            auto func = std::make_shared< LibraryFunction< decltype(StrUTF8GlyphCount) > >( &StrUTF8GlyphCount );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_strglyphs", std::move( val ) );
        }

        // _strglyphtobytepos : i64 ( String, i64 ) --> returns the byte position of the given glyph in given string, or -1 if out of range.
        {
            auto func = std::make_shared< LibraryFunction< decltype(StrUTF8GlyphToBytePos) > >( &StrUTF8GlyphToBytePos );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_strglyphtobytepos", std::move( val ) );
        }

        // _strat : String ( String, i64 ) --> returns a substring of one complete UTF-8 code point where position points into. empty string if out of range.
        {
            auto func = std::make_shared< LibraryFunction< decltype(StrAt) > >( &StrAt );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_strat", std::move( val ) );
        }

        // _substr : String ( String, from: i64, count: i64 ) --> returns a substring [from, from+count). count -1 means until end of string. returns empty string on invalid arguments.
        {
            auto func = std::make_shared< LibraryFunction< decltype(SubStr) > >( &SubStr );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_substr", std::move( val ) );
        }

        // _strfind : i64 ( String, substring: String, offset: i64 ) --> searches for substring from offset and returns found pos of first occurrence. -1 if not found.
        {
            auto func = std::make_shared< LibraryFunction< decltype(StrFind) > >( &StrFind );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_strfind", std::move( val ) );
        }

        // _strfromascii : String|Error ( char: Number ) --> returns a String build from the ascii char. For invalid chars (>127) Error will be returned.
        {
            auto func = std::make_shared< LibraryFunction< decltype(StrFromAscii) > >( &StrFromAscii );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_strfromascii", std::move( val ) );
        }


        if( core_level >= config::LevelCore ) {
            // _strfindreverse : i64 ( String, substring: String, offset: i64 ) --> searches for substring from behind from offset and returns found pos of first occurrence. -1 if not found.
            {
                auto func = std::make_shared< LibraryFunction< decltype(StrReverseFind) > >( &StrReverseFind );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_strfindreverse", std::move( val ) );
            }

            // _strreplacepos : Bool ( str: String, start: i64, count: i64, new: String ) --> replaces the section [start, start+count) in str with new. returns false on error, e.g. start is out of range.
            {
                auto func = std::make_shared< LibraryFunction< decltype(StrReplacePos) > >( &StrReplacePos );
                ValueObject val{std::move( func ), cfg};
                tea_add_var( "_strreplacepos", std::move( val ) );
            }
        }


        // time / misc

        // _timestamp : f64 ( void ) --> gets the elapsed time in (fractional) seconds as f64 from an unspecified time point during program start. Time is monotonic increasing.
        {
            auto func = std::make_shared< LibraryFunction< decltype(GetTimeStamp) > >( &GetTimeStamp );
            ValueObject val{std::move( func ), cfg};
            tea_add_var( "_timestamp", std::move( val ) );
        }

        if( core_level >= config::LevelUtil ) {
            // clock : f64 ( void ) --> gets the local wall clock time of the current day in (fractional) seconds as f64. 
            {
                auto func = std::make_shared< LibraryFunction< decltype(GetLocalTimeInSecs) > >( &GetLocalTimeInSecs );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "clock", std::move( val ) ); // missing _ is intended for now.
            }

            // clock_utc : f64 ( void ) --> gets the UTC time of the current day in (fractional) seconds as f64.
            {
                auto func = std::make_shared< LibraryFunction< decltype(GetUTCTimeInSecs) > >( &GetUTCTimeInSecs );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "clock_utc", std::move( val ) ); // missing _ is intended for now.
            }

            // sleep : void ( i64 ) --> sleeps (at least) for given amount of seconds.
            {
                auto func = std::make_shared< LibraryFunction< decltype(SleepSecs) > >( &SleepSecs );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "sleep", std::move( val ) );
            }

            // random : i64 ( i64, i64 ) --> creates a random number in between [start,end]. start, end must be >= 0 and <= UINT_MAX.
            {
                auto func = std::make_shared< LibraryFunction< decltype(CreateRandomNumber) > >( &CreateRandomNumber );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "random", std::move( val ) );
            }

#if TEASCRIPT_TOMLSUPPORT
            // readtomlstring : Tuple|Error ( String ) --> creates a named tuple from the given TOML formatted string (or Error on error).
            {
                auto func = std::make_shared< LibraryFunction< decltype(TomlSupport::ReadTomlString) > >( &TomlSupport::ReadTomlString );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "readtomlstring", std::move( val ) ); // missing _ is intended
            }

            // writetomlstring : String|Error ( Tuple ) --> creates a TOML formatted string from the given tuple (or Error on error).
            {
                auto func = std::make_shared< LibraryFunction< decltype(TomlSupport::WriteTomlString) > >( &TomlSupport::WriteTomlString );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "writetomlstring", std::move( val ) ); // missing _ is intended
            }

            // toml_make_array : Tuple ( ... ) --> creates a compatible toml array from the passed parameters. parameter count is variable and type Any.
            {
                auto func = std::make_shared< MakeTupleFunc >( MakeTupleFunc::eFlavor::TomlJsonArray );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "toml_make_array", std::move( val ) ); // missing _ is intended
            }

            // toml_is_array : Bool ( Any ) --> checks if given parameter is an Toml compatible array.
            {
                auto func = std::make_shared< LibraryFunction< decltype(tuple::TomlJsonUtil::IsAnArray) > >( &tuple::TomlJsonUtil::IsAnArray );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "toml_is_array", std::move( val ) ); // missing _ is intended
            }

            // toml_array_empty : Bool ( Tuple ) --> checks if given Tuple is an empty Toml array.
            {
                auto func = std::make_shared< LibraryFunction< decltype(tuple::TomlJsonUtil::IsArrayEmpty) > >( &tuple::TomlJsonUtil::IsArrayEmpty );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "toml_array_empty", std::move( val ) ); // missing _ is intended
            }

            // toml_array_append : void ( Tuple, Any ) --> appends value to Toml comaptible array (the Tuple must be compatible, e.g., toml_is_array returned true!)
            {
                auto func = std::make_shared< LibraryFunction< decltype(tuple::TomlJsonUtil::ArrayAppend) > >( &tuple::TomlJsonUtil::ArrayAppend );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "toml_array_append", std::move( val ) ); // missing _ is intended
            }

            // toml_array_insert : Bool ( Tuple, I64, Any ) --> inserts value at idx to Toml comaptible array (the Tuple must be compatible, e.g., toml_is_array returned true!)
            {
                auto func = std::make_shared< LibraryFunction< decltype(tuple::TomlJsonUtil::ArrayInsert) > >( &tuple::TomlJsonUtil::ArrayInsert );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "toml_array_insert", std::move( val ) ); // missing _ is intended
            }

            // toml_array_remove : Bool ( Tuple, I64 ) --> removes value at idx from Toml comaptible array (the Tuple must be compatible, e.g., toml_is_array returned true!)
            {
                auto func = std::make_shared< LibraryFunction< decltype(tuple::TomlJsonUtil::ArrayRemove) > >( &tuple::TomlJsonUtil::ArrayRemove );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "toml_array_remove", std::move( val ) ); // missing _ is intended
            }
#endif

#if TEASCRIPT_JSONSUPPORT
            // readjsonstring : Any ( String ) --> creates a value of appropriate type from the given JSON formatted string (or Error on error).
            {
                auto func = std::make_shared< LibraryFunction< decltype(JsonSupport<>::ReadJsonString) > >( &JsonSupport<>::ReadJsonString );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "readjsonstring", std::move( val ) ); // missing _ is intended
            }

            // writejsonstring : String|Error ( Any ) --> creates a Json formatted string (or Error on error).
            {
                auto func = std::make_shared< LibraryFunction< decltype(JsonSupport<>::WriteJsonString) > >( &JsonSupport<>::WriteJsonString );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "writejsonstring", std::move( val ) ); // missing _ is intended
            }

            // json_make_array : Tuple ( ... ) --> creates a compatible json array from the passed parameters. parameter count is variable and type Any.
            {
                auto func = std::make_shared< MakeTupleFunc >( MakeTupleFunc::eFlavor::TomlJsonArray );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "json_make_array", std::move( val ) ); // missing _ is intended
            }

            // jaon_is_array : Bool ( Any ) --> checks if given parameter is an Json compatible array.
            {
                auto func = std::make_shared< LibraryFunction< decltype(tuple::TomlJsonUtil::IsAnArray) > >( &tuple::TomlJsonUtil::IsAnArray );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "json_is_array", std::move( val ) ); // missing _ is intended
            }

            // json_array_empty : Bool ( Tuple ) --> checks if given Tuple is an empty Json array.
            {
                auto func = std::make_shared< LibraryFunction< decltype(tuple::TomlJsonUtil::IsArrayEmpty) > >( &tuple::TomlJsonUtil::IsArrayEmpty );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "json_array_empty", std::move( val ) ); // missing _ is intended
            }

            // json_array_append : void ( Tuple, Any ) --> appends value to Json comaptible array (the Tuple must be compatible, e.g., json_is_array returned true!)
            {
                auto func = std::make_shared< LibraryFunction< decltype(tuple::TomlJsonUtil::ArrayAppend) > >( &tuple::TomlJsonUtil::ArrayAppend );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "json_array_append", std::move( val ) ); // missing _ is intended
            }

            // json_array_insert : Bool ( Tuple, I64, Any ) --> inserts value at idx to Json comaptible array (the Tuple must be compatible, e.g., json_is_array returned true!)
            {
                auto func = std::make_shared< LibraryFunction< decltype(tuple::TomlJsonUtil::ArrayInsert) > >( &tuple::TomlJsonUtil::ArrayInsert );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "json_array_insert", std::move( val ) ); // missing _ is intended
            }

            // json_array_remove : Bool ( Tuple, I64 ) --> removes value at idx from Json comaptible array (the Tuple must be compatible, e.g., json_is_array returned true!)
            {
                auto func = std::make_shared< LibraryFunction< decltype(tuple::TomlJsonUtil::ArrayRemove) > >( &tuple::TomlJsonUtil::ArrayRemove );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "json_array_remove", std::move( val ) ); // missing _ is intended
            }

            // EXPERIMENTAL BSON support (nlohmann adapter must be used!)
# if TEASCRIPT_BSONSUPPORT
            // readbsonbuffer : Any ( Buffer ) --> creates a value of appropriate type from the given BSON buffer (or Error on error).
            {
                auto func = std::make_shared< LibraryFunction< decltype(JsonSupport<>::ReadBsonBuffer) > >( &JsonSupport<>::ReadBsonBuffer );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "readbsonbuffer", std::move( val ) ); // missing _ is intended
            }

            // writebsonbuffer : Buffer|Error ( Any ) --> creates a Bson buffer (or Error on error).
            {
                auto func = std::make_shared< LibraryFunction< decltype(JsonSupport<>::WriteBsonBuffer) > >( &JsonSupport<>::WriteBsonBuffer );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "writebsonbuffer", std::move( val ) ); // missing _ is intended
            }
# endif
#endif
        }

        // minimalistic (text) file io support
        if( core_level >= config::LevelUtil && (config::NoFileWrite | config::NoFileRead | config::NoFileDelete) !=
                                             (opt_out & (config::NoFileWrite | config::NoFileRead | config::NoFileDelete)) ) {
            // cwd : String ( void ) --> returns the current working directory as String
            {
                auto func = std::make_shared< LibraryFunction< decltype(CurrentPath) > >( &CurrentPath );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "cwd", std::move( val ) ); // missing _ is intended for now.
            }

            // change_cwd : Bool ( String ) --> changes the current working dir
            {
                auto func = std::make_shared< LibraryFunction< decltype(ChangeCurrentPath) > >( &ChangeCurrentPath );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "change_cwd", std::move( val ) ); // missing _ is intended for now.
            }

            // tempdir : String ( void ) --> returns configured temp directory as String
            {
                auto func = std::make_shared< LibraryFunction< decltype(TempPath) > >( &TempPath );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "tempdir", std::move( val ) ); // missing _ is intended for now.
            }

            // path_exists : Bool ( String ) --> returns whether path in String exists as directory or file.
            {
                auto func = std::make_shared< LibraryFunction< decltype(PathExists) > >( &PathExists );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "path_exists", std::move( val ) ); // missing _ is intended for now.
            }

            // file_size : i64 ( String ) --> returns file size in bytes. -1 on error / file not exists / is not a file.
            {
                auto func = std::make_shared< LibraryFunction< decltype(FileSize) > >( &FileSize );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "file_size", std::move( val ) ); // missing _ is intended for now.
            }

            // last_modified : String ( String ) --> returns the last modified time as String for the given path or empty string if not exists/error.
            {
                auto func = std::make_shared< LibraryFunction< decltype(LastModified) > >( &LastModified );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "last_modified", std::move( val ) ); // missing _ is intended for now.
            }

            // readdirfirst : Tuple ( String ) --> returns the first direntry of given path (see direntry for details)
            {
                auto func = std::make_shared< LibraryFunction< decltype(ReadDirFirst) > >( &ReadDirFirst );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "readdirfirst", std::move( val ) ); // missing _ is intended for now.
            }

            // readdirnext : Tuple ( Tuple ) --> returns the next direntry of given direntry (see direntry for details)
            {
                auto func = std::make_shared< LibraryFunction< decltype(ReadDirNext) > >( &ReadDirNext );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "readdirnext", std::move( val ) ); // missing _ is intended for now.
            }
        }

        if( core_level >= config::LevelFull && not (opt_out & (config::NoFileRead|config::NoFileWrite)) )
        {
            // file_copy : Bool ( file: String, dest_dir: String, overwrite: Bool ) 
            // --> copies file to dest_dir if not exist or overwrite is true
            {
                auto func = std::make_shared< LibraryFunction< decltype(FileCopy) > >( &FileCopy );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "file_copy", std::move( val ) ); // missing _ is intended for now.
            }

            // file_copy_newer : Bool ( file: String, dest_dir: String ) 
            // --> copies file to dest_dir if not exist or file is newer as the file in dest_dir
            {
                auto func = std::make_shared< LibraryFunction< decltype(FileCopyIfNewer) > >( &FileCopyIfNewer );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "file_copy_newer", std::move( val ) ); // missing _ is intended for now.
            }
        }

        // readtextfile : String|Error ( String ) --> reads the content of an UTF-8 text file and returns it in a String. An optional BOM is removed.
        if( core_level >= config::LevelFull && not (opt_out & config::NoFileRead) )
        {
            auto func = std::make_shared< LibraryFunction< decltype(ReadTextFile) > >( &ReadTextFile );
            ValueObject val{std::move( func ), cfg_mutable};
            tea_add_var( "readtextfile", std::move( val ) ); // missing _ is intended for now.
        }

        // readfile : Buffer|Error ( String ) --> reads the content of a file and returns it in a Buffer.
        if( core_level >= config::LevelFull && not (opt_out & config::NoFileRead) ) {
            auto func = std::make_shared< LibraryFunction< decltype(ReadBinaryFile) > >( &ReadBinaryFile );
            ValueObject val{std::move( func ), cfg_mutable};
            tea_add_var( "readfile", std::move( val ) ); // missing _ is intended for now.
        }


        if( core_level >= config::LevelFull && not (opt_out & config::NoFileWrite) )
        {
            // create_dir : Bool ( String, Bool ) --> creates directories for path in String. Bool == true means recursively
            {
                auto func = std::make_shared< LibraryFunction< decltype(CreateDir) > >( &CreateDir );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "create_dir", std::move( val ) ); // missing _ is intended for now.
            }

            // writetextfile : Bool ( file: String, str: String, overwrite: Bool, bom: Bool ) 
            // --> writes the content of String to text file. An optional UTF-8 BOM can be written (last Bool param). overwrite indicates if a prior existing file shall be overwritten (old content destroyed!)
            {
                auto func = std::make_shared< LibraryFunction< decltype(WriteTextFile) > >( &WriteTextFile );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "writetextfile", std::move( val ) ); // missing _ is intended for now.
            }

            // writefile : Bool ( file: String, content: Buffer, overwrite: Bool ) 
            // --> writes the content of the Buffer into file. overwrite indicates if a prior existing file shall be overwritten (old content destroyed!)
            {
                auto func = std::make_shared< LibraryFunction< decltype(WriteBinaryFile) > >( &WriteBinaryFile );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "writefile", std::move( val ) ); // missing _ is intended for now.
            }
        }

        if( core_level >= config::LevelFull && not (opt_out & config::NoFileDelete) ) 
        {
            // path_delete : Bool ( String ) --> deletes(!) file or (empty) directory
            {
                auto func = std::make_shared< LibraryFunction< decltype(PathDelete) > >( &PathDelete );
                ValueObject val{std::move( func ), cfg_mutable};
                tea_add_var( "path_delete", std::move( val ) ); // missing _ is intended for now.
            }
        }

        rTmpContext.InjectVars( std::move( res ) );
    }
public:
    CoreLibrary() = default;
    virtual ~CoreLibrary() {}

    // The source code parts of TeaScript Core Library.

    static constexpr char core_lib_util[] = R"_SCRIPT_(
// convenience for can write 'return void' if function shall return nothing
const void := () // void has value NaV (Not A Value)

// constant number PI
const PI := 3.14159265358979323846


// exits the script (with stack unwinding/scope cleanup) with code EXIT_FAILURE
func fail()
{
    fail_with_error( _exit_failure )
}

// exits the script (with stack unwinding/scope cleanup) with error_code
func fail_with_error( error_code )
{
    _exit( error_code )
}

// converts val to string (note: if val is an integer _numtostr is an alternative)
func to_string( val )
{
    val % ""
}

// converts val to a Number. returns Error on error. (note: if val is a String _strtonum / _strtonumex is an alternative)
func to_number( val )
{
    if( val is String ) {
        _strtonumex( val ) // this can convert i64 and f64
    } else if( val is Number ) {
        val
    } else {
        val as i64 //TODO: error handling!
    }
}

// converts val to f64. val must be a number already! returns Error on error.
// example use case: to_f64( to_number( some_var ) ) // ensures some_var is converted to f64
// NOTE: this function is only provisionally and will be replaced by a cast later!
func to_f64( val )
{
    if( val is Number ) { val as f64 } else { "Not a number!" as Error }
}

// convenience function. ensures given Number is used as i64. returns Error on error.
// example use case: to_i64( to_number( some_var ) ) // ensures some_var is converted to i64
// NOTE: this function is only provisionally and will be replaced by a cast later!
func to_i64( val )
{
    if( val is Number ) {
        val as i64
    } else {
        "Not a number!" as Error
    }
}

// returns the minimum of a and b
func min( a, b )
{
    if( a < b ) { a } else { b }
}

// returns the maximum of a and b
func max( a, b )
{
    if( b < a ) { a } else { b }
}

// returns low if val is less than low, high if val is greater than high, otherwise val. garbage in, garbage out.
func clamp( val, low, high )
{
    min( max( val, low ), high )
}

// swaps the values of a and b (a and b are passed via shared assign)
func swap( a @=, b @= )
{
    if( not (a @@ b) ) { // only if b is not shared by a
        const tmp := a
        a := b
        b := tmp
    }
    void
}

// convenience for _strfind with default offset
func strfind( str, what, offset := 0 )
{
    _strfind( str, what, offset )
}

// returns the absolute value of n (as same type as n). n must be a Number.
func abs( n )
{
    if( n < 0 ) { -n } else { n }
}

// rounds up or down the given Number to nearest integer as f64. e.g. 1.1 will yield 1.0, 1.6 as well as 1.5 will yield 2.0
func round( n )
{
    const num := (n + 0.0)
    _trunc( num + if( num < 0 ) { -0.5 } else { 0.5 } )
}

// make_rgb : i64 (r: i64, g: i64, b: i64) --> makes a 32 bit rgb color (garbage in, garbage out)
// (same level as cprint)
func make_rgb( r, g, b )
{
    r bit_lsh 16 bit_or g bit_lsh 8 bit_or b
}

// increments by given step
func inc( n @=, step := 1 )
{
    n := n + step
}

// decrements by given step
func dec( n @=, step := 1 )
{
    n := n - step
}

// fills the complete capacity of the buffer with zeroes (convenience). post: size == capacity
func buf_zero( buf @= )
{
    _buf_fill( buf, 0, -1, 0u8 )
}
)_SCRIPT_";

    static constexpr char core_lib_stdout[] = R"_SCRIPT_(
// prints s to stdout, will do to string conversion of s
func print( s )
{
    _out( s % "" )
}

// prints s + line feed to stdout, will do to string conversion of s
func println( s )
{
    _out( s % "\n" )
}

// cprintln : void (i64, String)  --> same as cprint but adds a new line to the end.
is_defined cprint and (func cprintln( rgb, s )
{ 
    cprint( rgb, s % "\n" ) 
})
)_SCRIPT_";

    static constexpr char core_lib_stderr[] = R"_SCRIPT_(
// prints e + line feed to stderr, will do to string conversion of e (usually e should be String or Error)
func print_error( e )
{
    //TODO: add log to common logfile
    _err( e % "\n" )
}

// prints err to stderr (usually err should be String or Error), exits the script (with stack unwinding/scope cleanup) with error_code
func fail_with_message( err, error_code := _exit_failure )
{
    print_error( err )
    fail_with_error( error_code )
}
)_SCRIPT_";

    static constexpr char core_lib_file[] = R"_SCRIPT_(
func file_exists( file )
{
    file_size( file ) >= 0
}
)_SCRIPT_";

#if TEASCRIPT_TOMLSUPPORT
    static constexpr char core_lib_toml[] = R"_SCRIPT_(
func toml_is_table( const val @= )
{
    not toml_is_array( val ) and val is Tuple
}

def toml_table_size := _tuple_size

def toml_make_table := _tuple_named_create

func toml_array_size( const arr @= )
{
    if( toml_array_empty( arr ) ) { 0 } else { _tuple_size( arr ) }
}
)_SCRIPT_";
    static constexpr char core_lib_toml_read[] = R"_SCRIPT_(
func readtomlfile( file )
{
    const content := readtextfile( file ) catch( err ) return err
    readtomlstring( content )
}
)_SCRIPT_";
    static constexpr char core_lib_toml_write[] = R"_SCRIPT_(
func writetomlfile( tuple, file, overwrite := false )
{
    // writetextfile returns a Bool, so do we!
    const content := writetomlstring( tuple ) catch return false
    writetextfile( file, content, overwrite, false )
}
)_SCRIPT_";
#endif

#if TEASCRIPT_JSONSUPPORT
    static constexpr char core_lib_json[] = R"_SCRIPT_(
func json_is_object( const val @= )
{
    not json_is_array( val ) and val is Tuple
}

def json_object_size := _tuple_size

def json_make_object := _tuple_named_create

func json_array_size( const arr @= )
{
    if( json_array_empty( arr ) ) { 0 } else { _tuple_size( arr ) }
}
)_SCRIPT_";
    static constexpr char core_lib_json_write[] = R"_SCRIPT_(
func writejsonfile( tuple, file, overwrite := false )
{
    // writetextfile returns a Bool, so do we!
    const content := writejsonstring( tuple ) catch return false
    writetextfile( file, content, overwrite, false )
}
)_SCRIPT_";
    static constexpr char core_lib_json_read[] = R"_SCRIPT_(
func readjsonfile( file )
{
    const content := readtextfile( file ) catch( err ) return err
    readjsonstring( content )
}
)_SCRIPT_";
#endif

    static constexpr char core_lib_teascript[] = R"_SCRIPT_(
// checks whether the tuple contains the given name or index
func tuple_contains( const tup @=, idx_or_name )
{
    if( idx_or_name is String ) {
        _tuple_index_of( tup, idx_or_name ) >= 0
    } else {
        _tuple_size( tup ) > idx_or_name
    }
}

// pushes value to end of stack / tuple
func stack_push( stack @=, val @= )
{
    _tuple_append( stack, val )
}

// pops value from stack / tuple
func stack_pop( stack @= )
{
    const idx := _tuple_size( stack ) - 1
    if( idx >= 0 ) {
        const  val := _tuple_val( stack, idx )
        _tuple_remove( stack, idx )
        val
    } else {
        void
    }
}

func strreplacefirst( str @=, what, new, offset := 0 )
{
    const pos := _strfind( str, what, offset )
    if( pos >= 0 ) {
        _strreplacepos( str, pos, _strlen( what ), new )
    } else {
        false
    }
}

func strreplacelast( str @=, what, new, offset := -1 ) // offset -1 == whole string
{
    const pos := _strfindreverse( str, what, offset )
    if( pos >= 0 ) {
        _strreplacepos( str, pos, _strlen( what ), new )
    } else {
        false
    }
}

// trims the string if it starts or ends with characters in given set.
// e.g. strtrim( s, " \t\r\n", false, true ) will remove all spaces, tabs, carriage returns and new lines at the end of the string.
func strtrim( def str @=, set, leading := true, trailing := true )
{
    def res := false
    if( leading ) {
        def c := 0
        repeat {
            const x := _strat( str, c ) // will return a complete UTF-8 code point.
            if( _strfind( set, x, 0 ) >= 0 ) {
                c := c + _strlen( x )
            } else {
                stop
            }
        }
        if( c > 0 ) {
            res := _strreplacepos( str, 0, c, "" )
        }
    }
    if( trailing ) {
        def i := _strlen( str ) - 1
        def c := 0
        repeat {
            const x := _strat( str, i - c ) // will return a complete UTF-8 code point.
            if( _strfind( set, x, 0 ) >= 0 ) {
                c := c + _strlen( x )
            } else {
                stop
            }
        }
        if( c > 0 ) {
            res := _strreplacepos( str, i - c + 1, -1, "" ) or res
        }
    }
    res
}

// splits the given string at every occurring separator and returns a tuple with the elements.
func strsplit( const str @=, separator, skip_empty := false )
{
    def res := _tuple_create()
    const str_size := _strlen( str )
    if( str_size == 0 ) {
        return res
    }
    const sep_size := _strlen( separator )
    if( sep_size == 0 ) {
        def res.0 := str
        return res
    }
    def pos := 0
    repeat {
        const x := _strfind( str, separator, pos )
        if( x >= 0 ) {
            const s := _substr( str, pos, x - pos )
            if( not skip_empty or _strlen( s ) > 0 ) {
                _tuple_append( res, s )
            }
            pos := x + sep_size
            if( pos == str_size ) {
                if( not skip_empty ) {
                    _tuple_append( res, "" )
                }
                stop
            }
        } else {
            // do we have a last element?
            if( pos < str_size ) {
                _tuple_append( res, _substr( str, pos, -1 ) )
            }
            stop
        }
    }
    res
}

// joins all elements of a tuple to a string with given separator.
func strjoin( const tup @=, separator, add_leading := false, add_trailing := false )
{
    def res := ""
    if( add_leading ) {
        res := separator
    }
    const last := _tuple_size( tup ) - 1
    forall( idx in tup ) {
        res := res % tup[ idx ]
        if( idx != last ) {
            res := res % separator
        }
    }
    if( add_trailing ) { // this covers also empty tuple case
        res := res % separator
    }
    res
}

// creates an utf-8 iterator for the given string and sets .cur to first utf-8 glyph.
func utf8_begin( const str @=  )
{
    def   it       := _tuple_create()
    def   it.cur   := ""
    def   it._pos  := 0
    const it._base @= str
    const it._size := _strlen( str )
    
    it.cur := _strat( str, 0 )
    
    it
}

// checks whether the given utf-8 iterator is at end.
func utf8_end( const it @= )
{
    it._pos + _strlen( it.cur ) >= it._size
}

// advances the utf-8 iterator to next utf-8 glyph and updates .cur. 
// if there is no more glyph it will point to the end of the string (\0).
func utf8_next( it @= )
{
    const npos := it._pos + _strlen( it.cur )
    it.cur  := _strat( it._base, npos )
    it._pos := npos
    
    it
}


// computes power of n with integer exponent. if exp is a float it will get truncated. returns a f64.
func pow( n, exp )
{
    const num := (n + 0.0)  // make a f64
    def   e   := exp as i64 // ensure an integer is used.
    def   res := 1.0
    repeat {
        if( e == 0 ) { stop }
        if( e > 0 ) {
            res := res * num
            e := e - 1
        } else {
            res := res / num
            e := e + 1
        }
    }
    res
}

// convenience for call _sqrt with other types than f64
func sqrt( val )
{
    _sqrt( val + 0.0 )
}

// convenience for call _trunc with other types than f64
func trunc( n )
{
    _trunc( n + 0.0 )
}

// rounds down the given Number to next smaller integer as f64. e.g. 1.9 will yield 1.0, -2.1 will yield -3.0
func floor( n )
{
    const num := (n + 0.0)
    const trunced := _trunc( num )
    if( trunced == num or num > 0.0 ) { // integer already or positive (then trunced is correct)
        trunced
    } else { // < 0.0 and not trunced
        trunced - 1.0
    }
}

// rounds up the given Number to next greater integer as f64. e.g. 1.1 will yield 2.0, -1.9 will yield -1.0
func ceil( n )
{
    const num := (n + 0.0)
    const trunced := _trunc( num )
    if( trunced == num or num < 0.0 ) { // integer already or negative (then trunced is correct)
        trunced
    } else { // > 0.0 and not trunced
        trunced + 1.0
    }
}


// computes the hour, minute, second and (optionally) millisecond part of given time in seconds (e.g. from clock())
// note: hours can be greater than 23/24, it will not be cut at day boundary!
func timevals( t, HH @=, MM @=, S @=, ms @= 0 )
{
    if( t is f64 and t >= 0.0 ) {
        const secs := t as i64
        HH   := secs / 60 / 60
        MM   := (secs - (HH * 60 * 60)) / 60
        S    := (secs - (HH * 60 * 60) - (MM * 60))
        ms   := ( (t - secs) * 1000.0 ) as i64
        true
    } else {
        false
    }
}

// builds a 24 hour wall clock string with the format HH:MM:SS.mmm (milliseconds are optional)
// note: if t is greater than 24 hours it will not be cut.
func timetostr( t, with_ms := false )
{
    def HH := 0, def MM := 0, def S := 0, def ms := 0
    if( timevals( t, HH, MM, S, ms ) ) {
        const hours   := if( HH < 10 ) { "0" % HH } else { "" % HH }
        const minutes := if( MM < 10 ) { "0" % MM } else { "" % MM }
        const seconds := if( S < 10  ) { "0" % S  } else { "" % S  }
        if( with_ms ) {
            const millis := if( ms < 10  ) { "00" % ms  } else if( ms < 100 ) { "0" % ms  } else { "" % ms  }
            "%(hours):%(minutes):%(seconds).%(millis)"
        } else {
            "%(hours):%(minutes):%(seconds)"
        }
    } else {
        false
    }
}

func rolldice( eyes := 6 )
{
    random( 1, eyes )
}

//const ts_core_init_done_stamp := _timestamp()

)_SCRIPT_";


    /// Will bootstrap the standard Core Lib into the Context. \param config specifies what will be loaded.
    /// IMPORTANT: Any previous data in rContext will be lost / overwritten.
    virtual void Bootstrap( Context &rContext, config::eConfig const config, bool const eval_only = false )
    {
        {
            //TODO: Move the internal type registration to a better place.
            TypeSystem  sys;
            sys.RegisterType<FunctionPtr>("Function");
            sys.RegisterType<std::vector<ValueObject>>("ValueObjectVector");

            Context tmp{ std::move( sys ), true };
            tmp.is_debug = rContext.is_debug; // take over from possible old instance.
            tmp.dialect  = rContext.dialect;  // take over from possible old instance.

            BuildInternals( tmp, config );

            rContext = std::move( tmp );
            // finalize
            rContext.SetBootstrapDone();
        }

        if( (config & config::LevelMask) < config::LevelUtil ) {
            return;
        }


        Parser p; //FIXME: for later versions: must use correct state with correct factory.
        //p.OverwriteDialect( rContext.dialect ); // internal core lib always shall use default dialect
#if !defined(NDEBUG)  //TODO: Do we want this block always enabled for the internal core lib?
        p.SetDebug( rContext.is_debug );
        eOptimize opt_level = rContext.is_debug ? eOptimize::Debug : eOptimize::O0;
#else
        eOptimize opt_level = eOptimize::O1;
#endif

        p.ParsePartial( core_lib_util, "Core" );
        if( not (config & config::NoStdOut) ) {
            p.ParsePartial( core_lib_stdout, "Core" );
        }
        if( not (config & config::NoStdErr) ) {
            p.ParsePartial( core_lib_stderr, "Core" );
        }

#if TEASCRIPT_TOMLSUPPORT
        p.ParsePartial( core_lib_toml, "Core" );
#endif
#if TEASCRIPT_JSONSUPPORT
        p.ParsePartial( core_lib_json, "Core" );
#endif

        if( (config & config::LevelMask) >= config::LevelFull ) {
            if( (config::NoFileWrite | config::NoFileRead | config::NoFileDelete) !=
                (config & (config::NoFileWrite | config::NoFileRead | config::NoFileDelete)) ) {
                p.ParsePartial( core_lib_file, "Core" );
            }
#if TEASCRIPT_TOMLSUPPORT
            if( not (config & config::NoFileRead) ) {
                p.ParsePartial( core_lib_toml_read, "Core" );
            }
            if( not (config & config::NoFileWrite) ) {
                p.ParsePartial( core_lib_toml_write, "Core" );
            }
#endif
#if TEASCRIPT_JSONSUPPORT
            if( not (config & config::NoFileRead) ) {
                p.ParsePartial( core_lib_json_read, "Core" );
            }
            if( not (config & config::NoFileWrite) ) {
                p.ParsePartial( core_lib_json_write, "Core" );
            }
#endif
            p.ParsePartial( core_lib_teascript, "Core" );
        }

        auto ast = p.ParsePartialEnd();

        if( eval_only ) {
            ast->Eval( rContext );
        } else {
            StackVM::Compiler  compiler;
            auto program = compiler.Compile( ast, opt_level );
            StackVM::Machine<false>  machine;
            machine.Exec( program, rContext );
            machine.ThrowPossibleErrorException();
        }
    }
};

} // namespace teascript
