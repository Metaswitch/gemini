/**
 * @file mobiletwinned.cpp Implementation of the mobile twinned AS which
 * is part of gemini.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2014  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#include "log.h"
#include "mobiletwinned.h"
#include "pjutils.h"
#include "gemini_constants.h"
#include "custom_headers.h"
#include "geminisasevent.h"
#include "constants.h"

const char* STR_SERVER = "server";
const char* STR_PRINCIPAL = "principal";

/// Returns a new MobileTwinnedAppServerTsx if the request is either a
/// SUBSCRIBE or a INVITE.
AppServerTsx* MobileTwinnedAppServer::get_app_tsx(SproutletHelper* helper,
                                                  pjsip_msg* req,
                                                  pjsip_sip_uri*& next_hop,
                                                  pj_pool_t* pool,
                                                  SAS::TrailId trail)
{
  if ((req->line.req.method.id != PJSIP_INVITE_METHOD) &&
      (pjsip_method_cmp(&req->line.req.method, pjsip_get_subscribe_method())))
  {
    // Request isn't an INVITE or SUBSCRIBE, no processing is required.
    return NULL;
  }

  MobileTwinnedAppServerTsx* mobile_twinned_tsx =
                                          new MobileTwinnedAppServerTsx();
  return mobile_twinned_tsx;
}

/// Constructor
MobileTwinnedAppServerTsx::MobileTwinnedAppServerTsx() :
  AppServerTsx(),
  _mobile_fork_id(0),
  _attempted_mobile_voip_client(false),
  _single_target(false)
{
}

/// Destructor
MobileTwinnedAppServerTsx::~MobileTwinnedAppServerTsx()
{
}

void MobileTwinnedAppServerTsx::on_initial_request(pjsip_msg* req)
{
  TRC_DEBUG("MobileTwinnedAS - process request %p", req);

  pjsip_uri* req_uri = req->line.req.uri;

  if (!PJSIP_URI_SCHEME_IS_SIP(req_uri))
  {
    TRC_DEBUG("Request URI isn't a SIP URI");
    pjsip_msg* rsp = create_response(req, PJSIP_SC_TEMPORARILY_UNAVAILABLE);
    send_response(rsp);
    free_msg(req);
    return;
  }

  // If the Request URI contains a 'gr' parameter, then this is a
  // request targeted at a specific VoIP client. Set the single_target flag.
  if (pjsip_param_find(&((pjsip_sip_uri*)req_uri)->other_param, &STR_GR))
  {
    TRC_DEBUG("Call is targeted at a specific VoIP client");

    SAS::Event event(trail(), SASEvent::CALL_TO_VOIP_CLIENT, 0);
    SAS::report_event(event);

    _single_target = true;
    send_request(req);
    return;
  }

  // Get the twin-prefix
  pjsip_param* twin_prefix = NULL;
  const pjsip_route_hdr* route_header = route_hdr();

  if (route_header != NULL)
  {
    pjsip_sip_uri* route_hdr_uri = (pjsip_sip_uri*)route_header->name_addr.uri;
    twin_prefix = pjsip_param_find(&route_hdr_uri->other_param, &STR_TWIN_PRE);
  }

  // If the request has a Accept-Contact header that contains g.3gpp.ics
  // then this is a request targeted at the native device. Add the twin
  // prefix to the request URI and set the single_target flag
  if (accept_contact_header_has_3gpp_ics(req))
  {
    TRC_DEBUG("Call is targeted at the native device");

    add_twin_prefix(req_uri, twin_prefix, get_pool(req));

    SAS::Event event(trail(), SASEvent::CALL_TO_NATIVE_DEVICE, 0);
    event.add_var_param(PJUtils::uri_to_string(PJSIP_URI_IN_REQ_URI, req_uri));
    SAS::report_event(event);

    _single_target = true;
    _mobile_fork_id = send_request(req);
    return;
  }

  // Otherwise, fork the call. Create a copy of the request we can manipulate
  // (and change the name of the existing request so we don't accidentally
  // use it).
  pjsip_msg* mobile_req = clone_request(req);
  pjsip_msg* voip_req = req; req = NULL;

  // Set up the fork to the VoIP client.
  // To prevent both the VoIP client and native mobile service both
  // ringing on the same device, add a Reject-Contact header containing
  // +sip.with-twin to the request headed for the VoIP client. (We
  // require that Gemini-compatible clients set this if they detect
  // that they are colocated with a native client).
  TRC_DEBUG("Creating forked request to VoIP client");
  pj_pool_t* voip_pool = get_pool(voip_req);
  pjsip_reject_contact_hdr* reject_hdr =
                                   pjsip_reject_contact_hdr_create(voip_pool);
  pjsip_param* reject_colocated_voip = PJ_POOL_ALLOC_T(voip_pool, pjsip_param);
  reject_colocated_voip->name = STR_WITH_TWIN;
  reject_colocated_voip->value.slen = 0;
  pj_list_insert_after(&reject_hdr->feature_set, reject_colocated_voip);
  pjsip_msg_add_hdr(voip_req, (pjsip_hdr*)reject_hdr);

  // We also need a Reject-Contact header containg "g.3gpp.ics", to
  // ensure that this never matches a native client without a
  // colocated VoIP phone (which should be rung by the other fork).
  std::string g_3gpp_ics = std::string("\"") + STR_SERVER + ","
                                             + STR_PRINCIPAL + "\"";
  pjsip_reject_contact_hdr* reject_hdr2 =
                                   pjsip_reject_contact_hdr_create(voip_pool);
  pjsip_param* reject_native = PJ_POOL_ALLOC_T(voip_pool, pjsip_param);
  reject_native->name = STR_3GPP_ICS;
  reject_native->value = pj_strdup3(voip_pool, g_3gpp_ics.c_str());
  pj_list_insert_after(&reject_hdr2->feature_set, reject_native);
  pjsip_msg_add_hdr(voip_req, (pjsip_hdr*)reject_hdr2);

  // Set up the fork to the native device.
  // Append the twin prefix (if set) to the request URI, and add an
  // Accept-Contact header specifying g.3gpp.ics.
  TRC_DEBUG("Creating forked request to twinned mobile device");
  pj_pool_t* mobile_pool = get_pool(mobile_req);
  add_twin_prefix(mobile_req->line.req.uri, twin_prefix, mobile_pool);
  pjsip_accept_contact_hdr* accept_hdr =
                                   pjsip_accept_contact_hdr_create(mobile_pool);
  accept_hdr->explicit_match = true;
  accept_hdr->required_match = true;
  pjsip_param* force_native = PJ_POOL_ALLOC_T(mobile_pool, pjsip_param);
  force_native->name = STR_3GPP_ICS;
  force_native->value = pj_strdup3(mobile_pool, g_3gpp_ics.c_str());
  pj_list_insert_after(&accept_hdr->feature_set, force_native);
  pjsip_msg_add_hdr(mobile_req, (pjsip_hdr*)accept_hdr);

  // Add Reject-Contact "+sip.with-twin", to guard against the
  // unexpected case where a phone specifies both "+sip.with-twin" and "+g.3gpp.ics".
  pjsip_reject_contact_hdr* reject_hdr3 =
                                   pjsip_reject_contact_hdr_create(mobile_pool);
  pjsip_param* reject_colocated_voip2 = PJ_POOL_ALLOC_T(mobile_pool, pjsip_param);
  reject_colocated_voip2->name = STR_WITH_TWIN;
  reject_colocated_voip2->value.slen = 0;
  pj_list_insert_after(&reject_hdr3->feature_set, reject_colocated_voip2);
  pjsip_msg_add_hdr(mobile_req, (pjsip_hdr*)reject_hdr3);

  // Report the fact we're forking the request to SAS, including
  // the new native mobile URI.
  SAS::Event event(trail(), SASEvent::FORKING_ON_REQ, 0);
  event.add_var_param(PJUtils::uri_to_string(PJSIP_URI_IN_REQ_URI,
                                             mobile_req->line.req.uri));
  SAS::report_event(event);

  send_request(voip_req);
  _mobile_fork_id = send_request(mobile_req);
}

void MobileTwinnedAppServerTsx::on_response(pjsip_msg* rsp, int fork_id)
{
  // In on_initial_request we add a Reject-Contact header to INVITEs
  // going to the VoIP client to stop the client and the native mobile
  // service ringing at the same time. If we receive a 480 from the
  // fork to the native mobile device, indicating it isn't registered,
  // we should now try sending the INVITE to any VoIP clients
  // hosted on mobile devices.
  if ((PJSIP_MSG_CSEQ_HDR(rsp)->method.id == PJSIP_INVITE_METHOD) &&
      (rsp->line.status.code == PJSIP_SC_TEMPORARILY_UNAVAILABLE) &&
      (fork_id == _mobile_fork_id) &&
      (!_attempted_mobile_voip_client))
  {
    if (_single_target)
    {
      TRC_DEBUG("No retry as original call was targeted at a specific device");
      SAS::Event event(trail(), SASEvent::NO_RETRY_ON_480_RSP, 0);
      SAS::report_event(event);
      send_response(rsp);
      return;
    }

    TRC_DEBUG("Creating a new fork to mobile hosted VoIP clients");
    SAS::Event event(trail(), SASEvent::FORKING_ON_480_RSP, 0);
    SAS::report_event(event);

    // Create an Accept-Contact header and add the "+sip.with-twin" parameter.
    pjsip_msg* req = original_request();
    pj_pool_t* pool = get_pool(req);

    pjsip_accept_contact_hdr* new_hdr = pjsip_accept_contact_hdr_create(pool);
    new_hdr->explicit_match = true;
    new_hdr->required_match = true;
    pjsip_param* force_twinned = PJ_POOL_ALLOC_T(pool, pjsip_param);
    force_twinned->name = STR_WITH_TWIN;
    force_twinned->value.slen = 0;
    pj_list_insert_after(&new_hdr->feature_set, force_twinned);
    pjsip_msg_add_hdr(req, (pjsip_hdr*)new_hdr);

    send_request(req);
    free_msg(rsp);

    // Set the flag to indicate we've now tried reaching the VoIP client
    // on the mobile already so we don't come through here again. We
    // shouldn't do anyway because the fork_ids won't match, but just in
    // case.
    _attempted_mobile_voip_client = true;
  }
  else
  {
    send_response(rsp);
  }
}

void MobileTwinnedAppServerTsx::add_twin_prefix(pjsip_uri* req_uri,
                                                pjsip_param* twin_prefix,
                                                pj_pool_t* pool)
{
  if ((twin_prefix != NULL) && (twin_prefix->value.slen != 0))
  {
    std::string new_user = PJUtils::pj_str_to_string(&twin_prefix->value) +
              PJUtils::pj_str_to_string(&((pjsip_sip_uri*)req_uri)->user);
    ((pjsip_sip_uri*)req_uri)->user = pj_strdup3(pool,
                                                 new_user.c_str());
  }
}

bool MobileTwinnedAppServerTsx::accept_contact_header_has_3gpp_ics(pjsip_msg* req)
{
  // Loop through the feature list on every Accept-Contact header
  // to check whether any contain 'g.3gpp.ics'
  bool accept_contact_header_has_3gpp_ics = false;
  pjsip_accept_contact_hdr* accept_header =
      (pjsip_accept_contact_hdr*)pjsip_msg_find_hdr_by_name(req,
                                                            &STR_ACCEPT_CONTACT,
                                                            NULL);
  while ((accept_header != NULL) && (!accept_contact_header_has_3gpp_ics))
  {
    for (pjsip_param* feature_param = accept_header->feature_set.next;
         (feature_param != &accept_header->feature_set) &&
         (!accept_contact_header_has_3gpp_ics);
         feature_param = feature_param->next)
    {
      if (pj_stricmp(&feature_param->name, &STR_3GPP_ICS) == 0)
      {
        std::string ics_values = PJUtils::pj_str_to_string(&feature_param->value);
        accept_contact_header_has_3gpp_ics =
            ((ics_values.find(STR_SERVER) != std::string::npos) ||
            (ics_values.find(STR_PRINCIPAL) != std::string::npos));
      }
    }

    accept_header =
     (pjsip_accept_contact_hdr*)pjsip_msg_find_hdr_by_name(req,
                                                           &STR_ACCEPT_CONTACT,
                                                           accept_header->next);
  }

  return accept_contact_header_has_3gpp_ics;
}
