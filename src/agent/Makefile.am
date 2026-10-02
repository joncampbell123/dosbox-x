AM_CPPFLAGS = -I$(top_srcdir)/include

noinst_LIBRARIES = libagent.a

libagent_a_SOURCES = \
	protocol/agent_protocol.cpp \
	bridge/agent_bridge.cpp \
	debugger/debugger_adapter.cpp \
	parser/debugger_output_parser.cpp \
	server/agent_server.cpp \
	trace/trace_store.cpp \
	transport/local_transport.cpp \
	transport/rpc_transport.h
