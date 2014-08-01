/**
 * @file localroaming.cpp Implementation of the local roaming AS which
 * is part of gemini.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
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
#include "localroaming.h"
#include "pjutils.h"
#include "constants.h"
#include "custom_headers.h"
#include "geminisasevent.h"

/// Returns a new LocalRoamingAppServerTsx if the request is either a
/// SUBSCRIBE or a INVITE.
AppServerTsx* LocalRoamingAppServer::get_app_tsx(AppServerTsxHelper* helper,
                                 pjsip_msg* req)
{
  if ((req->line.req.method.id != PJSIP_INVITE_METHOD) &&
      (pjsip_method_cmp(&req->line.req.method, pjsip_get_subscribe_method())))
  {
    // Request isn't an INVITE or SUBSCRIBE, no processing is required.
    return NULL;
  }

  LocalRoamingAppServerTsx* local_roaming_tsx = new LocalRoamingAppServerTsx(helper);
  return local_roaming_tsx;
}

/// Constructor
LocalRoamingAppServerTsx::LocalRoamingAppServerTsx(AppServerTsxHelper* helper) :
  AppServerTsx(helper),
  _mobile_fork_id(0),
  _attempted_mobile_voip_client(false)
{
}

/// Destructor
LocalRoamingAppServerTsx::~LocalRoamingAppServerTsx()
{
  if (_original_req != NULL)
  {
    free_msg(_original_req);
    _original_req = NULL;
  }
}

void LocalRoamingAppServerTsx::on_initial_request(pjsip_msg* req)
{
  _method = req->line.req.method.id;
  pjsip_sip_uri* sip_uri;

  LOG_DEBUG("LocalRoamingAS - process request %p, method %s", req, _method);

  // Check the URI in top route header has a "twin-prefix" parameter.
  pjsip_route_hdr* route_hdr = (pjsip_route_hdr*)pjsip_msg_find_hdr(req, PJSIP_H_ROUTE, NULL);
  pjsip_sip_uri* route_hdr_uri = (pjsip_sip_uri*)route_hdr->name_addr.uri;
  pjsip_param* twinning_prefix = pjsip_param_find(&route_hdr_uri->other_param, &STR_TWIN_PRE);
  if (twinning_prefix == NULL)
  {
    LOG_ERROR("No twinning prefix for local-roaming forking");
    SAS::Event event(trail(), SASEvent::NO_TWIN_PREFIX, 0);
    SAS::report_event(event);
    create_response(req, PJSIP_SC_TEMPORARILY_UNAVAILABLE);
    send_response(req);
    return;
  }

  // We're about to fork the call. Create copies of the request which
  // we can manipulate.
  pjsip_msg* voip_req = clone_request(req);
  pjsip_msg* mobile_req = clone_request(req);

  // To prevent both the VoIP client and native mobile service both
  // ringing on the same device, add a Reject-Contact header to the
  // INVITE headed for the VoIP client.
  if (_method == PJSIP_INVITE_METHOD)
  {
    LOG_DEBUG("Add Reject-Contact header to VoIP client request");
    pj_pool_t* pool = get_pool(voip_req);

    // Create an Reject-Contact header and add the "+sip.phone" parameter.
    pjsip_reject_contact_hdr* new_hdr = pjsip_reject_contact_hdr_create(pool);
    pjsip_param* phone = PJ_POOL_ALLOC_T(pool, pjsip_param);
    pj_strdup(pool, &phone->name, &STR_PHONE);
    phone->value.slen = 0;
    pj_list_insert_after(&new_hdr->feature_set, phone);
    pjsip_msg_add_hdr(voip_req, (pjsip_hdr*)new_hdr);
  }

  // Add the twinned-prefix to the front of the URI to fork the second
  // request off to the twinned mobile device.
  if (PJSIP_URI_SCHEME_IS_SIP(mobile_req->line.req.uri)) 
  {
    LOG_DEBUG("Creating forked request to twinned mobile device");
    sip_uri = (pjsip_sip_uri*)mobile_req->line.req.uri;
    pj_str_t new_user = twinning_prefix->value;
    pj_strcat(&new_user, &sip_uri->user);
    pj_strdup(get_pool(mobile_req), &sip_uri->user, &new_user);
  }
  else
  {
    LOG_DEBUG("Not SIP URI, so cancel forking");
    free_msg(voip_req);
    free_msg(mobile_req);
    create_response(req, PJSIP_SC_TEMPORARILY_UNAVAILABLE);
    send_response(req);
    return;
  }

  // Report the fact we're forking the request to SAS, including
  // the new native mobile URI.
  SAS::Event event(trail(), SASEvent::FORKING_ON_REQ, 0);
  event.add_var_param(PJUtils::uri_to_string(PJSIP_URI_IN_REQ_URI, (pjsip_uri*)sip_uri));
  SAS::report_event(event);

  _original_req = req;
  send_request(voip_req);
  _mobile_fork_id = send_request(mobile_req);
}

void LocalRoamingAppServerTsx::on_response(pjsip_msg* rsp, int fork_id)
{
  // In on_initial_request we add a Reject-Contact header to INVITEs
  // going to the VoIP client to stop the client and the native mobile
  // service ringing at the same time. If we receive a 480 from the
  // fork to the native mobile device, indicating it isn't registered,
  // we should now try sending the INVITE to any VoIP clients
  // hosted on mobile devices.
  if ((_method == PJSIP_INVITE_METHOD) &&
      (rsp->line.status.code == PJSIP_SC_TEMPORARILY_UNAVAILABLE) &&
      (fork_id == _mobile_fork_id) &&
      (!_attempted_mobile_voip_client))
  {
    LOG_DEBUG("Creating a new fork to mobile hosted VoIP clients");
    SAS::Event event(trail(), SASEvent::FORKING_ON_480_RSP, 0);
    SAS::report_event(event);

    // Create an Accept-Contact header and add the "+sip.phone" parameter.
    pj_pool_t* pool = get_pool(_original_req);
    pjsip_accept_contact_hdr* new_hdr = pjsip_accept_contact_hdr_create(pool);
    new_hdr->explicit_match = true;
    new_hdr->required_match = true;
    pjsip_param* phone = PJ_POOL_ALLOC_T(pool, pjsip_param);
    pj_strdup(pool, &phone->name, &STR_PHONE);
    phone->value.slen = 0;
    pj_list_insert_after(&new_hdr->feature_set, phone);
    pjsip_msg_add_hdr(_original_req, (pjsip_hdr*)new_hdr);

    free_msg(rsp);
    send_request(_original_req);

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
