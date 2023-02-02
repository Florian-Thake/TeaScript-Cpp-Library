/*
 * SPDX-FileCopyrightText:  Copyright (c) 2023 Florian Thake <support |at| tea-age.solutions>. All rights reserved.
 * SPDX-License-Identifier: SEE LICENSE IN LICENSE.txt
 *
 * Licensed under the TeaScript Library Standard License. See LICENSE.txt or you may find a copy at
 * https://tea-age.solutions/teascript/product-variants/
 */
#pragma once

#include "Content.hpp"
#include "ParsingState.hpp"
#include "ASTNode.hpp"
#include "Util.hpp"
#include "Exception.hpp"
#include <unordered_set>
#include <cctype> // isdigit
#include <charconv>


namespace teascript {

/// The Parser for TeaScript code.
class Parser
{
    std::shared_ptr<ParsingState> mState;

    static constexpr char LF  = '\n';
    static constexpr char NUL = '\0';

    static inline bool is_whitespace( unsigned char const c )
    {
        // NEW Super Fast, if all values in a switch are in (0..63) then it can be tested with one CPU instruction!
        switch( c ) {
        case ' ':
        case '\t':
        case '\r': // we handle \r as whitespace. only \n is line feed..
            return true;
        default:
            return false;
        }
    }

    static inline bool is_keyword( std::string_view const &s )
    {
        struct KeywordLookup
        {
            std::unordered_set<std::string_view> table;

            KeywordLookup()
            {
                table.insert( "def" );
                table.insert( "undef" );
                table.insert( "const" );
                table.insert( "mutable" );
                table.insert( "is_defined" );
                table.insert( "debug" ); //?
                //table.insert( "is" ); // binop, will be eaten before.
                table.insert( "as" ); // binop (reserved)
                table.insert( "in" ); // binop (reserved)
                table.insert( "if" );
                table.insert( "else" );
                table.insert( "stop" );
                table.insert( "with" ); // cannot occur solo so far...
                table.insert( "loop" );
                table.insert( "repeat" );
                table.insert( "return" );
                table.insert( "forall" );
                table.insert( "func" );
                table.insert( "typeof" );
                table.insert( "typename" );
            }
        };

        static const auto kw_lookup = KeywordLookup();

        return kw_lookup.table.contains( s );
    }

    static bool CheckWord( std::string_view const sv, Content &rHere )
    {
        if( rHere.Remaining() < sv.size() - 1 ) {
            return false;
        }
        if( 0 != std::memcmp( sv.data(), &(rHere.get()), sv.size() ) ) {
            return false;
        }
        // word boundary ?
        unsigned char const  c = static_cast<unsigned char>( rHere[sv.size()] );
        return !std::isalnum( c ) && c != '_';
    }

    static bool CheckWordAndMove( std::string_view const sv, Content &rHere )
    {
        if( CheckWord( sv, rHere ) ) {
            rHere.MoveInLine_Unchecked( static_cast<int>(sv.size()) );
            return true;
        }
        return false;
    }

    // a simple string does not handle escapes and other special things. It is taken like it is. Inner " as well as tabs or line breaks are not possible.
    static bool SimpleString( std::string_view &rOut, Content &rHere )
    {
        if( rHere[0] != '"' ) {
            return false;
        }
        Content cur = rHere + 1;
        for( std::size_t i = 0; i < cur.Remaining(); ++i ) {
            if( not std::isprint( static_cast<unsigned char>(cur[i]) ) ) {
                return false;
            }
            if( cur[i] == '"' ) {
                rHere.MoveInLine_Unchecked( static_cast<int>(i + 2) /* the 2 " */ );
                rOut = std::string_view( &cur.get(), i );
                return true;
            }
        }
        return false;
    }

    inline SourceLocation MakeSrcLoc( Content const &rHere ) const
    {
        return util::make_srcloc( mState->GetFilePtr(), rHere, mState->is_debug );
    }

public:
    /// Default construct the Parser with a default parsing state.
    Parser() : mState( std::make_shared<ParsingState>() ) {}
    Parser( std::shared_ptr<ParsingState> &rState ) : mState( rState ) {};

    /// clears the parsing state.
    void ClearState()
    {
        mState->Clear();
    }

    /// enables or disables debug mode (default: off).
    /// \note enabled debug mode will preserve the source code for the ASTNodes. Thus, the parsing will take slightly longer and the ASTNodes use more memory.
    void SetDebug( bool const enabled ) noexcept
    {
        mState->is_debug = enabled;
    }

    /// Parses one complete code block / script file in once. It must be at least one complete toplevel block/statement/entity.
    /// This method does not suppport partial parsing. The content is not allowed to end in the middle of one statement/expression/block. 
    /// For line-by-line parsing \see ParsePartial()
    /// \returns the moved out ASTNodes (if any) inside an ASTNodeFile instance. The shared pointer is always valid. 
    ASTNodePtr Parse( Content const &rContent, std::string const &rFile = "_EVAL_" )
    {
        ClearState();

        ParsePartial( rContent, rFile );
        return ParsePartialEnd();
    }

    /// Line-By-Line parsing. The given content must consists of 1 to N complete lines including line ending or be completely empty. 
    /// Parsing of incomplete lines is not supported. Line offsets are maintained by this function.
    /// \note If the last character of content is NUL it is not interpreted as script end, but as valid new line.
    void ParsePartial( Content const &rContent, std::string const &rFile = "_EVAL_PARTIAL_" )
    {
        Content content = rContent;

        if( mState->CheckAndChangeFile( rFile ) ) { // new file!
            // remove possible UTF-8 BOM ( \xEF \xBB \xBF ) (for new files only!)
            Content tmp( rContent );
            if( tmp.Remaining() >= 2 /*cur char + 2 remainig = 3 chars */ && *tmp++ == '\xEF' && *tmp++ == '\xBB' && *tmp++ == '\xBF' ) {
                content = tmp.SubContent();
                mState->utf8_bom_removed = true;
            }
        }

        // apply possible line offset of previous partial parse
        if( mState->saved_loc.IsSet() ) {
            content.SetLineOffset( mState->saved_loc.GetEndLine() );
        }

        if( !ParseStatements( content ) ) {
            if( content.HasMore() && content != NUL ) {
                //util::debug_print( content );
                //util::debug_print_currentline( content );
                util::throw_parsing_error( content, mState->GetFilePtr(), "Unknown content at current position! Don't know how to parse!" );
            } else if( content != LF && content != NUL ) {
                util::throw_parsing_error( content, mState->GetFilePtr(), "All parsable content must either end with \\n (line feed) or \\0 (nul)!" );
            }
        }

        // update and memorize current position for possible next partial parse.
        // NOTE: content might be in the next (empty) line already. With that we accumulate an off-by-one error with each call if content ends with \n\0 .
        auto  const  correct_offset = (content == NUL && content[-1] == LF) ? 1LL : 0LL;
        auto  const  line = content.CurrentLine() - correct_offset;
        // NOTE: In case of correct_offset CurrentColumn() always points to 1 instead of the last col of previous line!
        if( mState->saved_loc.IsSet() ) { // NOTE: Start line / column is also updated by still open multi-line comments in Comment()
            if( line == mState->saved_loc.GetStartLine() ) { // Be aware of GetStartColumn() > CurrentColumn() !!!
                mState->saved_loc.SetEnd( line, std::max( content.CurrentColumn(), mState->saved_loc.GetEndColumn() ) );
            } else {
                mState->saved_loc.SetEnd( line, content.CurrentColumn() );
            }
        } else {
            mState->saved_loc = SourceLocation( line, content.CurrentColumn() );
        }
    }

    /// Checks for leftovers after several ParsePartial() calls, e.g. unfinished multi-line comments or incomplete ASTNode...
    /// This method should be called after the complete content was passed to 1 to N ParsePartial() calls. \throws if there are leftovers.
    /// \returns the moved out ASTNodes (if any) inside an ASTNodeFile instance. The shared pointer is always valid. 
    ASTNodePtr ParsePartialEnd()
    {
        if( mState->is_in_comment ) {
            throw exception::parsing_error( mState->saved_loc, "multi line comment not closed! ( '*/' )" );
        }
        auto incomplete = mState->GetFirstIncompleteASTNode();
        if( incomplete ) {
            //FIXME: Better use exception from ASTNode. But that is an eval_error and can only obtained via call to Eval() actually!
            throw exception::parsing_error( incomplete->GetSourceLocation(), "Parsing error: " + incomplete->GetInfoStr() + " is not complete!" );
        }
        return std::make_shared<ASTNode_File>( mState->GetFileName(), mState->MoveOutASTCollection() );
    }

    // low level access to the last available top level ASTNode. ASTNodePtr might be a nullptr! (internal use only, might be reomved without further notice!)
    ASTNodePtr GetLastToplevelASTNode() const
    {
        return mState->GetLastToplevelASTNode();
    }

    static inline void SkipToNextLine( Content &rHere )
    {
        // Benchmarked: OLD needs nearly double of time in combination with OLD SkipWhiteSpace benchmark.
#if 1 // NEW
        // advance to line feed
        rHere.MoveToLineFeed();
        ++rHere; // skip it.
#else //OLD, the ++op also tests for 'has more' and LF and it conditional updates col for each iteration.
        // advance to line feed
        while( rHere.HasMore() && *rHere != LF ) ++rHere;
        ++rHere; // skip it.
#endif
    }

    static inline void SkipWhitespace( Content &rHere )
    {
        while( is_whitespace( static_cast<unsigned char>(*rHere) ) && rHere.HasMore() ) rHere.IncInLine_Unchecked();
    }


    bool HashLine( Content &rHere )
    {
        if( rHere.CurrentColumn() == 1 && *rHere == '#' ) {
            //TODO: parse '##option' for set options of the parser/engine.
            SkipToNextLine( rHere );
            return true;
        }
        return false;
    }


    bool Comment( Content &rHere )
    {
        if( mState->is_in_comment ) {
            // search for the end of the multi line comment.
            while( rHere.HasMore() ) {
                if( rHere == '*' && rHere[1] == '/' ) {
                    ++rHere; // skip '*'
                    ++rHere; // skip '/'
                    mState->is_in_comment = false;
                    return true;
                }
                ++rHere;
            }
            return true;
        } else {
             // single line comment (counts until end of line)
            if( rHere.Remaining() > 0 && rHere == '/' && rHere[1] == '/' ) {
                SkipToNextLine( rHere );
                return true;
            }

            /* multi line comment (counts until end marker found) */
            if( rHere.Remaining() > 0 && rHere == '/' && rHere[1] == '*' ) {
                Content const  saved( rHere );
                ++rHere; // skip '/'
                ++rHere; // skip '*'
                while( rHere.HasMore() ) {
                    if( rHere == '*' && rHere[1] == '/' ) {
                        ++rHere; // skip '*'
                        ++rHere; // skip '/'
                        return true;
                    }
                    ++rHere;
                }

                // end not found
                mState->saved_loc = MakeSrcLoc( saved );
                mState->is_in_comment = true;
                return true;
            }

            return false;
        }
    }


    /// Parses an integer (long long default) or decimal (double)
    bool Int( Content &rHere )
    {
        bool skip = false;
        unsigned char const  first = static_cast<unsigned char>(*rHere);
        if( !std::isdigit( first ) ) {
            if( (first == '-' || first == '+') && std::isdigit( static_cast<unsigned char>(rHere[1]) ) ) {
                skip = true;
            } else {
                return false;
            }
        }
        Content const start = first=='+' ? rHere + 1 : rHere; // skip unary +
        if( skip ) ++rHere; // advance to digit

        while( rHere.HasMore() && std::isdigit( static_cast<unsigned char>(*rHere) ) ) ++rHere;

        // floating point number?
        if( rHere == '.' || rHere == 'e' ) {
            if( rHere == 'e' && rHere[1] == '-' ) {
                ++rHere;
            }
            ++rHere;
            //FIXME: 123.456e-12 not supported (decimal point _and_ e)
            while( rHere.HasMore() && std::isdigit( static_cast<unsigned char>(*rHere) ) ) ++rHere;
            double val = -1.;
#if !_LIBCPP_VERSION // libc++14 fails here            
            auto const  res = std::from_chars( &(*start), (&(*rHere)), val );
            if( std::errc::result_out_of_range == res.ec ) {
                throw std::out_of_range( "Double constant too big!" );
            } else if( std::errc::invalid_argument == res.ec ) { // huh? safety...
                rHere = start; // rollback
                return false;
            }
#else
            std::string const copy( &(*start), (&(*rHere)) );
            val = std::stod( copy ); // may throw ...
#endif

            mState->AddASTNode( std::make_shared<ASTNode_Constant>( val, util::make_srcloc( mState->GetFilePtr(), start ) ) );

            return true;
        }

        long long val = -1;
        auto const  res = std::from_chars( &(*start), (&(*rHere)), val, 10 );
        if( std::errc::result_out_of_range == res.ec ) {
            throw std::out_of_range( "Integer constant too big!" );
        } else if( std::errc::invalid_argument == res.ec ) { // huh? safety...
            rHere = start; // rollback
            return false;
        }

        mState->AddASTNode( std::make_shared<ASTNode_Constant>( val, util::make_srcloc( mState->GetFilePtr(), start ) ) );

        return true;
    }

    /// Parses a string connstant or a string with in-string evaluation.
    bool String( Content &rHere, bool const in_string_eval_enabled )
    {
        if( rHere != '"' ) {
            return false;
        }
        Content const  start = rHere;
        ++rHere;
        bool finish = false;
        bool in_string_eval = false;
        while( !finish ) {
            if( !in_string_eval ) {
                std::string  str;
                while( rHere.HasMore() && rHere != '"' && !in_string_eval ) {
                    switch( *rHere ) {
                    case '\\':  /* all escape sequences */
                        switch( rHere[1] ) {
                        case 't':  str.push_back( '\t' ); break;
                        case 'r':  str.push_back( '\r' ); break;
                        case 'n':  str.push_back( '\n' ); break;
                        case '"':  str.push_back( '"' );  break;
                        case '\\': str.push_back( '\\' ); break;
                        case '%':  str.push_back( '%' );  break;
                        default:
                            util::throw_parsing_error( rHere, mState->GetFilePtr(), "Invalid string escape sequence!" );
                        }
                        ++rHere;
                        break;
                    case '%':  /* in string eval ? */
                        if( in_string_eval_enabled ) {
                            if( rHere[1] == '(' ) {
                                in_string_eval = true;
                                break;
                            }
                        }
                        [[fallthrough]];
                    default:
                        str.push_back( *rHere );
                    }
                    ++rHere;
                } // end while inner
                if( !rHere.HasMore() ) {
                    util::throw_parsing_error( start, mState->GetFilePtr(), "End of string not found!" );
                }
                // add the until here parsed string.
                mState->AddASTNode( std::make_shared<ASTNode_Constant>( std::move(str) ) );
                if( !in_string_eval ) {
                    ++rHere;
                    finish = true;
                }
            } else {
                assert( rHere == '(' );
                Content const  start_eval = rHere;
                std::string expr;
                expr.reserve( 32 );
                // We are finish when we found the outer closing bracket ).
                // Open brackets are counted with nested.
                // Don't count brackets inside a string literal!
                // Handle escape \" in string literal to not end the string too early!
                bool in_string = false;
                int nested = 0;
                while( rHere.HasMore() ) {
                    switch( *rHere ) {
                    case '(': if( !in_string ) ++nested; break;
                    case ')': if( !in_string ) --nested; break;
                    case '"': in_string = not in_string; break;
                    case '\\': /* possible escape sequence */
                        if( in_string ) {
                            // add escape and advance to next char which will be added unchecked.
                            expr.push_back( *rHere++ );
                        }
                        break;
                    default: break;
                    }
                    expr.push_back( *rHere );
                    ++rHere;
                    if( 0 == nested ) {
                        break;
                    }
                } // end while

                if( !rHere.HasMore() && nested > 0 ) {
                    util::throw_parsing_error( start_eval, mState->GetFilePtr(), "End of in-string-eval expression not found!" );
                }

                // add binary string concatenation operator first.
                mState->AddASTNode( std::make_shared<ASTNode_Binary_Operator>( "%" ) );

                // then parse and add the in-string expression.
                Content in_string_expr( expr );
                ParseStatements( in_string_expr );
                // add binary string concatenation operator for the rest of the string.
                mState->AddASTNode( std::make_shared<ASTNode_Binary_Operator>( "%" ) );

                in_string_eval = false;
            } // if
        } // end while outer
        return true;
    }

    enum eIDResult
    {
        IDNotFound = 0,
        IDResultOperator,
        IDResultID
    };

    eIDResult ID( Content &rHere )
    {
        // first char must be alpha or _, all others alpha, _ or number
        if( rHere != '_' && !std::isalpha( static_cast<unsigned char>(*rHere) ) ) {
            return IDNotFound;
        }
        Content const start = rHere++;
        while( rHere.HasMore() && (rHere == '_' || std::isalnum(static_cast<unsigned char>(*rHere))) ) ++rHere;

        std::string_view const id{&(*start), &(*rHere)};

        // check for constants first (this is more like a workaround, could be done anywhere else also...)
        if( id == "true" ) {
            mState->AddASTNode( std::make_shared<ASTNode_Constant>( true, util::make_srcloc( mState->GetFilePtr(), start ) ) );
            return IDResultID; // same effect
        } else if( id == "false" ) {
            mState->AddASTNode( std::make_shared<ASTNode_Constant>( false, util::make_srcloc( mState->GetFilePtr(), start ) ) );
            return IDResultID; // same effect
        } else if( id == "and" || id == "or" || id == "mod" // logical and / or, arithmetic modulo
                || id == "lt"  || id == "le" || id == "gt" || id == "ge" || id == "ne" || id == "eq" ) { // comparison
            mState->AddASTNode( std::make_shared<ASTNode_Binary_Operator>( std::string( id ), MakeSrcLoc( start ) ) );
            return IDResultOperator;
        } else if( id == "not" ) { // logical not
            mState->AddASTNode( std::make_shared<ASTNode_Unary_Operator>( std::string( id ), MakeSrcLoc( start ) ) );
            return IDResultOperator;
        } else if( id == "is" ) { // is type operator
            mState->AddASTNode( std::make_shared<ASTNode_Is_Type>( MakeSrcLoc( start ) ) );
            return IDResultOperator;
        }

        // NOTE: With the actual design kind of superfluous. Only for somebody calls this method from outside directly.
        if( is_keyword( id ) ) {
            util::throw_parsing_error( start, mState->GetFilePtr(), "Keyword not allowed as identifier!" );
        }
     
        mState->AddASTNode( std::make_shared<ASTNode_Identifier>( id, MakeSrcLoc( start ) ) );

        return IDResultID;
    }

    enum class eSymFound
    {
        Nothing = 0,
        Operator,
        OpenExpr,
        CloseExpr,
        OpenBlock,
        CloseBlock,
    };

    /// parses all non alpha-numeric symbols (except strings)
    eSymFound Symbol( Content &rHere )
    {
        Content const start = rHere;
        switch( *rHere ) {
        case '-':
        case '+': 
            {
                char const op = *rHere;
                if( mState->CanAddNodeWhichNeedLHS() ) {
                    mState->AddASTNode( std::make_shared<ASTNode_Binary_Operator>( std::string( 1, op ), MakeSrcLoc( start ) ) );
                } else if( std::isdigit( static_cast<unsigned char>(rHere[1])) ) {
                    // parse as -Num/+Num constant!
                    return eSymFound::Nothing;
                } else {
                    // unary operator +/-
                    mState->AddASTNode( std::make_shared<ASTNode_Unary_Operator>( std::string( 1, op ), MakeSrcLoc( start ) ) );
                }
                ++rHere;
                return eSymFound::Operator;
            }
        case '*':
        case '/':
        case '%':
            {
                char const op = *rHere;
                mState->AddASTNode( std::make_shared<ASTNode_Binary_Operator>( std::string( 1, op ), MakeSrcLoc( start ) ) );
                ++rHere;
                return eSymFound::Operator;
            }
        case '<':
        case '>':
            {
                std::string op( 1, *rHere );
                ++rHere;
                if( rHere == '=' ) {
                    op += '=';
                    ++rHere;
                }
                mState->AddASTNode( std::make_shared<ASTNode_Binary_Operator>( op, MakeSrcLoc( start ) ) );
                return eSymFound::Operator;
            }
        case '=':
            if( rHere[1] == '=' ) {
                rHere += 2;
                mState->AddASTNode( std::make_shared<ASTNode_Binary_Operator>( "==", MakeSrcLoc( start ) ) );
                return eSymFound::Operator;
            }
            return eSymFound::Nothing;
        case '!':
            if( rHere[1] == '=' ) {
                rHere += 2;
                mState->AddASTNode( std::make_shared<ASTNode_Binary_Operator>( "!=", MakeSrcLoc( start ) ) );
                return eSymFound::Operator;
            }
            return eSymFound::Nothing;
        case ':':
            if( rHere[1] == '=' ) {
                rHere += 2;
                mState->AddASTNode( std::make_shared<ASTNode_Assign>( false /*not shared / copy*/, MakeSrcLoc( start ) ) );
                return eSymFound::Operator;
            }
            return eSymFound::Nothing;
        case '@':
            switch( rHere[1] ) {
            case '@':
                rHere += 2;
                mState->AddASTNode( std::make_shared<ASTNode_Binary_Operator>( "@@", MakeSrcLoc( start ) ) ); // shared_with
                return eSymFound::Operator;
            case '=':
                rHere += 2;
                mState->AddASTNode( std::make_shared<ASTNode_Assign>( true /* shared*/, MakeSrcLoc( start ) ) );
                return eSymFound::Operator;
            case '?':
                rHere += 2;
                mState->AddASTNode( std::make_shared<ASTNode_Unary_Operator>( "@?", MakeSrcLoc( start ) ) ); // share_count
                return eSymFound::Operator;
            }
            return eSymFound::Nothing;
        case '(':
            mState->StartExpression( MakeSrcLoc( start ) );
            ++rHere;
            return eSymFound::OpenExpr;
        case ')':
            mState->EndExpression( MakeSrcLoc( start ) );
            ++rHere;
            return eSymFound::CloseExpr;
        case '{':
            mState->StartBlock( MakeSrcLoc( start ) );
            ++rHere;
            return eSymFound::OpenBlock;
        case '}':
            mState->EndBlock( MakeSrcLoc( start ) );
            ++rHere;
            return eSymFound::CloseBlock;
        case LF:  // we handle \r as whitespace. only \n is line feed!
        case ',': // like new line
            mState->NewLine();
            ++rHere;
            return eSymFound::Operator; // same effect.
        default:
            return eSymFound::Nothing;
        }   
    }

    bool Var_Def_Undef( Content &rHere )
    {
        Content const start = rHere;
        if( CheckWordAndMove( "def", rHere ) ) {
            mState->AddASTNode( std::make_shared<ASTNode_Var_Def_Undef>( ASTNode_Var_Def_Undef::eType::Def, MakeSrcLoc( start ) ) );
            return true;
        } else if( CheckWordAndMove( "const", rHere ) ) {
            mState->AddASTNode( std::make_shared<ASTNode_Var_Def_Undef>( ASTNode_Var_Def_Undef::eType::Const, MakeSrcLoc( start ) ) );
            return true;
        } else if( CheckWordAndMove( "undef", rHere ) ) {
            mState->AddASTNode( std::make_shared<ASTNode_Var_Def_Undef>( ASTNode_Var_Def_Undef::eType::Undef, MakeSrcLoc( start ) ) );
            return true;
        } else if( CheckWordAndMove( "is_defined", rHere ) ) {
            mState->AddASTNode( std::make_shared<ASTNode_Var_Def_Undef>( ASTNode_Var_Def_Undef::eType::IsDef, MakeSrcLoc( start ) ) );
            return true;
        } else if( CheckWordAndMove( "debug", rHere ) ) {
            mState->AddASTNode( std::make_shared<ASTNode_Var_Def_Undef>( ASTNode_Var_Def_Undef::eType::Debug, MakeSrcLoc( start ) ) );
            return true;
        }
        return false;
    }

    bool Typeof_Typename( Content &rHere )
    {
        Content const start = rHere;
        if( CheckWordAndMove( "typename", rHere ) ) {
            mState->AddASTNode( std::make_shared<ASTNode_Typeof_Typename>( true, MakeSrcLoc( start ) ) );
            return true;
        } else if( CheckWordAndMove( "typeof", rHere ) ) {
            mState->AddASTNode( std::make_shared<ASTNode_Typeof_Typename>( false, MakeSrcLoc( start ) ) );
            return true;
        }
        return false;
    }


    /// return value of Parsing functions which must signal more than just a 'found',
    /// e.g. if/else, stop, loop, return
    enum eFound
    {
        NotFound = 0,
        FoundOpen,     // obsolete now (handled by Symbol())
        FoundClose,    // obsolete now (handled by Symbol())
        FoundIf   = FoundOpen,
        FoundElse = FoundClose,
        FoundControl = FoundOpen,
        FoundWith    = FoundClose,
    };


    eFound Control_Stop_Loop( Content &rHere )
    {
        Content const start = rHere;
        if( CheckWordAndMove( "stop", rHere ) ) {
            SkipWhitespace( rHere );
            std::string_view sv;
            std::string label = "";
            if( SimpleString( sv, rHere ) ) {
                label = sv;
            }
            bool with_statement = false;
            SkipWhitespace( rHere );
            if( CheckWordAndMove( "with", rHere ) ) {
                with_statement = true;
            }
            mState->AddASTNode( std::make_shared<ASTNode_StopLoop_Statement>( label, with_statement, MakeSrcLoc( start ) ) );
            return with_statement ? FoundWith : FoundControl;
        } else if( CheckWordAndMove( "loop", rHere ) ) {
            SkipWhitespace( rHere );
            std::string_view sv;
            std::string label = "";
            if( SimpleString( sv, rHere ) ) {
                label = sv;
            }
            mState->AddASTNode( std::make_shared<ASTNode_LoopToHead_Statement>( label, MakeSrcLoc( start ) ) );
            return FoundControl;
        }
        return NotFound;
    }

    eFound Return( Content &rHere )
    {
        Content const start = rHere;
        if( CheckWordAndMove( "return", rHere ) ) {
            // for the time being a statement/expression is mandatory!
            mState->AddASTNode( std::make_shared<ASTNode_Return_Statement>( true, MakeSrcLoc( start ) ) );
            return FoundWith;
        }
        return NotFound;
    }

    eFound If_Else( Content &rHere )
    {
        if( CheckWord( "if", rHere ) ) {
            mState->StartIf( MakeSrcLoc( rHere ) );
            rHere += 2;
            return FoundIf;
        } else if( CheckWord( "else", rHere ) ) {
            mState->StartElse( MakeSrcLoc( rHere ) );
            rHere.MoveInLine_Unchecked( 4 );
            return FoundElse;
        }
        return NotFound;
    }

    bool Repeat( Content &rHere )
    {
        if( CheckWordAndMove( "repeat", rHere ) ) {
            Content const start = rHere;
            SkipWhitespace( rHere );
            std::string_view sv;
            std::string label = "";
            if( SimpleString( sv, rHere ) ) {
                label = sv;
            }
            mState->StartRepeat( label, MakeSrcLoc( start ) );
            return true;
        }
        return false;
    }

    bool Func( Content &rHere )
    {
        if( CheckWordAndMove( "func", rHere ) ) {
            mState->StartFunc( MakeSrcLoc( rHere ) );
            return true;
        }
        return false;
    }


    /// Parses all of content in \param rHere. Every line must be complete including the line ending. 
    /// Partial parsing is supported (rHere must not be the complete script, but at least one line (or empty).).
    bool ParseStatements( Content &rHere )
    {
        // Helper struct to check required line break after a statement is complete.
        // Otherwise whitespace as statement separator would be enough.
        struct NextLineRequired_Helper
        {
            bool is_set = false;
            long long line = -1;

            void Unset()
            {
                is_set = false;
            }

            void Set( Content const &r )
            {
                line = r.CurrentLine();
                is_set = true;
            }

            bool HasViolation( Content const &r )
            {
                if( is_set ) {
                    is_set = false;
                    return !(r.CurrentLine() > line);
                }
                return false;
            }
        } next_line_required;

        // the loop tries to parse all until HasMore() of given input content returns false.
        // one loop iteration is one symbol with one prior skipped whitespace.

        do {

            Content const  pos1 = rHere; // save current pos.

            // first parse until not in multi-line comment anymore.
            if( mState->is_in_comment ) {
                Comment( rHere );
                if( mState->is_in_comment ) { // still in comment ...
                    return not rHere.HasMore();
                }
            } else if( HashLine( rHere ) ) {

            } else {
                SkipWhitespace( rHere );

                if( Comment( rHere ) ) {
                    if( mState->is_in_comment ) {
                        return not rHere.HasMore();
                    }
                } else if( eSymFound const symfound = Symbol( rHere ); symfound != eSymFound::Nothing ) {
                    switch( symfound ) {
                    case eSymFound::Operator:
                        next_line_required.Unset();
                        break;
                    case eSymFound::OpenExpr:
                        if( mState->IsInCall() ) {
                            next_line_required.Unset();
                        } else if( next_line_required.HasViolation( rHere ) ) {
                            util::throw_parsing_error( rHere, mState->GetFilePtr(), "More than one statement/expression per line! '\\n' (line feed) missing!" );
                        }
                        break;
                    case eSymFound::CloseExpr:
                        if( mState->IsInIf() || mState->IsInFunc() ) {
                            next_line_required.Unset();
                        } else {
                            next_line_required.Set( rHere );
                        }
                        break;
                    case eSymFound::OpenBlock:
                        if( next_line_required.HasViolation( rHere ) ) {
                            util::throw_parsing_error( rHere, mState->GetFilePtr(), "More than one statement/expression per line! '\\n' (line feed) missing!" );
                        }
                        break;
                    case eSymFound::CloseBlock:
                        next_line_required.Set( rHere );
                        break;
                    case eSymFound::Nothing: [[fallthrough]];
                    default:
                        //TODO: use std::unreachable in C++23
#if defined( _MSC_VER ) // MSVC
                        __assume(false);
#else // GCC, clang, ...
                        __builtin_unreachable();
#endif
                    }
                } else if( Var_Def_Undef( rHere ) ) {
                    if( next_line_required.HasViolation( rHere ) ) {
                        util::throw_parsing_error( rHere, mState->GetFilePtr(), "More than one statement/expression per line! '\\n' (line feed) missing!" );
                    }
                } else if( Typeof_Typename( rHere ) ) {
                    next_line_required.Unset();
                } else if( eFound const found_return = Return( rHere ) ) {
                    if( next_line_required.HasViolation( rHere ) ) {
                        util::throw_parsing_error( rHere, mState->GetFilePtr(), "More than one statement/expression per line! '\\n' (line feed) missing!" );
                    }
                    if( found_return != FoundWith ) {
                        next_line_required.Set( rHere );
                    }
                } else if( eFound const found_control = Control_Stop_Loop( rHere ) ) {
                    if( next_line_required.HasViolation( rHere ) ) {
                        util::throw_parsing_error( rHere, mState->GetFilePtr(), "More than one statement/expression per line! '\\n' (line feed) missing!" );
                    }
                    if( found_control != FoundWith ) {
                        next_line_required.Set( rHere );
                    }
                } else if( Func( rHere ) ) {
                    if( next_line_required.HasViolation( rHere ) ) {
                        util::throw_parsing_error( rHere, mState->GetFilePtr(), "More than one statement/expression per line! '\\n' (line feed) missing!" );
                    }
                } else if( Repeat( rHere ) ) {
                    if( next_line_required.HasViolation( rHere ) ) {
                        util::throw_parsing_error( rHere, mState->GetFilePtr(), "More than one statement/expression per line! '\\n' (line feed) missing!" );
                    }
                } else if( eFound const found_if = If_Else( rHere ) ) {
                    if( found_if == FoundIf && next_line_required.HasViolation( rHere ) ) {
                        util::throw_parsing_error( rHere, mState->GetFilePtr(), "More than one statement/expression per line! '\\n' (line feed) missing!" );
                    } else if( found_if == FoundElse ) {
                        next_line_required.Unset();
                    }
                } else if( eIDResult const resid = ID( rHere ) ) {
                    if( resid == IDResultID ) {
                        if( next_line_required.HasViolation( rHere ) ) {
                            util::throw_parsing_error( rHere, mState->GetFilePtr(), "More than one statement/expression per line! '\\n' (line feed) missing!" );
                        }
                        if( !mState->IsInFunc() ) {
                            next_line_required.Set( rHere );
                        }
                    } else { // Operator
                        next_line_required.Unset();
                    }
                } else if( String( rHere, true ) ) {
                    if( next_line_required.HasViolation( rHere ) ) {
                        util::throw_parsing_error( rHere, mState->GetFilePtr(), "More than one statement/expression per line! '\\n' (line feed) missing!" );
                    }
                    next_line_required.Set( rHere );
                } else if( Int( rHere ) ) {
                    if( next_line_required.HasViolation( rHere ) ) {
                        util::throw_parsing_error( rHere, mState->GetFilePtr(), "More than one statement/expression per line! '\\n' (line feed) missing!" );
                    }
                    next_line_required.Set( rHere );
                }
            }

            // check if there was progress in the last run or we reached EOS.
            if( pos1.Processed() == rHere.Processed() || rHere == NUL ) {
                break;
            }

        } while( rHere.HasMore() );

        return not rHere.HasMore();
    }

};


} // namespace teascript

