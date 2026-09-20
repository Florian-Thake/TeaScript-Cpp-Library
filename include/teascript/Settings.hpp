/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2026 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "ConfigEnums.hpp"
#include "Dialect.hpp"

#include <filesystem>

namespace teascript {

class Settings
{
    Dialect          mDialect;
    config::eConfig  mCoreConfig   = config::full();
    eOptimize        mOptimization = eOptimize::O0;

    std::filesystem::path  mModulePath; // empty == no modules can be loaded or saved.
    std::filesystem::path  mTsbPath;    // empty == no compiled programs can be loaded or saved from/to disk (always fresh compile)

public:
    Settings() = default;
    explicit Settings( config::eConfig const config, eOptimize const opt_level = eOptimize::O0 )
        : mCoreConfig( config )
        , mOptimization( opt_level )
    {

    }

    Settings( config::eConfig const config, unsigned int const opt_out, eOptimize const opt_level = eOptimize::O0 )
        : mCoreConfig( config::build( config, opt_out ) )
        , mOptimization( opt_level )
    {

    }

    Settings( Dialect const &dialect, config::eConfig const config, eOptimize const opt_level, std::filesystem::path const &rModulePath, std::filesystem::path const &rTsbPath )
        : mDialect(dialect)
        , mCoreConfig(config)
        , mOptimization(opt_level)
        , mModulePath(rModulePath)
        , mTsbPath(rTsbPath)
    {

    }

    /// Use this for finally move this instance after construction chaining, e.g., Engine( Settings().SetCoreConfig( config::full() ).Move() );
    Settings &&Move() { return std::move( *this ); }

    Settings &SetModulePath( std::filesystem::path const &rPath ) { mModulePath = rPath; return *this; }
    Settings &SetTsbPath( std::filesystem::path const &rPath ) { mTsbPath = rPath; return *this; }

    std::filesystem::path const &GetModulePath() const { return mModulePath; }
    std::filesystem::path const &GetTsbPath() const { return mTsbPath; }

    config::eConfig GetCoreConfig() const { return mCoreConfig; }
    Settings &SetCoreConfig( config::eConfig const config ) { mCoreConfig = config; return *this; }

    eOptimize GetOptimizationLevel() const { return mOptimization; }
    Settings &SetOptimizationLevel( eOptimize const opt_level ) { mOptimization = opt_level; return *this; }

    bool IsDebug() const { return mOptimization == eOptimize::Debug; }


    Dialect const &GetDialect()  const { return mDialect; }

};

} // namespace teascript


