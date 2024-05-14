/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
/* classical guards if someone uses very old C  ... and of course also only C90 comments ;-) */
#ifndef TEASCRIPT_VERSION_HEADER_INC_GAURD
#define TEASCRIPT_VERSION_HEADER_INC_GAURD



#define TEASCRIPT_NAME       "TeaScript"

#define TEASCRIPT_CONTACT    "contact@tea-age.solutions"

#define TEASCRIPT_COPYRIGHT  "Copyright (C) 2024 Florian Thake <" TEASCRIPT_CONTACT ">."


#define TEASCRIPT_VERSION_MAJOR   0
#define TEASCRIPT_VERSION_MINOR   14
#define TEASCRIPT_VERSION_PATCH   0

/* TODO: ? //#define TEASCRIPT_VERSION_RELEASE_KIND  alpha/beta/... */

/* builds a version number as (unsigned) int. note: one byte left for e.g. alpha, beta or hotfix stuff, if needed. */
#define TEASCRIPT_BUILD_VERSION_NUMBER( major, minor, patch )  (((major) << 24) + ((minor) << 16) + ((patch) << 8) )

/* the current (combined) version number of TeaScript */
#define TEASCRIPT_VERSION    TEASCRIPT_BUILD_VERSION_NUMBER( TEASCRIPT_VERSION_MAJOR, TEASCRIPT_VERSION_MINOR, TEASCRIPT_VERSION_PATCH )

/* extracts the major, minor and patch part from a combined version number */
#define TEASCRIPT_VERSION_EXTRACT_MAJOR( combined )  ((combined >> 24) & 0xFF)
#define TEASCRIPT_VERSION_EXTRACT_MINOR( combined )  ((combined >> 16) & 0xFF)
#define TEASCRIPT_VERSION_EXTRACT_PATCH( combined )  ((combined >>  8) & 0xFF)

/* helper for version string */
#define TEASCRIPT_STRINGIFY(x) TEASCRIPT_DO_STRINGIFY(x)
#define TEASCRIPT_DO_STRINGIFY(x) #x

#define TEASCRIPT_MAKE_VERSION_STRING( major, minor, patch )  TEASCRIPT_STRINGIFY( major ) "." TEASCRIPT_STRINGIFY( minor ) "." TEASCRIPT_STRINGIFY( patch ) 

#define TEASCRIPT_VERSION_STR  TEASCRIPT_MAKE_VERSION_STRING( TEASCRIPT_VERSION_MAJOR, TEASCRIPT_VERSION_MINOR, TEASCRIPT_VERSION_PATCH )

#define TEASCRIPT_BUILD_DATE_TIME __DATE__ "  " __TIME__


/* The first step for compatibility with plain old C. You can use at least the version macros... ;-)  */
#ifdef __cplusplus



namespace teascript {

namespace version {

inline constexpr unsigned int Major = TEASCRIPT_VERSION_MAJOR;
inline constexpr unsigned int Minor = TEASCRIPT_VERSION_MINOR;
inline constexpr unsigned int Patch = TEASCRIPT_VERSION_PATCH;

/*! \return the major version of TeaScript */
constexpr unsigned int get_major() noexcept
{
    return Major;
}

/*! \return the minor version of TeaScript */
constexpr unsigned int get_minor() noexcept
{
    return Minor;
}

/*! \return the patch version of TeaScript */
constexpr unsigned int get_patch() noexcept
{
    return Patch;
}

/*! \return the combined version number for easy comapare. */
constexpr unsigned int combined_number() noexcept
{
    return TEASCRIPT_VERSION;
}

/*! \return the version number of TeaScript as string */
constexpr char const *as_str() noexcept
{
    return TEASCRIPT_VERSION_STR;
}

/*! \return a build date and time string 
 *  \note this does not reflect the build date/time of the TeaScript library. 
 *  \note It will change for every compile of the TU where it is used!
 */
constexpr char const *build_date_time_str() noexcept
{
    return TEASCRIPT_BUILD_DATE_TIME;
}

} /* namespace version */


/*! \return the name of this library */
constexpr char const *self_name_str() noexcept
{
    return TEASCRIPT_NAME;
}

/* \return contact information for support queries, bug reports and feature wishes.*/
constexpr char const *contact_info() noexcept
{
    return TEASCRIPT_CONTACT;
}

/*! \return copyright information for the TeaScript Library.*/
constexpr char const *copyright_info() noexcept
{
    return TEASCRIPT_COPYRIGHT;
}

} /* namespace teascript */


#endif /* __cplusplus */

#endif /* TEASCRIPT_VERSION_HEADER_INC_GAURD */
