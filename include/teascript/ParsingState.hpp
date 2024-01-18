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

#include <string>
#include <vector>
#include <stack>

#include "ASTNode.hpp"
#include "ASTNodeFunc.hpp"
#include "Exception.hpp"
#include "Dialect.hpp"


#define TEASCRIPT_ENABLED_STARTCALL_DEEP_LIFTING   1
#define TEASCRIPT_DEBUG_ADDASTNODE  0

#if TEASCRIPT_DEBUG_ADDASTNODE
//#define TEASCRIPT_DISABLE_FMTLIB 1
# include "Print.hpp"
#endif


namespace teascript {

class ParsingState
{
    std::shared_ptr<std::string> mFileName;

    using ASTNodeCollection = std::vector<ASTNodePtr>;
    ASTNodeCollection  mWorkingAST;

    // node cache for memory nodes which can be optionally extended, e.g. optional else branch.
    struct CacheEntry
    {
        ASTNodePtr  node;
        bool hit = false;
    };

    using NodeCache = std::stack<CacheEntry>;

    NodeCache  mCache;


    struct IndexState
    {
        size_t  idx;
        enum eType
        {
            Expr = 1,
            Block,
            If,
            Else,
            Repeat,
            Func,
            Params, // more generic: List / Tuple ?
            Call,
            Subscript, // [
            Forall,
            ForallCond,
        } type;
        inline operator size_t() const noexcept
        {
            return idx;
        }
        inline ptrdiff_t to_ptrdiff() const noexcept
        {
            return static_cast<ptrdiff_t>(idx);
        }
    };

    std::stack<IndexState> mIndexStack;

    // for partial evaluation. memorize last partial index.
    size_t ast_offset = 0;

    // NOTE: This bool does _NOT_ reflect nested operations, it does only reflect the status of the last added statement!
    bool open_statement = false; // true if the very last added statement is still open. 

public:
    // settings, these survive clear calls.
    Dialect  dialect;      // TeaScipt language behavior. (default is TeaScript standard language)
    bool is_debug = false;

    // these settings will be reset on clear calls.
    // these state infos are maintained and only be used by the outer level. It is just here to have everything collected in one state object.
    bool utf8_bom_removed = false;
    bool is_in_comment    = false;
    size_t is_in_rawstring = 0;    // >0 in rawstring. Is the amount of parsed starting ".
    std::string     raw_string;
    SourceLocation  saved_loc; // saved starting position for line-by-line calls, e.g. during multi line comments.

public:
    ParsingState()
        : mFileName( std::make_shared<std::string>( "" ) )
    {
    }

    /// Clears previous state
    void Clear()
    {
        ast_offset = 0;
        open_statement = false;
        utf8_bom_removed = false;
        is_in_comment = false;
        is_in_rawstring = 0;
        raw_string.clear();
        saved_loc = SourceLocation();
        mFileName = std::make_shared<std::string>( "" );
        mWorkingAST.clear();
        mCache = NodeCache();
        mIndexStack = std::stack<IndexState>();
        mWorkingAST.reserve( 8 );
    }

    /// clears previous state if \param rFile is different from the last. After that sets the new file name.
    /// \return If the rFile name is equal to the old, it does nothing and returns false, true otherwise.
    bool CheckAndChangeFile( std::string const &rFile )
    {
        if( *mFileName != rFile ) {
            Clear();

            mFileName = std::make_shared<std::string>( rFile );
            return true;
        }
        return false;
    }

    std::string const &GetFileName() const noexcept
    {
        return *mFileName;
    }

    std::shared_ptr<std::string> const &GetFilePtr() const noexcept
    {
        return mFileName;
    }

    bool IsInIf() const noexcept
    {
        return !mIndexStack.empty() && mIndexStack.top().type == IndexState::If;
    }

    bool IsInFunc() const noexcept
    {
        return !mIndexStack.empty() && mIndexStack.top().type == IndexState::Func;
    }

    bool IsInParams() const noexcept
    {
        return !mIndexStack.empty() && mIndexStack.top().type == IndexState::Params;
    }

    bool IsInCall() const noexcept
    {
        return !mIndexStack.empty() && mIndexStack.top().type == IndexState::Call;
    }

    bool IsInForall() const noexcept
    {
        return !mIndexStack.empty() && mIndexStack.top().type == IndexState::Forall;
    }

    bool IsInForallCondition() const noexcept
    {
        return !mIndexStack.empty() && mIndexStack.top().type == IndexState::ForallCond;
    }

    void NewLine()
    {
        if( open_statement ) {
            // special case: "func name( id @=,"  --> no default value/expr specified. Then a parameter is mandatory, add a dummy to signal it.
            if( IsInParams() && not mWorkingAST.empty() && mWorkingAST.back()->IsIncomplete() ) {
                if( nullptr != dynamic_cast<ASTNode_Assign *>(mWorkingAST.back().get()) && not mWorkingAST.back()->NeedLHS() ) {
                    mWorkingAST.back()->AddChildNode( std::make_shared<ASTNode_Dummy>() );
                }
            }
            if( mWorkingAST.empty() || (mWorkingAST.back()->IsComplete() && !mWorkingAST.back()->IsDummy()) ) {
                open_statement = false;
            }
        }
    }

    void StartFunc( SourceLocation loc = {} )
    {
        mIndexStack.push( IndexState{mWorkingAST.size(), IndexState::Func} );
        // dummy node ensures that new nodes will be inserted after it - not inside it!
        mWorkingAST.emplace_back( std::make_shared<ASTNode_Dummy>("func", std::move(loc)));
    }

    void EndFunc( SourceLocation loc = {} )
    {
        // check if we have a starting if.
        if( mIndexStack.empty() || !mWorkingAST.at( mIndexStack.top() )->IsDummy() || mIndexStack.top().type != IndexState::Func ) {
            throw exception::parsing_error( std::move( loc ), "EndFunc: There is no function definition!" );
        }

        // merge the source locations start func / end func
        SourceLocation  start_loc = mWorkingAST.at( mIndexStack.top() ).get()->GetSourceLocation();
        if( loc.IsSet() ) {
            start_loc.SetEnd( loc.GetStartLine(), loc.GetStartColumn() );
        }

        //TODO: optional return type
        // func name( params )  {} -->  
        // func[a=b] ( params ) {} -->  func [capture|name] expr block    -->   need 2 or 3 ASTNodes after(!) the dummy, one expr, one block, an optional name or capture list

        if( mIndexStack.top() != mWorkingAST.size() - 3 /*dummy + expr + block*/ 
         && mIndexStack.top() != mWorkingAST.size() - 4 /*dummy + capture|name + expr + block*/  ) {
            throw exception::parsing_error( std::move(start_loc), "EndFunc: wrong function definition. Need 'func' following by an optional name + one expr + one block." );
        }

        // safety!
        if( mWorkingAST.back()->IsIncomplete() ) {
            throw exception::parsing_error( mWorkingAST.back()->GetSourceLocation(), "EndFunc: Last node is not complete!" );
        }
        // TODO: relax this check??
        if( nullptr == dynamic_cast<ASTNode_Block *>(mWorkingAST.at( mWorkingAST.size() - 1 ).get()) ) {
            throw exception::parsing_error( mWorkingAST.at( mWorkingAST.size() - 1 )->GetSourceLocation(), 
                                            "EndFunc: wrong func definition. Last ASTNode must be a block." );
        }
        if( nullptr == dynamic_cast<ASTNode_Expression *>(mWorkingAST.at( mWorkingAST.size() - 2 ).get()) ) {
            throw exception::parsing_error( mWorkingAST.at( mWorkingAST.size() - 2 )->GetSourceLocation(), 
                                            "EndFunc: wrong func definition. Wrong ASTNode for parameters, must be Expression." );
        }
        if( mWorkingAST.size() - mIndexStack.top() > 3 /* first is dummy */ ) {
            if( nullptr == dynamic_cast<ASTNode_Identifier *>(mWorkingAST.at( mWorkingAST.size() - 3 ).get()) ) {
                throw exception::parsing_error( mWorkingAST.at( mWorkingAST.size() - 3 )->GetSourceLocation(), 
                                                "EndFunc: wrong func definition. Wrong ASTNode for function name, must be Identifier." );
            }
        }

        // === START COMMON CODE BLOCK ===

        auto func_def = std::make_shared<ASTNode_Func>( std::move(start_loc) );
        // collect all childs which are after the dummy
        for( size_t i = mIndexStack.top() + 1; i != mWorkingAST.size(); ++i ) {
            func_def->AddChildNode( mWorkingAST[i] );
        }
        // this func definition is complete now.
        func_def->SetComplete();
        // erase all nodes from dummy to end
        mWorkingAST.erase( mWorkingAST.begin() + mIndexStack.top().to_ptrdiff(), mWorkingAST.end());
        // remove saved index
        mIndexStack.pop();
        // finally add new func definition.
        AddASTNode( std::move( func_def ) );

        // === END COMMON CODE BLOCK ===
    }

    void StartRepeat( std::string const &rLabel = "", SourceLocation loc = {} )
    {
        mIndexStack.push( IndexState{mWorkingAST.size(), IndexState::Repeat} );
        // dummy node ensures that new nodes will be inserted after it - not inside it!
        mWorkingAST.emplace_back( std::make_shared<ASTNode_Dummy>( "repeat", rLabel, std::move(loc)));
    }

    void EndRepeat( SourceLocation loc = {} )
    {
        // check if we have a starting repeat.
        if( mIndexStack.empty() || !mWorkingAST.at( mIndexStack.top() )->IsDummy() || mIndexStack.top().type != IndexState::Repeat ) {
            throw exception::parsing_error( std::move( loc ), "EndRepeat: There is no repeat statement!" );
        }

        // merge the source locations start repeat / end repeat
        SourceLocation  start_loc = mWorkingAST.at( mIndexStack.top() ).get()->GetSourceLocation();
        if( loc.IsSet() ) {
            start_loc.SetEnd( loc.GetStartLine(), loc.GetStartColumn() );
        }

        if( mIndexStack.top() != mWorkingAST.size() - 2 /*dummy + block*/ ) {
            throw exception::parsing_error( std::move(start_loc), "Endrepeat: wrong repeat statement. Need 'repeat' following by one block." );
        }
        // safety!
        if( mWorkingAST.back()->IsIncomplete() ) {
            throw exception::parsing_error( mWorkingAST.back()->GetSourceLocation(), "EndRepeat: Last node is not complete!" );
        }

        // TODO: relax this check??
        if( nullptr == dynamic_cast<ASTNode_Block *>(mWorkingAST.at( mWorkingAST.size() - 1 ).get()) ) {
            throw exception::parsing_error( mWorkingAST.at( mWorkingAST.size() - 1 )->GetSourceLocation(), 
                                            "EndRepeat: wrong repeat statement. Need 'repeat' following by one block." );
        }


        // === START COMMON CODE BLOCK ===

        auto loop = std::make_shared<ASTNode_Repeat>( mWorkingAST.at( mIndexStack.top() )->GetDetail(), std::move(start_loc) ); // NOT COMMON! (detail is the optional label string)
        // collect all childs which are after the dummy
        for( size_t i = mIndexStack.top() + 1; i != mWorkingAST.size(); ++i ) {
            loop->AddChildNode( mWorkingAST[i] );
        }
        // this repeat statement is complete now.
        loop->SetComplete();
        // erase all nodes from dummy to end
        mWorkingAST.erase( mWorkingAST.begin() + mIndexStack.top().to_ptrdiff(), mWorkingAST.end() );
        // remove saved index
        mIndexStack.pop();
        // finally add new repeat statement
        AddASTNode( std::move( loop ) );
    }


    void StartForall( std::string const &rLabel = "", SourceLocation loc = {} )
    {
        mIndexStack.push( IndexState{mWorkingAST.size(), IndexState::Forall} );
        // dummy node ensures that new nodes will be inserted after it - not inside it!
        mWorkingAST.emplace_back( std::make_shared<ASTNode_Dummy>( "forall", rLabel, std::move( loc ) ) );
    }

    void EndForall( SourceLocation loc = {} )
    {
        // check if we have a starting forall.
        if( mIndexStack.empty() || !mWorkingAST.at( mIndexStack.top() )->IsDummy() || mIndexStack.top().type != IndexState::Forall ) {
            throw exception::parsing_error( std::move( loc ), "EndForall: There is no forall!" );
        }

        // merge the source locations start forall / end forall
        SourceLocation  start_loc = mWorkingAST.at( mIndexStack.top() ).get()->GetSourceLocation();
        if( loc.IsSet() ) {
            start_loc.SetEnd( loc.GetStartLine(), loc.GetStartColumn() );
        }

        // first after dummy must be the identifier, second is either "in" or (later) "from".

        // first check if we have 4 + 1 (the dummy) nodes (TODO: 'from' needs more nodes!)
        if( mWorkingAST.size() < 5 || mIndexStack.top() != mWorkingAST.size() - 5 /*dummy + id + "in/from" + seq + block */ ) {
            throw exception::parsing_error( std::move( start_loc ), "EndForall: wrong forall statement. Need 'forall(' following by one id + \"in\" + statement + ')' + block." );
        }

        // safety!
        if( mWorkingAST.back()->IsIncomplete() ) {
            throw exception::parsing_error( mWorkingAST.back()->GetSourceLocation(), "EndForall: Last node is not complete!" );
        }

        // TODO: relax this check??
        if( nullptr == dynamic_cast<ASTNode_Block *>(mWorkingAST.at( mWorkingAST.size() - 1 ).get()) ) {
            throw exception::parsing_error( mWorkingAST.at( mWorkingAST.size() - 1 )->GetSourceLocation(),
                                            "EndForall: wrong forall statement. The block is missing!" );
        }

        // NOTE: Actually we are only supporting 'in' keyword for iterate over tuples.
        // check for 'in/from'
        if( mWorkingAST[mIndexStack.top() + 2]->GetDetail() != "in" ) {
            throw exception::parsing_error( std::move( start_loc ), "EndForall: wrong condition. 'in' keyword missing or at wrong position!" );
        }
        // remove the 'in'
        mWorkingAST.erase( mWorkingAST.begin() + mIndexStack.top().to_ptrdiff() + 2 );


        // === START COMMON CODE BLOCK ===

        auto loop = std::make_shared<ASTNode_Forall>( mWorkingAST.at( mIndexStack.top() )->GetDetail(), std::move( start_loc ) ); // NOT COMMON! (detail is the optional label string)
        // collect all childs which are after the dummy
        for( size_t i = mIndexStack.top() + 1; i != mWorkingAST.size(); ++i ) {
            loop->AddChildNode( mWorkingAST[i] );
        }
        // this forall statement is complete now.
        loop->SetComplete();
        // erase all nodes from dummy to end
        mWorkingAST.erase( mWorkingAST.begin() + mIndexStack.top().to_ptrdiff(), mWorkingAST.end() );
        // remove saved index
        mIndexStack.pop();
        // finally add new repeat statement
        AddASTNode( std::move( loop ) );

    }

    void StartForallCondition( SourceLocation loc = {} )
    {
        mIndexStack.push( IndexState{mWorkingAST.size(), IndexState::ForallCond} );
        // dummy node ensures that new nodes will be inserted after it - not inside it!
        mWorkingAST.emplace_back( std::make_shared<ASTNode_Dummy>( "forallcondition", std::move( loc ) ) );
    }

    void EndForallCondition( SourceLocation loc = {} )
    {
        // check if we have a starting forall condition.
        if( mIndexStack.empty() || !mWorkingAST.at( mIndexStack.top() )->IsDummy() || mIndexStack.top().type != IndexState::ForallCond ) {
            throw exception::parsing_error( std::move( loc ), "EndForallCondition: There is no forall condition!" );
        }

        // merge the source locations opening ( to closing )
        SourceLocation  start_loc = mWorkingAST.at( mIndexStack.top() ).get()->GetSourceLocation();
        if( loc.IsSet() ) {
            start_loc.SetEnd( loc.GetStartLine(), loc.GetStartColumn() );
        }

        // first after dummy must be the identifier, second is either "in" or (later) "from".
        
        // first check if we have 3 + 1 (the dummy) nodes (TODO: 'from' needs more nodes!)
        if( mWorkingAST.size() < 4 || mIndexStack.top() != mWorkingAST.size() - 4 /*dummy + id + "in/from" + seq */ ) {
            throw exception::parsing_error( std::move( start_loc ), "EndForallCondition: wrong condition. Need 'forall(' following by one id + \"in\" + one statement." );
        }

        if( mWorkingAST[mIndexStack.top() + 2]->GetDetail() != "in" ) {
            throw exception::parsing_error( std::move( start_loc ), "EndForallCondition: wrong condition. 'in' keyword missing or at wrong position!" );
        }

        // for the time being we do all the work in EndForall()
        // just remove the dummy here.
        mWorkingAST.erase( mWorkingAST.begin() + mIndexStack.top().to_ptrdiff() );
        // remove saved index
        mIndexStack.pop();
    }


    void StartIf( SourceLocation loc = {} )
    {
        mIndexStack.push( IndexState{mWorkingAST.size(), IndexState::If} );
        // dummy node ensures that new nodes will be inserted after it - not inside it!
        mWorkingAST.emplace_back( std::make_shared<ASTNode_Dummy>( "if", std::move(loc)));
    }

    void EndIf( SourceLocation loc = {} )
    {
        // check if we have a starting if.
        if( mIndexStack.empty() || !mWorkingAST.at( mIndexStack.top() )->IsDummy() || mIndexStack.top().type != IndexState::If ) {
            throw exception::parsing_error( std::move( loc ), "EndIf: There is no if statement!" );
        }

        // merge the source locations start if / end if
        SourceLocation  start_loc = mWorkingAST.at( mIndexStack.top() ).get()->GetSourceLocation();
        if( loc.IsSet() ) {
            start_loc.SetEnd( loc.GetStartLine(), loc.GetStartColumn() );
        }

        // if( con ) {}   -->   if expr block   -->   need 2 ASTNodes after(!) the dummy, one expr, one block

        if( mIndexStack.top() != mWorkingAST.size() - 3 /*dummy + expr + block*/ ) {
            throw exception::parsing_error( std::move(start_loc), "EndIf: wrong if statement. Need 'if' following by one expr + one block." );
        }
        // safety!
        if( mWorkingAST.back()->IsIncomplete() ) {
            throw exception::parsing_error( mWorkingAST.back()->GetSourceLocation(), "EndIf: Last node is not complete!" );
        }

        // TODO: relax this check??
        if( nullptr == dynamic_cast<ASTNode_Block *>(mWorkingAST.at( mWorkingAST.size() - 1 ).get()) ) {
            throw exception::parsing_error( mWorkingAST.at( mWorkingAST.size() - 1 )->GetSourceLocation(), 
                                            "EndIf: wrong if statement. Last ASTNode must be a block." );
        }
        if( nullptr == dynamic_cast<ASTNode_Expression *>(mWorkingAST.at( mWorkingAST.size() - 2 ).get()) ) {
            throw exception::parsing_error( mWorkingAST.at( mWorkingAST.size() - 2 )->GetSourceLocation(), 
                                            "EndIf: wrong if statement. Condition must be an expression." );
        }


        // === START COMMON CODE BLOCK ===

        auto if_statement = std::make_shared<ASTNode_If>(std::move(start_loc));
        // collect all childs which are after the dummy
        for( size_t i = mIndexStack.top() + 1; i != mWorkingAST.size(); ++i ) {
            if_statement->AddChildNode( mWorkingAST[i] );
        }
        // this if statement is complete now.
        if_statement->SetComplete();
        // erase all nodes from dummy to end
        mWorkingAST.erase( mWorkingAST.begin() + mIndexStack.top().to_ptrdiff(), mWorkingAST.end() );
        // remove saved index
        mIndexStack.pop();
        // finally add new if statement
        AddASTNode( if_statement ); // SPECIAL: DONT MOVE HERE, WE NEED TO SAVE IT!!!!

        // === END COMMON CODE BLOCK ===

        // NOTE: an if can end an else (else if...)!
        // check if we closed an else
        if( !mIndexStack.empty() ) {
            if( mIndexStack.top().type == IndexState::Else ) {
                EndElse(std::move(loc));
            }
        }

        // save the last added if for an eventually following else.
        // NOTE: Must after EndElse() call above, otherwise the else belongs to the prior if !!!
        mCache.push( CacheEntry{if_statement} );
    }

    void StartElse( SourceLocation loc = {} )
    {
        if( !mCache.empty() && !mCache.top().hit ) {
            mCache.top().hit = true;
        } else {
            throw exception::parsing_error( std::move(loc), "StartElse: No if found for the else." );
        }
        mIndexStack.push( IndexState{mWorkingAST.size(), IndexState::Else} );
        // dummy node ensures that new nodes will be inserted after it - not inside it!
        mWorkingAST.emplace_back( std::make_shared<ASTNode_Dummy>( "else", std::move(loc)));
    }

    void EndElse( SourceLocation loc = {} )
    {
        // check if we have a starting else.
        if( mIndexStack.empty() || !mWorkingAST.at( mIndexStack.top() )->IsDummy() || mIndexStack.top().type != IndexState::Else ) {
            throw exception::parsing_error( std::move( loc ), "EndElse: There is no else statement!" );
        }

        // merge the source locations start else / end else
        SourceLocation  start_loc = mWorkingAST.at( mIndexStack.top() ).get()->GetSourceLocation();
        if( loc.IsSet() ) {
            start_loc.SetEnd( loc.GetStartLine(), loc.GetStartColumn() );
        }

        if( mIndexStack.top() != mWorkingAST.size() - 2 /*dummy + block/statement*/ ) {
            throw exception::parsing_error( std::move(start_loc), "EndElse: wrong else statement. Need 'else' following by one block or if statement." );
        }
        // safety!
        if( mWorkingAST.back()->IsIncomplete() ) {
            throw exception::parsing_error( mWorkingAST.back()->GetSourceLocation(), "EndElse: Last node is not complete!" );
        }
    
        // === START COMMON CODE BLOCK ===

        auto else_statement = std::make_shared<ASTNode_Else>(std::move(start_loc));
        // collect all childs which are after the dummy
        for( size_t i = mIndexStack.top() + 1; i != mWorkingAST.size(); ++i ) {
            else_statement->AddChildNode( mWorkingAST[i] );
        }
        // this else statement is complete now.
        else_statement->SetComplete();
        // erase all nodes from dummy to end
        mWorkingAST.erase( mWorkingAST.begin() + mIndexStack.top().to_ptrdiff(), mWorkingAST.end() );
        // remove saved index
        mIndexStack.pop();
        // finally add new else statement
        AddASTNode( std::move( else_statement ) );

        // === END COMMON CODE BLOCK ===


        // Special final step: move the else into the if
        if( !mCache.empty() ) {
            mCache.top().node->AddChildNode( mWorkingAST.back() );
            mWorkingAST.pop_back();
            mCache.pop();
        } else {
            throw exception::runtime_error( mWorkingAST.back()->GetSourceLocation(), "EndElse: Internal error: No if statement in cache." );
        }
        
    }

    void StartBlock( SourceLocation loc = {} )
    {
        mIndexStack.push( IndexState{mWorkingAST.size(), IndexState::Block} );
        // dummy node ensures that new nodes will be inserted after it - not inside it!
        mWorkingAST.emplace_back( std::make_shared<ASTNode_Dummy>("block", std::move(loc)));
    }

    void EndBlock( SourceLocation loc = {} )
    {
        // check if we have a starting block.
        if( mIndexStack.empty() || !mWorkingAST.at( mIndexStack.top() )->IsDummy() || mIndexStack.top().type != IndexState::Block ) {
            throw exception::parsing_error( std::move( loc ), "EndBlock: There is no (start of a) block!" );
        }

        // no node after dummy? that is an empty block. for the meanwhile create a NoOp, but it is not really needed
        if( mIndexStack.top() == mWorkingAST.size() - 1 ) {
             mWorkingAST.emplace_back( std::make_shared<ASTNode_NoOp>() );
        } else {
            if( mWorkingAST.back()->IsIncomplete() ) {
                throw exception::parsing_error( mWorkingAST.back()->GetSourceLocation(), "EndBlock: Last node is not complete, probably a RHS is missing!" );
            }
        }

        // merge the source locations start block / end block
        SourceLocation  start_loc = mWorkingAST.at( mIndexStack.top() ).get()->GetSourceLocation();
        if( loc.IsSet() ) {
            start_loc.SetEnd( loc.GetStartLine(), loc.GetStartColumn() );
        }

        // === START COMMON CODE BLOCK ===

        auto block = std::make_shared<ASTNode_Block>( std::move(start_loc) );
        // collect all childs which are after the dummy
        for( size_t i = mIndexStack.top() + 1; i != mWorkingAST.size(); ++i ) {
            block->AddChildNode( mWorkingAST[i] );
        }
        // this block is complete now.
        block->SetComplete();
        // erase all nodes from dummy to end
        mWorkingAST.erase( mWorkingAST.begin() + mIndexStack.top().to_ptrdiff(), mWorkingAST.end() );
        // remove saved index
        mIndexStack.pop();
        // finally add new block
        AddASTNode( std::move( block ) );

        // === END COMMON CODE BLOCK ===

        // check if we closed an if/else or repeat or a func
        if( !mIndexStack.empty() ) {
            if( mIndexStack.top().type == IndexState::If ) {
                EndIf( std::move( loc ) );
            } else if( mIndexStack.top().type == IndexState::Else ) {
                EndElse( std::move( loc ) );
            } else if( mIndexStack.top().type == IndexState::Repeat ) {
                EndRepeat( std::move( loc ) );
            } else if( mIndexStack.top().type == IndexState::Forall ) {
                EndForall( std::move( loc ) );
            } else if( mIndexStack.top().type == IndexState::Func ) {
                EndFunc( std::move(loc) );
            }
        }
    }

    void StartParams( SourceLocation loc = {} )
    {
        mIndexStack.push( IndexState{mWorkingAST.size(), IndexState::Params} );
        
        // dummy node ensures that new nodes will be inserted after it - not inside it!
        mWorkingAST.emplace_back( std::make_shared<ASTNode_Dummy>( "parameter defintion", std::move(loc)));
    }

    void EndParams( SourceLocation loc = {} )
    {
        // check if we have a starting expression.
        if( mIndexStack.empty() || !mWorkingAST.at( mIndexStack.top() )->IsDummy() || mIndexStack.top().type != IndexState::Params ) {
            throw exception::parsing_error( std::move( loc ), "EndParams: There is no (start of a) function parameter specifictaion!" );
        }

        // parameter list can be empty.
        if( mIndexStack.top() == mWorkingAST.size() - 1 ) {
        } else {
            if( mWorkingAST.back()->IsIncomplete() ) {
                // special case: "func name( id @= )"  --> no default value/expr specified. Then a parameter is mandatory, add a dummy to signal it.
                if( nullptr != dynamic_cast<ASTNode_Assign *>(mWorkingAST.back().get()) && not mWorkingAST.back()->NeedLHS() ) {
                    mWorkingAST.back()->AddChildNode( std::make_shared<ASTNode_Dummy>() );
                } else {
                    throw exception::parsing_error( mWorkingAST.back()->GetSourceLocation(), "EndParams: Last node is not complete, probably a RHS is missing!" );
                }
            }
        }

        // merge the source locations start params / end params
        SourceLocation  start_loc = mWorkingAST.at( mIndexStack.top() ).get()->GetSourceLocation();
        if( loc.IsSet() ) {
            start_loc.SetEnd( loc.GetStartLine(), loc.GetStartColumn() );
        }

        // === START COMMON CODE BLOCK ===

        auto param_spec = std::make_shared<ASTNode_ParamSpec>( std::move(start_loc) );
        // collect all childs, which are after the dummy
        for( size_t i = mIndexStack.top() + 1; i != mWorkingAST.size(); ++i ) {
            // NOT COMMON!!!!
            // building assigment statements with FromParamList[_Or] as RHS, and Def Id as LHS
            auto cur = mWorkingAST[i];
            if( cur->GetName() == "Id" ) {
                // 1. case: we have a simple identifier.
                //          make it to a const (or def) assign: const ID := <from param list>
                auto const def_type = dialect.parameters_are_default_const ? ASTNode_Var_Def_Undef::eType::Const : ASTNode_Var_Def_Undef::eType::Def;
                auto def_node = std::make_shared<ASTNode_Var_Def_Undef>( def_type );
                def_node->AddChildNode( std::move( cur ) );
                auto assign_node = std::make_shared<ASTNode_Assign>( );
                assign_node->AddChildNode( std::move( def_node ) );
                assign_node->AddChildNode( std::make_shared<ASTNode_FromParamList>() );
                cur = assign_node;
            } else if( nullptr != dynamic_cast<ASTNode_Var_Def_Undef *>(cur.get()) ) {
                // 2. case: we have a def + identifier.
                //          make it to a def assign: DEF+ID := <from param list>
                auto assign_node = std::make_shared<ASTNode_Assign>();
                assign_node->AddChildNode( std::move( cur ) );
                assign_node->AddChildNode( std::make_shared<ASTNode_FromParamList>() );
                cur = assign_node;
            } else if( ASTNode_Assign *pAssign = dynamic_cast<ASTNode_Assign *>(cur.get()); nullptr != pAssign ) {
                // 3. case: we have a def assign already, either with or without default value/expr.
                //          make it to a def assign: DEF ASSIGN <from param list>, or DEF ASSIGN <from param list or>
                auto rhs = cur->PopChild();
                auto lhs = cur->PopChild(); 
                // if the assign node contains the def already, we can just use it. //FIXME: don't pop and add!
                if( pAssign->IsAssignWithDef() ) {
                    cur->AddChildNode( std::move( lhs ) );
                } else {
                    // default const only for not shared assign :=, shared assign @= is always mutable by default.
                    auto const def_type = dialect.parameters_are_default_const && not pAssign->IsSharedAssign() 
                                        ? ASTNode_Var_Def_Undef::eType::Const : ASTNode_Var_Def_Undef::eType::Def;
                    auto def_node = std::make_shared<ASTNode_Var_Def_Undef>( def_type );
                    def_node->AddChildNode( std::move( lhs ) );
                    cur->AddChildNode( std::move( def_node ) );
                }
                // handle possible deault value/expression
                if( rhs->IsDummy() ) { // ommit dummy, then we dont have a default expression.
                    auto params = std::make_shared<ASTNode_FromParamList>();
                    cur->AddChildNode( std::move( params ) );
                } else {
                    auto param_or = std::make_shared<ASTNode_FromParamList_Or>();
                    param_or->AddChildNode( std::move( rhs ) );
                    cur->AddChildNode( std::move( param_or ) );
                }
            }
            //--- NOT COMMON!!!!
            param_spec->AddChildNode( std::move(cur) );
        }
        // this param spec is complete now.
        param_spec->SetComplete();
        // erase all nodes from dummy to end
        mWorkingAST.erase( mWorkingAST.begin() + mIndexStack.top().to_ptrdiff(), mWorkingAST.end() );
        // remove saved index
        mIndexStack.pop();
        // finally add new param spec
        AddASTNode( std::move( param_spec ) );

        // === END COMMON CODE BLOCK ===
    }

    void StartCall( SourceLocation loc = {} )
    {
        mIndexStack.push( IndexState{mWorkingAST.size(), IndexState::Call} );

        // dummy node ensures that new nodes will be inserted after it - not inside it!
        mWorkingAST.emplace_back( std::make_shared<ASTNode_Dummy>( "function call", std::move(loc)));
    }

    void EndCall( SourceLocation loc = {} )
    {
        // check if we have a starting call.
        if( mIndexStack.empty() || !mWorkingAST.at( mIndexStack.top() )->IsDummy() || mIndexStack.top().type != IndexState::Call ) {
            throw exception::parsing_error( std::move( loc ), "EndCall: There is no (start of a) parameter list / function call!" );
        }

        // call can be empty.
        if( mIndexStack.top() == mWorkingAST.size() - 1 ) {
        } else {
            if( mWorkingAST.back()->IsIncomplete() ) {
                throw exception::parsing_error( mWorkingAST.back()->GetSourceLocation(), "EndCall: Last node is not complete, probably a RHS is missing!" );
            }
        }

        // merge the source locations start call / end call
        SourceLocation  start_loc = mWorkingAST.at( mIndexStack.top() ).get()->GetSourceLocation();
        if( loc.IsSet() ) {
            start_loc.SetEnd( loc.GetStartLine(), loc.GetStartColumn() );
        }

        // === START COMMON CODE BLOCK ===

        auto param_list = std::make_shared<ASTNode_ParamList>(std::move(start_loc));
        // collect all childs, which are after the dummy
        for( size_t i = mIndexStack.top() + 1; i != mWorkingAST.size(); ++i ) {
            param_list->AddChildNode( mWorkingAST[i] );
        }
        // this param list is complete now.
        param_list->SetComplete();
        // erase all nodes from dummy to end
        mWorkingAST.erase( mWorkingAST.begin() + mIndexStack.top().to_ptrdiff(), mWorkingAST.end() );
        // remove saved index
        mIndexStack.pop();
        // SPECIAL here... need the node for ASTNode_FuncCall
        //UNCOMMON!//AddASTNode( std::move( param_list ) );

        // === END COMMON CODE BLOCK ===

        // last node is now the id
        auto id = mWorkingAST.back();
        mWorkingAST.pop_back();
        auto call_func = std::make_shared<ASTNode_CallFunc>( param_list->GetSourceLocation() );
        call_func->AddChildNode( std::move( id ) );
        call_func->AddChildNode( std::move( param_list ) );
        call_func->SetComplete();

        // finally add new 
        AddASTNode( std::move( call_func ) );
    }


    void StartExpression( SourceLocation loc = {} )
    {
        if( IsInFunc() ) {
            StartParams( std::move(loc) );
            return;
        }

        if( IsInForall() ) {
            StartForallCondition( std::move( loc ) );
            return;
        }

        if( !mWorkingAST.empty() && open_statement && 
            //              call function by name  ||                   direct call a lambda  || direct call the return value of a called func!
            (mWorkingAST.back()->GetName() == "Id" || mWorkingAST.back()->GetName() == "Func" || mWorkingAST.back()->GetName() == "CallFunc"  
            //                                   call function after sequence of dot operators
            || (mWorkingAST.back()->GetName() == "BinOp" && mWorkingAST.back()->GetDetail() == "." && mWorkingAST.back()->IsComplete() ) 
            //                                   call function after subscript operator
            || (mWorkingAST.back()->GetName() == "Subscript" && mWorkingAST.back()->IsComplete() ) ) ) {
            StartCall( std::move(loc) );
            return;
        }
#if 1 //NOTE: Workaround needed to lift up (RHS) Operand for building the function call with it instead! Can be removed only with "Parse ahead" or other mechanics in StartCall/EndCall.
        // WORKAROUND for call a func in return or stop-with statement and also in unary/binary operators. TODO: Make clean!
        if( !mWorkingAST.empty() && open_statement && 
            (mWorkingAST.back()->GetName() == "Return" || mWorkingAST.back()->GetName() == "Stop"
              || mWorkingAST.back()->GetName() == "UnOp" || mWorkingAST.back()->GetName() == "BinOp") ) {
            if( mWorkingAST.back()->HasChildren() && mWorkingAST.back()->IsComplete() ) {
                auto ch = mWorkingAST.back()->PopChild();
#if TEASCRIPT_ENABLED_STARTCALL_DEEP_LIFTING
                // lift up all operators except (sequence of) dot operators.
                while( (ch->GetName() == "UnOp" || (ch->GetName() == "BinOp" && ch->GetDetail() != ".") )
                       && ch->HasChildren() /*safety!*/ ) {
                    auto inner = ch->PopChild();
                    // add it uncoditional as last item, ch is inomplete now!
                    mWorkingAST.push_back( std::move( ch ) );
                    ch = std::move( inner );
                }
#endif
                // start the call for the id or lambda or (sequence of) dot operators.
                if( ch->GetName() == "Id" || ch->GetName() == "Func" || (ch->GetName() == "BinOp" && ch->GetDetail() == ".") ) {
                    // add it uncoditional as last item and start Call-State
                    mWorkingAST.push_back( std::move( ch ) );
                    StartCall( std::move(loc) );
                    return;
                } else {
                    // bad luck, put it back...
#if TEASCRIPT_ENABLED_STARTCALL_DEEP_LIFTING
                    AddASTNode( std::move( ch ) ); // this will re-build a complete branch if we lifted it up above!
#else 
                    mWorkingAST.back()->AddChildNode( std::move( ch ) );
#endif
                }
            }
        }
        // END WORKAROUND
#endif

        mIndexStack.push( IndexState{mWorkingAST.size(), IndexState::Expr} );

        // dummy node ensures that new nodes will be inserted after it - not inside it!
        mWorkingAST.emplace_back( std::make_shared<ASTNode_Dummy>( "expression", std::move(loc)));
    }

    void EndExpression( SourceLocation loc = {} )
    {
        if( not mIndexStack.empty() && mIndexStack.top().type == IndexState::Params ) {
            EndParams( std::move(loc) );
            return;
        }

        if( not mIndexStack.empty() && mIndexStack.top().type == IndexState::Call ) {
            EndCall( std::move(loc) );
            return;
        }

        if( not mIndexStack.empty() && mIndexStack.top().type == IndexState::ForallCond ) {
            EndForallCondition( std::move( loc ) );
            return;
        }

        // check if we have a starting expression.
        if( mIndexStack.empty() || !mWorkingAST.at( mIndexStack.top() )->IsDummy() || mIndexStack.top().type != IndexState::Expr ) {
            throw exception::parsing_error( std::move(loc), "EndExpression: There is no (start of an) expression!" );
        }

        // no node after dummy? that is an empty expression. create a NoOp.
        if( mIndexStack.top() == mWorkingAST.size() - 1 ) {
            mWorkingAST.emplace_back( std::make_shared<ASTNode_NoOp>() );
        } else {
            if( mWorkingAST.back()->IsIncomplete() ) {
                throw exception::parsing_error( mWorkingAST.back()->GetSourceLocation(), "EndExpression: Last node is not complete, probably a RHS is missing!" );
            }
        }

        // merge the source locations start expr / end expr
        SourceLocation  start_loc = mWorkingAST.at( mIndexStack.top() ).get()->GetSourceLocation();
        if( loc.IsSet() ) {
            start_loc.SetEnd( loc.GetStartLine(), loc.GetStartColumn() );
        }

        // === START COMMON CODE BLOCK ===

        auto expr = std::make_shared<ASTNode_Expression>( std::move(start_loc) );
        // collect all childs (usually there should be only one) which are after the dummy
        for( size_t i = mIndexStack.top() + 1; i != mWorkingAST.size(); ++i ) {
            expr->AddChildNode( mWorkingAST[i] );
        }
        // this expression is complete now.
        expr->SetComplete();
        // erase all nodes from dummy to end
        mWorkingAST.erase( mWorkingAST.begin() + mIndexStack.top().to_ptrdiff(), mWorkingAST.end() );
        // remove saved index
        mIndexStack.pop();
        // finally add new expression
        AddASTNode( std::move(expr) );

        // === END COMMON CODE BLOCK ===
    }

    void StartSubscript( SourceLocation loc = {} )
    {

        // IMPORTANT: This workaround block differs a litte from the one in StartExpression. Check before merge blocks!!
#if 1 //NOTE: Workaround needed to lift up (RHS) Operand for building the subscript operator with it instead! Can be removed only with "Parse ahead".
        // WORKAROUND for use subscript operator in return or stop-with statement and also in unary/binary operators. TODO: Make clean!
        if( !mWorkingAST.empty() && open_statement &&
            (mWorkingAST.back()->GetName() == "Return" || mWorkingAST.back()->GetName() == "Stop"
              || mWorkingAST.back()->GetName() == "UnOp" || (mWorkingAST.back()->GetName() == "BinOp" && mWorkingAST.back()->GetDetail() != ".")) ) {
            if( mWorkingAST.back()->HasChildren() && mWorkingAST.back()->IsComplete() ) {
                auto ch = mWorkingAST.back()->PopChild();
#if TEASCRIPT_ENABLED_STARTCALL_DEEP_LIFTING
                // lift up all operators except (sequence of) dot operators.
                while( (ch->GetName() == "UnOp" || (ch->GetName() == "BinOp" && ch->GetDetail() != "."))
                       && ch->HasChildren() /*safety!*/ ) {
                    auto inner = ch->PopChild();
                    // add it uncoditional as last item, ch is inomplete now!
                    mWorkingAST.push_back( std::move( ch ) );
                    ch = std::move( inner );
                }
#endif
                // start the subscript operator for the id or called lambda/func or (sequence of) dot operators.
                if( ch->GetName() == "Id" || ch->GetName() == "CallFunc" || (ch->GetName() == "BinOp" && ch->GetDetail() == ".") ) {
                    // add it uncoditional as last item and start Call-State
                    mWorkingAST.push_back( std::move( ch ) );
                    //StartCall( std::move( loc ) );
                    //return;
                } else {
                    // bad luck, put it back...
#if TEASCRIPT_ENABLED_STARTCALL_DEEP_LIFTING
                    AddASTNode( std::move( ch ) ); // this will re-build a complete branch if we lifted it up above!
#else 
                    mWorkingAST.back()->AddChildNode( std::move( ch ) );
#endif
                }
            }
        }
        // END WORKAROUND
#endif

        mIndexStack.push( IndexState{mWorkingAST.size(), IndexState::Subscript} );

        // dummy node ensures that new nodes will be inserted after it - not inside it!
        mWorkingAST.emplace_back( std::make_shared<ASTNode_Dummy>( "subscript", std::move( loc ) ) );
    }

    void EndSubscript( SourceLocation loc = {} )
    {
        // check if we have a starting subscript operator.
        if( mIndexStack.empty() || !mWorkingAST.at(mIndexStack.top())->IsDummy() || mIndexStack.top().type != IndexState::Subscript ) {
            throw exception::parsing_error( std::move( loc ), "EndSubscript: There is no (start of a) subscript oprator!" );
        }

        // empty subscript operators not allowed
        if( mIndexStack.top() == mWorkingAST.size() - 1 ) {
            throw exception::parsing_error( mWorkingAST.back()->GetSourceLocation(), "EndSubscript: No index or key operand present!" );
        } else {
            if( mWorkingAST.back()->IsIncomplete() ) {
                throw exception::parsing_error( mWorkingAST.back()->GetSourceLocation(), "EndSubscript: Last node is not complete, probably a RHS is missing!" );
            }
        }

        // merge the source locations start call / end call
        SourceLocation  start_loc = mWorkingAST.at( mIndexStack.top() ).get()->GetSourceLocation();
        if( loc.IsSet() ) {
            start_loc.SetEnd( loc.GetStartLine(), loc.GetStartColumn() );
        }

        // === START COMMON CODE BLOCK ===

        auto param_list = std::make_shared<ASTNode_ParamList>( std::move( start_loc ) );
        // collect all childs, which are after the dummy
        for( size_t i = mIndexStack.top() + 1; i != mWorkingAST.size(); ++i ) {
            param_list->AddChildNode( mWorkingAST[i] );
        }
        // this param list is complete now.
        param_list->SetComplete();
        // erase all nodes from dummy to end
        mWorkingAST.erase( mWorkingAST.begin() + mIndexStack.top().to_ptrdiff(), mWorkingAST.end() );
        // remove saved index
        mIndexStack.pop();
        // SPECIAL here... need the node for ASTNode_SubscriptOperator
        //UNCOMMON!//AddASTNode( std::move( param_list ) );

        // === END COMMON CODE BLOCK ===

        // last node is now the id
        auto id = mWorkingAST.back();
        mWorkingAST.pop_back();
        auto sub = std::make_shared<ASTNode_Subscript_Operator>( param_list->GetSourceLocation() );
        sub->AddChildNode( std::move( id ) );
        sub->AddChildNode( std::move( param_list ) );
        sub->SetComplete();

        // finally add new 
        AddASTNode( std::move( sub ) );
    }

    // helper function. checks if a new node, which needs a LHS, can be added.
    // If this returns false, AddASTNode would throw if node->NeedLHS() returns true.
    bool CanAddNodeWhichNeedLHS() const noexcept
    {
        if( mWorkingAST.empty() || !open_statement ) {
            return false;
        } else if( mWorkingAST.back()->IsIncomplete() ) {
            return false;
        } else if( mWorkingAST.back()->IsDummy() ) {
            return false;
        }
        return true;
    }

    void AddASTNode( ASTNodePtr node )
    {
#if TEASCRIPT_DEBUG_ADDASTNODE
#define TRACE1( node1, text ) TEASCRIPT_PRINT( text, node1->GetName() )
#define TRACE2( node1, node2, text ) TEASCRIPT_PRINT( text, node1->GetName(), node2->GetName() ) 
#define TRACE3( node1, node2, node3, text ) TEASCRIPT_PRINT( text, node1->GetName(), node2->GetName(), node3->GetName() ) 
#else
#define TRACE1( node1, text ) (void)0
#define TRACE2( node1, node2, text ) (void)0
#define TRACE3( node1, node2, node3, text ) (void)0
#endif
        assert( node.get() != nullptr );
        if( node->NeedLHS() ) {   // usually binary operators need LHS
            if( mWorkingAST.empty() || !open_statement ) {
                throw exception::lhs_missing( node->GetSourceLocation(), "AddASTNode: LHS not present");
            } else if( mWorkingAST.back()->IsIncomplete() ) {
                throw exception::lhs_missing( mWorkingAST.back()->GetSourceLocation(), "AddASTNode: LHS needed but last ast node not complete yet." );
            } else if( mWorkingAST.back()->IsDummy() ) {
                throw exception::lhs_missing( mWorkingAST.back()->GetSourceLocation(), "AddASTNode: LHS needed but last ast node is dummy.");
            }

            // smaller precedences are the inner expressions of the bigger presedence operators,
            // e.g. true and true or false --> or (and true true) false
            if( node->Precedence() >= mWorkingAST.back()->Precedence() ) {
                // example: current: and true true, node or --> new: or (and true true), rhs missing
                TRACE2( node, mWorkingAST.back(), "{0} precedence >= {1}, adding {1} as child to {0}\n" );

                node->AddChildNode( mWorkingAST.back() );
                mWorkingAST.pop_back();
                mWorkingAST.emplace_back( std::move( node ) );

            } else {

                // example: current: or true true, node and --> new: or true, rhs missing, and true, rhs missing
                auto popped_node = mWorkingAST.back()->PopChild();
                TRACE3( node, mWorkingAST.back(), popped_node, "{0} precedence < {1}, popped {2} from {1}\n" );

                // lift the childs back to toplevel (and remove their RHS) until we find the correct precedence order.
                while( node->Precedence() < popped_node->Precedence() ) {
                    TRACE2( node, popped_node, "{0} precedence also < {1}, adding {1} as toplevel, popping next.\n" );
                    mWorkingAST.emplace_back( std::move( popped_node ) );
                    popped_node = mWorkingAST.back()->PopChild();
                }

                // mWorkingAST.back() is now incomplete! The 'go-backwards-and-check-incomplete' routine in the else branch below 
                // will make it complete again later once the nodes in the expression chain gets complete...
                // add the popped child as lhs of new node.
                TRACE2( node, popped_node, "finally: moving {1} to {0} and adding {0}\n" );
                node->AddChildNode( std::move(popped_node) );
                mWorkingAST.emplace_back( std::move( node ) );
            }

        } else if( !mWorkingAST.empty() && mWorkingAST.back()->IsIncomplete() ) {  // node is either RHS operand for operator, or unary operator (with missing RHS)

            // if the last node is not complete add the new one (probably as RHS) child.
            if( node->IsComplete() ) { // ... but only if it is complete.
                TRACE2( node, mWorkingAST.back(), "Adding {0} as child to {1}\n" );
                mWorkingAST.back()->AddChildNode( std::move( node ) );
                // if node is complete now, check if we need to make the previous one complete as well.
                if( mWorkingAST.back()->IsComplete() ) {
                    // go backwards and handle all continues incomplete nodes.
                    for( size_t i = mWorkingAST.size() - 1; i != 0; --i ) {
                        if( mWorkingAST[i - 1]->IsComplete() ) { // nothing (more) to be done...
                            break;
                        }
                        
                        TRACE2( mWorkingAST[i], mWorkingAST[i - 1], "Last node complete: Adding {0} as child to {1}\n" );
                        mWorkingAST[i - 1]->AddChildNode( mWorkingAST[i] ); // this may change IsComplete state!
                        mWorkingAST.pop_back();
                        
                        // still not complete? keep it as last working element!
                        if( !mWorkingAST[i - 1]->IsComplete() ) {
                            break;
                        } // else: complete now, check previous node if it can take the current
                    }
                }
            } else { // ... otherwise add the new node as last element so that we can make it complete first before add it to previous one.
                TRACE1( node, "{0} not complete, adding it to make it complete first.\n" );
                mWorkingAST.emplace_back( std::move( node ) );
            }

        } else { // usually first node of a statement... (unary op or constant/id, etc...)

            //NOTE: here we could optimize the last ?
            TRACE1( node, "adding node {0}.\n" );
            mWorkingAST.emplace_back( std::move( node ) );

            // clear cache when new statement was added.
            if( !mCache.empty() && !mCache.top().hit ) {
                mCache.pop();
            }

            open_statement = true;
        }
    }

    // experimental
    size_t GetCompleteStmCount() const noexcept
    {
        size_t count = 0;
        size_t idx   = ast_offset;
        for( ; idx < mWorkingAST.size(); ++idx ) {
            if( mWorkingAST[idx]->IsIncomplete() || mWorkingAST[idx]->IsDummy() ) {
                break;
            }
            ++count;
        }
        if( count > 0 ) {
            // check if last is an if and the node cache is filled for a possible else.
            if( mWorkingAST[idx - 1]->GetName() == "If" && not mCache.empty() ) {
                --count; // don't use this if yet!
            }
        }
        return count;
    }

    // internal
    ASTNodePtr GetFirstIncompleteASTNode() const
    {
        for( auto const &ast : mWorkingAST ) {
            if( ast->IsIncomplete() || ast->IsDummy() ) {
                return ast;
            }
        }
        return {};
    }

    // internal
    ASTNodePtr GetLastToplevelASTNode() const
    {
        return mWorkingAST.empty() ? ASTNodePtr() : mWorkingAST.back();
    }

    // experimental  (want == 0 means all possible available)
    ASTNodeCollection GetPartialASTNodes( size_t const want = 0 )
    {
        auto const available = GetCompleteStmCount();
        if( want > available ) {
            throw exception::out_of_range( "GetOutPartialASTNodes(): not sufficient amout of ASTNodes available!" );
        }
        if( not available ) {
            return {};
        }

        auto const count = want > 0 ? want : available;
        ASTNodeCollection  res( mWorkingAST.begin() + ast_offset, mWorkingAST.begin() + ast_offset + count );
        ast_offset += count;
        return res;
    }

    //TODO: revise?
    ASTNodeCollection MoveOutASTCollection()
    {
        ASTNodeCollection  res = std::move( mWorkingAST );
        if( ast_offset > 0 ) {
            res.erase( res.begin(), res.begin() + ast_offset );
        }
        mWorkingAST.clear(); // make re-usable
        mWorkingAST.reserve( 8 ); // reserve again here (for _EVAL_). note: probably not the best place.
        mCache = NodeCache();
        mIndexStack = std::stack<IndexState>();
        open_statement = false;
        ast_offset = 0;
        return res;
    }
};

} // namespace teascript

