all: build

TARGETS := gemini-as.so

TEST_TARGETS := gemini-as_test

ROOT := $(abspath $(shell pwd)/../..)
BUILD_DIR := ${ROOT}/plugins/gemini/build

MK_DIR := ${ROOT}/plugins/gemini/mk
PREFIX ?= ${ROOT}/usr
INSTALL_DIR ?= ${PREFIX}
MODULE_DIR := ${ROOT}/plugins/gemini/modules

DEB_COMPONENT := gemini-as
DEB_MAJOR_VERSION ?= 1.0${DEB_VERSION_QUALIFIER}
DEB_NAMES += gemini-as gemini-as-dbg

INCLUDE_DIR := ${INSTALL_DIR}/include
LIB_DIR := ${INSTALL_DIR}/lib

SUBMODULES :=

include $(patsubst %, ${MK_DIR}/%.mk, ${SUBMODULES})

GEMINI_AS_COMMON_SOURCES := mobiletwinned.cpp

gemini-as.so_SOURCES := ${GEMINI_AS_COMMON_SOURCES} \
                        geminiasplugin.cpp \

gemini-as_test_SOURCES := ${GEMINI_AS_COMMON_SOURCES} \
                          accesslogger.cpp \
                          accumulator.cpp \
                          acr.cpp \
                          alarm.cpp \
                          associated_uris.cpp \
                          a_record_resolver.cpp \
                          base64.cpp \
                          base_communication_monitor.cpp \
                          baseresolver.cpp \
                          communicationmonitor.cpp \
                          connection_tracker.cpp \
                          counter.cpp \
                          custom_headers.cpp \
                          curl_interposer.cpp \
                          dnscachedresolver.cpp \
                          static_dns_cache.cpp \
                          dnsparser.cpp \
                          exception_handler.cpp \
                          fakecurl.cpp \
                          faketransport_tcp.cpp \
                          health_checker.cpp \
                          httpclient.cpp \
                          http_request.cpp \
                          http_connection_pool.cpp \
                          httpstack.cpp \
                          load_monitor.cpp \
                          log.cpp \
                          logger.cpp \
                          mobiletwinned_test.cpp \
                          mock_sas.cpp \
                          namespace_hop.cpp \
                          pjutils.cpp \
                          pthread_cond_var_helper.cpp \
                          quiescing_manager.cpp \
                          saslogger.cpp \
                          sip_common.cpp \
                          sipresolver.cpp \
                          stack.cpp \
                          statistic.cpp \
                          test_interposer.cpp \
                          test_main.cpp \
                          thread_dispatcher.cpp \
                          unique.cpp \
                          uri_classifier.cpp \
                          utils.cpp \
                          zmq_lvc.cpp

COMMON_CPPFLAGS := -I${ROOT}/include \
                   -I${ROOT}/usr/include \
                   -I${ROOT}/modules/cpp-common/include \
                   -I${ROOT}/modules/clearwater-s4/include \
                   -I${ROOT}/modules/sas-client/include \
                   -I${ROOT}/modules/app-servers/include \
                   -I${ROOT}/modules/rapidjson/include \
                   -I${ROOT}/plugins/gemini/include \
                   -Wno-write-strings \
                   -fPIC

gemini-as.so_CPPFLAGS := ${COMMON_CPPFLAGS}

gemini-as_test_CPPFLAGS := ${COMMON_CPPFLAGS} \
                           -Wno-write-strings \
                           -I${ROOT}/modules/cpp-common/test_utils \
                           -I${ROOT}/modules/app-servers/test \
                           -I${ROOT}/src/ut \
                           `PKG_CONFIG_PATH=${ROOT}/usr/lib/pkgconfig pkg-config --cflags libpjproject` \
                           -DGTEST_USE_OWN_TR1_TUPLE=0

COMMON_LDFLAGS := -L${ROOT}/usr/lib

gemini-as.so_LDFLAGS := ${COMMON_LDFLAGS} \
                        -shared

gemini-as_test_LDFLAGS := ${COMMON_LDFLAGS} \
                          -levent \
                          -levhtp \
                          -lsas \
                          -lcares \
                          -levent_pthreads \
                          -lboost_regex \
                          -lpthread \
                          -ldl \
                          -lboost_system \
                          -lcurl \
                          -lzmq \
                          -lz \
                          $(shell PKG_CONFIG_PATH=${ROOT}/usr/lib/pkgconfig pkg-config --libs libpjproject)

VPATH = ${ROOT}/src:${ROOT}/modules/cpp-common/src:${ROOT}/modules/clearwater-s4/src:${ROOT}/plugins/gemini/src:${ROOT}/plugins/gemini/ut:${ROOT}/modules/cpp-common/test_utils:${ROOT}/src/ut:${ROOT}/modules/sas-client/source

include ${ROOT}/build-infra/cpp.mk

${gemini-as_test_OBJECT_DIR}/test_interposer.so : ${ROOT}/modules/cpp-common/test_utils/test_interposer.cpp ${ROOT}/modules/cpp-common/test_utils/test_interposer.hpp
	$(CXX) $(gemini-as_test_CPPFLAGS) -shared -fPIC -ldl $< -o $@

${gemini-as_test_OBJECT_DIR}/curl_interposer.so : ${ROOT}/modules/cpp-common/test_utils/curl_interposer.cpp ${ROOT}/modules/cpp-common/test_utils/curl_interposer.hpp ${ROOT}/modules/cpp-common/test_utils/fakecurl.cpp ${ROOT}/modules/cpp-common/test_utils/fakecurl.hpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(gemini-as_test_CPPFLAGS) -shared -fPIC -ldl $< -o $@
CLEANS += ${gemini_test_OBJECT_DIR}/curl_interposer.so

build: ${SUBMODULES} gemini-as.so

test: ${SUBMODULES} gemini-as_test

clean: $(patsubst %, %_clean, ${SUBMODULES})
	rm -rf ${ROOT}/plugins/gemini/build

include ${ROOT}/build-infra/cw-deb.mk

.PHONY: deb
deb: all deb-only

.PHONY: all build test clean
