/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2026 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "Settings.hpp"
#include "StackVMProgram.hpp"
#include "version.h"

#include <filesystem>

namespace teascript {


/// [T]ea[S]cript [B]inary Store - Manage storage of compiled programs as .tsb on disk.
class TsbStore
{
public:

    /// Loads a present and compatible program (.tsb) from disk for given script and settings.
    /// \note The script must be the TeaScript source file (usually .tea) and a tsb path must be present in the settings.
    /// \return valid program ptr on success, otherwise nullptr.
    static StackVM::ProgramPtr LoadProgramFor( std::filesystem::path const &rScript, Settings const &rSettings )
    {
        auto const  program_file = FindTsbFor( rScript, rSettings );
        if( program_file.empty() ) {
            return {};
        }

        auto program = StackVM::Program::Load( program_file );

        if( program != nullptr && program->GetCompilerVersion() != teascript::version::combined_number() ) {
            program.reset(); // cannot use from another compiler version!
        }
        return program;
    }

    /// Stores the given program as a .tsb file to disk. Tsb path must be set in the settings and be an existing directory.
    /// \return true on success, false otherise.
    static bool StoreProgram( StackVM::ProgramPtr const &rProgram, Settings const &rSettings )
    {
        if( rSettings.GetTsbPath().empty() || not rProgram ) {
            return false;
        }

        std::string const filename = BuildTsbFileName( rProgram->GetName(), rProgram->GetUsedOptimization() );
        if( filename.empty() ) {
            return {};
        }
        return rProgram->Save( std::filesystem::absolute( rSettings.GetTsbPath() ) / filename );
    }

    /// Looks up whether a corresponding .tsb file is present for and not older than given TeaScript files and the corresponding settings.
    /// \note The script must be the TeaScript source file (usually .tea) and a tsb path must be present in the settings.
    /// \return a non empty std::filesystem::path with the absolute .tsb file path on success, empty otherwise.
    static std::filesystem::path FindTsbFor( std::filesystem::path const &rScript, Settings const &rSettings )
    {
        std::error_code  ec;
        if( not std::filesystem::is_regular_file( rScript, ec ) ) {
            return {};
        }
        auto const  last_mod_script = std::filesystem::last_write_time( rScript, ec );
        auto const  file = BuildTsbFilePathAndNameFor( rScript, rSettings );
        if( ec || file.empty() || not std::filesystem::is_regular_file( file ) ) {
            return {};
        }
        auto const  last_mod_tsb = std::filesystem::last_write_time( file );
        if( last_mod_tsb < last_mod_script ) {
            return {};
        }
        return file;
    }

    /// Builds the absolute file path for a .tsb file for the given TeaScript file and settings.
    /// \note The script must be the TeaScript source file (usually .tea) and a tsb path must be present in the settings.
    static std::filesystem::path BuildTsbFilePathAndNameFor( std::filesystem::path const &rScript, Settings const &rSettings )
    {
        if( rSettings.GetTsbPath().empty() ) {
            return {};
        }
        std::string const filename = BuildTsbFileName( rScript, rSettings.GetOptimizationLevel() );
        if( filename.empty() ) {
            return {};
        }
        return std::filesystem::absolute( rSettings.GetTsbPath() ) / filename;
    }

    /// Builds the file name for a .tsb file for the given TeaScript file and optimization level.
    static std::string BuildTsbFileName( std::filesystem::path const &rScript, eOptimize const opt_level )
    {
        auto filename = rScript.filename().string();
        if( not filename.empty() ) {
            switch( opt_level ) {
            case eOptimize::Debug:
                filename += ".debug";
                break;
            case eOptimize::O0:
                filename += ".O0";
                break;
            case eOptimize::O1:
                filename += ".O1";
                break;
            case eOptimize::O2:
                filename += ".O2";
                break;
            default:
                //TODO: use std::unreachable in C++23
#if defined( _MSC_VER ) // MSVC
                __assume(false);
#else // GCC, clang, ...
                __builtin_unreachable();
#endif
            }
            filename += ".tsb";
        }
        return filename;
    }
};

} // namespace teascript
