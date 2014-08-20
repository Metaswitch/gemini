/**
 * @file localroaming_test.cpp UT fixture for the local roaming AS which is
 * part of gemini.
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


#include <string>
#include <unordered_map>
#include "gtest/gtest.h"

#include "siptest.hpp"
#include "mockappserver.hpp"
#include "localroaming.h"
#include "stack.h"
#include "pjutils.h"
#include "custom_headers.h"
#include "constants.h"
#include "gemini_constants.h"

using namespace std;
using testing::InSequence;
using testing::Return;

/// Fixture for LocalRoamingAppServerTest.
///
/// This derives from SipTest to ensure PJSIP is set up correctly, but doesn't
/// actually use most of its function (and doesn't register a module).
class LocalRoamingAppServerTest : public SipTest
{
public:
  static void SetUpTestCase()
  {
    SipTest::SetUpTestCase();
    _helper = new MockAppServerTsxHelper();
  }

  static void TearDownTestCase()
  {
    delete _helper; _helper = NULL;
    SipTest::TearDownTestCase();
  }

  LocalRoamingAppServerTest() : SipTest(NULL)
  {
  }

  ~LocalRoamingAppServerTest()
  {
  }

  // Check the list of expected_params are found in the list of actual_params.
  bool check_params(pjsip_param* actual_params, std::unordered_map<std::string, std::string>& expected_params)
  {
    for (std::unordered_map<std::string, std::string>::const_iterator it = expected_params.begin();
         it != expected_params.end();
         ++it)
    {
      pj_str_t header_name = pj_str((char*)it->first.c_str());
      pjsip_param* param = pjsip_param_find(actual_params, &header_name);
      if ((param == NULL) ||
          (PJUtils::pj_str_to_string(&param->value) != it->second))
      {
        return false;
      }
    }
    return true;
  }

  static MockAppServerTsxHelper* _helper;

  static const int VOIP_FORK_ID;
  static const int MOBILE_FORK_ID;
  static const int MOBILE_VOIP_FORK_ID;
};
MockAppServerTsxHelper* LocalRoamingAppServerTest::_helper = NULL;

const int LocalRoamingAppServerTest::VOIP_FORK_ID = 11111;
const int LocalRoamingAppServerTest::MOBILE_FORK_ID = 11112;
const int LocalRoamingAppServerTest::MOBILE_VOIP_FORK_ID = 11113;

namespace LocalRoamingAS
{
class Message
{
public:
  string _method;
  string _toscheme;
  string _status;
  string _from;
  string _fromdomain;
  string _to;
  string _todomain;
  string _route;

  Message() :
    _method("INVITE"),
    _toscheme("sip"),
    _status("200 OK"),
    _from("6505551000"),
    _fromdomain("homedomain"),
    _to("6505551234"),
    _todomain("homedomain"),
    _route("")
  {
  }

  string get_request();
  string get_response();
};
}

string LocalRoamingAS::Message::get_request()
{
  char buf[16384];

  // The remote target.
  string target = string(_toscheme).append(":").append(_to);
  if (!_todomain.empty())
  {
    target.append("@").append(_todomain);
  }

  int n = snprintf(buf, sizeof(buf),
                   "%1$s %4$s SIP/2.0\r\n"
                   "Via: SIP/2.0/TCP 10.114.61.213;branch=z9hG4bK0123456789abcdef\r\n"
                   "From: <sip:%2$s@%3$s>;tag=10.114.61.213+1+8c8b232a+5fb751cf\r\n"
                   "To: <%4$s>\r\n"
                   "%5$s"
                   "Max-Forwards: 68\r\n"
                   "Call-ID: 0gQAAC8WAAACBAAALxYAAAL8P3UbW8l4mT8YBkKGRKc5SOHaJ1gMRqsUOO4ohntC@10.114.61.213\r\n"
                   "CSeq: 16567 %1$s\r\n"
                   "User-Agent: Accession 2.0.0.0\r\n"
                   "Allow: PRACK, INVITE, ACK, BYE, CANCEL, UPDATE, SUBSCRIBE, NOTIFY, REFER, MESSAGE, OPTIONS\r\n"
                   "Content-Length: 0\r\n\r\n",
                   /*  1 */ _method.c_str(),
                   /*  2 */ _from.c_str(),
                   /*  3 */ _fromdomain.c_str(),
                   /*  4 */ target.c_str(),
                   /*  5 */ _route.empty() ? "" : string(_route).append("\r\n").c_str()
    );

  EXPECT_LT(n, (int)sizeof(buf));

  string ret(buf, n);
  return ret;
}

string LocalRoamingAS::Message::get_response()
{
  char buf[16384];

  // The remote target.
  string target = string(_toscheme).append(":").append(_to);
  if (!_todomain.empty())
  {
    target.append("@").append(_todomain);
  }

  int n = snprintf(buf, sizeof(buf),
                   "SIP/2.0 %1$s\r\n"
                   "Via: SIP/2.0/TCP 10.114.61.213;branch=z9hG4bK0123456789abcdef\r\n"
                   "From: <sip:%2$s@%3$s>;tag=10.114.61.213+1+8c8b232a+5fb751cf\r\n"
                   "To: <%4$s>\r\n"
                   "%5$s"
                   "Max-Forwards: 68\r\n"
                   "Call-ID: 0gQAAC8WAAACBAAALxYAAAL8P3UbW8l4mT8YBkKGRKc5SOHaJ1gMRqsUOO4ohntC@10.114.61.213\r\n"
                   "CSeq: 16567 %6$s\r\n"
                   "User-Agent: Accession 2.0.0.0\r\n"
                   "Allow: PRACK, INVITE, ACK, BYE, CANCEL, UPDATE, SUBSCRIBE, NOTIFY, REFER, MESSAGE, OPTIONS\r\n"
                   "Content-Length: 0\r\n\r\n",
                   /*  1 */ _status.c_str(),
                   /*  2 */ _from.c_str(),
                   /*  3 */ _fromdomain.c_str(),
                   /*  4 */ target.c_str(),
                   /*  5 */ _route.empty() ? "" : string(_route).append("\r\n").c_str(),
                   /*  6 */ _method.c_str()
    );

  EXPECT_LT(n, (int)sizeof(buf));

  string ret(buf, n);
  return ret;
}

using LocalRoamingAS::Message;

/// Compares a pjsip_msg's request URI with a std::string.
MATCHER_P(ReqUriEquals, uri, "")
{
  std::string arg_uri = PJUtils::uri_to_string(PJSIP_URI_IN_REQ_URI, arg->line.req.uri);
  LOG_DEBUG("arg_uri %s", arg_uri.c_str());
  return arg_uri == uri;
}

// Test creation and destruction of the LocalRoamingAppServer objects.
TEST_F(LocalRoamingAppServerTest, CreateLocalRoamingAppServer)
{
  // Create a LocalRoamingAppServer object
  LocalRoamingAppServer* as = new LocalRoamingAppServer("gemini");

  // Test creating an app server transaction with an invalid method -
  // it shouldn't be created.
  Message msg;
  msg._method = "OPTIONS";
  pjsip_msg* req = parse_msg(msg.get_request());
  LocalRoamingAppServerTsx* as_tsx = (LocalRoamingAppServerTsx*)as->get_app_tsx(_helper, req);
  EXPECT_TRUE(as_tsx == NULL);

  // Try with a valid method (INVITE). This creates the application server
  // transaction.
  msg._method = "INVITE";
  req = parse_msg(msg.get_request());
  as_tsx = (LocalRoamingAppServerTsx*)as->get_app_tsx(_helper, req);
  EXPECT_TRUE(as_tsx != NULL);
  delete as_tsx; as_tsx = NULL;

  // Try with a valid method (SUBSCRIBE). This creates the application server
  // transaction.
  msg._method = "SUBSCRIBE";
  req = parse_msg(msg.get_request());
  as_tsx = (LocalRoamingAppServerTsx*)as->get_app_tsx(_helper, req);
  EXPECT_TRUE(as_tsx != NULL);
  delete as_tsx; as_tsx = NULL;

  delete as; as = NULL;
}

// Test a call that gets forked to the twinned devices. The mobile
// client returns a 480, and the call gets forked again to the mobile
// hosted VoIP clients. Call successfully reaches one of these.
TEST_F(LocalRoamingAppServerTest, ForkMobileVoipClient)
{
  Message msg;
  LocalRoamingAppServerTsx as_tsx(_helper);

  pjsip_route_hdr* hdr = pjsip_rr_hdr_create(stack_data.pool);
  hdr->name_addr.uri = PJUtils::uri_from_string("sip:mobile-twinned@gemini.homedomain;twin-prefix=111", stack_data.pool);
  pjsip_msg* req = parse_msg(msg.get_request());
  pjsip_msg* mobile = parse_msg(msg.get_request());
  {
    // Use a sequence to ensure this happens in order.
    InSequence seq;
    EXPECT_CALL(*_helper, route_hdr()).WillOnce(Return(hdr));
    EXPECT_CALL(*_helper, clone_request(req))
      .WillOnce(Return(mobile));
    EXPECT_CALL(*_helper, get_pool(req))
      .WillOnce(Return(stack_data.pool));
    EXPECT_CALL(*_helper, get_pool(mobile))
      .WillOnce(Return(stack_data.pool));
    EXPECT_CALL(*_helper, send_request(req));
    EXPECT_CALL(*_helper, send_request(mobile))
      .WillOnce(Return(MOBILE_FORK_ID));
  }
  as_tsx.on_initial_request(req);

  EXPECT_THAT(req, ReqUriEquals("sip:6505551234@homedomain"));
  pjsip_reject_contact_hdr* reject_contact_hdr =
    (pjsip_reject_contact_hdr*)pjsip_msg_find_hdr_by_name(req, &STR_REJECT_CONTACT, NULL);
  std::unordered_map<std::string, std::string> expected_params;
  expected_params[PJUtils::pj_str_to_string(&STR_PHONE)] = "";
  EXPECT_TRUE(check_params(&reject_contact_hdr->feature_set, expected_params));
  EXPECT_THAT(mobile, ReqUriEquals("sip:1116505551234@homedomain"));

  msg._status = "480 Temporarily Unavailable";
  pjsip_msg* rsp = parse_msg(msg.get_response());
  EXPECT_CALL(*_helper, original_request())
    .WillOnce(Return(req));
  EXPECT_CALL(*_helper, get_pool(req))
    .WillOnce(Return(stack_data.pool));
  EXPECT_CALL(*_helper, free_msg(rsp));
  EXPECT_CALL(*_helper, send_request(req)).
    WillOnce(Return(MOBILE_VOIP_FORK_ID));
  as_tsx.on_response(rsp, MOBILE_FORK_ID);

  EXPECT_THAT(req, ReqUriEquals("sip:6505551234@homedomain"));
  pjsip_accept_contact_hdr* accept_contact_hdr =
    (pjsip_accept_contact_hdr*)pjsip_msg_find_hdr_by_name(req, &STR_ACCEPT_CONTACT, NULL);
  EXPECT_TRUE(check_params(&accept_contact_hdr->feature_set, expected_params));

  msg._status = "200 OK";
  rsp = parse_msg(msg.get_response());
  EXPECT_CALL(*_helper, send_response(rsp));
  as_tsx.on_response(rsp, MOBILE_VOIP_FORK_ID);
}

// Test a subscribe that is forked to twinned devices. One of the
// devices returns a 480, but the second device successfully
// receives the SUBSCRIBE.
TEST_F(LocalRoamingAppServerTest, ForkSubscribe)
{
  Message msg;
  msg._method = "SUBSCRIBE";
  LocalRoamingAppServerTsx as_tsx(_helper);

  pjsip_route_hdr* hdr = pjsip_rr_hdr_create(stack_data.pool);
  hdr->name_addr.uri = PJUtils::uri_from_string("sip:mobile-twinned@gemini.homedomain;twin-prefix=111", stack_data.pool);
  pjsip_msg* req = parse_msg(msg.get_request());
  pjsip_msg* mobile = parse_msg(msg.get_request());
  {
    // Use a sequence to ensure this happens in order.
    InSequence seq;
    EXPECT_CALL(*_helper, route_hdr()).WillOnce(Return(hdr));
    EXPECT_CALL(*_helper, clone_request(req))
      .WillOnce(Return(mobile));
    EXPECT_CALL(*_helper, get_pool(mobile))
      .WillOnce(Return(stack_data.pool));
    EXPECT_CALL(*_helper, send_request(req));
    EXPECT_CALL(*_helper, send_request(mobile))
      .WillOnce(Return(MOBILE_FORK_ID));
  }
  as_tsx.on_initial_request(req);

  EXPECT_THAT(req, ReqUriEquals("sip:6505551234@homedomain"));
  pjsip_reject_contact_hdr* reject_contact_hdr =
    (pjsip_reject_contact_hdr*)pjsip_msg_find_hdr_by_name(req, &STR_REJECT_CONTACT, NULL);
  EXPECT_TRUE(reject_contact_hdr == NULL);
  EXPECT_THAT(mobile, ReqUriEquals("sip:1116505551234@homedomain"));

  msg._status = "480 Temporarily Unavailable";
  pjsip_msg* rsp = parse_msg(msg.get_response());

  EXPECT_CALL(*_helper, send_response(rsp));
  as_tsx.on_response(rsp, MOBILE_FORK_ID);

  msg._status = "200 OK";
  rsp = parse_msg(msg.get_response());
  EXPECT_CALL(*_helper, send_response(rsp));
  as_tsx.on_response(rsp, VOIP_FORK_ID);
}

// Test with no gemini route header. Call is rejected with a 480.
TEST_F(LocalRoamingAppServerTest, NoRouteHeader)
{
  Message msg;
  LocalRoamingAppServerTsx as_tsx(_helper);

  const pjsip_route_hdr* hdr = NULL;
  pjsip_msg* req = parse_msg(msg.get_request());
  msg._status = "480 Temporarily Unavailable";
  pjsip_msg* rsp = parse_msg(msg.get_response());
  {
    InSequence seq;
    EXPECT_CALL(*_helper, route_hdr()).WillOnce(Return(hdr));
    EXPECT_CALL(*_helper, create_response(req, PJSIP_SC_TEMPORARILY_UNAVAILABLE, ""))
      .WillOnce(Return(rsp));
    EXPECT_CALL(*_helper, send_response(rsp));
    EXPECT_CALL(*_helper, free_msg(req));
  }
  as_tsx.on_initial_request(req);
}

// Test with no twin-prefix on the gemini route header.
// Call is rejected with a 480.
TEST_F(LocalRoamingAppServerTest, NoTwinPrefix)
{
  Message msg;
  LocalRoamingAppServerTsx as_tsx(_helper);
  pjsip_msg* req = parse_msg(msg.get_request());
  pjsip_route_hdr* hdr = pjsip_rr_hdr_create(stack_data.pool);
  hdr->name_addr.uri = PJUtils::uri_from_string("sip:mobile-twinned@gemini.homedomain", stack_data.pool);

  pjsip_msg* rsp = parse_msg(msg.get_response());
  {
    InSequence seq;
    EXPECT_CALL(*_helper, route_hdr()).WillOnce(Return(hdr));
    EXPECT_CALL(*_helper, create_response(req, PJSIP_SC_TEMPORARILY_UNAVAILABLE, ""))
      .WillOnce(Return(rsp));
    EXPECT_CALL(*_helper, send_response(rsp));
    EXPECT_CALL(*_helper, free_msg(req));
  }
  as_tsx.on_initial_request(req);
}

// Test with an empty twin-prefix on the gemini route header.
// Call is rejected with a 480.
TEST_F(LocalRoamingAppServerTest, EmptyTwinPrefix)
{
  Message msg;
  LocalRoamingAppServerTsx as_tsx(_helper);
  pjsip_msg* req = parse_msg(msg.get_request());
  pjsip_route_hdr* hdr = pjsip_rr_hdr_create(stack_data.pool);
  hdr->name_addr.uri = PJUtils::uri_from_string("sip:mobile-twinned@gemini.homedomain;twin-prefix=", stack_data.pool);
  pjsip_msg* rsp = parse_msg(msg.get_response());
  {
    InSequence seq;
    EXPECT_CALL(*_helper, route_hdr()).WillOnce(Return(hdr));
    EXPECT_CALL(*_helper, create_response(req, PJSIP_SC_TEMPORARILY_UNAVAILABLE, ""))
      .WillOnce(Return(rsp));
    EXPECT_CALL(*_helper, send_response(rsp));
    EXPECT_CALL(*_helper, free_msg(req));
  }
  as_tsx.on_initial_request(req);
}

// Test with a non SIP URI. Call is rejected with a 480.
TEST_F(LocalRoamingAppServerTest, NoSIPURI)
{
  Message msg;
  msg._toscheme = "tel";
  msg._todomain = "";

  LocalRoamingAppServerTsx as_tsx(_helper);
  pjsip_msg* req = parse_msg(msg.get_request());
  pjsip_msg* mobile = parse_msg(msg.get_request());
  pjsip_route_hdr* hdr = pjsip_rr_hdr_create(stack_data.pool);
  hdr->name_addr.uri = PJUtils::uri_from_string("sip:mobile-twinned@gemini.homedomain;twin-prefix=111", stack_data.pool);
  pjsip_msg* rsp = parse_msg(msg.get_response());
  {
    InSequence seq;
    EXPECT_CALL(*_helper, route_hdr()).WillOnce(Return(hdr));
    EXPECT_CALL(*_helper, clone_request(req))
      .WillOnce(Return(mobile));
    EXPECT_CALL(*_helper, get_pool(req))
      .WillOnce(Return(stack_data.pool));
    EXPECT_CALL(*_helper, create_response(req, PJSIP_SC_TEMPORARILY_UNAVAILABLE, ""))
      .WillOnce(Return(rsp));
    EXPECT_CALL(*_helper, send_response(rsp));
    EXPECT_CALL(*_helper, free_msg(req));
    EXPECT_CALL(*_helper, free_msg(mobile));
  }
  as_tsx.on_initial_request(req);
}

// Tests forking where the mobile device rejects the call, but
// not with a 480.
TEST_F(LocalRoamingAppServerTest, ForkMobileRejectsNot480)
{
  Message msg;
  LocalRoamingAppServerTsx as_tsx(_helper);

  pjsip_route_hdr* hdr = pjsip_rr_hdr_create(stack_data.pool);
  hdr->name_addr.uri = PJUtils::uri_from_string("sip:mobile-twinned@gemini.homedomain;twin-prefix=111", stack_data.pool);

  pjsip_msg* req = parse_msg(msg.get_request());
  pjsip_msg* mobile = parse_msg(msg.get_request());
  {
    // Use a sequence to ensure this happens in order.
    InSequence seq;
    EXPECT_CALL(*_helper, route_hdr()).WillOnce(Return(hdr));
    EXPECT_CALL(*_helper, clone_request(req))
      .WillOnce(Return(mobile));
    EXPECT_CALL(*_helper, get_pool(req))
      .WillOnce(Return(stack_data.pool));
    EXPECT_CALL(*_helper, get_pool(mobile))
      .WillOnce(Return(stack_data.pool));
    EXPECT_CALL(*_helper, send_request(req));
    EXPECT_CALL(*_helper, send_request(mobile))
      .WillOnce(Return(MOBILE_FORK_ID));
  }
  as_tsx.on_initial_request(req);

  msg._status = "408 Request Timeout";
  pjsip_msg* rsp = parse_msg(msg.get_response());
  EXPECT_CALL(*_helper, send_response(rsp));
  as_tsx.on_response(rsp, MOBILE_FORK_ID);

  msg._status = "200 OK";
  rsp = parse_msg(msg.get_response());
  EXPECT_CALL(*_helper, send_response(rsp));
  as_tsx.on_response(rsp, VOIP_FORK_ID);
}
