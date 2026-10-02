#include "lls-l.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace
{
	sttc	HANDLE		pipeHandle			(::lls::pipe_t pipe) { return (HANDLE)pipe; }

	sttc	::llc::err_t	pipeWriteBytes		(HANDLE pipe, const void * data, ::llc::u2_t byteCount) {
		::llc::u2_t		bytesWrittenTotal	= 0;
		while(bytesWrittenTotal < byteCount) {
			DWORD		bytesWritten		= 0;
			if_zero_fe(::WriteFile(pipe, (const uint8_t*)data + bytesWrittenTotal, byteCount - bytesWrittenTotal, &bytesWritten, 0));
			if_zero_fe(bytesWritten);
			bytesWrittenTotal		+= bytesWritten;
		}
		return 0;
	}

	sttc	::llc::err_t	pipeReadBytes		(HANDLE pipe, void * data, ::llc::u2_t byteCount) {
		::llc::u2_t		bytesReadTotal		= 0;
		while(bytesReadTotal < byteCount) {
			DWORD		bytesRead			= 0;
			if_zero_fe(::ReadFile(pipe, (uint8_t*)data + bytesReadTotal, byteCount - bytesReadTotal, &bytesRead, 0));
			if_zero_fe(bytesRead);
			bytesReadTotal			+= bytesRead;
		}
		return 0;
	}
}

::llc::err_t	lls::eventMakeCommand		(::llc::SEventSystem & output, LLS_COMMAND type, ::llc::vcu0_t payload) {
	output.Type			= ::llc::SYSTEM_EVENT_Command;
	output.Data.clear();
	return ::llc::eventWrapChild(output, type, payload);
}

::llc::err_t	lls::eventMakeResult		(::llc::SEventSystem & output, LLS_RESULT type, ::llc::vcu0_t payload) {
	output.Type			= ::llc::SYSTEM_EVENT_Runtime;
	output.Data.clear();
	return ::llc::eventWrapChild(output, type, payload);
}

::llc::err_t	lls::eventExtractCommand	(const ::llc::SEventSystem & input, SEViewCommand & output) {
	if_true_ve(::llc::OS_INVALID_PARAMETER, ::llc::SYSTEM_EVENT_Command != input.Type);
	return input.ExtractChild(output);
}

::llc::err_t	lls::eventExtractResult		(const ::llc::SEventSystem & input, SEViewResult & output) {
	if_true_ve(::llc::OS_INVALID_PARAMETER, ::llc::SYSTEM_EVENT_Runtime != input.Type);
	return input.ExtractChild(output);
}

::llc::err_t	lls::eventSerialize			(const ::llc::SEventSystem & input, ::llc::au0_t & output) {
	output.clear();
	return input.Save(output);
}

::llc::err_t	lls::eventDeserialize		(::llc::vcu0_t input, ::llc::SEventSystem & output) {
	llc_necs(output.Load(input));
	if_true_ve(::llc::OS_INVALID_PARAMETER, input.size());
	return 0;
}

::llc::err_t	lls::pipeServerCreate		(pipe_t & output) {
	output				= 0;
	HANDLE		pipe	= ::CreateNamedPipeA
		( PIPE_NAME
		, PIPE_ACCESS_DUPLEX
		, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS
		, PIPE_UNLIMITED_INSTANCES
		, 0x10000
		, 0x10000
		, 0
		, 0
		);
	if_true_ve(::llc::OS_ERROR, INVALID_HANDLE_VALUE == pipe);
	output				= pipe;
	return 0;
}

::llc::err_t	lls::pipeServerWait			(pipe_t pipe) {
	if_zero_ve(::llc::OS_INVALID_PARAMETER, pipe);
	if(::ConnectNamedPipe(::pipeHandle(pipe), 0))
		return 0;
	return (ERROR_PIPE_CONNECTED == ::GetLastError()) ? 0 : ::llc::OS_ERROR;
}

::llc::err_t	lls::pipeServerDisconnect		(pipe_t pipe) {
	if_zero_ve(::llc::OS_INVALID_PARAMETER, pipe);
	::FlushFileBuffers(::pipeHandle(pipe));
	if_zero_ve(::llc::OS_ERROR, ::DisconnectNamedPipe(::pipeHandle(pipe)));
	return 0;
}

::llc::err_t	lls::pipeClientConnect			(pipe_t & output, ::llc::u2_t timeoutMilliseconds) {
	output				= 0;
	if_zero(::WaitNamedPipeA(PIPE_NAME, timeoutMilliseconds)) {
		const DWORD	lastError	= ::GetLastError();
		return (ERROR_SEM_TIMEOUT == lastError) ? ::llc::OS_TIMEOUT : ::llc::OS_NOT_FOUND;
	}
	HANDLE		pipe	= ::CreateFileA(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
	if_true_ve(::llc::OS_ERROR, INVALID_HANDLE_VALUE == pipe);
	output				= pipe;
	return 0;
}

::llc::err_t	lls::pipeClose				(pipe_t & pipe) {
	if(0 == pipe)
		return 0;
	HANDLE		handle	= ::pipeHandle(pipe);
	pipe				= 0;
	if_zero_ve(::llc::OS_ERROR, ::CloseHandle(handle));
	return 0;
}

::llc::err_t	lls::pipeWriteEvent			(pipe_t pipe, const ::llc::SEventSystem & eventToWrite) {
	if_zero_ve(::llc::OS_INVALID_PARAMETER, pipe);
	::llc::au0_t		payload		= {};
	llc_necs(eventSerialize(eventToWrite, payload));
	if_true_ve(::llc::OS_OVERRUN, payload.size() > FRAME_SIZE_MAX);

	const SFrameHeader	header		= {FRAME_MAGIC, PROTOCOL_VERSION, 0, payload.size()};
	llc_necs(::pipeWriteBytes(::pipeHandle(pipe), &header, sizeof(header)));
	if(payload.size())
		llc_necs(::pipeWriteBytes(::pipeHandle(pipe), payload.begin(), payload.size()));
	return 0;
}

::llc::err_t	lls::pipeReadEvent			(pipe_t pipe, ::llc::SEventSystem & eventToRead) {
	if_zero_ve(::llc::OS_INVALID_PARAMETER, pipe);
	SFrameHeader		header			= {};
	llc_necs(::pipeReadBytes(::pipeHandle(pipe), &header, sizeof(header)));
	if_true_ve(::llc::OS_INVALID_PARAMETER, FRAME_MAGIC != header.Magic);
	if_true_ve(::llc::OS_INVALID_PARAMETER, PROTOCOL_VERSION != header.Version);
	if_true_ve(::llc::OS_OVERRUN, header.PayloadBytes > FRAME_SIZE_MAX);

	::llc::au0_t		payload		= {};
	llc_necs(payload.resize(header.PayloadBytes));
	if(header.PayloadBytes)
		llc_necs(::pipeReadBytes(::pipeHandle(pipe), payload.begin(), header.PayloadBytes));
	return eventDeserialize(payload, eventToRead);
}

::llc::err_t	lls::pipeRequest				(const ::llc::SEventSystem & request, ::llc::SEventSystem & response, ::llc::u2_t timeoutMilliseconds) {
	pipe_t		pipe		= 0;
	const ::llc::err_t	connectResult	= pipeClientConnect(pipe, timeoutMilliseconds);
	if(::llc::failed(connectResult))
		return connectResult;
	const ::llc::err_t	writeResult	= pipeWriteEvent(pipe, request);
	if(::llc::failed(writeResult)) {
		pipeClose(pipe);
		return writeResult;
	}
	const ::llc::err_t	readResult	= pipeReadEvent(pipe, response);
	pipeClose(pipe);
	return readResult;
}
