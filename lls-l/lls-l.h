#include "llc_system_event.h"

#ifndef LLS_L_H_23627
#define LLS_L_H_23627

namespace lls
{
	stxp	::llc::u1_c		PROTOCOL_VERSION		= 1;
	stxp	::llc::u2_c		FRAME_MAGIC				= 0x31534C4C; // "LLS1" in little-endian order.
	stxp	::llc::u2_c		FRAME_SIZE_MAX			= 0x01000000;
	stxp	const char		PIPE_NAME				[] = "\\\\.\\pipe\\lls.v1";

	GDEFINE_ENUM_TYPE (LLS_COMMAND, uint8_t);
	GDEFINE_ENUM_VALUE(LLS_COMMAND, Ping		, 0);
	GDEFINE_ENUM_VALUE(LLS_COMMAND, Status		, 1);
	GDEFINE_ENUM_VALUE(LLS_COMMAND, Shutdown	, 2);

	GDEFINE_ENUM_TYPE (LLS_RESULT, uint8_t);
	GDEFINE_ENUM_VALUE(LLS_RESULT, Ok			, 0);
	GDEFINE_ENUM_VALUE(LLS_RESULT, Pong			, 1);
	GDEFINE_ENUM_VALUE(LLS_RESULT, Status		, 2);
	GDEFINE_ENUM_VALUE(LLS_RESULT, Shutting_down	, 3);
	GDEFINE_ENUM_VALUE(LLS_RESULT, Invalid_request	, 4);
	GDEFINE_ENUM_VALUE(LLS_RESULT, Error			, 5);

	tydf	::llc::SEView<LLS_COMMAND>	SEViewCommand;
	tydf	::llc::SEView<LLS_RESULT>	SEViewResult;
	tydf	void*						pipe_t;

#pragma pack(push, 1)
	stct SFrameHeader {
		::llc::u2_t	Magic		= FRAME_MAGIC;
		::llc::u1_t	Version		= PROTOCOL_VERSION;
		::llc::u1_t	Reserved	= 0;
		::llc::u2_t	PayloadBytes	= 0;
	};

	stct SServiceStatus {
		::llc::u1_t	ProtocolVersion		= PROTOCOL_VERSION;
		::llc::u1_t	Reserved			= 0;
		::llc::u2_t	ProcessId			= 0;
		::llc::u3_t	UptimeMilliseconds	= 0;
		::llc::u3_t	RequestCount		= 0;
	};
#pragma pack(pop)

	static_assert(12 == sizeof(SFrameHeader));
	static_assert(24 == sizeof(SServiceStatus));

	::llc::err_t	eventMakeCommand		(::llc::SEventSystem & output, LLS_COMMAND type, ::llc::vcu0_t payload = {});
	::llc::err_t	eventMakeResult		(::llc::SEventSystem & output, LLS_RESULT type, ::llc::vcu0_t payload = {});
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
