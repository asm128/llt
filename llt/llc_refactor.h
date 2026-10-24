#pragma once
#include "llc_array_obj.h"
#include "llc_string.h"

namespace llc {
	stct SRefactorProvenance {
		string		Repository	= {};
		string		Commit		= {};
		string		Date		= {};
		string		Subject		= {};
	};

	stct SRefactorHunk {
		string		FileBefore	= {};
		string		FileAfter	= {};
		u2_t		LineBefore	= {}, CountBefore = {};
		u2_t		LineAfter	= {}, CountAfter = {};
		string		Before		= {};
		string		After		= {};
	};

	stct SRefactorPatch {
		SRefactorProvenance	Source	= {};
		aobj<SRefactorHunk>	Hunks	= {};
	};
	stct SRefactorMatch {
		u2_t	Patch	= u2_t(-1);
		u2_t	Hunk	= u2_t(-1);
		f3_t	Score	= {};
	};

	err_t	refactorDiffParse	(vcst_t unifiedDiff, SRefactorPatch & output);
	err_t	refactorCodeNormalize	(vcst_t code, string & output);
	err_t	refactorCodeSimilarity	(vcst_t left, vcst_t right, f3_t & similarity);
	err_t	refactorFindBest	(vcst_t code, view<cnst SRefactorPatch> corpus, SRefactorMatch & match);
}
