/**
 * @file mobiletwinned.h Interface declaration for the mobile twinning AS which
 * is part of gemini.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOBILETWINNED_H__
#define MOBILETWINNED_H__

extern "C" {
#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <pjsip-simple/evsub.h>
}

#include "appserver.h"

class MobileTwinnedAppServer;
class MobileTwinnedAppServerTsx;

/// The MobileTwinnedAppServer implements the gemini service, and subclasses
/// the abstract AppServer class.
///
/// Sprout calls the get_app_tsx method on MobileTwinnedAppServer when
///
/// -  an IFC triggers with ServiceName containing a host name of the form
///    sip:mobile-twinned@<gemini domain>;
class MobileTwinnedAppServer : public AppServer
{
public:
  /// Constructor
  MobileTwinnedAppServer(const std::string& _service_name) : AppServer(_service_name)
  {
  }

  /// Called when the system determines the service should be invoked for a
  /// received request.  The AppServer can either return NULL indicating it
  /// does not want to process the request, or create a suitable object
  /// derived from the AppServerTsx class to process the request.
  ///
  /// @param  helper        - The Sproutlet helper.
  /// @param  req           - The received request message.
  /// @param  next_hop      - The Sproutlet can use this field to specify a
  ///                         next hop URI when it returns a NULL Tsx.
  /// @param  pool          - The pool for creating the next_hop uri.
  /// @param  trail         - The SAS trail id for the message.
  virtual AppServerTsx* get_app_tsx(SproutletHelper* helper,
                                    pjsip_msg* req,
                                    pjsip_sip_uri*& next_hop,
                                    pj_pool_t* pool,
                                    SAS::TrailId trail);
};

/// The MobileTwinnedAppServerTsx class subclasses AppServerTsx and provides
/// application-server-specific processing of a single transaction.  It
/// encapsulates a ServiceTsx, which it calls through to to perform the
/// underlying service-related processing.
///
class MobileTwinnedAppServerTsx : public AppServerTsx
{
public:
  /// Constructor.
  MobileTwinnedAppServerTsx();

  /// Virtual destructor.
  virtual ~MobileTwinnedAppServerTsx();

  /// Called for an initial request (dialog-initiating or out-of-dialog) with
  /// the original received request for the transaction.
  ///
  /// This function tries to fork INVITEs and SUBSCRIBEs to twinned mobile
  /// devices. If successful, it will call send_request twice to forward the
  /// request to the two forks. If unsuccessful, it will create and send a
  /// 480 response.
  ///
  /// @param req           - The received initial request.
  virtual void on_initial_request(pjsip_msg* req);

  /// Called with all responses received on the transaction.  If a transport
  /// error or transaction timeout occurs on a downstream leg, this method is
  /// called with a 408 response.
  ///
  /// Upon receiving a 480 from the mobile device fork, this function creates
  /// a third fork specifically aimed at VoIP clients hosted on mobile devices.
  /// Otherwise it just passes the response on.
  ///
  /// @param  rsp          - The received request.
  /// @param  fork_id      - The identity of the downstream fork on which
  ///                        the response was received.
  virtual void on_response(pjsip_msg* rsp, int fork_id);

private:
  /// Adds a twin prefix to a request URI
  ///
  /// @param req_uri        - <in/out> The Request URI to manipulate
  /// @param twin_prefix    - The prefix to add (can be empty)
  /// @param pool           - The pool to use
  void add_twin_prefix(pjsip_uri* req_uri,
                       pjsip_param* twin_prefix,
                       pj_pool_t* pool);

  /// Returns whether any Accept-Contact headers in the request contain
  /// the feature 'g.3gpp.ics'
  ///
  /// @param req            - The request to check
  /// @returns whether there's the matching feature
  bool accept_contact_header_has_3gpp_ics(pjsip_msg* req);

  /// Fork ID for the INVITE that has been sent on to the mobile device.
  int _mobile_fork_id;

  /// Whether or not we've already tried to fork an INVITE to the
  /// VoIP client on the mobile device, which we try to do after
  /// the native mobile client returns a 480 suggesting it's not
  /// registered.
  bool _attempted_mobile_voip_client;

  /// Whether the request should only be sent to a single target
  bool _single_target;
};

#endif
