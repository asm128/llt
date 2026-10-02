#include "llc_system_event.h"

#ifndef LLS_L_H_23627
#define LLS_L_H_23627

namespace lls
{
	stxp	::llc::u1_c		PROTOCOL_VERSION		= 1;
	stxp	::llc::u2_c		FRAME_MAGIC				= 0x31534C4C; // "LLS1" in little-endian order.
	stxp	::llc::u2_c		FRAME_SIZE_MAX			= 0x01000000;
	stxp	const char		PIPE_NAME				[] = "\\\\.\\pipe\\lls.v1";

	GDEFINE_ENUM_TYPE (LLS_COMMAND, llc::u0_t);
	GDEFINE_ENUM_VALUED(LLS_COMMAND, ping		, 0, "Check that lls is reachable and optionally echo text.");
	GDEFINE_ENUM_VALUED(LLS_COMMAND, status		, 1, "Show protocol, process, uptime, and request count."	);
	GDEFINE_ENUM_VALUED(LLS_COMMAND, shutdown	, 2, "Ask lls to stop cleanly."								);
	GDEFINE_ENUM_VALUED(LLS_COMMAND, MAX		, 3, "Number of enumerated commands."						);

	GDEFINE_ENUM_TYPE (LLS_RESULT, llc::u0_t);
	GDEFINE_ENUM_VALUE(LLS_RESULT, ok				, 0);
	GDEFINE_ENUM_VALUE(LLS_RESULT, pong				, 1);
	GDEFINE_ENUM_VALUE(LLS_RESULT, status			, 2);
	GDEFINE_ENUM_VALUE(LLS_RESULT, shutting_down	, 3);
	GDEFINE_ENUM_VALUE(LLS_RESULT, invalid_request	, 4);
	GDEFINE_ENUM_VALUE(LLS_RESULT, error			, 5);
	GDEFINE_ENUM_VALUE(LLS_RESULT, MAX				, 6);

	tydf	::llc::SEView<LLS_COMMAND>	SEViewCommand;
	tydf	::llc::SEView<LLS_RESULT>	SEViewResult;
	tydf	void*						pipe_t;

#pragma pack(push, 1)
	stct SFrameHeader {
		::llc::u2_t	Magic				= FRAME_MAGIC;
		::llc::u1_t	Version				= PROTOCOL_VERSION;
		::llc::u1_t	Reserved			= {};
		::llc::u2_t	PayloadBytes		= {};
	};

	stct SServiceStatus {
		::llc::u1_t	ProtocolVersion		= PROTOCOL_VERSION;
		::llc::u1_t	Reserved			= {};
		::llc::u2_t	ProcessId			= {};
		::llc::u3_t	UptimeMilliseconds	= {};
		::llc::u3_t	RequestCount		= {};
	};
#pragma pack(pop)

	static_assert(12 == sizeof(SFrameHeader));
	static_assert(24 == sizeof(SServiceStatus));

			::llc::err_t	eventMakeCommand	(::llc::SEventSystem & output, LLS_COMMAND type, ::llc::vcu0_t payload = {});
			::llc::err_t	eventMakeResult		(::llc::SEventSystem & output, LLS_RESULT type, ::llc::vcu0_t payload = {});
	stin	::llc::err_t	eventMakeResult		(::llc::SEventSystem & output, LLS_RESULT type, ::llc::vcst_t payload)	{ return eventMakeResult(output, type, payload.cu8()); }
			::llc::err_t	eventExtractCommand	(const ::llc::SEventSystem & input, SEViewCommand & output);
			::llc::err_t	eventExtractResult	(const ::llc::SEventSystem & input, SEViewResult & output);
			::llc::err_t	eventSerialize		(const ::llc::SEventSystem & input, ::llc::au0_t & output);
			::llc::err_t	eventDeserialize	(::llc::vcu0_t input, ::llc::SEventSystem & output);

	::llc::err_t	pipeServerCreate		(pipe_t & output);
	::llc::err_t	pipeServerWait		(pipe_t pipe);
	::llc::err_t	pipeServerDisconnect	(pipe_t pipe);
	::llc::err_t	pipeClientConnect	(pipe_t & output, ::llc::u2_t timeoutMilliseconds = 2000);
	::llc::err_t	pipeClose			(pipe_t & pipe);
	::llc::err_t	pipeWriteEvent		(pipe_t pipe, const ::llc::SEventSystem & eventToWrite);
	::llc::err_t	pipeReadEvent		(pipe_t pipe, ::llc::SEventSystem & eventToRead);
	::llc::err_t	pipeRequest			(const ::llc::SEventSystem & request, ::llc::SEventSystem & response, ::llc::u2_t timeoutMilliseconds = 2000);
}

#endif // LLS_L_H_23627
