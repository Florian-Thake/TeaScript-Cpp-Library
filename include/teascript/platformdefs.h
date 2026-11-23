/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2026 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
/* classical guards because this file might become useful to be includeable in C at some point. Should be compatible to C89/90 */
#ifndef TEASCRIPT_PLATFORMDEFS_HEADER_INC_GUARD
#define TEASCRIPT_PLATFORMDEFS_HEADER_INC_GUARD
#pragma once

/* This file is meant to define several platform / compiler depending things which are relevant for specific TeaScript feature implementations. 
*/

/* custom user header for set global defines, e.g., TEASCRIPT_PRINT(...) */
#if defined(TEASCRIPT_USER_CONFIG_HEADER)
# include TEASCRIPT_USER_CONFIG_HEADER
#endif


/* TeaScript's helper macro for the C++ standard version */
#if defined(__cplusplus)
/* try _MSVC_LANG if available and carries a greater value (probably /Zc:__cplusplus was not used for MSVC) */
# if defined(_MSVC_LANG) && _MSVC_LANG > __cplusplus
#  define TEASCRIPT_CPLUSPLUS    _MSVC_LANG
# else
#  define TEASCRIPT_CPLUSPLUS    __cplusplus
# endif
#else
# define TEASCRIPT_CPLUSPLUS   0
#endif

#endif /* TEASCRIPT_PLATFORMDEFS_HEADER_INC_GUARD */

