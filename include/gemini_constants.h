/**
 * @file gemini_constants.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef GEMINI_CONSTANTS_H_
#define GEMINI_CONSTANTS_H_

#include <pjsip.h>

const pj_str_t STR_TWIN_PRE = pj_str((char*)"twin-prefix");
const pj_str_t STR_WITH_TWIN = pj_str((char*)"+sip.with-twin");
const pj_str_t STR_3GPP_ICS = pj_str((char*)"+g.3gpp.ics");

#endif
