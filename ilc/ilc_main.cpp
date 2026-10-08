#include "llc_args.h"
#include "llc_functional.h"
#include "llc_runtime.h"

sttc	::llc::err_t	ilc_entry_point		(::llc::SRuntimeValues & runtimeValues);
LLC_SYSTEM_OS_ENTRY_POINT(::ilc_entry_point);

using FStreamRead	= ::llc::function<::llc::err_t(::llc::vu0_t)>;
using FStreamWrite	= ::llc::function<::llc::err_t(::llc::vcu0_t)>;
using FStreamOpen	= ::llc::function<::llc::err_t(FStreamRead &, FStreamWrite &, bool, ::llc::vcst_t)>;

stct SILCApp {
	FStreamOpen		Open		= {};
	FStreamRead		Read		= {};
	FStreamWrite	Write		= {};
	::llc::vcst_t	Endpoint	= {};
	bool			Host		= {};
};

sttc	::llc::err_t	ilcTCP		(FStreamRead &, FStreamWrite &, bool, ::llc::vcst_t)	{ rtrn -1; }
sttc	::llc::err_t	ilcUDP		(FStreamRead &, FStreamWrite &, bool, ::llc::vcst_t)	{ rtrn -1; }
sttc	::llc::err_t	ilcHost		(FStreamRead &, FStreamWrite &)							{ rtrn 0; }
sttc	::llc::err_t	ilcJoin		(FStreamRead &, FStreamWrite &)							{ rtrn 0; }

sttc	::llc::err_t	ilcConfigure	(cnst ::llc::SCommandLineArgs & arguments, SILCApp & appInstance) {
	cnst bool host = 0 <= ::llc::argsOptionIndex(arguments, LLC_CXS("host"));
	cnst bool join = 0 <= ::llc::argsOptionIndex(arguments, LLC_CXS("join"));
	cnst bool tcp  = 0 <= ::llc::argsOptionIndex(arguments, LLC_CXS("tcp" ));
	cnst bool udp  = 0 <= ::llc::argsOptionIndex(arguments, LLC_CXS("udp" ));
	if_true_fef(host == join, "%s", "Choose either -host or -join.");
	if_true_fef(tcp  == udp , "%s", "Choose either -tcp or -udp.");
	appInstance.Host = host;
	appInstance.Open = tcp ? ::ilcTCP : ::ilcUDP;
	if_fail_fe(::llc::argsOptionValue(arguments, tcp ? LLC_CXS("tcp") : LLC_CXS("udp"), appInstance.Endpoint));
	rtrn 0;
}

sttc	::llc::err_t	ilcRun		(SILCApp & appInstance) {
	if_fail_fe(appInstance.Open(appInstance.Read, appInstance.Write, appInstance.Host, appInstance.Endpoint));
	rtrn appInstance.Host ? ::ilcHost(appInstance.Read, appInstance.Write) : ::ilcJoin(appInstance.Read, appInstance.Write);
}

sttc	::llc::err_t	ilc_entry_point		(::llc::SRuntimeValues & runtimeValues) {
	SILCApp appInstance = {};
	if_fail_fe(::ilcConfigure(runtimeValues.EntryPointArgs, appInstance));
	rtrn ::ilcRun(appInstance);
}
