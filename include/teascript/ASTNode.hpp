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

#include <cassert>
#include <iostream> // std::cout for debug operator

#include "ValueObject.hpp"
#include "Number.hpp"
#include "Context.hpp"
#include "Exception.hpp"
#include "ControlFlow.hpp"
#include "TupleUtil.hpp"
#include "Print.hpp"
#include "ASTNodeBase.hpp"


namespace teascript {


/// placeholder dummy ast node.
class ASTNode_Dummy : public ASTNode
{
    std::string const mPlaceholderFor;
public:
    explicit ASTNode_Dummy( SourceLocation loc = {} )
        : ASTNode( "Dummy", std::move( loc ) )
    {
    }

    explicit ASTNode_Dummy( std::string  placeholderfor, SourceLocation loc = {} )
        : ASTNode( "Dummy", std::move( loc ) )
        , mPlaceholderFor( std::move( placeholderfor ) )
    {
    }

    explicit ASTNode_Dummy( std::string  placeholderfor, std::string const &rDetail, SourceLocation loc = {} )
        : ASTNode( "Dummy", rDetail, std::move( loc ) )
        , mPlaceholderFor( std::move(placeholderfor) )
    {
    }

    virtual ~ASTNode_Dummy() {}

    bool IsDummy() const noexcept final
    {
        return true;
    }

    std::string GetInfoStr() const override
    {
        if( mPlaceholderFor.empty() ) {
            return ASTNode::GetInfoStr();
        } else {
            return "Dummy for " + mPlaceholderFor;
        }
    }

    ValueObject Eval( Context & ) override
    {
        if( mPlaceholderFor.empty() ) {
            throw exception::eval_error( GetSourceLocation(), "Internal Error! Dummy AST Node was not replaced!" );
        } else {
            throw exception::eval_error( GetSourceLocation(), std::string("Node for '") + mPlaceholderFor + "' is not complete or consists of wrong child nodes!" );
        }
    }

#if 0 // disabled so far, we make a copy (don't optimize for 'debug').
    SourceLocation &&MoveOutSrcLoc()
    {
        return std::move( mLocation );
    }
#endif
};


/// NoOp ASTNode - does nothing and returns always NaV.
class ASTNode_NoOp : public ASTNode
{
public:
    ASTNode_NoOp( SourceLocation loc = {} )
        : ASTNode( "NoOp", std::move( loc ) )
    {

    }

    virtual ~ASTNode_NoOp()
    {
    }


    ValueObject Eval( Context & ) override
    {
        return {NaV};
    }

};


/// ASTNode for constant values like 1, 2, 3, true or "Hello".
class ASTNode_Constant : public ASTNode
{
    ValueObject  mConstantValue;

public:
    ASTNode_Constant( ValueObject &&value, SourceLocation loc = {} )
        : ASTNode( "Constant", value.GetTypeInfo()->GetName(), std::move( loc ) )
        , mConstantValue( std::move( value ) )
    {

    }

    explicit ASTNode_Constant( bool const b, SourceLocation loc = {} ) // convenience
        : ASTNode_Constant( ValueObject(b), std::move(loc) )
    {

    }

    explicit ASTNode_Constant( long long const i, SourceLocation loc = {} ) // convenience
        : ASTNode_Constant( ValueObject(i), std::move(loc) )
    {

    }

    explicit ASTNode_Constant( int const i, SourceLocation loc = {} ) // more convenience
        : ASTNode_Constant( ValueObject( static_cast<long long>(i) ), std::move(loc) )
    {

    }

    explicit ASTNode_Constant( double const d, SourceLocation loc = {} ) // convenience
        : ASTNode_Constant( ValueObject( d ), std::move(loc) )
    {

    }

    explicit ASTNode_Constant( std::string const &rStr, SourceLocation loc = {} ) // convenience
        : ASTNode_Constant( ValueObject( rStr ), std::move(loc) )
    {

    }

    explicit ASTNode_Constant( std::string &&rStr, SourceLocation loc = {} ) // convenience
        : ASTNode_Constant( ValueObject( rStr ), std::move(loc) )
    {

    }

    virtual ~ASTNode_Constant()
    {
    }

    ValueObject Eval( Context & ) override
    {
        return mConstantValue;
    }

    // INTERNAL: returns a copy of the constant value (for debug information)
    ValueObject GetValue() const noexcept
    {
        return mConstantValue;
    }

    std::string GetInfoStr() const override
    {
        std::string name = GetName();
        if( !mConstantValue.HasValue() ) {
            name += ": NaV";
        } else if( mConstantValue.HasPrintableValue() ) {
            std::string const v = mConstantValue.PrintValue();
            name += " (" + GetDetail() + "): " + v;
        }
        return name;
    }
};

/// ASTNode representing a named identifier, returning the value of the identifier if found.
class ASTNode_Identifier : public ASTNode
{
public:
    ASTNode_Identifier( std::string_view const id, SourceLocation loc = {} )
        : ASTNode( "Id", std::string( id ), std::move(loc) )
    {
    }

    virtual ~ASTNode_Identifier()
    {
    }

    ValueObject Eval( Context & rContext ) override
    {
        return rContext.FindValueObject( GetDetail(), GetSourceLocation() );
    }
};

/// Common base class for all ASTNodes which must or can have children and don't need to specialize differently.
class ASTNode_Child_Capable : public ASTNode
{
protected:
    std::vector<ASTNodePtr> mChildren;

    void ApplyChildren( std::function<bool( ASTNode const *, int )> const &callback, int depth ) const
    {
        for( auto p : mChildren ) {
            p->Apply( callback, depth + 1 );
        }
    }

public:
    explicit ASTNode_Child_Capable( std::string name, SourceLocation loc = {} )
        : ASTNode( std::move( name ), std::string(), std::move(loc) )
    { }

    ASTNode_Child_Capable( std::string name, std::string const &rDetail, SourceLocation loc = {} )
        : ASTNode( std::move(name), rDetail, std::move(loc) )
    { }

    virtual ~ASTNode_Child_Capable()
    {
    }

    bool HasChildren() const noexcept override
    {
        return not mChildren.empty();
    }

    size_t ChildCount() const noexcept override
    {
        return mChildren.size();
    }

    ASTNodePtr PopChild() override
    {
        if( mChildren.empty() ) {
            throw exception::runtime_error( GetSourceLocation(), "ASTNode_Child_Capable::PopChild(): No childrens available!" );
        }

        auto const child = mChildren.back();
        mChildren.pop_back();
        return child;
    }

    void Apply( std::function<bool( ASTNode const *, int )> const &callback, int depth = 1 ) const override
    {
        if( callback( this, depth ) ) {
            ApplyChildren( callback, depth );
        }
    }
};

/// ASTNode representing an expression inside round brackets.
class ASTNode_Expression : public ASTNode_Child_Capable
{
public:
    enum class Mode
    {
        ExprOrTuple, // evals last node or build a tuple
        Cond, // evals all nodes
    };

private:
    bool mIsComplete = false;

protected:
     Mode mMode = {Mode::ExprOrTuple};

    explicit ASTNode_Expression( std::string name, SourceLocation loc = {} )
        : ASTNode_Child_Capable( std::move(name), std::move(loc) )
    {
    }

public:
    explicit ASTNode_Expression( SourceLocation loc = {} )
        : ASTNode_Child_Capable( "Expression", std::move(loc) )
    {
    }

    virtual ~ASTNode_Expression()
    {
    }

    bool IsComplete() const noexcept override
    {
        return mIsComplete;
    }

    void SetComplete() noexcept
    {
        mIsComplete = true;
    }

    void SetMode( Mode const mode ) noexcept
    {
        mMode = mode;
    }

    void AddChildNode( ASTNodePtr node ) override
    {
        assert( node.get() != nullptr );
        if( IsComplete() ) {
            throw exception::runtime_error( GetSourceLocation(), "Expression ASTNode complete! Cannot add additional child!" );
        }
        mChildren.emplace_back( std::move( node ) );
    }

    ValueObject Eval( Context & rContext ) override
    {
        if( !IsComplete() ) {
            throw exception::eval_error( GetSourceLocation(), "Expression ASTNode incomplete! Closing ')' were not parsed!" );
        }
        // empty expression shall have NoOp
        if( mChildren.empty() ) {
            throw exception::eval_error( GetSourceLocation(), "Internal error! No inner expression node for eval!" );
        }
        // actually expr. with more than one node are reserved for build tuples and thus only the last node get executed (like it contains only one node)
        if( mMode == Mode::ExprOrTuple ) {
            if( mChildren.size() == 1 ) { // 1 child is one value.
                return mChildren.back()->Eval( rContext );
            } else {
                // more than 1 child will produce a tuple.
                Collection<ValueObject>  tuple;
                if( mChildren.size() > 1 ) {
                    tuple.Reserve( mChildren.size() );
                }
                for( ASTNodePtr &node : mChildren ) {
                    tuple.AppendValue( node->Eval(rContext).MakeShared() );
                }
               
                return ValueObject( std::move( tuple ), ValueConfig{ValueShared,ValueMutable,rContext.GetTypeSystem()} );
            }
        } else { /* Cond: Expr. used in Conditions evals all nodes, e.g. for if( def z:= fun(), z ) {} */
            ValueObject res;
            for( ASTNodePtr &node : mChildren ) {
                res = node->Eval( rContext );
            }
            return res;
        }
    }
};

/// Unary Operators which have exactly one operand (at right hand site)
class ASTNode_Unary_Operator : public ASTNode_Child_Capable
{
    std::string const mOperator; //TODO: Use enum!

public:
    ASTNode_Unary_Operator( std::string const &rOperator, SourceLocation loc = {} )
        : ASTNode_Child_Capable( "UnOp", rOperator, std::move(loc) )
        , mOperator( rOperator )
    { }

    bool IsComplete() const noexcept final
    {
        return !mChildren.empty();
    }

    void AddChildNode( ASTNodePtr node ) override
    {
        assert( node.get() != nullptr );
        if( IsComplete() ) {
            throw exception::runtime_error( GetSourceLocation(), "Unary Operator ASTNode complete! Cannot add additional child!" );
        }
        mChildren.emplace_back( std::move( node ) );
    }

    int Precedence() const noexcept override
    {
        // NOTE: Until we need a different solution we simply use the presedence values of C++ as a starting point....
        //TODO: check for @?. check for typename, typeof
        return 3; // not needed so far, but just in case ... (+, -, ! are all 3)
    }

    ValueObject Eval( Context & rContext ) override
    {
        if( IsIncomplete() ) {
            throw exception::eval_error( GetSourceLocation(), "Unary Operator ASTNode incomplete! Operand missing!" );
        }

        ValueObject const  operand = mChildren[0]->Eval( rContext );

        // logical
        if( mOperator == "not" ) {
            return ValueObject{!operand.GetAsBool()};
        }

        // FIXME: use Operator categories! / enum ranges (>= BinOpPlus && <= BinOpMod)
        // arithmetic
        if( mOperator == "-" || mOperator == "+" ) {
            return util::ArtithmeticFactory::ApplyUnOp( operand, mOperator );
        }

        // share_count
        if( mOperator == "@?" ) {
            return ValueObject{operand.ShareCount()};
        }

        throw exception::eval_error( GetSourceLocation(), "Internal Error! Unknown Unary Operator!!" );
        //return {NaV};
    }
};


/// Binary Operators which have a LHS and RHS.
class ASTNode_Binary_Operator : public ASTNode_Child_Capable
{
    std::string const mOperator; //TODO: Use enum!
public:
    explicit ASTNode_Binary_Operator( std::string const &rOperator, SourceLocation loc = {} )
        : ASTNode_Child_Capable( "BinOp", rOperator, std::move(loc) )
        , mOperator( rOperator )
    { }

    bool IsComplete() const noexcept override
    {
        return mChildren.size() > 1;
    }

    bool NeedLHS() const noexcept override
    {
        return mChildren.empty();
    }

    void AddChildNode( ASTNodePtr node ) override
    {
        assert( node.get() != nullptr );
        if( IsComplete() ) {
            throw exception::runtime_error( GetSourceLocation(), "Binary Operator ASTNode complete! Cannot add additional child!" );
        }
        mChildren.emplace_back( std::move( node ) );
    }

    int Precedence() const noexcept override
    {
        // NOTE: Until we need a different solution we simply use the presedence values of C++ as a starting point....
        //TODO: Use lookup table!
        if( mOperator == "and" ) {
            return 14;
        } else if( mOperator == "or" ) {
            return 15;
        } else if( mOperator == "*" || mOperator == "/" || mOperator == "mod" ) {
            return 5;
        } else if( mOperator == "+" || mOperator == "-" ) {
            return 6;
        } else if( mOperator == "<" || mOperator == "<=" || mOperator == ">" || mOperator == ">=" ) {
            return 9;
        } else if( mOperator == "lt" || mOperator == "le" || mOperator == "gt" || mOperator == "ge" ) {
            return 9;
        } else if( mOperator == "!=" || mOperator == "==" ) {
            return 10;
        } else if( mOperator == "ne" || mOperator == "eq" ) {
            return 10;
        } else if( mOperator == "%" ) {
            return 7; //TODO: check and adjust if needed!
        } else if( mOperator == ":=" || mOperator == "@=" ) {
            return 16;
        } else if( mOperator == "@@" ) {
            return 17; //TODO: check and adjust if needed!
        } else if( mOperator == "is" ) {
            return 2; //very small to bind with closest neighbour , e.g. 3 + a is Number => 3 + (a is Number)  //TODO: check and adjust if needed!
        }

        return INT_MAX;
    }

    ValueObject Eval( Context & rContext ) override
    {
        if( !IsComplete() ) {
            if( NeedLHS() ) {
                throw exception::eval_error( GetSourceLocation(), "Binary Operator ASTNode incomplete! LHS and RHS missing!" );
            } else { // mChildren.size() < 2
                throw exception::eval_error( GetSourceLocation(), "Binary Operator ASTNode incomplete! RHS missing!" );
            }
        }

        ValueObject const  lhs = mChildren[0]->Eval( rContext );
        // don't pre-compute rhs for logical operators!

        // logical
        if( mOperator == "and" ) {
            return ValueObject{lhs.GetAsBool() && mChildren[1]->Eval( rContext ).GetAsBool()};
        } else if( mOperator == "or" ) {
            return ValueObject{lhs.GetAsBool() || mChildren[1]->Eval( rContext ).GetAsBool()};
        }

        // after logical we can compute rhs always...
        ValueObject const  rhs = mChildren[1]->Eval( rContext );

        // arithmentic
        //WORKAROUND: BUG - Add SoruceLocation to division by zero / floating point modulo exceptions
        bool const arithemetic_may_throw = mOperator == "/" || mOperator == "mod";
        if( arithemetic_may_throw ) {
            try {
                return util::ArtithmeticFactory::ApplyBinOp( lhs, rhs, mOperator );
            } catch( exception::division_by_zero const & ) {
                throw exception::division_by_zero( GetSourceLocation() );
            } catch( exception::modulo_with_floatingpoint const & ) {
                throw exception::modulo_with_floatingpoint( GetSourceLocation() );
            }
        }
        // FIXME: use Operator categories! / enum ranges (>= BinOpPlus && <= BinOpMod)
        bool const is_arithmetic_binop = mOperator == "+" || mOperator == "-" || mOperator == "*" /*|| mOperator == "/" || mOperator == "mod"*/;
        if( is_arithmetic_binop ) {
            return util::ArtithmeticFactory::ApplyBinOp( lhs, rhs, mOperator );
        }

        // string concat
        if( mOperator == "%" ) {
            return ValueObject{lhs.GetAsString() + rhs.GetAsString()};
        }

        // comparison (relational)
        if( mOperator == "<" || mOperator == "lt" ) {
            return ValueObject{lhs < rhs};
        } else if( mOperator == "<=" || mOperator == "le" ) {
            return ValueObject{lhs <= rhs};
        } else if( mOperator == ">" || mOperator == "gt" ) {
            return ValueObject{lhs > rhs};
        } else if( mOperator == ">=" || mOperator == "ge" ) {
            return ValueObject{lhs >= rhs};
        }
        
        // comparison (equality)
        if( mOperator == "==" || mOperator == "eq" ) {
            return ValueObject{lhs == rhs};
        } else if( mOperator == "!=" || mOperator == "ne" ) {
            return ValueObject{lhs != rhs};
        }

        // shared_with
        if( mOperator == "@@" ) {
            return ValueObject{lhs.IsSharedWith( rhs )};
        }

        throw exception::eval_error( GetSourceLocation(), "Internal Error! Unknown Binary Operator!!" );
        //return {NaV};
    }
};


/// The Subscript operator ( lhs [ op1,...] ) for index or key based access via square brackets.
class ASTNode_Subscript_Operator : public ASTNode_Child_Capable
{
    bool mIsComplete = false;

    inline
    void Check() const
    {
        if( IsIncomplete() ) {
            throw exception::eval_error( GetSourceLocation(), "Subscript ASTNode incomplete! Closing ']' were not parsed!" );
        } else if( mChildren.empty() ) {
            throw exception::eval_error( GetSourceLocation(), "Subscript ASTNode incomplete! LHS missing!" );
        } else if( mChildren.size() < 2 ) {
            throw exception::eval_error( GetSourceLocation(), "Subscript ASTNode incomplete! No index or key operand present!" );
        }
    }

public:
    explicit ASTNode_Subscript_Operator( SourceLocation loc = {} )
        : ASTNode_Child_Capable( "Subscript", std::move( loc ) )
    {
    }

    //FIXME: not working with standard operator mechanics, AddASTNode will not repair possible incomplete BinOps.
    bool NeedLHS() const noexcept override
    {
        return mChildren.empty();
    }

    int Precedence() const noexcept override
    {
        return 1;
    }

    bool IsComplete() const noexcept override
    {
        return mIsComplete;
    }

    void SetComplete() noexcept
    {
        mIsComplete = true;
    }

    void AddChildNode( ASTNodePtr node ) override
    {
        assert( node.get() != nullptr );
        if( IsComplete() ) {
            throw exception::runtime_error( GetSourceLocation(), "Subscript ASTNode complete! Cannot add additional child!" );
        }
        mChildren.emplace_back( std::move( node ) );
    }

    ValueObject SetValueObject( Context &rContext, ValueObject const &rValue, bool const shared )
    {
        Check();
        ValueObject  lhs = mChildren[0]->Eval( rContext ); // NOTE: lhs might be a temporary object!!!
        auto &tuple = lhs.GetValue<Collection<ValueObject>>();

        if( lhs.IsConst() ) {
            throw exception::eval_error( GetSourceLocation(), "Tuple is const. Elements cannot be changed!" );
        }

        // get and evaluate parameter list
        auto  paramval = mChildren[1]->Eval( rContext );
        auto &params = paramval.GetValue< std::vector< ValueObject> >();

        if( params.empty() ) {
            throw exception::eval_error( GetSourceLocation(), "Subscript ASTNode incomplete! No index or key operand present!" );
        } else if( params.size() > 1 ) {
            throw exception::eval_error( GetSourceLocation(), "Subscript ASTNode with more than one operand is not implemented!" );
        }

        auto const &index_or_key = params[0];

        auto &obj = index_or_key.GetTypeInfo()->IsSame<String>() ? tuple.GetValueByKey( index_or_key.GetValue<String>() ) : tuple.GetValueByIdx( index_or_key.GetAsInteger() );
        if( shared ) {
            obj.SharedAssignValue( rValue, GetSourceLocation() );
        } else {
            obj.AssignValue( rValue, GetSourceLocation() );
        }
        return obj;
    }

    ValueObject GetValueObject( Context &rContext ) const
    {
        Check();
        ValueObject  lhs = mChildren[0]->Eval( rContext ); // NOTE: lhs might be a temporary object!!!
        auto &tuple = lhs.GetValue<Collection<ValueObject>>();

        // get and evaluate parameter list
        auto  paramval = mChildren[1]->Eval( rContext );
        auto &params = paramval.GetValue< std::vector< ValueObject> >();

        if( params.empty() ) {
            throw exception::eval_error( GetSourceLocation(), "Subscript ASTNode incomplete! No index or key operand present!" );
        } else if( params.size() > 1 ) {
            throw exception::eval_error( GetSourceLocation(), "Subscript ASTNode with more than one operand is not implemented!" );
        }

        auto const &index_or_key = params[0];

        if( index_or_key.GetTypeInfo()->IsSame<String>() ) {
            return tuple.GetValueByKey( index_or_key.GetValue<String>() );
        } else {
            return tuple.GetValueByIdx( index_or_key.GetAsInteger() );
        }
    }

    ValueObject Eval( Context &rContext ) override
    {
        try {
            return GetValueObject( rContext );
        } catch( exception::bad_value_cast const & ) {
            throw exception::eval_error( GetSourceLocation(), "Subscript Operator: LHS is not a Tuple or Array!" );
        }
    }
};


/// The dot operator ( lhs . rhs ), working with Tuples.
class ASTNode_Dot_Operator : public ASTNode_Binary_Operator
{
    inline
    void Check() const
    {
        if( !IsComplete() ) {
            if( NeedLHS() ) {
                throw exception::eval_error( GetSourceLocation(), "Dot Operator ASTNode incomplete! LHS and RHS missing!" );
            } else { // mChildren.size() < 2
                throw exception::eval_error( GetSourceLocation(), "Dot Operator ASTNode incomplete! RHS missing!" );
            }
        }
    }


    std::size_t GetIndex( Tuple const &tuple, Context &rContext ) const
    {
        std::size_t  idx = static_cast<std::size_t>(-1);
        std::string  identifier;

        if( mChildren[1]->GetName() == "Id" ) {
            identifier = mChildren[1]->GetDetail();
        // if Expression is allowed, then this code is valid: tup.(4-1).0 , but now we can also just use this: tup[4-1].0
        } else if( mChildren[1]->GetName() == "Constant" /*|| mChildren[1]->GetName() == "Expression"*/ ) {
            auto const  val = mChildren[1]->Eval( rContext );
            if( val.GetTypeInfo()->IsSame( TypeString ) ) {
                identifier = val.GetValue<String>();
            } else {
                idx = static_cast<std::size_t>(val.GetAsInteger());
            }
        } else {
            throw exception::eval_error( GetSourceLocation(), "Dot Operator: Invalid access!" );
        }

        if( not identifier.empty() ) {
            idx = static_cast<std::size_t>(tuple.IndexOfKey( identifier ));
        }

        if( idx == static_cast<std::size_t>(-1) ) {
            throw exception::unknown_identifier( GetSourceLocation(), identifier );
        } else if( not tuple.ContainsIdx( idx ) ) {
            throw exception::out_of_range( GetSourceLocation() );
        }

        return idx;
    }

public:
    explicit ASTNode_Dot_Operator( SourceLocation loc = {} )
        : ASTNode_Binary_Operator( ".", std::move( loc ) )
    {
    }

    int Precedence() const noexcept override
    {
        return 1;
    }

    /// \warning EXPERIMENTAL interface, builds the branch string of all nested dot ops
    std::string BuildBranchString() const
    {
        std::string res;
        Apply( [&res]( ASTNode const *p, int ) -> bool {
            if( p->GetName() == "Id" ) {
                if( not res.empty() ) { res += "."; }
                res += p->GetDetail();
            } else if( p->GetName() == "Constant" ) {
                if( not res.empty() ) { res += "."; }
                res += static_cast<ASTNode_Constant const *>(p)->GetValue().PrintValue();
            }
            return true;
        } );
        return res;
    }


    ValueObject AddValueObject( Context &rContext, ValueObject const &rVal )
    {
        Check();
        ValueObject  lhs = mChildren[0]->Eval( rContext ); // NOTE: lhs might be a temporary object!!!
        auto &tuple = lhs.GetValue<Collection<ValueObject>>();

        if( lhs.IsConst() ) {
            throw exception::eval_error( GetSourceLocation(), "Tuple is const. Elements cannot be added!" );
        }

        std::string identifier;
        if( mChildren[1]->GetName() == "Id" ) {
            identifier = mChildren[1]->GetDetail();
        } else if( mChildren[1]->GetName() == "Constant" && mChildren[1]->GetDetail() == "String" ) {
            identifier = mChildren[1]->Eval( rContext ).GetValue<String>();
        }

        if( not identifier.empty() ) {
            if( !tuple.AppendKeyValue( identifier, rVal ) ) {
                throw exception::redefinition_of_variable( GetSourceLocation(), identifier );
            }
            return tuple.GetValueByKey( identifier );
        } else if( mChildren[1]->GetName() == "Constant" ) {
            auto const idx = mChildren[1]->Eval( rContext ).GetAsInteger();
            if( idx < 0 || static_cast<std::size_t>(idx) > tuple.Size() ) {
                throw exception::out_of_range( GetSourceLocation() );
            } else if( static_cast<std::size_t>(idx) != tuple.Size() ) {
                throw exception::redefinition_of_variable( GetSourceLocation(), std::to_string(idx) );
            } else {
                tuple.AppendValue( rVal );
                return tuple.GetValueByIdx( static_cast<std::size_t>(idx) );
            }
        } else {
            
        }

        throw exception::eval_error( GetSourceLocation(), "Dot Operator: Invalid access!" );
    }

    ValueObject SetValueObject( Context &rContext, ValueObject const &rValue, bool const shared )
    {
        Check();
        ValueObject  lhs = mChildren[0]->Eval( rContext ); // NOTE: lhs might be a temporary object!!!
        auto &tuple = lhs.GetValue<Collection<ValueObject>>();

        if( lhs.IsConst() ) {
            throw exception::eval_error( GetSourceLocation(), "Tuple is const. Elements cannot be changed!" );
        }

        auto const idx = GetIndex( tuple, rContext ); // throws on error!
        auto &obj = tuple.GetValueByIdx_Unchecked( idx );
        if( shared ) {
            obj.SharedAssignValue( rValue, GetSourceLocation() );
        } else {
            obj.AssignValue( rValue, GetSourceLocation() );
        }
        return obj;
    }

    ValueObject RemoveValueObject( Context &rContext )
    {
        Check();
        ValueObject  lhs = mChildren[0]->Eval( rContext ); // NOTE: lhs might be a temporary object!!!
        auto &tuple = lhs.GetValue<Collection<ValueObject>>();
        if( lhs.IsConst() ) {
            throw exception::eval_error( GetSourceLocation(), "Tuple is const. Elements cannot be removed!" );
        }

        auto const idx = GetIndex( tuple, rContext ); // throws on error!
        auto obj = tuple.GetValueByIdx_Unchecked( idx );
        tuple.RemoveValueByIdx( idx );
        return obj;
    }

    ValueObject GetValueObject( Context &rContext ) const
    {
        Check();
        ValueObject  lhs = mChildren[0]->Eval( rContext ); // NOTE: lhs might be a temporary object!!!
        auto &tuple = lhs.GetValue<Collection<ValueObject>>();

        auto const idx = GetIndex( tuple, rContext ); // throws on error!
        auto obj = tuple.GetValueByIdx_Unchecked( idx );
        if( lhs.IsConst() ) obj.MakeConst();
        return obj;
    }

    ValueObject Eval( Context &rContext ) override
    {
        try {
            return GetValueObject( rContext );
        } catch( exception::bad_value_cast const & ) {
            throw exception::eval_error( GetSourceLocation(), "Dot Operator: LHS is not a Tuple/Record/Class/Module/Namespace!" );
        }
    }

};


/// ASTNode for assignment.
class ASTNode_Assign : public ASTNode_Binary_Operator
{
    bool const mbShared;
    enum class eMode
    {
        Assign,
        DefAssign,
        ConstAssign,
    } mMode = eMode::Assign;
public:
    ASTNode_Assign( bool const shared = false, SourceLocation loc = {} )
        : ASTNode_Binary_Operator( shared ? "@=" : ":=", std::move(loc) )
        , mbShared( shared )
    {
    }

    inline bool IsAssignWithDef() const noexcept
    {
        return mMode != eMode::Assign;
    }

    void AddChildNode( ASTNodePtr node ) override
    {
        assert( node.get() != nullptr );
        if( IsComplete() ) {
            throw exception::runtime_error( GetSourceLocation(), "Assign Operator ASTNode complete! Cannot add additional child!" );
        }

        if( NeedLHS() && node->GetName() == "UnOp" ) {
            if( node->GetDetail() == "def" ) {
                mMode = eMode::DefAssign;
            } else if( node->GetDetail() == "const" ) {
                mMode = eMode::ConstAssign;
            } else {
                throw exception::runtime_error( GetSourceLocation(), "Unsupported define mode for Assign Operator!" );
            }
            node = node->PopChild(); // get the ID and release def UnOp.
        }

        mChildren.emplace_back( std::move( node ) );
    }

    ValueObject Eval( Context &rContext ) override
    {
        if( !IsComplete() ) {
            if( NeedLHS() ) {
                throw exception::eval_error( GetSourceLocation(), "Assign Operator ASTNode incomplete! LHS and RHS missing!" );
            } else { // mChildren.size() < 2
                throw exception::eval_error( GetSourceLocation(), "Assign Operator ASTNode incomplete! RHS missing!" );
            }
        }
        bool const is_id = mChildren[0]->GetName() == "Id";
        if( not is_id 
            && not (mChildren[0]->GetName() == "BinOp" && mChildren[0]->GetDetail() == ".")
            && not (mChildren[0]->GetName() == "Subscript" && mMode == eMode::Assign) ) {
            throw exception::eval_error( mChildren[0]->GetSourceLocation(), "Assign Operator can only assign to Identifiers! LHS ist not an identifier!" );
        }

        if( mMode == eMode::Assign ) {
            auto  val = mChildren[1]->Eval( rContext );
            try {
                // FIXME: Make clean!!
                if( is_id ) {
                    return rContext.SetValue( mChildren[0]->GetDetail(), val, mbShared, GetSourceLocation() );
                } else if( mChildren[0]->GetName() == "BinOp" ) {
                    return static_cast<ASTNode_Dot_Operator *>(mChildren[0].get())->SetValueObject( rContext, val, mbShared );
                } else {
                    return static_cast<ASTNode_Subscript_Operator *>(mChildren[0].get())->SetValueObject( rContext, val, mbShared );
                }
            } catch( exception::unknown_identifier const & ) {
                if( rContext.auto_define_unknown_identifiers ) {
                    if( !mbShared ) {
                        val.Detach( true ); // make copy.
                    }
                    if( is_id ) {
                        return rContext.AddValueObject( mChildren[0]->GetDetail(), val.MakeShared(), mChildren[0]->GetSourceLocation() );
                    } else {
                        return static_cast<ASTNode_Dot_Operator *>(mChildren[0].get())->AddValueObject( rContext, val.MakeShared() );
                    }
                }
                throw;
            }
        } else if( mMode == eMode::DefAssign ) {
            auto  val = mChildren[1]->Eval( rContext );
            if( !mbShared ) {
                if( val.ShareCount() > 1 ) { // only make copy for values living on some store already.
                    val.Detach( false ); // make copy
                }
            } else if( val.IsShared() && val.IsConst() ) {
                // FIXME: For function calls we want to show the call side. But the assign is at callee location! (mChildren[1] is then FromParamList, does also not have proper SrcLoc)
                throw exception::const_shared_assign( GetSourceLocation() );
            }
            if( is_id ) {
                return rContext.AddValueObject( mChildren[0]->GetDetail(), val.MakeShared(), mChildren[0]->GetSourceLocation() );
            } else {
                return static_cast<ASTNode_Dot_Operator *>(mChildren[0].get())->AddValueObject( rContext, val.MakeShared() );
            }
        } else { /* ConstAssign */
            auto  val = mChildren[1]->Eval( rContext );
            if( !mbShared && val.ShareCount() > 1 ) { // only make copy for values living on some store already.
                val.Detach( true ); // make copy
            }

            if( is_id ) {
                return rContext.AddValueObject( mChildren[0]->GetDetail(), val.MakeShared().MakeConst(), mChildren[0]->GetSourceLocation() );
            } else {
                return static_cast<ASTNode_Dot_Operator *>(mChildren[0].get())->AddValueObject( rContext, val.MakeShared().MakeConst() );
                //throw exception::eval_error( GetSourceLocation(), "Const definition of tuple elements is unsupported!" );
            }
        }
    }
};

/// ASTNode for defining, undefining and query defintion of variables
class ASTNode_Var_Def_Undef : public ASTNode_Unary_Operator
{
public:
    enum class eType
    {
        Def,
        Undef,
        IsDef,
        Const,
        Debug
    };

protected:
    eType const mType;

public:

    explicit ASTNode_Var_Def_Undef( eType const type, SourceLocation loc = {} )
        : ASTNode_Unary_Operator( type == eType::Def ? "def" : type == eType::Undef ? "undef" : type == eType::IsDef ? "is_defined" : type == eType::Const ? "const" : "debug", std::move(loc) )
        , mType(type)
    { 
    }

    void AddChildNode( ASTNodePtr node ) override
    {
        assert( node.get() != nullptr );
        if( node->GetName() != "Id" && (node->GetName() != "BinOp" && node->GetDetail() != ".") ) {
            throw exception::runtime_error( GetSourceLocation(), "Variable definition/undefinition requires an identifier name!" );
        }
        ASTNode_Unary_Operator::AddChildNode( std::move( node ) );
    }

    int Precedence() const noexcept override
    {
        return 2; //NOTE: smaller than assign op ( :=, 16 ), then this will become the lhs of assign, but bigger than dot op (., 1)
    }

    ValueObject Eval( Context &rContext ) override
    {
        if( IsIncomplete() ) {
            throw exception::eval_error( GetSourceLocation(), "Unary Operator ASTNode incomplete! Operand missing!" );
        }

        bool const is_id = mChildren[0]->GetName() == "Id";

        if( mType == eType::Def ) {
            // NOTE: the Def Operator stays only there if it is without assignment. Otherwise the Assign Operator will do the work.
            // IMPORTANT: The option to have a TeaScript dialect where declaration without assignment is allowed is actually unsupported and not functional.
            if( rContext.declare_identifiers_without_assign_allowed && is_id ) { // NOTE: Not implemented for Dot Op.
                return rContext.AddValueObject( mChildren[0]->GetDetail(), ValueObject().MakeShared(), GetSourceLocation() ); //NOTE: need to be marked with type 'undefined' for can assign any type
            } else {
                throw exception::declare_without_assign( GetSourceLocation(), mChildren[0]->GetDetail() );
            }
        } else if( mType == eType::IsDef ) {
            try {
                if( not is_id ) {
                    (void)mChildren[0]->Eval( rContext );
                    return ValueObject( true );
                } else {
                    long long scope = 0;
                    (void)rContext.FindValueObject( mChildren[0]->GetDetail(), GetSourceLocation(), &scope );
                    return ValueObject( scope );
                }

            } catch( exception::unknown_identifier const & ) {
                return ValueObject( false );
            } catch( exception::out_of_range const & ) {
                return ValueObject( false );
            }
        } else if( mType == eType::Undef ) {
            try {
                auto const val = mChildren[0]->Eval( rContext );
                if( val.IsConst() ) { // TODO: Check if this shall be moved into RemoveValueObject
                    throw exception::eval_error( mChildren[0]->GetSourceLocation(), "Variable is const. Const variables cannot be undefined!" );
                }
                if( is_id ) {
                    (void)rContext.RemoveValueObject( mChildren[0]->GetDetail(), GetSourceLocation() );
                } else {
                    static_cast<ASTNode_Dot_Operator *>(mChildren[0].get())->RemoveValueObject( rContext );
                }
                return ValueObject( true );
            } catch( exception::unknown_identifier const & ) {
                if( rContext.undefine_unknown_idenitifiers_allowed ) {
                    return ValueObject( false );
                }
                throw;
            } catch( exception::out_of_range const & ) {
                if( rContext.undefine_unknown_idenitifiers_allowed ) {
                    return ValueObject( false );
                }
                throw;
            }
        } else if( mType == eType::Const ) {
            // Const can only work when assign during declaration!
            throw exception::declare_without_assign( GetSourceLocation(), mChildren[0]->GetDetail() );
        } else if( mType == eType::Debug ) {
            try {
                auto const val = mChildren[0]->Eval( rContext );
                if( FunctionPtr const *p_func = val.GetValuePtr< FunctionPtr >(); p_func != nullptr ) {
                    TEASCRIPT_PRINT( "{} : <function>\n", mChildren[0]->GetDetail() ); // TODO: add parameter info!
                } else {
                    std::string valstr = val.PrintValue();
                    if( val.GetTypeInfo()->IsSame( TypeString ) ) {
                        valstr.erase( 0, 1 ); // cut "
                        valstr.erase( valstr.size() - 1 ); // cut "
                        auto size = util::utf8_string_length( valstr );
                        util::prepare_string_for_print( valstr, 40 );
                        valstr += " (" + std::to_string( size ) + " glyphs)";
                    }

                    std::string name;
                    if( mChildren[0]->GetName() == "BinOp" && mChildren[0]->GetDetail() == "." ) {
                        name = static_cast<ASTNode_Dot_Operator const *>(mChildren[0].get())->BuildBranchString();
                    } else {
                        name = mChildren[0]->GetDetail();
                    }

                    //               name (TypeName, const/mutable, address, schare count): value
                    TEASCRIPT_PRINT( "{} ({}, {}, {:#x}, sc:{}) : {}\n", name, val.GetTypeInfo()->GetName(),
                                     (val.IsConst() ? "const" : "mutable"), val.GetInternalID(), val.ShareCount(),
                                     valstr );
                }
                //return ValueObject( true );
            } catch( exception::unknown_identifier const & ) {
                std::string name;
                if( mChildren[0]->GetName() == "BinOp" && mChildren[0]->GetDetail() == "." ) {
                    name = static_cast<ASTNode_Dot_Operator const *>(mChildren[0].get())->BuildBranchString();
                } else {
                    name = mChildren[0]->GetDetail();
                }
                TEASCRIPT_PRINT( "{} : <undefined>\n", name );
                //return ValueObject( false );
            }
            return ValueObject();
        }
        throw exception::eval_error( GetSourceLocation(), "Internal Error! Unknown Type for ASTNode_Var_Def_Undef!!" );
    }

};

/// ASTNode for typeof and typename operator
class ASTNode_Typeof_Typename : public ASTNode_Unary_Operator
{
public:
    explicit ASTNode_Typeof_Typename( bool const name, SourceLocation loc = {} )
        : ASTNode_Unary_Operator( name ? "typename" : "typeof", std::move(loc) )
    {
    }

    ValueObject Eval( Context &rContext ) override
    {
        if( IsIncomplete() ) {
            throw exception::eval_error( GetSourceLocation(), "Unary Operator ASTNode incomplete! Operand missing!" );
        }

        auto const val = mChildren[0]->Eval( rContext );
        if( GetDetail().ends_with( "of" ) ) { // typeof
            return ValueObject( *val.GetTypeInfo(), ValueConfig{ValueShared,ValueMutable,rContext.GetTypeSystem()});
        } else { // typename
            return ValueObject( val.GetTypeInfo()->GetName() );
        }
    }
};


/// ASTNode for the is operator
class ASTNode_Is_Type : public ASTNode_Binary_Operator
{
public:
    explicit ASTNode_Is_Type( SourceLocation loc = {} )
        : ASTNode_Binary_Operator( "is", std::move(loc) )
    { }

    void AddChildNode( ASTNodePtr node ) override
    {
        /*assert( node.get() != nullptr );
        if( node->GetName() != "Id" ) {
            throw exception::runtime_error( GetSourceLocation(), "Is Operator requires an identifier name!" );
        }*/
        ASTNode_Binary_Operator::AddChildNode( std::move( node ) );
    }

    ValueObject Eval( Context &rContext ) override
    {
        if( !IsComplete() ) {
            if( NeedLHS() ) {
                throw exception::eval_error( GetSourceLocation(), "Is Operator ASTNode incomplete! LHS and RHS missing!" );
            } else { // mChildren.size() < 2
                throw exception::eval_error( GetSourceLocation(), "Is Operator ASTNode incomplete! RHS missing!" );
            }
        }

        auto const  lhs = mChildren[0]->Eval( rContext );
        auto const  rhs = mChildren[1]->Eval( rContext );

        auto const *pT1 = lhs.GetTypeInfo();
        auto const *pT2 = rhs.GetTypeInfo();
        if( pT1 && pT2 ) {
            if( pT2->GetName() == "TypeInfo" ) {
                if( pT1->GetName() == "TypeInfo" ) {
                    // both are TypeInfos
                    //auto const &t1 = lhs.GetValue<TypeInfo>();
                    auto const &t2 = rhs.GetValue<TypeInfo>();
                    if( t2.GetName() == "TypeInfo" ) { // here lhs is always also a/some TypeInfo
                        return ValueObject( true );
                    }
                    //return ValueObject( t1.GetName() == t2.GetName() ); // "Bool" == "Bool" ? NO! Bool is a TypeInfo and not a Bool!!!
                    return ValueObject( false );
                } else {
                    // normal path: only t2 (rhs) is TypeInfo
                    auto const &t2 = rhs.GetValue<TypeInfo>();
                    if( t2.GetName() == "Number" ) { // fake concept 'Number' //TODO: Make clean! Match interface? or switch to class Type ?
                        return ValueObject( pT1->IsArithmetic() );
                    } else if( t2.GetName() == "Const" ) { //EXPERIMENTAL: fake concept 'Const' //TODO: Make clean! Match interface? or switch to class Type ?
                        return ValueObject( lhs.IsConst() || not lhs.IsShared() ); // Contants are treated as Const here since the value cannot be changed.
                    }
                    return ValueObject( t2.IsSame( *pT1 ) );
                }
            } else if( pT1->GetName() == "TypeInfo" ) {
                // pT2 is not TypeInfo
                // does it make sense ?
                // Bool is false ? NO!
                return ValueObject( false );
            } else {
                // both sides are not TypeInfos. Compare if they are same type.
                return ValueObject( pT1->IsSame( *pT2 ) );
            }
        }
        return ValueObject{false};
    }

};



namespace detail {
//forward decl...
inline bool IsNodeIfOrElse( ASTNodePtr const &node );
}

/// ASTNode for if-statement/expression.
class ASTNode_If : public ASTNode_Child_Capable
{
public:
    explicit ASTNode_If( SourceLocation loc = {} )
        : ASTNode_Child_Capable( "If", std::move(loc) )
    {
    }

    bool IsComplete() const noexcept override
    {
        return mChildren.size() > 1; // need at least one expr + one block
    }

    void SetComplete() noexcept
    {
        // dummy
    }

    void AddChildNode( ASTNodePtr node ) override
    {
        assert( node.get() != nullptr );
        if( mChildren.empty() ) {
            //WORKAROUND for make more than one sub-expr. possible, e.g. if( def z := func(), z >= 100 ) {}
            ASTNode_Expression *const p = dynamic_cast<ASTNode_Expression *>(node.get());
            if( p ) {
                p->SetMode( ASTNode_Expression::Mode::Cond );
            }
        } else if( mChildren.size() == 3 ) {
            // chained 'else if/else'? nest it...
            if( detail::IsNodeIfOrElse( mChildren[2] ) && detail::IsNodeIfOrElse( node ) ) {
                mChildren[2]->AddChildNode( std::move(node) );
                return;
            }
            throw exception::runtime_error( GetSourceLocation(), "If ASTNode complete! Cannot add additional child!" );
        }
        //TODO: Check kind of node here ??? Maybe we can relax the check and just try to eval...
        mChildren.emplace_back( std::move( node ) );
    }

    ValueObject Eval( Context &rContext ) override
    {
        ScopedNewScope new_scope( rContext ); // if has a new scope...
        
        if( IsIncomplete() ) {
            throw exception::eval_error( GetSourceLocation(), "If ASTNode incomplete! Condition or Block missing!" );
        }

        auto const  condition = mChildren.front()->Eval( rContext );
        bool  exec_if_branch = false;
        try {
            exec_if_branch = condition.GetAsBool();
        } catch( std::bad_cast const & ) {
            throw exception::eval_error( GetSourceLocation(), "If condition does not evaluate to bool!" );
        }

        if( exec_if_branch ) {
            return mChildren[1]->Eval( rContext );
        } else if( mChildren.size() > 2 ) {
            return mChildren[2]->Eval( rContext );
        }
        // no else
        return {};
    }

};

/// ASTNode for Else-Statement/expression.
class ASTNode_Else : public ASTNode_Child_Capable
{
public:
    explicit ASTNode_Else( SourceLocation loc = {} )
        : ASTNode_Child_Capable( "Else", std::move( loc ) )
    {
    }

    bool IsComplete() const noexcept override
    {
        return mChildren.size() > 0;
    }

    void SetComplete() noexcept
    {
        // dummy
    }

    void AddChildNode( ASTNodePtr node ) override
    {
        assert( node.get() != nullptr );
        if( IsComplete() ) {

            // chained 'else if/else'? nest it...
            if( detail::IsNodeIfOrElse( mChildren[0] ) && detail::IsNodeIfOrElse(node) ) {
                mChildren[0]->AddChildNode( std::move( node ) );
                return;
            }

            throw exception::runtime_error( GetSourceLocation(), "Else ASTNode complete! Cannot add additional child!" );
        }
        //TODO: Check kind of node here ??? Maybe we can relax the check and just try to eval...
        mChildren.emplace_back( std::move( node ) );
    }

    ValueObject Eval( Context &rContext ) override
    {
        if( IsIncomplete() ) {
            throw exception::eval_error( GetSourceLocation(), "Else ASTNode incomplete! Block or If Statement missing!" );
        }

        return mChildren.front()->Eval( rContext );
    }

};

namespace detail {
// impl...
inline bool IsNodeIfOrElse( ASTNodePtr const &node )
{
    return nullptr != dynamic_cast<ASTNode_If *>(node.get()) || nullptr != dynamic_cast<ASTNode_Else *>(node.get());
}
} // namespace detail


/// ASTNode for loop statement
class ASTNode_LoopToHead_Statement : public ASTNode
{
public:
    ASTNode_LoopToHead_Statement( std::string const &rLabelName = "", SourceLocation loc = {} )
        : ASTNode( "Loop", rLabelName, std::move(loc) )
    {

    }

    ValueObject Eval( Context &rContext ) override
    {
        (void)rContext;
        throw control::Loop_To_Head( GetDetail() ); // TODO: add srcloc for can know from where it was issued?!
    }
};

/// ASTNode for stop statement
class ASTNode_StopLoop_Statement : public ASTNode_Child_Capable
{
    bool mbNeedWithNode = false;
protected:
    ASTNode_StopLoop_Statement( bool const with_node, std::string name, SourceLocation loc = {} )
        : ASTNode_Child_Capable( std::move( name ), std::move(loc) )
        , mbNeedWithNode( with_node )
    {
    }

public:
    explicit ASTNode_StopLoop_Statement( std::string const &rLabelName = "", bool with_node = false, SourceLocation loc = {} )
        : ASTNode_Child_Capable( "Stop", rLabelName, std::move(loc) )
        , mbNeedWithNode( with_node )
    {

    }

    bool IsComplete() const noexcept override
    {
        return (not mbNeedWithNode) || (not mChildren.empty());
    }

    void SetComplete() noexcept
    {
        // dummy
    }

    // needed for the with statemnt. 
    int Precedence() const noexcept override
    {
        return INT_MAX; // with this expressions are possible after with clause without mandatory round brackets!!
    }

    void AddChildNode( ASTNodePtr node ) override
    {
        assert( node.get() != nullptr );
        if( IsComplete() ) {
            throw exception::runtime_error( GetSourceLocation(), "StopLoop ASTNode complete! Cannot add additional child!" );
        }
        mChildren.emplace_back( std::move( node ) );
    }


    ValueObject Eval( Context &rContext ) override
    {
        if( IsIncomplete() ) {
            throw exception::eval_error( GetSourceLocation(), "StopLoop ASTNode incomplete! With statement is not complete!" );
        }
        throw control::Stop_Loop( mChildren.empty() ? ValueObject() : mChildren[0]->Eval( rContext ), GetDetail() ); // TODO: add srcloc for can know from where it was issued?!
    }
};

/// ASTNode for the repeat loop
class ASTNode_Repeat : public ASTNode_Child_Capable
{
public:
    explicit ASTNode_Repeat( std::string const & rLabelName = "", SourceLocation loc = {} )
        : ASTNode_Child_Capable( "Repeat", rLabelName, std::move(loc) )
    { 
    }

    bool IsComplete() const noexcept override
    {
        return mChildren.size() > 0;
    }

    void SetComplete() noexcept
    {
        // dummy
    }

    void AddChildNode( ASTNodePtr node ) override
    {
        assert( node.get() != nullptr );
        if( IsComplete() ) {
            throw exception::runtime_error( GetSourceLocation(), "Repeat ASTNode complete! Cannot add additional child!" );
        }
        mChildren.emplace_back( std::move( node ) );
    }

    ValueObject Eval( Context &rContext ) override
    {
        if( IsIncomplete() ) {
            throw exception::eval_error( GetSourceLocation(), "Repeat ASTNode incomplete! Block or Condition Statement missing!" );
        }

        ScopedNewScope new_scope( rContext );
        ValueObject res;
        
        // Idea: could add optional timeout and infinite loop prevention

        while( true ) {
            try {
                res = mChildren[0]->Eval( rContext );
            } catch( control::Loop_To_Head const &rLoop ) {
                if( rLoop.GetName() != GetDetail() ) {
                    throw;
                }
                continue;
            } catch( control::Stop_Loop const &rStop ) {
                if( rStop.GetName() != GetDetail() ) {
                    throw;
                }
                res = rStop.GetResult();
                break;
            }
        }

        return res;
    }
};

/// returns the next param of current param list from the context
class ASTNode_FromParamList : public ASTNode
{
public:
    ASTNode_FromParamList()
        : ASTNode( "FromParamList" )
    {
    }

    ValueObject Eval( Context &rContext ) override
    {
        if( rContext.CurrentParamCount() > 0 ) {
            return rContext.ConsumeParam();
        }
        throw exception::eval_error( rContext.GetCurrentSourceLocation(), "FromParamList ASTNode: Too less arguments!");
    }
};

/// returns the next param of current param list from the context (if any) or otherwise evals the child node.
class ASTNode_FromParamList_Or : public ASTNode_Child_Capable
{
public:
    ASTNode_FromParamList_Or( SourceLocation loc = {} )
        : ASTNode_Child_Capable( "FromParamList_Or", std::move(loc) )
    {
    }

    bool IsComplete() const noexcept override
    {
        return mChildren.size() > 0;
    }

    void AddChildNode( ASTNodePtr node ) override
    {
        assert( node.get() != nullptr );
        if( IsComplete() ) {
            throw exception::runtime_error( GetSourceLocation(), "FromParamList_Or ASTNode complete! Cannot add additional child!" );
        }
        mChildren.emplace_back( std::move( node ) );
    }

    ValueObject Eval( Context &rContext ) override
    {
        if( IsIncomplete() ) {
            throw exception::eval_error( GetSourceLocation(), "FromParamList_Or ASTNode incomplete! Default value/expression missing!" );
        }
        if( rContext.CurrentParamCount() > 0 ) {
            return rContext.ConsumeParam();
        }
        return mChildren[0]->Eval( rContext );
    }
};


/// represents a TeaScript Function Parameter List (list of statements each for one parameter)
class ASTNode_ParamList : public ASTNode_Expression
{
protected:
    explicit ASTNode_ParamList( std::string name, SourceLocation loc = {} )
        : ASTNode_Expression( std::move(name), std::move(loc) )
    {
    }
public:
    explicit ASTNode_ParamList( SourceLocation loc = {} )
        : ASTNode_Expression( "ParamList", std::move(loc) )
    {
    }

    ValueObject Eval( Context &rContext ) override
    {
        if( IsIncomplete() ) {
            throw exception::eval_error( GetSourceLocation(), GetName() + " ASTNode incomplete! Some parts are missing!" );
        }

        std::vector< ValueObject > vals;
        vals.reserve( mChildren.size() );
        for( auto &node : mChildren ) {
            vals.push_back( node->Eval( rContext ) );
        }

        return ValueObject( std::move( vals ), ValueConfig( ValueShared, ValueMutable, rContext.GetTypeSystem() ) );
    }
};


/// represents a TeaScript Function Parameter specification (list of assign statements)
class ASTNode_ParamSpec : public ASTNode_ParamList
{
public:
    explicit ASTNode_ParamSpec( SourceLocation loc = {} )
        : ASTNode_ParamList( "ParamSpec", std::move( loc ) )
    {
    }
};


/// represents the return statement of a function
class ASTNode_Return_Statement : public ASTNode_StopLoop_Statement
{
public:
    explicit ASTNode_Return_Statement( bool const need_statement = true, SourceLocation loc = {} )
        : ASTNode_StopLoop_Statement( need_statement, "Return", std::move(loc) )
    {
    }

    void AddChildNode( ASTNodePtr node ) override
    {
        // same as in base class except the error message.
        assert( node.get() != nullptr );
        if( IsComplete() ) {
            throw exception::runtime_error( GetSourceLocation(), "Return ASTNode complete! Cannot add additional child!" );
        }
        mChildren.emplace_back( std::move( node ) );
    }

    ValueObject Eval( Context &rContext ) override
    {
        if( IsIncomplete() ) {
            throw exception::eval_error( GetSourceLocation(), "Return ASTNode incomplete!" );
        }
        throw control::Return_From_Function( mChildren.empty() ? ValueObject() : mChildren[0]->Eval( rContext ) );
    }
};


/// contains all statements of one block with a new local scope.
class ASTNode_Block : public ASTNode_Child_Capable
{
    bool mIsComplete;
public:
    explicit ASTNode_Block( SourceLocation loc )
        : ASTNode_Child_Capable( "Block", std::move( loc ) )
        , mIsComplete( false )
    {
    }

    explicit ASTNode_Block( std::vector<ASTNodePtr> children = {}, SourceLocation loc = {} )
        : ASTNode_Child_Capable( "Block", std::move(loc) )
        , mIsComplete( false )
    {
        mChildren = std::move( children );
    }


    bool IsComplete() const noexcept override
    {
        return mIsComplete;
    }

    void SetComplete() noexcept
    {
        mIsComplete = true;
    }

    void AddChildNode( ASTNodePtr node ) override
    {
        assert( node.get() != nullptr );
        if( IsComplete() ) {
            throw exception::runtime_error( GetSourceLocation(), "Block ASTNode complete! Cannot add additional child!" );
        }
        mChildren.emplace_back( std::move( node ) );
    }

    ValueObject Eval( Context &rContext ) override
    {
        ScopedNewScope new_scope( rContext );
        ValueObject res;
        for( ASTNodePtr &node : mChildren ) {
            res = node->Eval( rContext );
        }
        return res;
    }
};


/// experimental ASTNode for a partial parsed file. 
/// All children of all ASTNode_FilePart innstances for one file would assemble the ASTNode_File.
class ASTNode_FilePart : public ASTNode_Child_Capable
{
public:
    ASTNode_FilePart( std::string const &rFileName, std::vector<ASTNodePtr> children )
        : ASTNode_Child_Capable( "FilePart", rFileName )
    {
        mChildren = std::move( children );
    }

    auto begin()
    {
        return mChildren.begin();
    }
    auto end()
    {
        return mChildren.end();
    }

    ValueObject Eval( Context &rContext ) override
    {
        ValueObject res;
        for( ASTNodePtr &node : mChildren ) {
            res = node->Eval( rContext );
        }
        return res;
    }
};

using ASTNode_FilePart_Ptr = std::shared_ptr< ASTNode_FilePart >;


/// contains all top level statements of one file / eval call.
class ASTNode_File : public ASTNode_Child_Capable
{    
public:
    ASTNode_File( std::string const &rFileName, std::vector<ASTNodePtr> children )
        : ASTNode_Child_Capable( "File", rFileName )
    {
        mChildren = std::move( children );
    }

    ValueObject Eval( Context & rContext ) override
    {
        ValueObject res;
        try {
            for( ASTNodePtr &node : mChildren ) {
                res = node->Eval( rContext );
            }
        } catch( control::Return_From_Function &rReturn ) { // return from 'main' is possible
            res = rReturn.MoveResult();
        }
        return res;
    }
};

//TODO: have ASTNode_Root additionally? E.g. combine more than one file. Then we could pass the last value as input (as a special var???) ?!


} // namespace teascript
