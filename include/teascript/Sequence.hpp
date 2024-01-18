/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, version 3.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
 * You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>
 */
#pragma once

#include <concepts>
#include <stdexcept>


namespace teascript {

/// class Sequence is for forming a sequence of integral numbers from [start,end] with a specified step.
/// If end cannot be reached exactly with step the value remains the last valid value before end.
/// An empty sequence is impossible. For start == end it will yield always start as the one and only current value.
template<std::integral T> requires( !std::is_same_v<std::remove_cvref_t<T>, bool> )
class Sequence
{
protected:
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
        : mStart( start ), mEnd( end ), mStep( step ), mCur( 0 )
    {
        if( !validate() ) {
            throw std::invalid_argument( "Seqeunce: invalid start,end,step combination!" );
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

    void Reset()
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
            if( mEnd - mCur >= mStep ) {
                mCur += mStep;
                return true;
            }
        } else { // backwards
            // mStep is always negative here!
            // example case: -3 (end) - -1 (cur)  == -2 (step) step must be -2 or -1 --> mEnd-mCur (-2) must be <= step (-2 is < -1)
            if( mEnd - mCur <= mStep ) {
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
