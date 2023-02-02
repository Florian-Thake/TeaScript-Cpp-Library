/*
 * SPDX-FileCopyrightText:  Copyright (c) 2023 Florian Thake <support |at| tea-age.solutions>. All rights reserved.
 * SPDX-License-Identifier: SEE LICENSE IN LICENSE.txt
 *
 * Licensed under the TeaScript Library Standard License. See LICENSE.txt or you may find a copy at
 * https://tea-age.solutions/teascript/product-variants/
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
#include "Print.hpp"
#include "Parser.hpp"
#include "version.h"

#include <filesystem>
#include <fstream>
#include <chrono>
#include <random>
#include <thread>

namespace teascript {

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
        , mpFunc(f)
    {
    }

    virtual ~LibraryFunction0() {}

    ValueObject Call( Context &/*rContext*/, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        if( 0u != rParams.size() ) {
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed paramters (must be 0)!" );
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

template< typename F, typename T1, typename RES = void>
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

    ValueObject Call( Context &/*rContext*/, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        if( 1u != rParams.size() ) {
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed paramters (must be 1)!" );
        }
        if constexpr( std::is_same_v<RES, ValueObject> ) {
            return mpFunc( rParams[0].GetValue<T1>() );
        } else if constexpr( not std::is_same_v<RES, void> ) {
            return ValueObject( mpFunc( rParams[0].GetValue<T1>() ) ); //TODO pass TypeSystem?
        } else {
            mpFunc( rParams[0].GetValue<T1>() );
            return {};
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
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed paramters (must be 2)!" );
        }
        if constexpr( std::is_same_v<RES, ValueObject> ) {
            return mpFunc( rParams[0].GetValue<T1>(), rParams[1].GetValue<T2>() );
        } else if constexpr( not std::is_same_v<RES, void> ) {
            return ValueObject( mpFunc( rParams[0].GetValue<T1>(), rParams[1].GetValue<T2>() ) ); //TODO pass TypeSystem?
        } else {
            mpFunc( rParams[0].GetValue<T1>(), rParams[1].GetValue<T2>() );
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
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed paramters (must be 3)!" );
        }
        if constexpr( std::is_same_v<RES, ValueObject> ) {
            return mpFunc( rParams[0].GetValue<T1>(), rParams[1].GetValue<T2>(), rParams[2].GetValue<T3>() );
        } else if constexpr( not std::is_same_v<RES, void> ) {
            return ValueObject( mpFunc( rParams[0].GetValue<T1>(), rParams[1].GetValue<T2>(), rParams[2].GetValue<T3>() ) ); //TODO pass TypeSystem?
        } else {
            mpFunc( rParams[0].GetValue<T1>(), rParams[1].GetValue<T2>(), rParams[2].GetValue<T3>() );
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
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed paramters (must be 4)!" );
        }
        if constexpr( std::is_same_v<RES, ValueObject> ) {
            return mpFunc( rParams[0].GetValue<T1>(), rParams[1].GetValue<T2>(), rParams[2].GetValue<T3>(), rParams[3].GetValue<T4>() );
        } else if constexpr( not std::is_same_v<RES, void> ) {
            return ValueObject( mpFunc( rParams[0].GetValue<T1>(), rParams[1].GetValue<T2>(), rParams[2].GetValue<T3>(), rParams[3].GetValue<T4>() ) ); //TODO pass TypeSystem?
        } else {
            mpFunc( rParams[0].GetValue<T1>(), rParams[1].GetValue<T2>(), rParams[2].GetValue<T3>(), rParams[3].GetValue<T4>() );
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
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed paramters (must be 1)!" );
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
                // TODO: Better return an erorr ?
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

/// The CoreLibrary of TeaScript providing core functionality for the scripts.
class CoreLibrary
{
protected:
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

    static std::string TempPath()
    {
        std::error_code  ec;
        auto cur = std::filesystem::temp_directory_path(ec).generic_u8string();
        if( !cur.empty() && !cur.ends_with(u8'/') ) {
            cur.append( 1, u8'/' );
        }
        return std::string( cur.begin(), cur.end() ); // must make a copy... :-(
    }

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

    virtual void BuildInternals( Context &rTmpContext )
    {
        Context::VariableStorage  res; //TODO: add diectly to context since it is the final context now.

        //TODO: Rethink about what belongs to internals and what to Bootstrap!
        // This first approach has standalone scripts more in mind than integrate small scripts into huge applications.
        // Maybe dealing with stdout/in/err is more important for standalone scripts only, as well as things like exit.

#if 0 // Possible way to add teascript code with underscore _name
        {
            Parser p;
            auto func_node_val = p.Parse( "func ( x ) { x * x }", "Core" )->Eval( rTmpContext );
            res.push_back( std::make_pair( "_squared", std::move( func_node_val ) ) );
        }
#endif
        // the standard ValueConfig
        auto const cfg = ValueConfig( ValueShared, ValueConst, rTmpContext.GetTypeSystem() );

        // add start time stamp of this CoreLibrary incarnation
        {
            res.push_back( std::make_pair( "_init_core_stamp", ValueObject( GetTimeStamp(), cfg)));
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
        
        // _print_version : void ( void ) --> prints TeaScript version to stdout
        {
            auto func = std::make_shared< LibraryFunction0< decltype(PrintVersion) > >( &PrintVersion );
            ValueObject val{std::move( func ), cfg };
            res.push_back( std::make_pair( "_print_version", std::move( val ) ) );
        }

        //TODO: add or change to _get_contact for get the string ?
        // _print_contact : void ( void ) --> prints contact info to stdout
        {
            Parser p;
            auto func_node_val = p.Parse( "func () { _out( \"Contact: " TEASCRIPT_CONTACT "\\n\" ) }", "Core" )->Eval( rTmpContext );
            res.push_back( std::make_pair( "_print_contact", std::move( func_node_val ) ) );
        }

        //TODO: add or change to _get_copyright for get the string ?
        // _print_copyright : void ( void ) --> prints copyright info to stdout
        {
            auto func = std::make_shared< LibraryFunction0< decltype(PrintCopyright) > >( &PrintCopyright );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_print_copyright", std::move( val ) ) );
        }

        // _out : void ( String ) --> prints param1 (String) to stdout
        {
            auto func = std::make_shared< LibraryFunction1< decltype(PrintStdOut), std::string > >( &PrintStdOut );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_out", std::move( val ) ) );
        }

        // _err : void ( String ) --> prints param1 (String) to stderr
        {
            auto func = std::make_shared< LibraryFunction1< decltype(PrintStdError), std::string > >( &PrintStdError );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_err", std::move( val ) ) );
        }

        //TODO: Maybe move this out from internals to Bootstrap
        // readline : String ( void ) --> read line from stdin (and blocks until line finished), returns the read line without line feed.
        {
            auto func = std::make_shared< LibraryFunction0< decltype(ReadLine), std::string > >( &ReadLine );
            ValueObject val{std::move( func ), cfg}; //TODO: Make this unconst for user can undef it?
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
        
        // _f64toi64 : i64 (f64) --> converts a f64 to i64. same effect as trunc() but yields a i64.
        {
            auto func = std::make_shared< LibraryFunction1< decltype(DoubleToLongLong), double, long long > >( &DoubleToLongLong );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_f64toi64", std::move( val ) ) );
        }

        // _sqrt : f64 (f64) --> calculates square root
        {
            auto func = std::make_shared< LibraryFunction1< decltype(Sqrt), double, double > >( &Sqrt );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_sqrt", std::move( val ) ) );
        }

        // _strtonum : i64|Bool (String) --> converts a String to i64. this works only with real String objects. convienience for use '+str'.
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

         // _numtostr : String (i64) --> converts a i64 to String. this works only with real i64 objects. convienience for use 'num % ""'
        {
            auto func = std::make_shared< LibraryFunction1< decltype(NumToStr), long long, std::string > >( &NumToStr );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_numtostr", std::move( val ) ) );
        }

        // evaluate and load //TODO: Maybe move this out from internals to Bootstrap?

        // eval : Any (String) --> parses and evaluates the String as TeaScript code and returns its result.
        {
            auto func = std::make_shared< EvalFunc >(false);
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_eval", std::move( val ) ) );
        }

        // eval_file : Any (String) --> parses and evaluates the content of the file and returns its result. All defined functions and variables in the top level scope will stay available.
        {
            auto func = std::make_shared< EvalFunc >( true );
            ValueObject val{std::move( func ), cfg}; //TODO: Make this unconst for user can undef it?
            res.push_back( std::make_pair( "eval_file", std::move( val ) ) ); // missing _ is intended for now.
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

        // _substr : String ( String, from: i64, count: i64 ) --> returns a substring [from, from+count). count -1 means untl end of string. returns empty string on invalid arguments.
        {
            auto func = std::make_shared< LibraryFunction3< decltype(SubStr), std::string, long long, long long, std::string > >( &SubStr );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_substr", std::move( val ) ) );
        }

        // _strfind : i64 ( String, substring: String, offset: i64 ) --> searches for substring from offset and returns found pos of first occurence. -1 if not found.
        {
            auto func = std::make_shared< LibraryFunction3< decltype(StrFind), std::string, std::string, long long, long long > >( &StrFind );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_strfind", std::move( val ) ) );
        }

        // _strfindreverse : i64 ( String, substring: String, offset: i64 ) --> searches for substring from behind from offset and returns found pos of first occurence. -1 if not found.
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


        // time / misc

        // clock : f64 ( void ) --> gets the local wall clock time of the current day in (fractional) seconds as f64. 
        {
            auto func = std::make_shared< LibraryFunction0< decltype(GetLocalTimeInSecs), double > >( &GetLocalTimeInSecs );
            ValueObject val{std::move( func ), cfg}; //TODO: Make this unconst for user can undef it?
            res.push_back( std::make_pair( "clock", std::move( val ) ) ); // missing _ is intended for now.
        }

        // clock_utc : f64 ( void ) --> gets the UTC time of the current day in (fractional) seconds as f64. (note: This UTC time is with leap seconds!)
        {
            auto func = std::make_shared< LibraryFunction0< decltype(GetUTCTimeInSecs), double > >( &GetUTCTimeInSecs );
            ValueObject val{std::move( func ), cfg}; //TODO: Make this unconst for user can undef it?
            res.push_back( std::make_pair( "clock_utc", std::move( val ) ) ); // missing _ is intended for now.
        }

        // _timestamp : f64 ( void ) --> gets the elapsed time in (fractional) seconds as f64 from an unspecified time point during program start. Time is monotonic increasing.
        {
            auto func = std::make_shared< LibraryFunction0< decltype(GetTimeStamp), double > >( &GetTimeStamp );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "_timestamp", std::move( val ) ) );
        }

        // sleep : void ( i64 ) --> sleeps (at least) for given amount of seconds.
        {
            auto func = std::make_shared< LibraryFunction1< decltype(SleepSecs), long long > >( &SleepSecs );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "sleep", std::move( val ) ) );
        }

        // random : i64 ( i64, i64 ) --> creates a random number in between [start,end]. start, end must be >= 0 and <= UINT_MAX.
        {
            auto func = std::make_shared< LibraryFunction2< decltype(CreateRandomNumber), long long, long long, long long > >( &CreateRandomNumber );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "random", std::move( val ) ) );
        }

        // minimalistic (text) file io support

        // cwd : String ( void ) --> returns the current working directory as String
        {
            auto func = std::make_shared< LibraryFunction0< decltype(CurrentPath), std::string > >( &CurrentPath );
            ValueObject val{std::move( func ), cfg}; //TODO: Make this unconst for user can undef it?
            res.push_back( std::make_pair( "cwd", std::move( val ) ) ); // missing _ is intended for now.
        }

        // tempdir : String ( void ) --> returns configured temp directory as String
        {
            auto func = std::make_shared< LibraryFunction0< decltype(TempPath), std::string > >( &TempPath );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "tempdir", std::move( val ) ) ); // missing _ is intended for now.
        }

        // file_size : i64 ( String ) --> returns file size in bytes. -1 on error / file not exists.
        {
            auto func = std::make_shared< LibraryFunction1< decltype(FileSize), std::string, long long> >( &FileSize );
            ValueObject val{std::move( func ), cfg};
            res.push_back( std::make_pair( "file_size", std::move( val ) ) ); // missing _ is intended for now.
        }

        // readtextfile : String|Bool ( String ) --> reads the content of an UTF-8 text file and returns it in a String. An optional BOM is removed.
        {
            auto func = std::make_shared< LibraryFunction1< decltype(ReadTextFile), std::string, ValueObject > >( &ReadTextFile );
            ValueObject val{std::move( func ), cfg}; //TODO: Make this unconst for user can undef it?
            res.push_back( std::make_pair( "readtextfile", std::move( val ) ) ); // missing _ is intended for now.
        }

        // writetextfile : Bool ( file: String, str: String, overwrite: Bool, bom: Bool ) 
        // --> writes the content of Sring to text file. An optional UTF-8 BOM can be written (last Bool param). overwrite indicates if a prior existing file shall be overwritten (old content destroyed!)
        {
            auto func = std::make_shared< LibraryFunction4< decltype(WriteTextFile), std::string, std::string, bool, bool, bool > >( &WriteTextFile );
            ValueObject val{std::move( func ), cfg}; //TODO: Make this unconst for user can undef it?
            res.push_back( std::make_pair( "writetextfile", std::move( val ) ) ); // missing _ is intended for now.
        }

        rTmpContext.BulkAdd( res );
    }
public:
    CoreLibrary() = default;
    virtual ~CoreLibrary() {}

    //TODO: add Config parameter for control more aspects, e.g. no eval, no stdin functions, etc. 

    /// Will bootstrap the standard Core Lib into the Context. if internals_only is true, a minimal version will be loaded (with the underscore names defined only).
    /// IMPORTANT: Any previous data in rContext will be lost / overwritten.
    virtual void Bootstrap( Context &rContext, bool internals_only = false )
    {
        {
            //TODO: Move the internal type regitration to a better place.
            TypeSystem  sys;
            sys.RegisterType<FunctionPtr>("Function");
            sys.RegisterType<std::vector<ValueObject>>("ValueObjectVector");

            Context tmp{ std::move( sys ), true };
            tmp.is_debug = rContext.is_debug; // take over from possible old instance.

            BuildInternals( tmp );

            rContext = std::move( tmp );
            // finalize
            rContext.SetBootstrapDone();
        }

        if( internals_only ) {
            return;
        }

        static constexpr char core_lib_teascript[] = R"_SCRIPT_(
// ===== TEASCRIPT CORE LIBRARY (STANDARD VERSION), API v0 =====

// convenience for can write 'return void' if function shall return nothing
const void := () // void has value NaV (Not A Value)

// constant number PI
const PI := 3.14159265358979323846

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

// prints s + line feed to stderr, will do to string conversion of s
func print_error( s )
{
    //TODO: add log to common logfile
    _err( s % "\n" )
}

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

// prints error_str to stderr, exits the script (with stack unwinding/scope cleanup) with error_code
func fail_with_message( error_str, error_code := _exit_failure )
{
    print_error( error_str )
    fail_with_error( error_code )
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


// parses and evaluates expr (will do to string conversion), returns result of expr
// NOTE: This function opens a new scope, so all new defined variables and functions will be undefined again after the call. Use _eval instead to keep definitions.
func eval( expr )
{
    _eval( expr % "" )
}

func file_exists( file )
{
    file_size( file ) >= 0
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

// covenience for _strfind with default offset
func strfind( str, what, offset := 0 )
{
    _strfind( str, what, offset )
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

func sqrt( val )
{
    _sqrt( to_f64( to_number( val ) ) )
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
        p.Parse( core_lib_teascript, "Core" )->Eval(rContext);
    }
};

} // namespace teascript
