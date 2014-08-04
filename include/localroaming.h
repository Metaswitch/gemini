/**
 * @file localroaming.h Interface declaration for the local roaming AS which
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

#ifndef LOCALROAMING_H__
#define LOCALROAMING_H__

extern "C" {
#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <pjsip-simple/evsub.h>
}

#include "appserver.h"

class LocalRoamingAppServer;
class LocalRoamingAppServerTsx;

/// The LocalRoamingAppServer implements the gemini service, and subclasses
/// the abstract AppServer class.
///
/// Sprout calls the get_app_tsx method on LocalRoamingAppServer when
///
/// -  an IFC triggers with ServiceName containing a host name of the form
///    gemini.<homedomain>;
class LocalRoamingAppServer : public AppServer
{
public:
  /// Constructor
  LocalRoamingAppServer(const std::string& _service_name) : AppServer(_service_name)
  {
  }

  /// Called when the system determines the service should be invoked for a
  /// received request.  The AppServer can either return NULL indicating it
  /// does not want to process the request, or create a suitable object
  /// derived from the AppServerTsx class to process the request.
  ///
  /// @param  helper        - The service helper to use to perform
  ///                         the underlying service-related processing.
  /// @param  req           - The received request message.
  virtual AppServerTsx* get_app_tsx(AppServerTsxHelper* helper,
                                    pjsip_msg* req);
};

/// The LocalRoamingAppServerTsx class subclasses AppServerTsx and provides
/// application-server-specific processing of a single transaction.  It
/// encapsulates a ServiceTsx, which it calls through to to perform the
/// underlying service-related processing.
///
class LocalRoamingAppServerTsx : public AppServerTsx
{
public:
  /// Constructor.
  LocalRoamingAppServerTsx(AppServerTsxHelper* helper);

  /// Virtual destructor.
  virtual ~LocalRoamingAppServerTsx();

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
  /// A copy of the original request.
  pjsip_msg* _original_req; 

  /// The method.
  pjsip_method_e _method;

  /// Fork ID for the INVITE that has been sent on to the mobile device.
  int _mobile_fork_id;

  /// Whether or not we’ve already tried to fork an INVITE to the
  /// VoIP client on the mobile device, which we try to do after
  /// the native mobile client returns a 480 suggesting it's not
  /// registered.
  bool _attempted_mobile_voip_client;
};

#endif
