# Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter

AM_CPPFLAGS = -I$(srcdir)
AM_CXXFLAGS = -std=c++17

bin_PROGRAMS = agent86-mcp
noinst_PROGRAMS = agent86-plugin-stage
check_PROGRAMS = agent86-native-tests
TESTS = agent86-native-tests

agent86_mcp_SOURCES = \
	dos_bridge_client.cpp dos_bridge_client.h \
	json.cpp json.h \
	main.cpp \
	mcp_server.cpp mcp_server.h \
	secure_random.cpp secure_random.h \
	session_logger.cpp session_logger.h \
	text_encoding.cpp text_encoding.h
agent86_mcp_LDADD = $(AGENT86_LIBS)

agent86_plugin_stage_SOURCES = \
	plugin_stager.cpp plugin_stager.h \
	stage_main.cpp

agent86_native_tests_SOURCES = \
	../../tests/agent86_native_tests.cpp \
	dos_bridge_client.cpp dos_bridge_client.h \
	json.cpp json.h \
	mcp_server.cpp mcp_server.h \
	secure_random.cpp secure_random.h \
	session_logger.cpp session_logger.h \
	text_encoding.cpp text_encoding.h
agent86_native_tests_LDADD = $(AGENT86_LIBS)

.PHONY: stage-agent86-plugins
stage-agent86-plugins: agent86-mcp agent86-plugin-stage
	$(AM_V_GEN)./agent86-plugin-stage "$(abs_top_srcdir)" "$(abs_top_builddir)/src" "$(abs_builddir)/agent86-mcp$(EXEEXT)"

all-local: stage-agent86-plugins
