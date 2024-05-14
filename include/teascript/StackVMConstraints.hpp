/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include <chrono>

namespace teascript {

namespace StackVM {

/// Constraint class for specify limits for program execution inside the TeaStackVM.
class Constraints
{
public:
    enum class eKind
    {
        None,
        InstrCount,
        Timed,
        AutoContinue
    };

private:
    eKind mKind;
    union
    {
        unsigned long long mMaxCount = 0;
        std::chrono::milliseconds mMaxTime;
    };
    Constraints( bool ) : mKind( eKind::AutoContinue ) {}
public:
    /// Default constructor sets no limit.
    Constraints() : mKind( eKind::None ) {}

    /// Limitation by time in milliseconds.
    explicit Constraints( std::chrono::milliseconds const ms )
        : mKind( eKind::Timed )
        , mMaxTime( ms )
    {
    }

    /// Limitation by amount of executed instructions.
    explicit Constraints( unsigned long long const count )
        : mKind( eKind::InstrCount )
        , mMaxCount( count )
    {
    }

    /// Factory function construction constraint with no limit.
    static Constraints None()
    {
        return Constraints();
    }

    /// Factory function constructing constraint with time limitation in milliseconds.
    static Constraints MaxTime( std::chrono::milliseconds const ms )
    {
        return Constraints( ms );
    }

    /// Factory function constructing constraint with maximum instruction limitation.
    static Constraints MaxInstructions( unsigned long long const count )
    {
        return Constraints( count );
    }

    /// Factory function for specify an auto continue for suspend statements.
    static Constraints AutoContinue()
    {
        return Constraints( true );
    }

    inline eKind Kind() const { return mKind; }

    inline std::chrono::milliseconds GetMaxTime() const { return mKind == eKind::Timed ? mMaxTime : std::chrono::milliseconds(); }
    inline unsigned long long GetMaxInstr() const { return mKind == eKind::InstrCount ? mMaxCount : 0ULL; }
};

} // namespace StackVM

} // namespace teascript
