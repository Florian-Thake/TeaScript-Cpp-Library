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
#include "StackVMProgram.hpp"
#include "ASTNode.hpp"
#include "ASTNodeTSVM.hpp"

#include <stack>
#include <list>


namespace teascript {

namespace StackVM {

/// The Compiler class compiles an AST into a Program for the TeaStackVM.
class Compiler
{
    struct LoopHeadState
    {
        std::string label;
        size_t      instr;
        size_t      scopes;
        size_t      pushes;
        // not sure why clang need this help for construct this simple struct...
#if defined( __clang__ )
        inline LoopHeadState( std::string const &l, size_t ins, size_t sco, size_t pus ) : label( l ), instr(ins), scopes(sco), pushes(pus) {}
#endif
    };
    // loop and function state (per function)
    struct LoopState
    {
        struct Request
        {
            std::string label;
            size_t      pos;
            size_t      nested_level;
            // not sure why clang need this help for construct this simple struct...
#if defined( __clang__ )
            inline Request( std::string const &l, size_t p, size_t nl ) : label(l), pos(p), nested_level(nl) {}
#endif
        };
        size_t                        current_scopes = 0;
        std::vector<LoopHeadState>    loop_head_stack;
        std::list<Request>            loop_looprequest_list;
        std::list<Request>            loop_stoprequest_list;
    };

    // helper struct for recursive ASTNode dispatching.
    struct BuildState
    {
        std::size_t          node_level = 0;
        std::stack<size_t>   stack_node_level;

        std::size_t          scope_level = 0;
        // loop state per (inlined) function call
        std::vector<LoopState>   mLoopState;
        size_t                   mLoopIndex = 0;
        std::stack<size_t>       mFuncStart;
        std::stack<size_t>       mParamOr;   // nested ParamOr's (possible if a labmda with default params is passed as default param!)
        std::stack<size_t>       mScopeStart;  // for optimization O2
    };

    
    // these members are all intermediate state during one build process and will be reset for each compile.
    
    BuildState      mState;

    eOptimize       mOptLevel = eOptimize::O0;
    
    InstrContainer  mInstructions;
    DebugInfo       mDebuginfo;

    inline
    void ResetState( eOptimize const optlevel )
    {
        mInstructions.clear();  // ensure defined state after a possible previous build
        mInstructions.reserve( 128 );
        mDebuginfo.clear();     // ensure defined state after a possible previous build

        mOptLevel = optlevel;
        mState.node_level = 0;
        mState.stack_node_level = std::stack<size_t>();

        mState.scope_level = 0;
        mState.mLoopState.clear();
        mState.mLoopState.emplace_back(); // always one state for the 'main' body
        mState.mLoopIndex = 0;
        mState.mFuncStart = std::stack<size_t>();
        mState.mParamOr   = std::stack<size_t>();
        mState.mScopeStart= std::stack<size_t>();
    }

public:
    Compiler() = default;

    /// Compiles the given ASTNode_FilePtr into a Program with given \param optlevel for optimization.
    /// \returns the compiled program as std::shared_ptr. The pointer is always valid.
    /// \throws exception::compile_error/eval_error/runtime_error.
    ProgramPtr Compile( ASTNode_FilePtr const &rAST_File, eOptimize const optlevel = eOptimize::O0 )
    {
        assert( rAST_File.get() != nullptr );

        ResetState( optlevel );

        RecursiveBuildTSVMCode( rAST_File );

        if( mOptLevel == eOptimize::Debug ) {
            mInstructions.emplace_back( eTSVM_Instr::ProgramEnd, ValueObject() );
        }

        return std::make_shared<Program>( rAST_File->GetDetail(), mOptLevel, version::combined_number(), std::move( mInstructions ), std::move(mDebuginfo) );
    }

    /// Compiles the given ASTNode_Ptr into a Program with given \param optlevel for optimization.
    /// The ASTNode_Ptr must point to a valid ASTNode_File.
    /// \returns the compiled program as std::shared_ptr. The pointer is always valid.
    /// \throws exception::compile_error/eval_error/runtime_error.
    ProgramPtr Compile( ASTNodePtr const &rAST, eOptimize const optlevel = eOptimize::O0 )
    {
        auto p_file = std::dynamic_pointer_cast<ASTNode_File>(rAST);
        if( not p_file ) {
            throw exception::runtime_error( "StackVM::Compiler::Compile(): ast must be ASTNode_File!" );
        }
        return Compile( p_file, optlevel );
    }

private:

    struct ScopedNodeLevel
    {
        BuildState &state;
        bool popped = false;
        ScopedNodeLevel( BuildState &rState ) : state( rState )
        {
            ++state.node_level;
            if( state.node_level == 1 && state.stack_node_level.empty() ) {
                Push();
            }
        }
        ~ScopedNodeLevel()
        {
            --state.node_level;
            if( not state.stack_node_level.empty() ) {
                if( state.stack_node_level.top() > state.node_level ) {
                    state.stack_node_level.pop();
                }
            }
        }

        inline
        void Push()
        {
            state.stack_node_level.push( state.node_level );
        }

        inline
        void Pop()
        {
            if( not state.stack_node_level.empty() && not popped ) {
                state.stack_node_level.pop();
                popped = true;
            }
        }
    };

    void RecursiveBuildTSVMCode( teascript::ASTNodePtr const &rNode )
    {
        rNode->Check(); // reject early on every unfinished / broken node!

        ScopedNodeLevel  scoped_node_level( mState );

        // ===
        // HEADER Section
        // ===

        if( rNode->GetName() == "TSVM" ) { // TSVM assembly, just extract the instruction
            auto const tsvm_node = std::static_pointer_cast<ASTNode_TSVM>(rNode);
            mInstructions.emplace_back( tsvm_node->GetInstruction() );
            return; // done!
        } else if( rNode->GetName() == "Constant" ) {
            // replace a prior Pop with Replace
            if( mOptLevel != eOptimize::Debug && not mInstructions.empty() && mInstructions.back().instr == eTSVM_Instr::Pop ) {
                mInstructions.pop_back();
#if 0           // DISABLED: this can affect jump addresses, e.g., Jump over a missing else!
                // remove all prior Replace (if any)
                while( not mInstructions.empty() && mInstructions.back().instr == eTSVM_Instr::Replace ) {
                    mInstructions.pop_back();
                }
#else 
                if( mOptLevel >= eOptimize::O1 ) {
                    // change all prior Replace with NoOp
                    for( auto it = mInstructions.rbegin(); it != mInstructions.rend(); ++it ) {
                        if( it->instr == eTSVM_Instr::Replace ) {
                            *it = Instruction( eTSVM_Instr::NoOp, teascript::ValueObject() );
                        } else {
                            break;
                        }
                    }
                }
#endif
                mInstructions.emplace_back( eTSVM_Instr::Replace, std::static_pointer_cast<teascript::ASTNode_Constant>(rNode)->GetValue() );
            } else {
                mInstructions.emplace_back( eTSVM_Instr::Push, std::static_pointer_cast<teascript::ASTNode_Constant>(rNode)->GetValue() );
            }
            return;
        } else if( rNode->GetName() == "Id" ) {
            mInstructions.emplace_back( eTSVM_Instr::Load, teascript::ValueObject( rNode->GetDetail() ) );
            if( mOptLevel == eOptimize::Debug || mOptLevel == eOptimize::O0 ) {
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }
            return;
        } else if( rNode->GetName() == "Block" ) {
            mInstructions.emplace_back( eTSVM_Instr::EnterScope, teascript::ValueObject() );
            if( mOptLevel >= eOptimize::O2 ) {
                mState.mScopeStart.push( mInstructions.size() - 1 ); // remember enter scope for possible optimization when block is closed.
            }
            if( mOptLevel == eOptimize::Debug ) {
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }
            scoped_node_level.Push();
            ++mState.scope_level;
        } else if( rNode->GetName() == "Expression" ) {
            auto const *const pExpr = static_cast<ASTNode_Expression *>(rNode.get());
            if( pExpr->GetMode() == ASTNode_Expression::eMode::Cond ) {
                scoped_node_level.Push();
            }

            if( mOptLevel == eOptimize::Debug ) {
                mInstructions.emplace_back( eTSVM_Instr::ExprStart, teascript::ValueObject() );
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }
        } else if( rNode->GetName() == "BinOp" && (rNode->GetDetail() == ":=" || rNode->GetDetail() == "@=") ) {

            auto const *const pAssign = static_cast<teascript::ASTNode_Assign *>(rNode.get());

            auto it = pAssign->begin();
            if( (*it)->GetName() == "Id" ) {
                // replace a prior Pop with Replace (note: in Debug this is unlikely to happen because of the debug instructions placed everywhere).
                if( not mInstructions.empty() && mInstructions.back().instr == eTSVM_Instr::Pop ) {
                    mInstructions.back() = Instruction( eTSVM_Instr::Replace, teascript::ValueObject( (*it)->GetDetail() ) );
                } else {
                    mInstructions.emplace_back( eTSVM_Instr::Push, teascript::ValueObject( (*it)->GetDetail() ) );
                }
                ++it; // advance to val!
                RecursiveBuildTSVMCode( *it );
                if( not pAssign->IsAssignWithDef() ) {
                    mInstructions.emplace_back( eTSVM_Instr::Stor, teascript::ValueObject( pAssign->IsSharedAssign() ) );
                } else if( pAssign->IsConstAssign() ) {
                    mInstructions.emplace_back( eTSVM_Instr::ConstVar, teascript::ValueObject( pAssign->IsSharedAssign() ) );
                } else if( pAssign->IsAutoAssign() ) {
                    mInstructions.emplace_back( eTSVM_Instr::AutoVar, teascript::ValueObject( pAssign->IsSharedAssign() ) );
                } else {
                    mInstructions.emplace_back( eTSVM_Instr::DefVar, teascript::ValueObject( pAssign->IsSharedAssign() ) );
                }
                if( mOptLevel == eOptimize::Debug || mOptLevel == eOptimize::O0 ) {
                    auto const &loc = rNode->GetSourceLocation();
                    if( loc.IsSet() ) {
                        mDebuginfo.emplace( mInstructions.size() - 1, loc );
                    }
                }
                return;
            } else if( (*it)->GetName() == "BinOp" && (*it)->GetDetail() == "." ) {

                auto c = (*it)->begin(); // lhs, the tuple (or the branch for it).

                RecursiveBuildTSVMCode( *c );

                ++c; // rhs, the element
                ValueObject val;
                if( (*c)->GetName() == "Constant" ) {
                    val = static_cast<ASTNode_Constant *>(c->get())->GetValue();
                } else { // Id
                    val = ValueObject( (*c)->GetDetail() );
                }
                mInstructions.emplace_back( eTSVM_Instr::Push, val );
                ++it; // advance to val!
                RecursiveBuildTSVMCode( *it );

                //auto const *const pAssign = static_cast<teascript::ASTNode_Assign *>(rNode.get());
                if( not pAssign->IsAssignWithDef() ) {
                    mInstructions.emplace_back( eTSVM_Instr::SetElement, teascript::ValueObject( pAssign->IsSharedAssign() ) );
                } else if( pAssign->IsConstAssign() ) {
                    mInstructions.emplace_back( eTSVM_Instr::ConstElement, teascript::ValueObject( pAssign->IsSharedAssign() ) );
                } else {
                    mInstructions.emplace_back( eTSVM_Instr::DefElement, teascript::ValueObject( pAssign->IsSharedAssign() ) );
                }
                if( mOptLevel == eOptimize::Debug || mOptLevel == eOptimize::O0 ) {
                    mDebuginfo.emplace( mInstructions.size() - 1, (*c)->GetSourceLocation() ); //FIXME: depending on the error c fits good (redefinition) or not (parent is const).
                }
                return;
            } else if( (*it)->GetName() == "Subscript" ) {
                // NOTE: This is handled here completely so that it is clear that only the SubscriptGet instruction must be handled in the footer section below
                auto c = (*it)->begin(); // the tuple/buffer (or the branch for it).
                RecursiveBuildTSVMCode( *c );
                ++c; // the index value(s) as ParamList branch.
                RecursiveBuildTSVMCode( *c );
                ++it; // advance to val!
                RecursiveBuildTSVMCode( *it );
                // set SubscriptSet
                mInstructions.emplace_back( eTSVM_Instr::SubscriptSet, teascript::ValueObject( pAssign->IsSharedAssign() ) );
                if( mOptLevel == eOptimize::Debug || mOptLevel == eOptimize::O0 ) {
                    mDebuginfo.emplace( mInstructions.size() - 1, (*it)->GetSourceLocation() );
                }
                // done!
                return;
            } else {
                mInstructions.emplace_back( eTSVM_Instr::NotImplemented, teascript::ValueObject( (*it)->GetName() ) );
                return;
            }

        } else if( rNode->GetName() == "BinOp" && (rNode->GetDetail() == "and" || rNode->GetDetail() == "or") ) {
            // SPECIAL HANDLING Logical Operators
            // must implement short circuits with jumps...

            //(TODO: insert a Mark NoOp in debug mode with "op" as payload?)

            // lhs is always evaluated
            auto it = rNode->begin();
            RecursiveBuildTSVMCode( *it );
            ++it; // advance to rhs!

            auto const pos = mInstructions.size(); // this will be the idx of our Jump below.
            if( rNode->GetDetail() == "or" ) {
                if( mInstructions.back().instr == eTSVM_Instr::Test ) {
                    mInstructions.emplace_back( eTSVM_Instr::JumpRel_If, teascript::ValueObject() ); // TODO: check if this is always posible or must use TestAnd...
                } else {
                    mInstructions.emplace_back( eTSVM_Instr::TestAndJumpRel_If, teascript::ValueObject() );
                }
            } else {
                if( mInstructions.back().instr == eTSVM_Instr::Test ) {
                    mInstructions.emplace_back( eTSVM_Instr::JumpRel_IfNot, teascript::ValueObject() ); // TODO: check if this is always posible or must use TestAnd...
                } else {
                    mInstructions.emplace_back( eTSVM_Instr::TestAndJumpRel_IfNot, teascript::ValueObject() );
                }
            }
            if( mOptLevel == eOptimize::Debug ) {
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }

            // for the case we did not jump remove the last value from stack
            mInstructions.emplace_back( eTSVM_Instr::Pop, teascript::ValueObject() );

            // now build the rhs part
            RecursiveBuildTSVMCode( *it );

            // for the case we did not jump convert the result of last instructions to Bool
            if( mInstructions.back().instr != eTSVM_Instr::Test ) { // avoid double tests for chained or/and combinations.
                mInstructions.emplace_back( eTSVM_Instr::Test, teascript::ValueObject() );
            }

            // calculate relative index for jump to in case lhs was enough to check.
            auto const diff = mInstructions.size() - pos;

            // set it in the jump
            mInstructions[pos].payload = teascript::ValueObject( static_cast<teascript::Integer>(diff) );

            // done!
            return;
        } else if( rNode->GetName() == "BinOp" && rNode->GetDetail() == "." ) {

            auto c = rNode->begin(); // lhs, the tuple (or the branch for it).

            RecursiveBuildTSVMCode( *c );

            ++c; // rhs, the element
            ValueObject val;
            if( (*c)->GetName() == "Constant" ) {
                val = static_cast<ASTNode_Constant *>(c->get())->GetValue();
            } else { // Id
                val = ValueObject( (*c)->GetDetail() );
            }
            mInstructions.emplace_back( eTSVM_Instr::Push, val );

            mInstructions.emplace_back( eTSVM_Instr::DotOp, teascript::ValueObject() );
            if( mOptLevel == eOptimize::Debug || mOptLevel == eOptimize::O0 ) {
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }
            // done
            return;
            
        } else if( rNode->GetName() == "If" ) {
            if( mOptLevel == eOptimize::Debug ) {
                mInstructions.emplace_back( eTSVM_Instr::If, teascript::ValueObject() );
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }

            //SPECIAL Handling If, must use jumps similar as for short circuit.

            // condition is always evaluated. conditions get a new scope for if( def a := true, a )
            mInstructions.emplace_back( eTSVM_Instr::EnterScope, teascript::ValueObject() );
            if( mOptLevel >= eOptimize::O2 ) {
                mState.mScopeStart.push( mInstructions.size() - 1 ); // remember enter scope for possible optimization when block is clased.
            }
            ++mState.scope_level;
            auto it = rNode->begin();
            RecursiveBuildTSVMCode( *it );
            ++it; // advance to if block!

            mInstructions.emplace_back( eTSVM_Instr::Test, teascript::ValueObject() );

            auto const pos = mInstructions.size(); // this will be the idx of our Jump below.
            mInstructions.emplace_back( eTSVM_Instr::JumpRel_IfNot, teascript::ValueObject() );

            // pop the condition
            mInstructions.emplace_back( eTSVM_Instr::Pop, teascript::ValueObject() );

            // now build the if block part
            RecursiveBuildTSVMCode( *it );
            ++it; // advance to (possible) else.

            // calculate relative index for jump to in case condition was not true.
            // + 1 because we will insert an additional jump before the else.
            auto const diff = mInstructions.size() - pos + 1;

            // set it in the jump
            mInstructions[pos].payload = teascript::ValueObject( static_cast<teascript::Integer>(diff) );

            // have an else?
            if( it != rNode->end() ) {
                // for the case the condition is true we must jump behind else!
                auto const pos_else = mInstructions.size(); // this will be the idx of our Jump below.
                mInstructions.emplace_back( eTSVM_Instr::JumpRel, teascript::ValueObject() );

                // pop the condition
                mInstructions.emplace_back( eTSVM_Instr::Pop, teascript::ValueObject() );

                // the else...
                RecursiveBuildTSVMCode( *it );

                 // calculate relative index for jump to in case condition was not true.
                auto const diff_else = mInstructions.size() - pos_else;

                // set it in the jump
                mInstructions[pos_else].payload = teascript::ValueObject( static_cast<teascript::Integer>(diff_else) );
            } else {
                // even if we don't have an else, it will produce a NaV!
                // for the case the condition is true we must jump behind else!
                mInstructions.emplace_back( eTSVM_Instr::JumpRel, teascript::ValueObject(2LL) );
                // replace the condition with NaV
                mInstructions.emplace_back( eTSVM_Instr::Replace, teascript::ValueObject() );
            }

            // remove the scope from the condition
            if( mOptLevel >= eOptimize::O2 ) {
                if( not OptimizeScope() ) {
                    mInstructions.emplace_back( eTSVM_Instr::ExitScope, teascript::ValueObject() );
                }
                mState.mScopeStart.pop();
            } else {
                mInstructions.emplace_back( eTSVM_Instr::ExitScope, teascript::ValueObject() );
            }
            --mState.scope_level;

            // done!
            return;
        } else if( rNode->GetName() == "Else" ) {
            if( mOptLevel == eOptimize::Debug ) {
                mInstructions.emplace_back( eTSVM_Instr::Else, teascript::ValueObject() );
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }
        } else if( rNode->GetName() == "ParamList" ) {
            if( mOptLevel == eOptimize::Debug ) {
                mInstructions.emplace_back( eTSVM_Instr::ParamList, teascript::ValueObject() );
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }
        } else if( rNode->GetName() == "UnOp" ) {
            auto const *const pDef = dynamic_cast<teascript::ASTNode_Var_Def_Undef *>(rNode.get());
            if( pDef != nullptr ) {
                if( pDef->GetType() == teascript::ASTNode_Var_Def_Undef::eType::Debug ) {
                    if( mOptLevel == eOptimize::Debug ) {
                        std::string name = (*(rNode->begin()))->GetDetail();
                        if( (*(rNode->begin()))->GetName() == "BinOp" && (*(rNode->begin()))->GetDetail() == "." ) {
                            name = std::static_pointer_cast<ASTNode_Dot_Operator>((*(rNode->begin())))->BuildBranchString();
                        }
                        mInstructions.emplace_back( eTSVM_Instr::Debug, ValueObject( name ) );
                        mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
                    }
                    // done
                    return;
                }
                if( mOptLevel == eOptimize::Debug || mOptLevel == eOptimize::O0 ) {
                    // all pathes add a instruction, at least a NotImplemented.
                    mDebuginfo.emplace( mInstructions.size(), rNode->GetSourceLocation() );
                }
                auto it = pDef->begin();
                if( (*it)->GetName() == "Id" ) {
                    if( pDef->GetType() == teascript::ASTNode_Var_Def_Undef::eType::Undef ) {
                        mInstructions.emplace_back( eTSVM_Instr::UndefVar, teascript::ValueObject( (*it)->GetDetail() ) );
                        return;
                    } else if( pDef->GetType() == teascript::ASTNode_Var_Def_Undef::eType::IsDef ) {
                        mInstructions.emplace_back( eTSVM_Instr::IsDef, teascript::ValueObject( (*it)->GetDetail() ) );
                        return;
                    } // else TODO: Def, Const, see ASTNode_Var_Def_Undef
                    mInstructions.emplace_back( eTSVM_Instr::NotImplemented, teascript::ValueObject( rNode->GetDetail() ) );
                    return;
                } else if( (*it)->GetName() == "BinOp" && (*it)->GetDetail() == "." ) {

                    auto c = (*it)->begin(); // lhs, the tuple (or the branch for it).

                    RecursiveBuildTSVMCode( *c );

                    ++c; // rhs, the element
                    teascript::ValueObject val;
                    if( (*c)->GetName() == "Constant" ) {
                        val = static_cast<ASTNode_Constant *>(c->get())->GetValue();
                    } else { // Id
                        val = ValueObject( (*c)->GetDetail() );
                    }
                    if( pDef->GetType() == teascript::ASTNode_Var_Def_Undef::eType::Undef ) {
                        mInstructions.emplace_back( eTSVM_Instr::UndefElement, val );
                        return;
                    } else if( pDef->GetType() == teascript::ASTNode_Var_Def_Undef::eType::IsDef ) {
                        mInstructions.emplace_back( eTSVM_Instr::IsDefElement, val );
                        return;
                    }
                    mInstructions.emplace_back( eTSVM_Instr::NotImplemented, teascript::ValueObject( rNode->GetDetail() ) );
                    return;
                }
                mInstructions.emplace_back( eTSVM_Instr::NotImplemented, teascript::ValueObject( "Var_Def_Undef with " + rNode->GetName() ) );
                return;
            }
        } else if( rNode->GetName() == "NoOp" ) {
            mInstructions.emplace_back( eTSVM_Instr::NoOp_NaV, teascript::ValueObject() );
        } else if( rNode->GetName() == "Repeat" ) {
            // add us to the stack                                                                 first instruction of the loop.            pushes to cleanup
            mState.mLoopState[mState.mLoopIndex].loop_head_stack.emplace_back( rNode->GetDetail(), mInstructions.size(), mState.scope_level, 0ULL );
            // TODO:  add debug info
            if( mOptLevel == eOptimize::Debug ) {
                mInstructions.emplace_back( eTSVM_Instr::RepeatStart, teascript::ValueObject( rNode->GetDetail() ) );
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }
        } else if( rNode->GetName() == "Loop" ) {
            bool found = false;
            size_t pushes = 0ULL;
            for( auto it = mState.mLoopState[mState.mLoopIndex].loop_head_stack.rbegin(); it != mState.mLoopState[mState.mLoopIndex].loop_head_stack.rend(); ++it ) {
                if( it->label == rNode->GetDetail() ) {
                    // scope cleanup
                    auto diff = mState.scope_level - it->scopes;
                    while( diff > 0 ) {
                        mInstructions.emplace_back( eTSVM_Instr::ExitScope, teascript::ValueObject() );
                        --diff;
                    }
                    while( pushes > 0 ) {
                        mInstructions.emplace_back( eTSVM_Instr::Pop, teascript::ValueObject() );
                        --pushes;
                    }
                    found = true;
                    break;
                } else {
                    pushes += it->pushes;
                }
            }
            if( not found ) {
                throw exception::compile_error( rNode->GetSourceLocation(), "No matching loop for loop statement found! Please, check the labels! label=\"" +rNode->GetDetail() + "\"" );
            }

            // a loop produces a 'result', needed for stack consistency.
            mInstructions.emplace_back( eTSVM_Instr::Push, teascript::ValueObject() );

            auto const pos = mInstructions.size(); // this will be the idx of our Jump below.
            // our Jump to the loop footer, filled later.
            mInstructions.emplace_back( eTSVM_Instr::JumpRel, teascript::ValueObject() );
            if( mOptLevel == eOptimize::Debug ) {
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }
            mState.mLoopState[mState.mLoopIndex].loop_looprequest_list.emplace_back( rNode->GetDetail(), pos, mState.mLoopState[mState.mLoopIndex].loop_head_stack.size() );
            // done
            return;
        } else if( rNode->GetName() == "Stop" ) {
            if( mState.mLoopState[mState.mLoopIndex].loop_head_stack.empty() ) {
                mInstructions.emplace_back( eTSVM_Instr::HALT, teascript::ValueObject() );
                if( mOptLevel == eOptimize::Debug ) {
                    mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
                }
                return;
            }
            size_t pushes = 0ULL;
            for( auto it = mState.mLoopState[mState.mLoopIndex].loop_head_stack.rbegin(); it != mState.mLoopState[mState.mLoopIndex].loop_head_stack.rend(); ++it ) {
                if( it->label == rNode->GetDetail() ) {
                    // Important: stop must cleanup also the last outer loop!
                    pushes += it->pushes;
                    while( pushes > 0 ) {
                        mInstructions.emplace_back( eTSVM_Instr::Pop, teascript::ValueObject() );
                        --pushes;
                    }
                    break;
                } else {
                    pushes += it->pushes;
                }
            }
            if( rNode->HasChildren() ) { //  optional 'with' statement
                RecursiveBuildTSVMCode( *(rNode->begin()) );
            } else { // no 'with' produces a NaV
                mInstructions.emplace_back( eTSVM_Instr::Push, teascript::ValueObject() );
            }
            // search matching loop for insert the scope cleanups.
            bool found = false;
            for( auto it = mState.mLoopState[mState.mLoopIndex].loop_head_stack.rbegin(); it != mState.mLoopState[mState.mLoopIndex].loop_head_stack.rend(); ++it ) {
                if( it->label == rNode->GetDetail() ) {
                    // scope cleanup
                    auto diff = mState.scope_level - it->scopes;
                    while( diff > 0 ) {
                        mInstructions.emplace_back( eTSVM_Instr::ExitScope, teascript::ValueObject() );
                        --diff;
                    }
                    found = true;
                    break;
                }
            }
            if( not found ) {
                throw exception::compile_error( rNode->GetSourceLocation(), "No matching loop for stop statement found! Please, check the labels! label=\"" + rNode->GetDetail() + "\"" );
            }
            auto const pos = mInstructions.size(); // this will be the idx of our Jump below.
            // our Jump to the loop end, filled later.
            mInstructions.emplace_back( eTSVM_Instr::JumpRel, teascript::ValueObject() );
            if( mOptLevel == eOptimize::Debug ) {
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }
            mState.mLoopState[mState.mLoopIndex].loop_stoprequest_list.emplace_back( rNode->GetDetail(), pos, mState.mLoopState[mState.mLoopIndex].loop_head_stack.size() );
            // done
            return;
        } else if( rNode->GetName() == "Forall" ) {

            auto node_it = rNode->begin();
            // first we need the identifier name, push it...
            mInstructions.emplace_back( eTSVM_Instr::Push, ValueObject( (*node_it)->GetDetail() ) );

            // then we need the Sequence (or the Tuple). Inject the instrcutions...
            ++node_it;
            RecursiveBuildTSVMCode( *node_it );

            // need a new scope for the id
            mInstructions.emplace_back( eTSVM_Instr::EnterScope, ValueObject() );
            ++mState.scope_level;

            // add the ForallHead instruction which will evaluate the results on the working stack.

            mInstructions.emplace_back( eTSVM_Instr::ForallHead, ValueObject( rNode->GetDetail() ) );
            if( mOptLevel == eOptimize::Debug ) {
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }

            // for the first iteration we must jump behind ForallNext.
            mInstructions.emplace_back( eTSVM_Instr::JumpRel, ValueObject(2LL) );

            // add us to the stack                                                                 first instruction (ForallNext).           pushes to cleanup
            mState.mLoopState[mState.mLoopIndex].loop_head_stack.emplace_back( rNode->GetDetail(), mInstructions.size(), mState.scope_level, 2ULL );

            // calculates next and jumps to end if finished.
            auto const nextpos = mInstructions.size();
            mInstructions.emplace_back( eTSVM_Instr::ForallNext, ValueObject( ) );
            if( mOptLevel == eOptimize::Debug ) {
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }

            // now comes the body.
            ++node_it;
            RecursiveBuildTSVMCode( *node_it );

            // the 'loop' statements jumps to here (loop back) //TODO: Optimize! For O1 or O2 we can remove Jumps to otherJumps by replace jump addres with final destination!
            // resolve all loop requests (if any)
            for( auto it = mState.mLoopState[mState.mLoopIndex].loop_looprequest_list.begin(); it != mState.mLoopState[mState.mLoopIndex].loop_looprequest_list.end(); ) {
                if( it->nested_level >= mState.mLoopState[mState.mLoopIndex].loop_head_stack.size() && it->label == rNode->GetDetail() ) { // found
                    mInstructions[it->pos].payload = ValueObject( static_cast<Integer>(mInstructions.size() - it->pos) ); // first instruction after the body.
                    it = mState.mLoopState[mState.mLoopIndex].loop_looprequest_list.erase( it );
                } else {
                    ++it;
                }
            }

            // loop to ForallNext
            auto diff = -static_cast<Integer>(mInstructions.size() - mState.mLoopState[mState.mLoopIndex].loop_head_stack.back().instr);
            mInstructions.emplace_back( eTSVM_Instr::JumpRel, ValueObject( diff ) );

            mInstructions[nextpos].payload = ValueObject( static_cast<Integer>(mInstructions.size() - nextpos) );

            // resolve all stop requests (if any)
            for( auto it = mState.mLoopState[mState.mLoopIndex].loop_stoprequest_list.begin(); it != mState.mLoopState[mState.mLoopIndex].loop_stoprequest_list.end(); ) {
                if( it->nested_level >= mState.mLoopState[mState.mLoopIndex].loop_head_stack.size() && it->label == rNode->GetDetail() ) { // found
                    mInstructions[it->pos].payload = ValueObject( static_cast<Integer>(mInstructions.size() - it->pos) ); // first instruction after the loop.
                    it = mState.mLoopState[mState.mLoopIndex].loop_stoprequest_list.erase( it );
                } else {
                    ++it;
                }
            }

            // cleanup (footer)
            mInstructions.emplace_back( eTSVM_Instr::ExitScope, ValueObject() );
            --mState.scope_level;

            // remove us from stack
            mState.mLoopState[mState.mLoopIndex].loop_head_stack.pop_back();

            // no loops left but stop/loop requests? (should not happen here, because caught at 'Stop'/'Loop' node handling above)
            if( mState.mLoopState[mState.mLoopIndex].loop_head_stack.empty() ) {
                if( not mState.mLoopState[mState.mLoopIndex].loop_stoprequest_list.empty() ) {
                    throw exception::compile_error( "Not all stop statements match a loop! Please, check the labels!" );
                }
                if( not mState.mLoopState[mState.mLoopIndex].loop_looprequest_list.empty() ) {
                    throw exception::compile_error( "Not all loop statements match a loop! Please, check the labels!" );
                }
            }
            
            // done
            return;
        } else if( rNode->GetName() == "Func" ) {

            auto it = rNode->begin();
            bool const lambda = (*it)->GetName() != "Id";

            mInstructions.emplace_back( eTSVM_Instr::FuncDef, ValueObject( lambda ? "<lambda>" : (*it)->GetDetail() ) );
            if( mOptLevel == eOptimize::Debug || mOptLevel == eOptimize::O0 ) {
                mDebuginfo.emplace( mInstructions.size() - 1, lambda ? rNode->GetSourceLocation() : (*it)->GetSourceLocation() );
            }
            mState.mFuncStart.push( mInstructions.size() ); // pos of the JumpRel below.
            mInstructions.emplace_back( eTSVM_Instr::JumpRel, ValueObject() );

            mState.mLoopState.emplace_back(); // new state for the function body.
            ++mState.mLoopIndex;
            // save the current scope level
            mState.mLoopState[mState.mLoopIndex].current_scopes = mState.scope_level;
        } else if( rNode->GetName() == "ParamSpec" ) {
            mInstructions.emplace_back( eTSVM_Instr::ParamSpec, teascript::ValueObject( static_cast<teascript::U64>(rNode->ChildCount()) ) );
            if( mOptLevel == eOptimize::Debug ) {
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }
            // Param Spec needs own scope
            mInstructions.emplace_back( eTSVM_Instr::EnterScope, teascript::ValueObject() );
            if( mOptLevel >= eOptimize::O2 ) {
                mState.mScopeStart.push( mInstructions.size() - 1 ); // remember enter scope for possible optimization when block is clased.
            }
            scoped_node_level.Push();
            ++mState.scope_level;
        } else if( rNode->GetName() == "FromParamList" ) {
            mInstructions.emplace_back( eTSVM_Instr::FromParam, teascript::ValueObject() );
            //NOTE: we need SourceLoc of the caller! But the caller is different for each call.
            /*if( mOptLevel == eOptimize::Debug ) {
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }*/
        } else if( rNode->GetName() == "FromParamList_Or" ) {
            mState.mParamOr.push( mInstructions.size() ); // pos of FromParam_Or below.
            mInstructions.emplace_back( eTSVM_Instr::FromParam_Or, teascript::ValueObject() );
        } else if( rNode->GetName() == "Suspend" ) {
            mInstructions.emplace_back( eTSVM_Instr::Suspend, teascript::ValueObject() );
            if( mOptLevel == eOptimize::Debug ) {
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }
            // every statement must produce a result actually (in most cases it will be optimized away later anyway)
            mInstructions.emplace_back( eTSVM_Instr::Push, teascript::ValueObject() );
            return; // done (has no childs)
        }


        // ===
        // BODY Section (generic recursive child handling)
        // ===

        if( rNode->HasChildren() ) {

            for( auto it = rNode->begin(); it != rNode->end(); ++it ) {
                RecursiveBuildTSVMCode( *it );
                // all but the last top level scope must pop their results for not make the stack dirty
                // but don't do this for TSVM assembly nodes. they must handle it manually!
                if( (*it)->GetName() == "TSVM" ) {
                    ; // nop
                } else if( mState.node_level == mState.stack_node_level.top() ) {
                    if( it != std::prev( rNode->end() ) ) {
                        // if last instr is a push we can replace or even remove it!
                        if( mInstructions.back().instr == eTSVM_Instr::Push ) {
                            if( mOptLevel >= eOptimize::O1 ) {
                                mInstructions.pop_back();
                            } else if( mOptLevel == eOptimize::O0 ) { // not for debug!
                                mInstructions.back() = Instruction( eTSVM_Instr::NoOp, teascript::ValueObject() );
                            } else {
                                mInstructions.emplace_back( eTSVM_Instr::Pop, teascript::ValueObject() );
                            }
                        } else if( mInstructions.back().instr == eTSVM_Instr::Replace ) {
                            if( mOptLevel >= eOptimize::O1 ) {
#if 0 // disabled, could affect jump address of an else
                                // replace the Replace with a Pop
                                mInstructions.back() = Instruction( eTSVM_Instr::Pop, teascript::ValueObject() );
#else
                                // make Replace to a NoOp and remove one (Pop)
                                mInstructions.back() = Instruction( eTSVM_Instr::NoOp, teascript::ValueObject() );
                                mInstructions.emplace_back( eTSVM_Instr::Pop, teascript::ValueObject() );
#endif
                            } else {
                                // add a Pop
                                mInstructions.emplace_back( eTSVM_Instr::Pop, teascript::ValueObject() );
                            }
                        } else {
                            mInstructions.emplace_back( eTSVM_Instr::Pop, teascript::ValueObject() );
                        }
                    }
                }
            }
        }


        // ===
        // FOOTER Section
        // ===

        if( rNode->GetName() == "Block" ) {
            if( mOptLevel >= eOptimize::O2 ) {
                if( not OptimizeScope() ) {
                    mInstructions.emplace_back( eTSVM_Instr::ExitScope, teascript::ValueObject() );
                }
                mState.mScopeStart.pop();
            } else {
                mInstructions.emplace_back( eTSVM_Instr::ExitScope, teascript::ValueObject() );
                if( mOptLevel == eOptimize::Debug ) {
                    mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
                }
            }
            scoped_node_level.Pop();
            --mState.scope_level;
        } else if( rNode->GetName() == "BinOp" ) {
            if( mOptLevel == eOptimize::Debug || mOptLevel == eOptimize::O0 ) {
                if( rNode->GetDetail() != "." ) {
                    mDebuginfo.emplace( mInstructions.size(), rNode->GetSourceLocation() );
                }
            }
            if( rNode->GetDetail() == "is" ) {
                mInstructions.emplace_back( eTSVM_Instr::IsType, teascript::ValueObject() );
            } else if( rNode->GetDetail() == "as" ) {
                mInstructions.emplace_back( eTSVM_Instr::AsType, teascript::ValueObject() );
            } else if( rNode->GetDetail() == "." ) {
                //NOT handled here! //mInstructions.emplace_back( eTSVM_Instr::DotOp, teascript::ValueObject() );
            } else if( rNode->GetDetail().starts_with( "bit_" ) ) {
                auto const  op = std::static_pointer_cast<teascript::ASTNode_Bit_Operator>(rNode)->GetBitOp();
                if( mOptLevel >= eOptimize::O1 ) {
                    if( not OptimizeBitOp( op, rNode->GetSourceLocation() ) ) {
                        mInstructions.emplace_back( eTSVM_Instr::BitOp, teascript::ValueObject( static_cast<teascript::U64>(op) ) );
                    }
                } else {
                    mInstructions.emplace_back( eTSVM_Instr::BitOp, teascript::ValueObject( static_cast<teascript::U64>(op) ) );
                }
            } else {
                auto const  op = std::static_pointer_cast<teascript::ASTNode_Binary_Operator>(rNode)->GetOperation();
                if( mOptLevel >= eOptimize::O1 ) {
                    if( not OptimizeBinaryOp( op, rNode->GetSourceLocation() ) ) {
                        mInstructions.emplace_back( eTSVM_Instr::BinaryOp, teascript::ValueObject( static_cast<teascript::U64>(op) ) );
                    }
                } else {
                    mInstructions.emplace_back( eTSVM_Instr::BinaryOp, teascript::ValueObject( static_cast<teascript::U64>(op) ) );
                }
            }
        } else if( rNode->GetName() == "UnOp" ) {
            auto const  op = std::static_pointer_cast<teascript::ASTNode_Unary_Operator>(rNode)->GetOperation();
            if( mOptLevel >= eOptimize::O1 ) {
                if( not OptimizeUnaryOp( op, rNode->GetSourceLocation() ) ) {
                    mInstructions.emplace_back( eTSVM_Instr::UnaryOp, teascript::ValueObject( static_cast<teascript::U64>(op) ) );
                }
            } else {
                mInstructions.emplace_back( eTSVM_Instr::UnaryOp, teascript::ValueObject( static_cast<teascript::U64>(op) ) );
                if( mOptLevel == eOptimize::Debug || mOptLevel == eOptimize::O0 ) {
                    mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
                }
            }
        } else if( rNode->GetName() == "Expression" ) {
            if( mOptLevel == eOptimize::Debug ) {
                mInstructions.emplace_back( eTSVM_Instr::ExprEnd, teascript::ValueObject() );
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }
            auto const *const pExpr = static_cast<ASTNode_Expression *>(rNode.get());
            if( pExpr->GetMode() == ASTNode_Expression::eMode::Cond ) {
                scoped_node_level.Pop();
            } else if( pExpr->ChildCount() > 1 ) { // Tuple
                mInstructions.emplace_back( eTSVM_Instr::MakeTuple, teascript::ValueObject(static_cast<U64>(pExpr->ChildCount()) ) );
            }
        } else if( rNode->GetName() == "ParamList" ) {
            mInstructions.emplace_back( eTSVM_Instr::Push, teascript::ValueObject( static_cast<teascript::U64>(rNode->ChildCount()) ) );
        } else if( rNode->GetName() == "CallFunc" ) {
            std::string name = (*(rNode->begin()))->GetDetail();
            if( (*(rNode->begin()))->GetName() == "BinOp" && (*(rNode->begin()))->GetDetail() == "." ) {
                name = std::static_pointer_cast<ASTNode_Dot_Operator>((*(rNode->begin())))->BuildBranchString();
            }
            mInstructions.emplace_back( eTSVM_Instr::CallFunc, teascript::ValueObject( name ) );
            if( mOptLevel == eOptimize::Debug || mOptLevel == eOptimize::O0 ) {
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }
        } else if( rNode->GetName() == "Repeat" ) {

            // the 'loop' statements jumps to here (cleanup, then loop back)
            // resolve all loop requests (if any)
            for( auto it = mState.mLoopState[mState.mLoopIndex].loop_looprequest_list.begin(); it != mState.mLoopState[mState.mLoopIndex].loop_looprequest_list.end(); ) {
                if( it->nested_level >= mState.mLoopState[mState.mLoopIndex].loop_head_stack.size() && it->label == rNode->GetDetail() ) { // found
                    mInstructions[it->pos].payload = ValueObject( static_cast<Integer>(mInstructions.size() - it->pos) ); // first instruction after the loop.
                    it = mState.mLoopState[mState.mLoopIndex].loop_looprequest_list.erase( it );
                } else {
                    ++it;
                }
            }

            // remove last result
            mInstructions.emplace_back( eTSVM_Instr::Pop, ValueObject() );

            // add the loop back to head
            mInstructions.emplace_back( eTSVM_Instr::JumpRel, ValueObject( -static_cast<Integer>(mInstructions.size() - mState.mLoopState[mState.mLoopIndex].loop_head_stack.back().instr ) ) );

            // resolve all stop requests (if any)
            for( auto it = mState.mLoopState[mState.mLoopIndex].loop_stoprequest_list.begin(); it != mState.mLoopState[mState.mLoopIndex].loop_stoprequest_list.end(); ) {
                if( it->nested_level >= mState.mLoopState[mState.mLoopIndex].loop_head_stack.size() && it->label == rNode->GetDetail() ) { // found
                    mInstructions[it->pos].payload = ValueObject( static_cast<Integer>( mInstructions.size() - it->pos ) ); // first instruction after the loop.
                    it = mState.mLoopState[mState.mLoopIndex].loop_stoprequest_list.erase( it );
                } else {
                    ++it;
                }
            }

            // remove us from stack
            mState.mLoopState[mState.mLoopIndex].loop_head_stack.pop_back();

            // no loops left but stop/loop requests? (should not happen here, because caught at 'Stop'/'Loop' node handling above)
            if( mState.mLoopState[mState.mLoopIndex].loop_head_stack.empty() ) {
                if( not mState.mLoopState[mState.mLoopIndex].loop_stoprequest_list.empty() ) {
                    throw exception::compile_error( "Not all stop statements match a loop! Please, check the labels!" );
                }
                if( not mState.mLoopState[mState.mLoopIndex].loop_looprequest_list.empty() ) {
                    throw exception::compile_error( "Not all loop statements match a loop! Please, check the labels!" );
                }
            }

            // TODO:  add debug info
            if( mOptLevel == eOptimize::Debug ) {
                mInstructions.emplace_back( eTSVM_Instr::RepeatEnd, teascript::ValueObject( rNode->GetDetail() ) );
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }
        } else if( rNode->GetName() == "Func" ) {

            // parameter spec scope cleanup.
            if( mOptLevel >= eOptimize::O2 ) {
                if( not OptimizeScope() ) {
                    mInstructions.emplace_back( eTSVM_Instr::ExitScope, teascript::ValueObject() );
                }
                mState.mScopeStart.pop();
            } else {
                mInstructions.emplace_back( eTSVM_Instr::ExitScope, teascript::ValueObject() );
            }
            --mState.scope_level;
            // return from function
            mInstructions.emplace_back( eTSVM_Instr::Ret, ValueObject() );

            auto it = rNode->begin();
            bool const lambda = (*it)->GetName() != "Id";
            // Id produces a Load (will never be reached). Replace it with NoOp.
            if( not lambda ) {
                mInstructions[mState.mFuncStart.top() + 1].instr = eTSVM_Instr::NoOp;
                mInstructions[mState.mFuncStart.top() + 1].payload = ValueObject();
            }

            // jump over the complete code of the func.
            mInstructions[mState.mFuncStart.top()].payload = ValueObject( static_cast<Integer>(mInstructions.size() - mState.mFuncStart.top()));

            mState.mFuncStart.pop();

            mState.mLoopState.pop_back();
            --mState.mLoopIndex;
        } else if( rNode->GetName() == "ParamSpec" ) {
            // remove last result from Param def, if any
            if( rNode->HasChildren() ) {
                mInstructions.emplace_back( eTSVM_Instr::Pop, teascript::ValueObject() );
            }
            // cleanup working stack.
            mInstructions.emplace_back( eTSVM_Instr::ParamSpecClean, teascript::ValueObject() );
        } else if( rNode->GetName() == "FromParamList_Or" ) {
            // jump over the complete code of the ParamOr if a parameter is given by caller.
            mInstructions[mState.mParamOr.top()].payload = ValueObject( static_cast<Integer>(mInstructions.size() - mState.mParamOr.top()) );
            mState.mParamOr.pop();
        } else if( rNode->GetName() == "Return" ) {
            // cleanup scopes
            auto diff = mState.scope_level - mState.mLoopState[mState.mLoopIndex].current_scopes;
            while( diff > 0 ) {
                mInstructions.emplace_back( eTSVM_Instr::ExitScope, teascript::ValueObject() );
                --diff;
            }
            // return from function
            mInstructions.emplace_back( eTSVM_Instr::Ret, ValueObject() );
        } else if( rNode->GetName() == "Exit" ) {
            mInstructions.emplace_back( eTSVM_Instr::ExitProgram, teascript::ValueObject() );
        } else if( rNode->GetName() == "Subscript" ) {
            // SubscriptSet is handled in the header section above, only SubscriptGet left here.
            mInstructions.emplace_back( eTSVM_Instr::SubscriptGet, teascript::ValueObject() );
            if( mOptLevel == eOptimize::Debug || mOptLevel == eOptimize::O0 ) {
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }
        } else if( rNode->GetName() == "Yield" ) {
            mInstructions.emplace_back( eTSVM_Instr::Yield, teascript::ValueObject() );
            if( mOptLevel == eOptimize::Debug ) {
                mDebuginfo.emplace( mInstructions.size() - 1, rNode->GetSourceLocation() );
            }
        }
    }

    bool OptimizeUnaryOp( ASTNode_Unary_Operator::eOperation const op, SourceLocation const &rLoc )
    {
        assert( not mInstructions.empty() );
        // check if we have a constant (Push or Replace)
        if( mInstructions.back().instr == eTSVM_Instr::Push || mInstructions.back().instr == eTSVM_Instr::Replace ) {
            // yes, calculate it now and save it.
            mInstructions.back().payload = ASTNode_Unary_Operator::StaticExec( op, mInstructions.back().payload, rLoc );
            return true;
        }
        return false;
    }

    bool OptimizeBinaryOp( ASTNode_Binary_Operator::eOperation const op, SourceLocation const &rLoc )
    {
        assert( mInstructions.size() >= 2 );
        if( (mInstructions.back().instr == eTSVM_Instr::Push || mInstructions.back().instr == eTSVM_Instr::Replace)
            && (mInstructions[mInstructions.size() - 2].instr == eTSVM_Instr::Push || mInstructions[mInstructions.size() - 2].instr == eTSVM_Instr::Replace) ) {

            mInstructions[mInstructions.size() - 2].payload = ASTNode_Binary_Operator::StaticExec( 
                                                                op, mInstructions[mInstructions.size() - 2].payload, 
                                                                mInstructions.back().payload, rLoc );
            mInstructions.pop_back();
            return true;
        }
        return false;
    }

    bool OptimizeBitOp( ASTNode_Bit_Operator::eBitOp const op, SourceLocation const &rLoc )
    {
        assert( mInstructions.size() >= 2 );
        if( (mInstructions.back().instr == eTSVM_Instr::Push || mInstructions.back().instr == eTSVM_Instr::Replace)
            && (mInstructions[mInstructions.size() - 2].instr == eTSVM_Instr::Push || mInstructions[mInstructions.size() - 2].instr == eTSVM_Instr::Replace) ) {

            mInstructions[mInstructions.size() - 2].payload = ASTNode_Bit_Operator::StaticExec(
                                                                op, mInstructions[mInstructions.size() - 2].payload,
                                                                mInstructions.back().payload, rLoc );
            mInstructions.pop_back();
            return true;
        }
        return false;
    }

    bool OptimizeScope()
    {
        assert( not mState.mScopeStart.empty() );

        size_t const start = mState.mScopeStart.top();
        size_t nested = 0;
        for( auto idx = start + 1; idx < mInstructions.size(); ++idx ) {
            if( nested == 0 ) { // only if in our scope
                if( mInstructions[idx].instr == eTSVM_Instr::DefVar || mInstructions[idx].instr == eTSVM_Instr::ConstVar || mInstructions[idx].instr == eTSVM_Instr::FuncDef ) {
                    return false;
                }
            }

            if( mInstructions[idx].instr == eTSVM_Instr::EnterScope ) {
                ++nested;
            } else if( mInstructions[idx].instr == eTSVM_Instr::ExitScope ) {
                if( nested != 0 ) {
                    --nested;
                } else {
                    return false; // this is cleanup code for Stop/Loop or Ret 
                    // TODO: handle as well! 
                    // This is difficult because we cannot just remember this pos and replace it with NoOp if the loop finished without early return.
                    // After the Ret or Jump we must restore the old nested value so that this loop algorithm stays functional.
                }
            }
        }

        // reaching here we can optimize and remove the scope by replacing it with a NoOp (safety for now, not break jump addresses!)
        mInstructions[start].instr = eTSVM_Instr::NoOp;

        return true;
    }
};

} // nameapace StackVM

} // namesapce teascript
