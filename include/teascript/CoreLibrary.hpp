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

#if defined _MSC_VER  && !defined _SILENCE_CXX20_U8PATH_DEPRECATION_WARNING
# define _SILENCE_CXX20_U8PATH_DEPRECATION_WARNING
#endif
#if defined _MSC_VER  && !defined _CRT_SECURE_NO_WARNINGS
# define _CRT_SECURE_NO_WARNINGS
#endif

#include "Func.hpp"
#include "Context.hpp"
#include "Type.hpp"
#include "TupleUtil.hpp"
#include "Print.hpp"
#include "Parser.hpp"
#include "version.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <random>
#include <thread>

namespace teascript {

namespace config {

/// Config enum for specify what shall be loaded.
enum eConfig
{
    LevelMask           = 0x00'00'00'0f,
    FeatureOptOutMask   = 0xff'ff'ff'00,

    // level numbers are not or'able, just have some spare room for future extensions.
    LevelMinimal        = 0x00'00'00'00, /// loading only Types and version variables NOTE: The language and usage is very limited since even basic things like creation of empty tuple or length of string are not available.
    LevelCoreReduced    = 0x00'00'00'01, /// this is a reduced variant of the Core level where not all string / tuple utilities are loaded. Not all language features / built-in types are fully usable in this mode.
    LevelCore           = 0x00'00'00'02, /// loading full tuple / string utility and some other type utilities. Language and its built-in types are fully usable.
    LevelUtil           = 0x00'00'00'04, /// loading more library utilities like clock, random, sleep, some math functions, etc.
    LevelFull           = 0x00'00'00'08, /// loading all normal and standard stuff.

    // optional feature disable (counts from Level >= LevelCoreReduced, below its always disabled)

    NoStdIn             = 0x00'00'01'00,
    NoStdErr            = 0x00'00'02'00,
    NoStdOut            = 0x00'00'04'00,
    NoFileRead          = 0x00'00'08'00,
    NoFileWrite         = 0x00'00'10'00,
    NoFileDelete        = 0x00'00'20'00,
    NoEval              = 0x00'00'40'00,
    NoEvalFile          = NoFileRead | NoEval,
    //NoNetworkClient,
    //NoNetworkServer,
};

/// helper function for build config, usage example: build( config::LevelFull, config::NoFileWrite | config::NoFileDelete )
constexpr eConfig build( eConfig const level, unsigned int const opt_out = 0 ) noexcept
{
    return static_cast<eConfig>((static_cast<unsigned int>(level) & LevelMask) | (opt_out & FeatureOptOutMask));
}

// some convenience helper functions to build custom configs

constexpr eConfig minimal() noexcept { return LevelMinimal; }
constexpr eConfig core_reduced() noexcept { return LevelCoreReduced; }
constexpr eConfig core() noexcept { return LevelCore; }
constexpr eConfig util() noexcept { return LevelUtil; }
constexpr eConfig full() noexcept { return LevelFull; }

constexpr eConfig optout_everything( eConfig const in = static_cast<eConfig>(0) )  noexcept { return static_cast<eConfig>(in | FeatureOptOutMask); }

constexpr eConfig no_stdio( eConfig const in = static_cast<eConfig>(0) )  noexcept { return static_cast<eConfig>(in | NoStdIn | NoStdOut | NoStdErr); }
constexpr eConfig no_fileio( eConfig const in = static_cast<eConfig>(0) )  noexcept { return static_cast<eConfig>(in | NoFileRead | NoFileWrite | NoFileDelete); }
constexpr eConfig no_eval( eConfig const in = static_cast<eConfig>(0) )  noexcept { return static_cast<eConfig>(in | NoEval); }

} // namespace config


namespace util {

inline
std::filesystem::path utf8_path( std::string const &rUtf8 )
{
#if _LIBCPP_VERSION
    _LIBCPP_SUPPRESS_DEPRECATED_PUSH
#endif
    return std::filesystem::u8path( rUtf8 );
#if _LIBCPP_VERSION
    _LIBCPP_SUPPRESS_DEPRECATED_POP
#endif
}

template< typename T >
inline
T & get_value( ValueObject &rObj )
{
    if constexpr( std::is_same_v<T, ValueObject> ) {
        return rObj;
    } else {
        return rObj.GetValue<T>();
    }
}

} // namespace util


//TODO: During a rainy afternoon and after 2 cups of coffee, I will convert these LibraryFunctions to one generic one dealing with arbitrary parameter count
//      by using variadic parameter pack... :-)

template< typename F, typename RES = void >
class LibraryFunction0 : public FunctionBase
{
    F *mpFunc;
public:
    LibraryFunction0( F *f )
        : FunctionBase()
        , mpFunc( f )
    {
    }

    virtual ~LibraryFunction0() {}

    ValueObject Call( Context &/*rContext*/, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        if( 0u != rParams.size() ) {
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed parameters (must be 0)!" );
        }
        if constexpr( std::is_same_v<RES, ValueObject> ) {
            return mpFunc();
        } else if constexpr( not std::is_same_v<RES, void> ) {
            return ValueObject( mpFunc() ); //TODO pass TypeSystem?
        } else {
            mpFunc();
            return {};
        }
    }
};


// experimental variant with bool with_context. If true a first parameter is added to thw call: the Context.
template< typename F, typename T1, typename RES = void, bool with_context = false>
class LibraryFunction1 : public FunctionBase
{
    F *mpFunc;
public:
    LibraryFunction1( F *f )
        : FunctionBase()
        , mpFunc( f )
    {
    }

    virtual ~LibraryFunction1() {}

    ValueObject Call( Context &rContext, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        if( 1u != rParams.size() ) {
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed parameters (must be 1)!" );
        }
        if constexpr( not with_context ) {
            if constexpr( std::is_same_v<RES, ValueObject> ) {
                return mpFunc( util::get_value<T1>( rParams[0] ) );
            } else if constexpr( not std::is_same_v<RES, void> ) {
                return ValueObject( mpFunc( util::get_value<T1>( rParams[0] ) ) ); //TODO pass TypeSystem?
            } else {
                mpFunc( util::get_value<T1>( rParams[0] ) );
                return {};
            }
        } else {
            if constexpr( std::is_same_v<RES, ValueObject> ) {
                return mpFunc( rContext, util::get_value<T1>( rParams[0] ) );
            } else if constexpr( not std::is_same_v<RES, void> ) {
                return ValueObject( rContext, mpFunc( util::get_value<T1>( rParams[0] ) ) ); //TODO pass TypeSystem?
            } else {
                mpFunc( rContext, util::get_value<T1>( rParams[0] ) );
                return {};
            }
        }
    }
};

template< typename F, typename T1, typename T2, typename RES = void>
class LibraryFunction2 : public FunctionBase
{
    F *mpFunc;
public:
    LibraryFunction2( F *f )
        : FunctionBase()
        , mpFunc( f )
    {
    }

    virtual ~LibraryFunction2() {}

    ValueObject Call( Context &/*rContext*/, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        if( 2u != rParams.size() ) {
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed parameters (must be 2)!" );
        }
        if constexpr( std::is_same_v<RES, ValueObject> ) {
            return mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>(rParams[1]) );
        } else if constexpr( not std::is_same_v<RES, void> ) {
            return ValueObject( mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ) ) ); //TODO pass TypeSystem?
        } else {
            mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ) );
            return {};
        }
    }
};

template< typename F, typename T1, typename T2, typename T3, typename RES = void>
class LibraryFunction3 : public FunctionBase
{
    F *mpFunc;
public:
    LibraryFunction3( F *f )
        : FunctionBase()
        , mpFunc( f )
    {
    }

    virtual ~LibraryFunction3() {}

    ValueObject Call( Context &/*rContext*/, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        if( 3u != rParams.size() ) {
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed parameters (must be 3)!" );
        }
        if constexpr( std::is_same_v<RES, ValueObject> ) {
            return mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ), util::get_value<T3>(rParams[2]) );
        } else if constexpr( not std::is_same_v<RES, void> ) {
            return ValueObject( mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ), util::get_value<T3>( rParams[2] ) ) ); //TODO pass TypeSystem?
        } else {
            mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ), util::get_value<T3>( rParams[2] ) );
            return {};
        }
    }
};

template< typename F, typename T1, typename T2, typename T3, typename T4, typename RES = void>
class LibraryFunction4 : public FunctionBase
{
    F *mpFunc;
public:
    LibraryFunction4( F *f )
        : FunctionBase()
        , mpFunc( f )
    {
    }

    virtual ~LibraryFunction4() {}

    ValueObject Call( Context &/*rContext*/, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        if( 4u != rParams.size() ) {
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed parameters (must be 4)!" );
        }
        if constexpr( std::is_same_v<RES, ValueObject> ) {
            return mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ), util::get_value<T3>( rParams[2] ), util::get_value<T4>(rParams[3]) );
        } else if constexpr( not std::is_same_v<RES, void> ) {
            return ValueObject( mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ), util::get_value<T3>( rParams[2] ), util::get_value<T4>( rParams[3] ) ) ); //TODO pass TypeSystem?
        } else {
            mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ), util::get_value<T3>( rParams[2] ), util::get_value<T4>( rParams[3] ) );
            return {};
        }
    }
};

/// The function object for evaluate TeaScript code within TeaScript code.
class EvalFunc : public FunctionBase
{
    bool mLoadFile;
public:
    explicit EvalFunc( bool file )
        : mLoadFile( file )
    {
    }
    virtual ~EvalFunc() {}

    ValueObject Call( Context &rContext, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        if( 1u != rParams.size() ) { // maybe can be relaxed (e.g. optional parameters, or list of expr)?
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed parameters (must be 1)!" );
        }

        //NOTE: since we don't open a new scope here, we can add/modify the scope of caller!
        //TODO: This might have unwanted side effects. Must provide an optional way for a clean scope and/or clean environment.

        Content content;
        std::vector<char> buf;
        std::string str;
        std::string filename;
        if( mLoadFile ) {
            // TODO: parameter for script ? Can register args as real ValueObjects instead of string! But must avoid to override args of caller script!!!
            str = rParams[0].GetValue<std::string>();
            // NOTE: TeaScript strings are UTF-8
            // TODO: apply include paths before try absolute()
            std::filesystem::path const script = std::filesystem::absolute( util::utf8_path( str ) );
            std::ifstream file( script, std::ios::binary | std::ios::ate ); // ate jumps to end.
            if( file ) {
                auto size = file.tellg();
                file.seekg( 0 );
                buf.resize( static_cast<size_t>(size) + 1 ); // ensure zero terminating!
                file.read( buf.data(), size );
                content = Content( buf.data(), buf.size() );
                // build utf-8 filename again... *grrr*
#if defined(_WIN32)
                auto const tmp_u8 = script.generic_u8string();
                filename = std::string( tmp_u8.begin(), tmp_u8.end() ); // must make a copy... :-(
#else
                // NOTE: On Windows it will be converted to native code page! Could be an issue when used as TeaScript string!
                filename = script.generic_string();
#endif
            } else {
                // TODO: Better return an error ?
                throw exception::load_file_error( rLoc, str );
            }
        } else {
            str = rParams[0].GetValue<std::string>();
            content = Content( str );
            filename = "_EVALFUNC_";
        }

        Parser p; //FIXME: for later versions: must use correct state with correct factory.
        p.SetDebug( rContext.is_debug );
        try {
            return p.Parse( content, filename )->Eval( rContext );
        } catch( exception::eval_error const &/*ex*/ ) {
            throw;
            //return {}; // TODO: unified and improved error handling. Return an eval_error? or just dont catch?
        } catch( exception::parsing_error const &/*ex*/ ) {
            throw;
            //return {}; // TODO: unified and improved error handling. Return an eval_error? or just dont catch?
        }
    }
};

/// creates a (unamed) tuple object with arbiratry amount of elements, e.g., accepts 0..N parameters.
class MakeTupleFunc : public FunctionBase
{
public:
    MakeTupleFunc() {}
    virtual ~MakeTupleFunc() {}

    ValueObject Call( Context &rContext, std::vector<ValueObject> &rParams, SourceLocation const &/*rLoc*/ ) override
    {
        Collection<ValueObject>  tuple;
        
        if( rParams.size() > 1 ) {
            tuple.Reserve( rParams.size() );
        }
        for( auto const &v : rParams ) {
            tuple.AppendValue( v );
        }

        return ValueObject( std::move( tuple ), ValueConfig{ValueShared,ValueMutable,rContext.GetTypeSystem()} );
    }
};


/// The CoreLibrary of TeaScript providing core functionality for the scripts.
class CoreLibrary
{
    // The Core Library C++ API - now public
public:
    static constexpr long long API_Version = 0LL;

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

    static std::string ReadLine()
    {
        std::string line;
        std::getline( std::cin, line );
        if( line.ends_with( '\r' ) ) { // temporary fix for Windows Unicode support hook up.
            line.erase( line.size() - 1 );
        }
        return line;
    }

    [[noreturn]] static void ExitScript( long long const code )
    {
        throw control::Exit_Script( code );
    }

    // as long as we dont support implicit cast.
    static long long DoubleToLongLong( double const d )
    {
        return static_cast<long long>(d);
    }

    static double Sqrt( double const d )
    {
        return sqrt( d );
    }

    static ValueObject StrToNum( std::string  const &rStr )
    {
        try {
            return ValueObject( ValueObject( rStr, false ).GetAsInteger() ); // ensure same conversion routine is used.
        } catch( ... ) {
            return ValueObject( false ); // First attempt of error handling. TODO: change to error code! Need match operator for dispatch nicely at user side!
        }
    }

    /// Converts string to either f64 or i64 or a Bool( false )
    static ValueObject StrToNumEx( std::string  const &rStr )
    {
        // Use Parser for parse a number, will either return a f64 or i64 or a Bool( false )
        try {
            Content content( rStr );
            Parser::SkipWhitespace( content );
            Parser  p;
            if( p.Int( content ) ) {
                auto ast = p.GetLastToplevelASTNode();
                if( ast.get() != nullptr ) {
                    Context dummy;
                    return ast->Eval( dummy );
                }
            }
            return ValueObject( false );
        } catch( ... ) {
            return ValueObject( false ); // First attempt of error handling. TODO: change to error code! Need match operator for dispatch nicely at user side!
        }
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
        return static_cast<long long>(util::utf8_string_length(rStr));
    }

    static std::string StrAt( std::string const &rStr, long long const at )
    {
        if( at < 0 || static_cast<size_t>(at) >= rStr.length() ) {
            return {};
        }
        return {rStr.at( static_cast<size_t>(at) )};
    }

    static std::string SubStr( std::string const &rStr, long long const from, long long const count )
    {
        if( from < 0 || static_cast<size_t>(from) >= rStr.length() || count < -1 ) { // -1 == npos == until end of string
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

    static bool StrReplacePos( std::string &rStr, long long const start, long long const count, std::string const &rNew )
    {
        try {
            rStr.replace( static_cast<size_t>(start), static_cast<size_t>(count), rNew );
            return true;
        } catch( ... ) {
            return false;
        }
    }

    /// gets the local wall clock time in fractional seconds.
    static double GetLocalTimeInSecs()
    {
#if _MSC_VER || __GNUC__ >= 13 // TODO: _LIBCPP_VERSION
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

    /// gets the UTC time in fractional seconds (with leap seconds!).
    static double GetUTCTimeInSecs()
    {
#if _MSC_VER || __GNUC__ >= 13 // TODO: _LIBCPP_VERSION
        auto const now = std::chrono::utc_clock::now(); // UTC with leap seconds.
#else
        auto const now = std::chrono::system_clock::now(); // UTC without(!) leap seconds.
#endif
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
#if _MSC_VER || __GNUC__ >= 13 // TODO: _LIBCPP_VERSION
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
        // TODO: error handling! return an Error instead of Bool / throw!
        auto const  path = std::filesystem::absolute( util::utf8_path( rFile ) );
        std::ifstream  file( path, std::ios::binary | std::ios::ate ); // ate jumps to end.
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
                return ValueObject( false );
            }
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
            return ValueObject( std::move( buf ), false );
        }
        return ValueObject( false );
    }

    static bool WriteTextFile( std::string const &rFile, std::string const & rContent, bool const overwrite, bool const bom )
    {
        // TODO: THREAD path building per CoreLib / Context instance? make thread safe and use the internal current path for make absolute!
        // TODO: error handling! return an Error instead of Bool / throw!
        auto const  path = std::filesystem::absolute( util::utf8_path( rFile ) );
        if( !overwrite ) {
            // must use fopen w. eXclusive mode for ensure file did not exist before.
            auto fp = fopen( path.string().c_str(), "w+x" );
            if( !fp ) {
                return false; // file exist already (or access denied)
            }
            fclose( fp );
        }
        std::ofstream  file( path, std::ios::binary | std::ios::trunc );
        if( file ) {
            if( bom ) {
                file.write( "\xEF\xBB\xBF", 3 );
            }
            file.write( rContent.data(), static_cast<std::streamsize>(rContent.size()) );
            return file.good();
        }
        return false;
    }


    static long long TupleSize( Collection<ValueObject> const & rTuple )
    {
        return static_cast<long long>(rTuple.Size());
    }

    static ValueObject TupleValue( Collection<ValueObject> const &rTuple, long long const idx )
    {
        return rTuple.GetValueByIdx( static_cast<std::size_t>(idx) );
    }

    static ValueObject TupleNamedValue( Collection<ValueObject> const &rTuple, std::string const &rName )
    {
        return rTuple.GetValueByKey( rName );
    }

    static void TupleSetValue( Collection<ValueObject> &rTuple, long long const idx, ValueObject &rVal )
    {
        rTuple.GetValueByIdx( static_cast<std::size_t>(idx) ).AssignValue( rVal.MakeShared() );
    }

    static void TupleSetNamedValue( Collection<ValueObject> &rTuple, std::string const &rName, ValueObject &rVal )
    {
        rTuple.GetValueByKey( rName ).AssignValue( rVal.MakeShared() );
    }

    static void TupleAppend( Collection<ValueObject> &rTuple, ValueObject &rVal )
    {
        rTuple.AppendValue( rVal.MakeShared() );
    }

    static bool TupleNamedAppend( Collection<ValueObject> &rTuple, std::string const &rName, ValueObject &rVal )
    {
        return rTuple.AppendKeyValue( rName, rVal.MakeShared() );
    }

    static void TupleInsert( Collection<ValueObject> &rTuple, long long const idx, ValueObject &rVal )
    {
        rTuple.InsertValue( static_cast<std::size_t>(idx), rVal.MakeShared() );
    }

    static void TupleNamedInsert( Collection<ValueObject> &rTuple, long long const idx, std::string const &rName, ValueObject &rVal )
    {
        rTuple.InsertKeyValue( static_cast<std::size_t>(idx), rName, rVal.MakeShared() );
    }

    static bool TupleRemove( Collection<ValueObject>  &rTuple, long long const idx )
    {
        return rTuple.RemoveValueByIdx( static_cast<std::size_t>(idx) );
    }

    static bool TupleNamedRemove( Collection<ValueObject> &rTuple, std::string const &rName )
    {
        return rTuple.RemoveValueByKey( rName );
    }

    static long long TupleIndexOf( Collection<ValueObject> const &rTuple, std::string const &rName )
    {
        auto const idx = rTuple.IndexOfKey( rName );
        return idx > static_cast<size_t>(std::numeric_limits<long long>::max()) ? -1LL : static_cast<long long>(idx);
    }

    static std::string TupleNameOf( Collection<ValueObject> const &rTuple, long long const idx )
    {
        return rTuple.KeyOfIndex( static_cast<size_t>(idx) );
    }

    static void TupleSwapValues( Collection<ValueObject> &rTuple, long long const idx1, long long const idx2 )
    {
        rTuple.SwapByIdx( static_cast<std::size_t>(idx1), static_cast<std::size_t>(idx2) );
    }

    static bool TupleSameTypes( Collection<ValueObject> const &rTuple1, Collection<ValueObject> const &rTuple2 )
    {
        return tuple::is_same_structure( rTuple1, rTuple2 );
    }

    static void TuplePrint( ValueObject &rTuple, std::string const &rRootName, long long max_nesting )
    {
        tuple::foreach_named_element( rRootName, rTuple, true, [=]( std::string const &name, ValueObject &rVal, int level ) -> bool {
            std::string const val_str = rVal.GetTypeInfo()->GetName() == "Tuple" ? "<Tuple>" : rVal.HasPrintableValue() ? rVal.PrintValue() : "<" + rVal.GetTypeInfo()->GetName() + ">";
            std::string const text = name + ": " + val_str + "\n";
            PrintStdOut( text );
            return level < max_nesting;
        } );
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
        auto name_utf8 = dir_it->path().filename().generic_u8string();
        auto const name_str = std::string( name_utf8.begin(), name_utf8.end() ); // must make a copy... :-(

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
        auto name_utf8 = dir_it->path().filename().generic_u8string();
        auto const name_str = std::string( name_utf8.begin(), name_utf8.end() ); // must make a copy... :-(

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

protected:

    virtual void BuildInternals( Context &rTmpContext, config::eConfig const config )
    {
        Context::VariableStorage  res; //TODO: add directly to context since it is the final context now.
        res.reserve( 128 );

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
            res.push_back( std::make_pair( "_init_core_stamp", ValueObject( GetTimeStamp(), cfg ) ) );
            res.push_back( std::make_pair( "_core_config", ValueObject( static_cast<long long>(config), cfg ) ) ); // config of Core Lib
            res.push_back( std::make_pair( "__teascript_copyright", ValueObject( std::string(TEASCRIPT_COPYRIGHT), cfg ) ) );
        }

        // Add the basic Types
        {
            res.push_back( std::make_pair( "TypeInfo", ValueObject( TypeTypeInfo, cfg ) ) );
            res.push_back( std::make_pair( "NaV", ValueObject( TypeNaV, cfg ) ) );
            res.push_back( std::make_pair( "Bool", ValueObject( TypeBool, cfg ) ) );
            res.push_back( std::make_pair( "i64", ValueObject( TypeLongLong, cfg ) ) );
            res.push_back( std::make_pair( "f64", ValueObject( TypeDouble, cfg ) ) );
            res.push_back( std::make_pair( "String", ValueObject( TypeString, cfg ) ) );
            res.push_back( std::make_pair( "Number", ValueObject( MakeTypeInfo<Number>("Number"), cfg))); //TEST TEST TEST - Fake concept for 'Number'
            res.push_back( std::make_pair( "Function", ValueObject( MakeTypeInfo<FunctionPtr>("Function"), cfg)));
            res.push_back( std::make_pair( "Tuple", ValueObject( MakeTypeInfo<Collection<ValueObject>>( "Tuple" ), cfg ) ) );
            //res.push_back( std::make_pair( "const", ValueObject( MakeTypeInfo<Number>( "const" ), cfg ) ) ); //TEST TEST TEST - Fake concept for 'const'
        }


        // _version_major | _version_minor | _version_patch | _version_combined_number | _api_version (long long) --> version information
        {
            res.push_back( std::make_pair( "_version_major", ValueObject( static_cast<long long>(version::Major), cfg ) ) );
            res.push_back( std::make_pair( "_version_minor", ValueObject( static_cast<long long>(version::Minor), cfg ) ) );
            res.push_back( std::make_pair( "_version_patch", ValueObject( static_cast<long long>(version::Patch), cfg ) ) );
            res.push_back( std::make_pair( "_version_combined_number", ValueObject( static_cast<long long>(version::combined_number()), cfg ) ) );
            res.push_back( std::make_pair( "_version_build_date_time", ValueObject( std::string(version::build_date_time_str()), cfg)));
            res.push_back( std::make_pair( "_api_version", ValueObject( API_Version, cfg ) ) ); // Core Lib API version
        }

        //NOTE: as long as we don't have a cast, this is the only possible way to get an i64 from a f64. Therefore it belongs to minimal.
        // _f64toi64 : i64 (f64) --> converts a f64 to i64. same effect as trunc() but yields a i64.
        {
            auto func = std::make_shared< LibraryFunction1< decltype(DoubleToLongLong), double, long long > >( &DoubleToLongLong );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_f64toi64", std::move( val ) ) );
        }

        // for a minimal core lib this is all already...
        if( core_level == config::LevelMinimal ) {
            rTmpContext.BulkAdd( res );
            return;
        }

        // _out : void ( String ) --> prints param1 (String) to stdout
        if( not (opt_out & config::NoStdOut) ) {
            auto func = std::make_shared< LibraryFunction1< decltype(PrintStdOut), std::string > >( &PrintStdOut );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_out", std::move( val ) ) );
        }

        // _err : void ( String ) --> prints param1 (String) to stderr
        if( not (opt_out & config::NoStdErr) ) {
            auto func = std::make_shared< LibraryFunction1< decltype(PrintStdError), std::string > >( &PrintStdError );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_err", std::move( val ) ) );
        }

        // readline : String ( void ) --> read line from stdin (and blocks until line finished), returns the read line without line feed.
        if( not (opt_out & config::NoStdIn) ) {
            auto func = std::make_shared< LibraryFunction0< decltype(ReadLine), std::string > >( &ReadLine );
            ValueObject val{std::move( func ), cfg_mutable};
            res.push_back( std::make_pair( "readline", std::move( val ) ) ); // missing _ is intended for now.
        }

        // _exit_failure | _exit_success (long long) --> common exit codes
        {
            res.push_back( std::make_pair( "_exit_failure", ValueObject( static_cast<long long>(EXIT_FAILURE), cfg ) ) );
            res.push_back( std::make_pair( "_exit_success", ValueObject( static_cast<long long>(EXIT_SUCCESS), cfg ) ) );
        }

        // _exit : void (i64) --> exits the script (with stack unwinding/scope cleanup) with param1 exit code. (this function never returns!)
        {
            auto func = std::make_shared< LibraryFunction1< decltype(ExitScript), long long > >( &ExitScript );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_exit", std::move( val ) ) );
        }

        if( core_level >= config::LevelCore ) {
            // _strtonum : i64|Bool (String) --> converts a String to i64. this works only with real String objects. alternative for '+str'.
            {
                auto func = std::make_shared< LibraryFunction1< decltype(StrToNum), std::string, ValueObject> >( &StrToNum );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_strtonum", std::move( val ) ) );
            }

            // _strtonumex : i64|f64|Bool (String) --> converts a String to i64 or f64, Bool(false) on error. this works only with real String objects.
            {
                auto func = std::make_shared< LibraryFunction1< decltype(StrToNumEx), std::string, ValueObject> >( &StrToNumEx );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_strtonumex", std::move( val ) ) );
            }

             // _numtostr : String (i64) --> converts a i64 to String. this works only with real i64 objects. alternative for 'num % ""'
            {
                auto func = std::make_shared< LibraryFunction1< decltype(NumToStr), long long, std::string > >( &NumToStr );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_numtostr", std::move( val ) ) );
            }
        }

        
        if( core_level >= config::LevelUtil ) {
             // _print_version : void ( void ) --> prints TeaScript version to stdout
            if( not (opt_out & config::NoStdOut) ) {
                auto func = std::make_shared< LibraryFunction0< decltype(PrintVersion) > >( &PrintVersion );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_print_version", std::move( val ) ) );
            }

            // _sqrt : f64 (f64) --> calculates square root
            {
                auto func = std::make_shared< LibraryFunction1< decltype(Sqrt), double, double > >( &Sqrt );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_sqrt", std::move( val ) ) );
            }
        }

        // evaluate and load

        // eval : Any (String) --> parses and evaluates the String as TeaScript code and returns its result.
        if( not (opt_out & config::NoEval) ) {
            auto func = std::make_shared< EvalFunc >( false );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_eval", std::move( val ) ) );
        }

        // eval_file : Any (String) --> parses and evaluates the content of the file and returns its result. All defined functions and variables in the top level scope will stay available.
        if( not (opt_out & config::NoEvalFile) ) {
            auto func = std::make_shared< EvalFunc >( true );
            ValueObject val{std::move( func ), cfg_mutable};
            res.push_back( std::make_pair( "eval_file", std::move( val ) ) ); // missing _ is intended for now.
        }


        // tuple support

        // _tuple_create : Tuple ( ... ) --> creates a tuple from the passed parameters. parameter count is variable and type Any.
        {
            auto func = std::make_shared< MakeTupleFunc >();
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_tuple_create", std::move( val ) ) );
        }

        // _tuple_size : i64 ( Tuple ) --> returns the element count of the Tuple
        {
            auto func = std::make_shared< LibraryFunction1< decltype(TupleSize), Collection<ValueObject>, long long > >( &TupleSize );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_tuple_size", std::move( val ) ) );
        }

        // _tuple_same_types : Bool ( Tuple, Tuple ) --> checks whether the 2 tuples have the same types in exactly the same order (and with same names).
        {
            auto func = std::make_shared< LibraryFunction2< decltype(TupleSameTypes), Collection<ValueObject>, Collection<ValueObject>, bool > >( &TupleSameTypes );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_tuple_same_types", std::move( val ) ) );
        }

        if( core_level >= config::LevelCore ) {
            // _tuple_val : Any ( Tuple, i64 ) --> returns the value at given index.
            {
                auto func = std::make_shared< LibraryFunction2< decltype(TupleValue), Collection<ValueObject>, long long, ValueObject > >( &TupleValue );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_tuple_val", std::move( val ) ) );
            }

            // _tuple_named_val : Any ( Tuple, String ) --> returns the value with given name (or throws)
            {
                auto func = std::make_shared< LibraryFunction2< decltype(TupleNamedValue), Collection<ValueObject>, std::string, ValueObject > >( &TupleNamedValue );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_tuple_named_val", std::move( val ) ) );
            }

            // _tuple_set : void ( Tuple, i64, Any ) --> sets the value at given index or throws if index not exist.
            {
                auto func = std::make_shared< LibraryFunction3< decltype(TupleSetValue), Collection<ValueObject>, long long, ValueObject > >( &TupleSetValue );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_tuple_set", std::move( val ) ) );
            }

            // _tuple_named_set : void ( Tuple, String, Any ) --> set the value with given name (or throws)
            {
                auto func = std::make_shared< LibraryFunction3< decltype(TupleSetNamedValue), Collection<ValueObject>, std::string, ValueObject > >( &TupleSetNamedValue );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_tuple_named_set", std::move( val ) ) );
            }

            // _tuple_append : void ( Tuple, Any ) --> appends new value to the end as new element
            {
                auto func = std::make_shared< LibraryFunction2< decltype(TupleAppend), Collection<ValueObject>, ValueObject > >( &TupleAppend );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_tuple_append", std::move( val ) ) );
            }

            // _tuple_named_append : Bool ( Tuple, String, Any ) --> appends new value with given name to the end as new element if the name not exist yet.
            {
                auto func = std::make_shared< LibraryFunction3< decltype(TupleNamedAppend), Collection<ValueObject>, std::string, ValueObject, bool > >( &TupleNamedAppend );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_tuple_named_append", std::move( val ) ) );
            }

            // _tuple_insert : void ( Tuple, i64, Any ) --> inserts new value at given index
            {
                auto func = std::make_shared< LibraryFunction3< decltype(TupleInsert), Collection<ValueObject>, long long, ValueObject > >( &TupleInsert );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_tuple_insert", std::move( val ) ) );
            }

            // _tuple_named_insert : void ( Tuple, i64, String, Any ) --> inserts a value with given name at given index
            {
                auto func = std::make_shared< LibraryFunction4< decltype(TupleNamedInsert), Collection<ValueObject>, long long, std::string, ValueObject > >( &TupleNamedInsert );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_tuple_named_insert", std::move( val ) ) );
            }

            // _tuple_remove : Bool ( Tuple, i64 ) --> removes element at given index, returns whether an element has been removed.
            {
                auto func = std::make_shared< LibraryFunction2< decltype(TupleRemove), Collection<ValueObject>, long long, bool > >( &TupleRemove );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_tuple_remove", std::move( val ) ) );
            }

            // _tuple_named_remove : Bool ( Tuple, String ) --> removes element with given name, returns whether an element has been removed.
            {
                auto func = std::make_shared< LibraryFunction2< decltype(TupleNamedRemove), Collection<ValueObject>, std::string, bool > >( &TupleNamedRemove );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_tuple_named_remove", std::move( val ) ) );
            }

            // _tuple_index_of : i64 ( Tuple, String ) --> returns the index of the element with given name or -1
            {
                auto func = std::make_shared< LibraryFunction2< decltype(TupleIndexOf), Collection<ValueObject>, std::string, long long > >( &TupleIndexOf );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_tuple_index_of", std::move( val ) ) );
            }

            // _tuple_name_of : String ( Tuple, i64 ) --> returns the name of the element with given idx (or throws)
            {
                auto func = std::make_shared< LibraryFunction2< decltype(TupleNameOf), Collection<ValueObject>, long long, std::string > >( &TupleNameOf );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_tuple_name_of", std::move( val ) ) );
            }

            // _tuple_swap : void ( Tuple, i64, i64 ) --> swaps elements of given indices
            {
                auto func = std::make_shared< LibraryFunction3< decltype(TupleSwapValues), Collection<ValueObject>, long long, long long > >( &TupleSwapValues );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_tuple_swap", std::move( val ) ) );
            }

            // tuple_print : void ( Tuple, String, i64 ) --> prints (recursively) all (named) elements, for debugging (String param is the "root" name, last param is for max nesting level)
            if( not (opt_out & config::NoStdOut) ) {
                auto func = std::make_shared< LibraryFunction3< decltype(TuplePrint), ValueObject, std::string, long long > >( &TuplePrint );
                ValueObject val{std::move( func ), cfg_mutable};
                res.push_back( std::make_pair( "tuple_print", std::move( val ) ) ); // missing _ is intended for now.
            }
        }


        // minimalistic string support

        // _strlen : i64 ( String ) --> returns the length of the string in bytes (excluding the ending 0).
        {
            auto func = std::make_shared< LibraryFunction1< decltype(StrLength), std::string, long long > >( &StrLength );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_strlen", std::move( val ) ) );
        }

        // _strglyphs : i64 ( String ) --> returns the utf-8 (Unicode) glyph count of the string (excluding the ending 0).
        {
            auto func = std::make_shared< LibraryFunction1< decltype(StrUTF8GlyphCount), std::string, long long > >( &StrUTF8GlyphCount );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_strglyphs", std::move( val ) ) );
        }

        // _strat : String ( String, i64 ) --> returns a substring of one character at given position. empty string if out of range.
        {
            auto func = std::make_shared< LibraryFunction2< decltype(StrAt), std::string, long long, std::string > >( &StrAt );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_strat", std::move( val ) ) );
        }

        // _substr : String ( String, from: i64, count: i64 ) --> returns a substring [from, from+count). count -1 means until end of string. returns empty string on invalid arguments.
        {
            auto func = std::make_shared< LibraryFunction3< decltype(SubStr), std::string, long long, long long, std::string > >( &SubStr );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_substr", std::move( val ) ) );
        }

        // _strfind : i64 ( String, substring: String, offset: i64 ) --> searches for substring from offset and returns found pos of first occurrence. -1 if not found.
        {
            auto func = std::make_shared< LibraryFunction3< decltype(StrFind), std::string, std::string, long long, long long > >( &StrFind );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_strfind", std::move( val ) ) );
        }

        if( core_level >= config::LevelCore ) {
            // _strfindreverse : i64 ( String, substring: String, offset: i64 ) --> searches for substring from behind from offset and returns found pos of first occurrence. -1 if not found.
            {
                auto func = std::make_shared< LibraryFunction3< decltype(StrReverseFind), std::string, std::string, long long, long long > >( &StrReverseFind );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_strfindreverse", std::move( val ) ) );
            }

            // _strreplacepos : Bool ( str: String, start: i64, count: i64, new: String ) --> replaces the section [start, start+count) in str with new. returns false on error, e.g. start is out of range.
            {
                auto func = std::make_shared< LibraryFunction4< decltype(StrReplacePos), std::string, long long, long long, std::string, bool > >( &StrReplacePos );
                ValueObject val{std::move( func ), cfg};
                res.push_back( std::make_pair( "_strreplacepos", std::move( val ) ) );
            }
        }


        // time / misc

        // _timestamp : f64 ( void ) --> gets the elapsed time in (fractional) seconds as f64 from an unspecified time point during program start. Time is monotonic increasing.
        {
            auto func = std::make_shared< LibraryFunction0< decltype(GetTimeStamp), double > >( &GetTimeStamp );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_timestamp", std::move( val ) ) );
        }

        if( core_level >= config::LevelUtil ) {
            // clock : f64 ( void ) --> gets the local wall clock time of the current day in (fractional) seconds as f64. 
            {
                auto func = std::make_shared< LibraryFunction0< decltype(GetLocalTimeInSecs), double > >( &GetLocalTimeInSecs );
                ValueObject val{std::move( func ), cfg_mutable};
                res.push_back( std::make_pair( "clock", std::move( val ) ) ); // missing _ is intended for now.
            }

            // clock_utc : f64 ( void ) --> gets the UTC time of the current day in (fractional) seconds as f64. (note: This UTC time is with leap seconds!)
            {
                auto func = std::make_shared< LibraryFunction0< decltype(GetUTCTimeInSecs), double > >( &GetUTCTimeInSecs );
                ValueObject val{std::move( func ), cfg_mutable};
                res.push_back( std::make_pair( "clock_utc", std::move( val ) ) ); // missing _ is intended for now.
            }

            // sleep : void ( i64 ) --> sleeps (at least) for given amount of seconds.
            {
                auto func = std::make_shared< LibraryFunction1< decltype(SleepSecs), long long > >( &SleepSecs );
                ValueObject val{std::move( func ), cfg_mutable};
                res.push_back( std::make_pair( "sleep", std::move( val ) ) );
            }

            // random : i64 ( i64, i64 ) --> creates a random number in between [start,end]. start, end must be >= 0 and <= UINT_MAX.
            {
                auto func = std::make_shared< LibraryFunction2< decltype(CreateRandomNumber), long long, long long, long long > >( &CreateRandomNumber );
                ValueObject val{std::move( func ), cfg_mutable};
                res.push_back( std::make_pair( "random", std::move( val ) ) );
            }
        }

        // minimalistic (text) file io support
        if( core_level >= config::LevelUtil && (config::NoFileWrite | config::NoFileRead | config::NoFileDelete) !=
                                             (opt_out & (config::NoFileWrite | config::NoFileRead | config::NoFileDelete)) ) {
            // cwd : String ( void ) --> returns the current working directory as String
            {
                auto func = std::make_shared< LibraryFunction0< decltype(CurrentPath), std::string > >( &CurrentPath );
                ValueObject val{std::move( func ), cfg_mutable};
                res.push_back( std::make_pair( "cwd", std::move( val ) ) ); // missing _ is intended for now.
            }

            // change_cwd : Bool ( String ) --> changes the current working dir
            {
                auto func = std::make_shared< LibraryFunction1< decltype(ChangeCurrentPath), std::string, bool> >( &ChangeCurrentPath );
                ValueObject val{std::move( func ), cfg_mutable};
                res.push_back( std::make_pair( "change_cwd", std::move( val ) ) ); // missing _ is intended for now.
            }

            // tempdir : String ( void ) --> returns configured temp directory as String
            {
                auto func = std::make_shared< LibraryFunction0< decltype(TempPath), std::string > >( &TempPath );
                ValueObject val{std::move( func ), cfg_mutable};
                res.push_back( std::make_pair( "tempdir", std::move( val ) ) ); // missing _ is intended for now.
            }

            // path_exists : Bool ( String ) --> returns whether path in String exists as directory or file.
            {
                auto func = std::make_shared< LibraryFunction1< decltype(PathExists), std::string, long long> >( &PathExists );
                ValueObject val{std::move( func ), cfg_mutable};
                res.push_back( std::make_pair( "path_exists", std::move( val ) ) ); // missing _ is intended for now.
            }

            // file_size : i64 ( String ) --> returns file size in bytes. -1 on error / file not exists / is not a file.
            {
                auto func = std::make_shared< LibraryFunction1< decltype(FileSize), std::string, long long> >( &FileSize );
                ValueObject val{std::move( func ), cfg_mutable};
                res.push_back( std::make_pair( "file_size", std::move( val ) ) ); // missing _ is intended for now.
            }

            // last_modified : String ( String ) --> returns the last modified time as String for the given path or empty string if not exists/error.
            {
                auto func = std::make_shared< LibraryFunction1< decltype(LastModified), std::string, std::string> >( &LastModified );
                ValueObject val{std::move( func ), cfg_mutable};
                res.push_back( std::make_pair( "last_modified", std::move( val ) ) ); // missing _ is intended for now.
            }

            // readdirfirst : Tuple ( String ) --> returns the first direntry of given path (see direntry for details)
            {
                auto func = std::make_shared< LibraryFunction1< decltype(ReadDirFirst), std::string, ValueObject, true> >( &ReadDirFirst );
                ValueObject val{std::move( func ), cfg_mutable};
                res.push_back( std::make_pair( "readdirfirst", std::move( val ) ) ); // missing _ is intended for now.
            }

            // readdirnext : Tuple ( Tuple ) --> returns the next direntry of given direntry (see direntry for details)
            {
                auto func = std::make_shared< LibraryFunction1< decltype(ReadDirNext), Tuple, ValueObject, true> >( &ReadDirNext );
                ValueObject val{std::move( func ), cfg_mutable};
                res.push_back( std::make_pair( "readdirnext", std::move( val ) ) ); // missing _ is intended for now.
            }
        }

        if( core_level >= config::LevelUtil && not (opt_out & (config::NoFileRead|config::NoFileWrite)) )
        {
            // file_copy : Bool ( file: String, dest_dir: String, overwrite: Bool ) 
            // --> copies file to dest_dir if not exist or overwrite is true
            {
                auto func = std::make_shared< LibraryFunction3< decltype(FileCopy), std::string, std::string, bool, bool> >( &FileCopy );
                ValueObject val{std::move( func ), cfg_mutable};
                res.push_back( std::make_pair( "file_copy", std::move( val ) ) ); // missing _ is intended for now.
            }

            // file_copy_newer : Bool ( file: String, dest_dir: String ) 
            // --> copies file to dest_dir if not exist or file is newer as the file in dest_dir
            {
                auto func = std::make_shared< LibraryFunction2< decltype(FileCopyIfNewer), std::string, std::string, bool> >( &FileCopyIfNewer );
                ValueObject val{std::move( func ), cfg_mutable};
                res.push_back( std::make_pair( "file_copy_newer", std::move( val ) ) ); // missing _ is intended for now.
            }
        }

        // readtextfile : String|Bool ( String ) --> reads the content of an UTF-8 text file and returns it in a String. An optional BOM is removed.
        if( core_level >= config::LevelUtil && not (opt_out & config::NoFileRead) )
        {
            auto func = std::make_shared< LibraryFunction1< decltype(ReadTextFile), std::string, ValueObject > >( &ReadTextFile );
            ValueObject val{std::move( func ), cfg_mutable};
            res.push_back( std::make_pair( "readtextfile", std::move( val ) ) ); // missing _ is intended for now.
        }


        if( core_level >= config::LevelUtil && not (opt_out & config::NoFileWrite) )
        {
            // create_dir : Bool ( String, Bool ) --> creates directories for path in String. Bool == true means recursively
            {
                auto func = std::make_shared< LibraryFunction2< decltype(CreateDir), std::string, bool, bool > >( &CreateDir );
                ValueObject val{std::move( func ), cfg_mutable};
                res.push_back( std::make_pair( "create_dir", std::move( val ) ) ); // missing _ is intended for now.
            }

            // writetextfile : Bool ( file: String, str: String, overwrite: Bool, bom: Bool ) 
            // --> writes the content of String to text file. An optional UTF-8 BOM can be written (last Bool param). overwrite indicates if a prior existing file shall be overwritten (old content destroyed!)
            {
                auto func = std::make_shared< LibraryFunction4< decltype(WriteTextFile), std::string, std::string, bool, bool, bool > >( &WriteTextFile );
                ValueObject val{std::move( func ), cfg_mutable};
                res.push_back( std::make_pair( "writetextfile", std::move( val ) ) ); // missing _ is intended for now.
            }
        }

        if( core_level >= config::LevelUtil && not (opt_out & config::NoFileDelete) ) 
        {
            // path_delete : Bool ( String ) --> deletes(!) file or (empty) directory
            {
                auto func = std::make_shared< LibraryFunction1< decltype(PathDelete), std::string, bool > >( &PathDelete );
                ValueObject val{std::move( func ), cfg_mutable};
                res.push_back( std::make_pair( "path_delete", std::move( val ) ) ); // missing _ is intended for now.
            }
        }

        rTmpContext.BulkAdd( res );
    }
public:
    CoreLibrary() = default;
    virtual ~CoreLibrary() {}

    /// Will bootstrap the standard Core Lib into the Context. if internals_only is true, a minimal version will be loaded (with the underscore names defined only).
    /// DEPRECATED: Please, use new overload with eConfig parameter instead.
    /// IMPORTANT: Any previous data in rContext will be lost / overwritten.
    [[deprecated("Please, use new overload with eConfig param.")]] 
    virtual void Bootstrap( Context &rContext, bool internals_only = false )
    {
        if( internals_only ) {
            Bootstrap( rContext, config::LevelUtil ); // this will load more as before, but ConfigLevelCore would load too less.
        } else {
            Bootstrap( rContext, config::LevelFull );
        }
    }

    /// Will bootstrap the standard Core Lib into the Context. \param config specifies what will be loaded.
    /// IMPORTANT: Any previous data in rContext will be lost / overwritten.
    virtual void Bootstrap( Context &rContext, config::eConfig const config )
    {
        {
            //TODO: Move the internal type registration to a better place.
            TypeSystem  sys;
            sys.RegisterType<FunctionPtr>("Function");
            sys.RegisterType<std::vector<ValueObject>>("ValueObjectVector");
            sys.RegisterType<Collection<ValueObject>>( "Tuple" );

            Context tmp{ std::move( sys ), true };
            tmp.is_debug = rContext.is_debug; // take over from possible old instance.

            BuildInternals( tmp, config );

            rContext = std::move( tmp );
            // finalize
            rContext.SetBootstrapDone();
        }

        if( (config & config::LevelMask) < config::LevelUtil ) {
            return;
        }

        static constexpr char core_lib_util[] = R"_SCRIPT_(
// convenience for can write 'return void' if function shall return nothing
const void := () // void has value NaV (Not A Value)

// constant number PI
const PI := 3.14159265358979323846

// exits the script (with stack unwinding/scope cleanup) with given code, will do to number conversion of code.
func exit( code )
{
    _exit( +code )
}

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

// converts val to a Number. returns Bool(false) on error. (note: if val is a String _strtonum / _strtonumex is an alternative)
func to_number( val )
{
    if( val is String ) {
        _strtonumex( val ) // this can convert i64 and f64
    } else {
        +val //TODO: error handling!
    }
}

// converts val to f64. val must be a number already! returns Bool(false) on error.
// example use case: to_f64( to_number( some_var ) ) // ensures some_var is converted to f64
// NOTE: this function is only provisionally and will be replaced by a cast later!
func to_f64( val )
{
    if( val is Number ) { val + 0.0 } else { false }
}

// convenience function. ensures given Number is used as i64. returns Bool(false) on error.
// example use case: to_i64( to_number( some_var ) ) // ensures some_var is converted to i64
// NOTE: this function is only provisionally and will be replaced by a cast later!
func to_i64( val )
{
    if( val is Number ) {
        _f64toi64( val + 0.0 ) // first convert to f64 looks odd but it covers all cases.
    } else {
        false
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

// rounds the given Number towards zero as f64. e.g. 1.9 will yield 1.0, -2.9 will yield -2.0.
func trunc( n )
{
    0.0 + _f64toi64( n + 0.0 ) // FIXME: must use real f64 trunc!
}

// rounds down the given Number to next smaller integer as f64. e.g. 1.9 will yield 1.0, -2.1 will yield -3.0
func floor( n )
{
    const num := (n + 0.0)
    const trunced := trunc( num )
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
    const trunced := trunc( num )
    if( trunced == num or num < 0.0 ) { // integer already or negative (then trunced is correct)
        trunced
    } else { // > 0.0 and not trunced
        trunced + 1.0
    }
}

// rounds up or down the given Number to nearest integer as f64. e.g. 1.1 will yield 1.0, 1.6 as well as 1.5 will yield 2.0
func round( n )
{
    const num := (n + 0.0)
    0.0 + _f64toi64( num + if( num < 0 ) { -0.5 } else { 0.5 } )
}

func sqrt( val )
{
    _sqrt( to_f64( to_number( val ) ) )
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
)_SCRIPT_";

        static constexpr char core_lib_stderr[] = R"_SCRIPT_(
// prints s + line feed to stderr, will do to string conversion of s
func print_error( s )
{
    //TODO: add log to common logfile
    _err( s % "\n" )
}

// prints error_str to stderr, exits the script (with stack unwinding/scope cleanup) with error_code
func fail_with_message( error_str, error_code := _exit_failure )
{
    print_error( error_str )
    fail_with_error( error_code )
}
)_SCRIPT_";


        static constexpr char core_lib_eval[] = R"_SCRIPT_(
// parses and evaluates expr (will do to string conversion), returns result of expr
// NOTE: This function opens a new scope, so all new defined variables and functions will be undefined again after the call. Use _eval instead to keep definitions.
func eval( expr )
{
    _eval( expr % "" )
}
)_SCRIPT_";

        static constexpr char core_lib_file[] = R"_SCRIPT_(
func file_exists( file )
{
    file_size( file ) >= 0
}
)_SCRIPT_";

        static constexpr char core_lib_teascript[] = R"_SCRIPT_(

// checks whether the tuple contains the given name or index
func tuple_contains( tup @=, idx_or_name )
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

// trims the string if it starts or ends with characters in given set. note: set must be ASCII only!
// e.g. strtrim( s, " \t\r\n", false, true ) will remove all spaces, tabs, carriage returns and new lines at the end of the string.
func strtrim( str @=, set, leading := true, trailing := true )
{
    def res := false
    if( leading ) {
        def c := 0
        repeat {
            if( _strfind( set, _strat( str, c ), 0 ) >= 0 ) {
                c := c + 1
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
            if( _strfind( set, _strat( str, i - c ), 0 ) >= 0 ) {
                c := c + 1
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


// computes power of n with integer exponent. if exp is a float it will get truncated. returns a f64.
func pow( n, exp )
{
    const num := n + 0.0 // make a f64
    def   e   := _f64toi64( +exp + 0.0 ) // ensure integer is used.
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

// computes the hour, minute, second and (optionally) millisecond part of given time in seconds (e.g. from clock())
// note: hours can be greater than 23/24, it will not be cut at day boundary!
func timevals( t, HH @=, MM @=, S @=, ms @= 0 )
{
    if( t is f64 and t >= 0.0 ) {
        const secs := _f64toi64( t )
        HH   := secs / 60 / 60
        MM   := (secs - (HH * 60 * 60)) / 60
        S    := (secs - (HH * 60 * 60) - (MM * 60))
        ms   := _f64toi64( (t - secs) * 1000.0 ) 
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

        Parser p; //FIXME: for later versions: must use correct state with correct factory.

        p.Parse( core_lib_util, "Core" )->Eval( rContext );
        if( not (config & config::NoStdOut) ) {
            p.Parse( core_lib_stdout, "Core" )->Eval( rContext );
        }
        if( not (config & config::NoStdErr) ) {
            p.Parse( core_lib_stderr, "Core" )->Eval( rContext );
        }
        if( not (config & config::NoEval) ) {
            p.Parse( core_lib_eval, "Core" )->Eval( rContext );
        }
        if( (config::NoFileWrite | config::NoFileRead | config::NoFileDelete) !=
            (config & (config::NoFileWrite | config::NoFileRead | config::NoFileDelete)) ) {
            p.Parse( core_lib_file, "Core" )->Eval( rContext );
        }

        if( (config & config::LevelMask) >= config::LevelFull ) {
            p.Parse( core_lib_teascript, "Core" )->Eval( rContext );
        }
    }
};

} // namespace teascript
