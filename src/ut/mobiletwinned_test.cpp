/**
 * @file mobiletwinned_test.cpp UT fixture for the mobile twinned AS which is
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
#include "mobiletwinned.h"
#include "stack.h"
#include "pjutils.h"
#include "custom_headers.h"
#include "constants.h"
#include "gemini_constants.h"

using namespace std;
using testing::InSequence;
using testing::Return;

/// Fixture for MobileTwinnedAppServerTest.
///
/// This derives from SipTest to ensure PJSIP is set up correctly, but doesn't
/// actually use most of its function (and doesn't register a module).
class MobileTwinnedAppServerTest : public SipTest
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

  MobileTwinnedAppServerTest() : SipTest(NULL)
  {
  }

  ~MobileTwinnedAppServerTest()
  {
  }

  // Test a call that gets forked to a VoIP client and the native device
  void test_with_two_forks(std::string method,
                           std::string status,
                           bool retry,
                           std::string extra = "");

  // Test a call that gets sent to a single VoIP client
  void test_with_gr(std::string method,
                    std::string status);

  // Test a call that gets sent to the native device
  void test_with_g_3gpp_ics(std::string method,
                            std::string status,
                            std::string twin_prefix = "111");

  // Check the list of expected_params are found in the list of actual_params.
  // The actual parameters are a vector containing the list of pjsip_params from each header
  bool check_params_multiple_headers(std::vector<pjsip_param*> actual_params,
                                     std::unordered_map<std::string, std::string>& expected_params)
  {
    for (std::unordered_map<std::string, std::string>::const_iterator exp = expected_params.begin();
         exp != expected_params.end();
         ++exp)
    {
      bool found_parameter = false;

      for (std::vector<pjsip_param*>::iterator act = actual_params.begin();
           ((act != actual_params.end()) && (!found_parameter));
           ++act)
      {
        pj_str_t parameter_name = pj_str((char*)exp->first.c_str());
        pjsip_param* param = pjsip_param_find(*act, &parameter_name);
        found_parameter = ((param != NULL) && (PJUtils::pj_str_to_string(&param->value) == exp->second));
      }

      if (!found_parameter)
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
MockAppServerTsxHelper* MobileTwinnedAppServerTest::_helper = NULL;

const int MobileTwinnedAppServerTest::VOIP_FORK_ID = 11111;
const int MobileTwinnedAppServerTest::MOBILE_FORK_ID = 11112;
const int MobileTwinnedAppServerTest::MOBILE_VOIP_FORK_ID = 11113;

namespace MobileTwinnedAS
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
  string _parameters;
  string _extra;

  Message() :
    _method("INVITE"),
    _toscheme("sip"),
    _status("200 OK"),
    _from("6505551000"),
    _fromdomain("homedomain"),
    _to("6505551234"),
    _todomain("homedomain"),
    _route(""),
    _parameters(""),
    _extra("")
  {
  }

  string get_request();
  string get_response();
};
}

string MobileTwinnedAS::Message::get_request()
{
  char buf[16384];

  // The remote target.
  string target = string(_toscheme).append(":").append(_to);

  if (!_todomain.empty())
  {
    target.append("@").append(_todomain);
  }

  if (!_parameters.empty())
  {
    target.append(_parameters);
  }

  int n = snprintf(buf, sizeof(buf),
                   "%1$s %4$s SIP/2.0\r\n"
                   "Via: SIP/2.0/TCP 10.114.61.213;branch=z9hG4bK0123456789abcdef\r\n"
                   "From: <sip:%2$s@%3$s>;tag=10.114.61.213+1+8c8b232a+5fb751cf\r\n"
                   "To: <%4$s>\r\n"
                   "%5$s"
                   "%6$s"
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
                   /*  5 */ _route.empty() ? "" : string(_route).append("\r\n").c_str(),
                   /*  6 */ _extra.empty() ? "" : string(_extra).append("\r\n").c_str()
    );

  EXPECT_LT(n, (int)sizeof(buf));

  string ret(buf, n);
  return ret;
}

string MobileTwinnedAS::Message::get_response()
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

using MobileTwinnedAS::Message;

/// Compares a pjsip_msg's request URI with a std::string.
MATCHER_P(ReqUriEquals, uri, "")
{
  std::string arg_uri = PJUtils::uri_to_string(PJSIP_URI_IN_REQ_URI, arg->line.req.uri);
  TRC_DEBUG("arg_uri %s", arg_uri.c_str());
  return arg_uri == uri;
}

void MobileTwinnedAppServerTest::test_with_two_forks(std::string method,
                                                     std::string status,
                                                     bool retry,
                                                     std::string extra)
{
  Message msg;
  msg._method = method;
  msg._extra = extra;
  MobileTwinnedAppServerTsx as_tsx;
  as_tsx.set_helper(_helper);

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

  // Extract all the Reject-Contact headers.
  std::vector<pjsip_param*> reject_params;
  pjsip_reject_contact_hdr* reject_header =
   (pjsip_reject_contact_hdr*)pjsip_msg_find_hdr_by_name(req,
                                                         &STR_REJECT_CONTACT,
                                                         NULL);
  while (reject_header != NULL)
  {
    reject_params.push_back(&reject_header->feature_set);
    reject_header = (pjsip_reject_contact_hdr*)pjsip_msg_find_hdr_by_name(req,
                                                                          &STR_REJECT_CONTACT,
                                                                          reject_header->next);
  }

  std::unordered_map<std::string, std::string> expected_reject_params;
  expected_reject_params["+sip.with-twin"] = "";
  expected_reject_params[PJUtils::pj_str_to_string(&STR_3GPP_ICS)] = "\"server,principal\"";
  EXPECT_TRUE(check_params_multiple_headers(reject_params, expected_reject_params));
  EXPECT_THAT(mobile, ReqUriEquals("sip:1116505551234@homedomain"));

  std::vector<pjsip_param*> accept_params;

  // Extract all the Accept-Contact headers.
  pjsip_accept_contact_hdr* accept_header =
   (pjsip_accept_contact_hdr*)pjsip_msg_find_hdr_by_name(mobile,
                                                         &STR_ACCEPT_CONTACT,
                                                         NULL);
  while (accept_header != NULL)
  {
    accept_params.push_back(&accept_header->feature_set);
    if (pjsip_param_find(&accept_header->feature_set, &STR_3GPP_ICS) != NULL)
    {
      // The Accept-Contact headers we add should have ";explicit;require" set
      EXPECT_TRUE(accept_header->explicit_match);
      EXPECT_TRUE(accept_header->required_match);
    }
    accept_header = (pjsip_accept_contact_hdr*)pjsip_msg_find_hdr_by_name(mobile,
                                                                          &STR_ACCEPT_CONTACT,
                                                                          accept_header->next);
  }

  std::unordered_map<std::string, std::string> expected_accept_params;
  expected_accept_params[PJUtils::pj_str_to_string(&STR_3GPP_ICS)] = "\"server,principal\"";
  EXPECT_TRUE(check_params_multiple_headers(accept_params, expected_accept_params));

  // Extract all the Reject-Contact headers.
  reject_params.clear();
  reject_header = (pjsip_reject_contact_hdr*)pjsip_msg_find_hdr_by_name(mobile,
                                                                        &STR_REJECT_CONTACT,
                                                                        NULL);
  while (reject_header != NULL)
  {
    reject_params.push_back(&reject_header->feature_set);
    reject_header = (pjsip_reject_contact_hdr*)pjsip_msg_find_hdr_by_name(mobile,
                                                                          &STR_REJECT_CONTACT,
                                                                          reject_header->next);
  }

  expected_reject_params.clear();
  expected_reject_params["+sip.with-twin"] = "";
  EXPECT_TRUE(check_params_multiple_headers(reject_params, expected_reject_params));

  msg._status = status;
  pjsip_msg* rsp = parse_msg(msg.get_response());

  if (retry)
  {
    {
      // Use a sequence to ensure this happens in order.
      InSequence seq;
      EXPECT_CALL(*_helper, original_request())
        .WillOnce(Return(req));
      EXPECT_CALL(*_helper, get_pool(req))
        .WillOnce(Return(stack_data.pool));
      EXPECT_CALL(*_helper, send_request(req)).
       WillOnce(Return(MOBILE_VOIP_FORK_ID));
      EXPECT_CALL(*_helper, free_msg(rsp));
    }
    as_tsx.on_response(rsp, MOBILE_FORK_ID);

    EXPECT_THAT(req, ReqUriEquals("sip:6505551234@homedomain"));

    // Extract all the Accept-Contact headers.
    accept_params.clear();
    accept_header =
     (pjsip_accept_contact_hdr*)pjsip_msg_find_hdr_by_name(req,
                                                           &STR_ACCEPT_CONTACT,
                                                           NULL);
    while (accept_header != NULL)
    {
      accept_params.push_back(&accept_header->feature_set);
      if (pjsip_param_find(&accept_header->feature_set, &STR_WITH_TWIN) != NULL)
      {
        // The Accept-Contact headers we add should have ";explicit;require" set
        EXPECT_TRUE(accept_header->explicit_match);
        EXPECT_TRUE(accept_header->required_match);
      }
      accept_header = (pjsip_accept_contact_hdr*)pjsip_msg_find_hdr_by_name(req,
                                                                            &STR_ACCEPT_CONTACT,
                                                                            accept_header->next);
    }

    std::unordered_map<std::string, std::string> expected_second_attempt_params;
    expected_second_attempt_params["+sip.with-twin"] = "";
    EXPECT_TRUE(check_params_multiple_headers(accept_params, expected_second_attempt_params));

    msg._status = "200 OK";
    rsp = parse_msg(msg.get_response());
    EXPECT_CALL(*_helper, send_response(rsp));
    as_tsx.on_response(rsp, MOBILE_VOIP_FORK_ID);
  }
  else
  {
    EXPECT_CALL(*_helper, send_response(rsp));
    as_tsx.on_response(rsp, MOBILE_FORK_ID);
    EXPECT_CALL(*_helper, send_response(rsp));
    as_tsx.on_response(rsp, VOIP_FORK_ID);
  }
}

void MobileTwinnedAppServerTest::test_with_gr(std::string method,
                                              std::string status)
{
  Message msg;
  msg._method = method;
  msg._parameters = ";gr=hello";
  MobileTwinnedAppServerTsx as_tsx;
  as_tsx.set_helper(_helper);

  pjsip_msg* req = parse_msg(msg.get_request());
  {
    // Use a sequence to ensure this happens in order.
    InSequence seq;
    EXPECT_CALL(*_helper, send_request(req));
  }
  as_tsx.on_initial_request(req);

  msg._status = status;
  pjsip_msg* rsp = parse_msg(msg.get_response());
  EXPECT_CALL(*_helper, send_response(rsp));
  as_tsx.on_response(rsp, VOIP_FORK_ID);
}

void MobileTwinnedAppServerTest::test_with_g_3gpp_ics(std::string method,
                                                      std::string status,
                                                      std::string twin_prefix)
{
  Message msg;
  msg._method = method;
  msg._extra = "Accept-Contact: *;audio\r\nAccept-Contact: *;+g.3gpp.ics=\"server,principal\"";
  MobileTwinnedAppServerTsx as_tsx;
  as_tsx.set_helper(_helper);

  pjsip_route_hdr* hdr = pjsip_rr_hdr_create(stack_data.pool);
  hdr->name_addr.uri = PJUtils::uri_from_string("sip:mobile-twinned@gemini.homedomain;twin-prefix=" + twin_prefix, stack_data.pool);
  pjsip_msg* req = parse_msg(msg.get_request());
  {
    // Use a sequence to ensure this happens in order.
    InSequence seq;
    EXPECT_CALL(*_helper, route_hdr()).WillOnce(Return(hdr));
    EXPECT_CALL(*_helper, get_pool(req))
       .WillOnce(Return(stack_data.pool));
    EXPECT_CALL(*_helper, send_request(req))
      .WillOnce(Return(MOBILE_FORK_ID));
  }
  as_tsx.on_initial_request(req);

  EXPECT_THAT(req, ReqUriEquals("sip:" + twin_prefix + "6505551234@homedomain"));
  std::vector<pjsip_param*> accept_params;

  // Extract all the Accept-Contact headers.
  pjsip_accept_contact_hdr* accept_header =
   (pjsip_accept_contact_hdr*)pjsip_msg_find_hdr_by_name(req,
                                                         &STR_ACCEPT_CONTACT,
                                                         NULL);
  while (accept_header != NULL)
  {
    accept_params.push_back(&accept_header->feature_set);
    accept_header = (pjsip_accept_contact_hdr*)pjsip_msg_find_hdr_by_name(req,
                                                                          &STR_ACCEPT_CONTACT,
                                                                          accept_header->next);
  }

  std::unordered_map<std::string, std::string> expected_accept_params;
  expected_accept_params[PJUtils::pj_str_to_string(&STR_3GPP_ICS)] = "\"server,principal\"";
  expected_accept_params["audio"] = "";
  EXPECT_TRUE(check_params_multiple_headers(accept_params, expected_accept_params));

  msg._status = status;
  pjsip_msg* rsp = parse_msg(msg.get_response());
  EXPECT_CALL(*_helper, send_response(rsp));
  as_tsx.on_response(rsp, MOBILE_FORK_ID);
}

// Test creation and destruction of the MobileTwinnedAppServer objects.
TEST_F(MobileTwinnedAppServerTest, CreateMobileTwinnedAppServer)
{
  // Create a MobileTwinnedAppServer object
  MobileTwinnedAppServer* as = new MobileTwinnedAppServer("gemini");

  // Test creating an app server transaction with an invalid method -
  // it shouldn't be created.
  Message msg;
  pjsip_sip_uri* uri = NULL;
  msg._method = "OPTIONS";
  pjsip_msg* req = parse_msg(msg.get_request());
  MobileTwinnedAppServerTsx* as_tsx = (MobileTwinnedAppServerTsx*)as->get_app_tsx(NULL, req, uri, NULL, 0);
  EXPECT_TRUE(as_tsx == NULL);

  // Try with a valid method (INVITE). This creates the application server
  // transaction.
  msg._method = "INVITE";
  req = parse_msg(msg.get_request());
  as_tsx = (MobileTwinnedAppServerTsx*)as->get_app_tsx(NULL, req, uri, NULL, 0);
  EXPECT_TRUE(as_tsx != NULL);
  delete as_tsx; as_tsx = NULL;

  // Try with a valid method (SUBSCRIBE). This creates the application server
  // transaction.
  msg._method = "SUBSCRIBE";
  req = parse_msg(msg.get_request());
  as_tsx = (MobileTwinnedAppServerTsx*)as->get_app_tsx(NULL, req, uri, NULL, 0);
  EXPECT_TRUE(as_tsx != NULL);
  delete as_tsx; as_tsx = NULL;

  delete as; as = NULL;
}

// Test a call that gets forked to the twinned devices. The mobile
// client returns a 480, and the call gets forked again to the mobile
// hosted VoIP clients. Call successfully reaches one of these.
TEST_F(MobileTwinnedAppServerTest, ForkMobileVoipClient)
{
  test_with_two_forks("INVITE", "480 Temporarily Unavailable", true);
}

// Tests forking where the mobile device rejects the call, but
// not with a 480.
TEST_F(MobileTwinnedAppServerTest, ForkMobileRejectsNot480)
{
  test_with_two_forks("INVITE", "486 Busy Here", false);
}

// Tests forking where the devices accept the call.
TEST_F(MobileTwinnedAppServerTest, ForkInviteSuccess)
{
  test_with_two_forks("INVITE", "200 OK", false);
}

// Test a subscribe that is forked to twinned devices. The mobile
// device returns a 480 but this doesn't trigger a retry
TEST_F(MobileTwinnedAppServerTest, ForkSubscribeMobile480)
{
  test_with_two_forks("SUBSCRIBE", "480 Temporarily Unavailable", false);
}

// Test forking where the devices accept the subscribe.
TEST_F(MobileTwinnedAppServerTest, ForkSubscribeSuccess)
{
  test_with_two_forks("SUBSCRIBE", "200 OK", false);
}

// Test a subscribe where the devices reject the subscribe
TEST_F(MobileTwinnedAppServerTest, ForkSubscribeBothReject)
{
  test_with_two_forks("SUBSCRIBE", "486 Busy Here", false);
}

// Test an INVITE that has Accept-Contact headers that don't include
// g.3gpp.ics
TEST_F(MobileTwinnedAppServerTest, ForkInviteWithExtraAcceptContact)
{
  test_with_two_forks("INVITE", "200 OK", false, "Accept-Contact: *;audio\r\nAccept-Contact: *;extra;extra2");
}

// Tests where a successful INVITE is targeted at a specific client
TEST_F(MobileTwinnedAppServerTest, SuccessfulInviteWithGR)
{
  test_with_gr("INVITE", "200 OK");
}

// Tests where a unsuccessful INVITE is targeted at a specific client.
// The call isn't retried.
TEST_F(MobileTwinnedAppServerTest, UnsuccessfulInviteWithGR)
{
  test_with_gr("INVITE", "480 Temporarily Unavailable");
}

// Tests where a successful SUBSCRIBE is targeted at a specific client
TEST_F(MobileTwinnedAppServerTest, SuccessfulSubscribeWithGR)
{
  test_with_gr("SUBSCRIBE", "200 OK");
}

// Tests where a unsuccessful SUBSCRIBE is targeted at a specific client.
// The call isn't retried.
TEST_F(MobileTwinnedAppServerTest, UnsuccessfulSubscribeWithGR)
{
  test_with_gr("SUBSCRIBE", "480 Temporarily Unavailable");
}

// Tests where a successful INVITE is targeted at the native device
TEST_F(MobileTwinnedAppServerTest, SuccessfulInviteWithAcceptContact)
{
  test_with_g_3gpp_ics("INVITE", "200 OK");
}

// Tests where a unsuccessful INVITE is targeted at the native device.
// The call isn't retried.
TEST_F(MobileTwinnedAppServerTest, UnsuccessfulInviteWithAcceptContact)
{
  test_with_g_3gpp_ics("INVITE", "480 Temporarily Unavailable");
}

// Tests where a successful SUBSCRIBE is targeted at the native device
TEST_F(MobileTwinnedAppServerTest, SuccessfulSubscribeWithAcceptContact)
{
  test_with_g_3gpp_ics("SUBSCRIBE", "200 OK");
}

// Tests where a unsuccessful SUBSCRIBE is targeted at the native device.
// The call isn't retried.
TEST_F(MobileTwinnedAppServerTest, UnsuccessfulSubscribeWithAcceptContact)
{
  test_with_g_3gpp_ics("SUBSCRIBE", "480 Temporarily Unavailable");
}

// Tests where a successful INVITE is targeted at the native device with no twin-prefix
TEST_F(MobileTwinnedAppServerTest, SuccessfulInviteWithAcceptContactNoTwinPrefix)
{
  test_with_g_3gpp_ics("INVITE", "200 OK", "");
}

// Test with a non SIP URI. Call is rejected with a 480.
TEST_F(MobileTwinnedAppServerTest, NoSIPURI)
{
  Message msg;
  msg._toscheme = "tel";
  msg._todomain = "";

  MobileTwinnedAppServerTsx as_tsx;
  as_tsx.set_helper(_helper);
  pjsip_msg* req = parse_msg(msg.get_request());
  pjsip_msg* rsp = parse_msg(msg.get_response());
  {
    InSequence seq;
    EXPECT_CALL(*_helper, create_response(req, PJSIP_SC_TEMPORARILY_UNAVAILABLE, ""))
      .WillOnce(Return(rsp));
    EXPECT_CALL(*_helper, send_response(rsp));
    EXPECT_CALL(*_helper, free_msg(req));
  }
  as_tsx.on_initial_request(req);
}
