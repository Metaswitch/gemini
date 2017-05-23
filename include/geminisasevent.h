/**
 * @file geminisasevent.h Memento-specific SAS event IDs
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef GEMINISASEVENT_H__
#define GEMINISASEVENT_H__

#include "sasevent.h"

namespace SASEvent
{
  //----------------------------------------------------------------------------
  // Gemini events.
  //----------------------------------------------------------------------------
  const int FORKING_ON_REQ = GEMINI_BASE + 0x000000;

  const int FORKING_ON_480_RSP = GEMINI_BASE + 0x000010;
  const int NO_RETRY_ON_480_RSP = GEMINI_BASE + 0x000011;

  const int CALL_TO_VOIP_CLIENT = GEMINI_BASE + 0x000020;
  const int CALL_TO_NATIVE_DEVICE = GEMINI_BASE + 0x000021;

} //namespace SASEvent

#endif
