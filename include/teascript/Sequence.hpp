/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include <concepts>
#include <stdexcept>


namespace teascript {

// TODO: check for allow floating point sequences as well.

/// class Sequence is for forming a sequence of integral numbers from [start,end] with a specified step.
/// If end cannot be reached exactly with step the value remains the last valid value before end.
/// An empty sequence is impossible. For start == end it will yield always start as the one and only current value.
template<std::integral T> requires( !std::is_same_v<std::remove_cvref_t<T>, bool> )
class Sequence
{
private:
    T  mStart;
    T  mEnd;
    std::make_signed_t<T>   mStep; // step must be signed.

    // current value, cannot be const or reference.
    std::remove_const_t<std::remove_reference_t<T>>  mCur;

    inline
    bool validate() const noexcept
    {
        return mStart == mEnd || (mStep != 0 && ((mStart > mEnd && mStep < 0) || (mStart < mEnd && mStep > 0)) );
    }
public:
    Sequence( T start, T end, std::make_signed_t<T> step )
        : mStart( start ), mEnd( end ), mStep( step ), mCur( mStart )
    {
        if( !validate() ) {
            throw std::invalid_argument( "Sequence: invalid start,end,step combination!" );
        }
    }

    inline
    bool IsForwards() const noexcept
    {
        return mEnd > mStart; // this implies mStep > 0
    }

    inline
    bool IsBackwards() const noexcept
    {
        return mEnd < mStart; // this implies mStep < 0
    }

    void Reset() noexcept
    {
        mCur = mStart;
    }

    inline
    T Current() const noexcept
    {
        return mCur;
    }

    inline
    T Start() const noexcept
    {
        return mStart;
    }

    inline
    T End() const noexcept
    {
        return mEnd;
    }

    inline
    auto Step() const noexcept -> std::make_signed_t<T>
    {
        return mStep;
    }

    /// If there is a next value available the current value will be updated.
    /// \return whether the current value was set to the next number of the sequence.
    bool Next() noexcept
    {
        if( mCur == mEnd ) {
            return false;
        }
        if( IsForwards() ) {
            // mStep is always positive here!
            // example case: -5 (end) - -6 (cur) == 1 (step) step must be 1 --> mEnd-mCur (1) must be >= step
            if( mEnd - mCur >= static_cast<T>(mStep) ) {
                mCur += mStep;
                return true;
            }
        } else { // backwards
            // mStep is always negative here!
            // example case: -3 (end) - -1 (cur)  == -2 (step) step must be -2 or -1 --> mEnd-mCur (-2) must be <= step (-2 is < -1)
            // mEnd - mCur will be always negative, so we must cast the result to the type of mStep for avoid a warning when usnigned is used
            // (the minus operation is well defined overflow for unsigned types!).
            if( static_cast<decltype(mStep)>(mEnd - mCur) <= mStep ) {
                mCur += mStep;
                return true;
            }
        }
        return false;
    }

    /// \returns either the next current value (which will be updated as well) or the end value if no more values are available.
    T Next_or_End() noexcept
    {
        if( Next() ) {
            return Current();
        }
        return End();
    }
};

} // namespace teascript
