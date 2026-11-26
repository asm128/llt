#include "llc_engine.h"
#include "llc_window.h"
#include "llc_gui_composite.h"
#include "llc_path.h"
#include "llc_gui_inputbox.h"
#include "llc_index.h"
#include "llc_process_broker.h"
#include <atomic>
#include <memory>
#include <mutex>

#ifndef LLC_BLITTER_SEARCH_H_23627
#define LLC_BLITTER_SEARCH_H_23627

stxp ::llc::u0_t SAMPLE_GUI_VIEWPORT_COUNT = 7;

stct SFileIndexWork {
	::llc::string Root = {};
	::llc::aobj<::llc::string> Paths = {};
	::llc::u2_t StoredPaths = 0;
	::llc::au3_t Chars = {};
	::llc::aobj<::llc::SWordIndices> Words = {};
	::llc::SWordDictData Cache = {};
	::llc::string Bytes = {};
	::llc::u2_t Unreadable = 0, Truncated = 0, SkippedFolders = 0, SkippedBinary = 0;
	::llc::s3_t ElapsedMs = 0;
	::llc::err_t SaveResult = 0;
	::llc::au2_t ShardStarts = {};
	::llc::u3_t ShardGeneration = 0;
	::llc::u2_t CompletedPaths = 0, ResumedPaths = 0;
	::llc::u3_t RetireGeneration = 0;
	::llc::u2_t RetireShards = 0;
	::std::atomic<::llc::u2_t> Discovered = 0, Processed = 0;
	::std::atomic<::llc::u2_t> IndexedWords = 0;
	::std::atomic<::llc::u3_t> BytesRead = 0;
	::std::atomic<bool> Cancel = false;
	::std::atomic<int> Status = 0;
};

stct SFileSearchWork {
	::llc::string Query = {}, DatabasePath = {};
	::llc::au2_t ShardStarts = {}, Results = {};
	::llc::aobj<::llc::au0_t> TermSeen = {};
	::llc::au1_t MatchedTerms = {};
	::llc::u3_t ShardGeneration = 0;
	::llc::u2_t TotalPaths = 0;
	::std::shared_ptr<::llc::au3_t> Chars = {};
	bool UseGramRoute = true;
	::llc::s3_t StartedUs = 0;
	::std::mutex ResultsMutex = {};
	::std::atomic<::llc::u2_t> ShardsDone = 0;
	::std::atomic<bool> Cancel = false;
	::std::atomic<int> Status = 0;
};

stct SConsoleWork {
	::llc::SProcessBrokerJob Job = {};
	::std::atomic<int> Status = 0;
};

stct SSampleGUIViewport {
	::llc::SEngine		Engine			= {};
	::llc::SWindow		& Window;
	::llc::SGUI			& GUI;

	SSampleGUIViewport(::llc::SWindow & window, ::llc::SGUI & gui) : Window(window), GUI(gui) {}
	::llc::cid_t			Desktop			= ::llc::CID_INVALID;
	::llc::cid_t			Help			= ::llc::CID_INVALID;
	::llc::SInputBox SearchInput = {};
	::llc::cid_t SearchTiming = ::llc::CID_INVALID;
	::llc::astsc_t<96> SearchTimingText = {};
	bool SearchFocused = false;
	::llc::cid_t			Performance		= ::llc::CID_INVALID;
	::llc::cid_t			Exit			= ::llc::CID_INVALID;
	::llc::astsc_t<96>		PerformanceText		= {};
	::llc::string			FolderPath			= {};
	::llc::pobj<::llc::SPathTree>	FolderEntries	= {};
	::llc::acid			FolderControls		= {};
	::llc::u2_t			FolderOffset		= 0;
	::llc::u2_t SelectedFileEntry = (::llc::u2_t)-1;
	::llc::string SelectedFile = {};
	::llc::cid_t OpenSelected = ::llc::CID_INVALID;
	::llc::cid_t DriveD = ::llc::CID_INVALID;
	::llc::string			OpenFile			= {};
	::llc::astsc_t<128> FileTitle = {};
	::llc::u3_t FileSize = 0;
	::llc::u3_t FileChunkOffset = 0;
	bool FileChunkBinary = false;
	::llc::string			FileBytes			= {};
	::llc::string			FileDisplay		= {};
	::llc::aobj<::llc::vcst_t> FileLines		= {};
	::llc::aobj<::llc::SGUIViewportTextRow> TextRows = {};
	::llc::acid			FileControls		= {};
	::llc::aobj<::llc::string> ProcessNames = {};
	::llc::aobj<::llc::string> ServiceNames = {};
	::llc::aobj<::llc::SWordIndices> ProcessWords = {};
	::llc::aobj<::llc::SWordIndices> ServiceWords = {};
	::llc::acid ProcessControls = {};
	::llc::acid ServiceControls = {};
	::llc::acid FileResultControls = {};
	::llc::cid_t IndexDirectory = ::llc::CID_INVALID;
	::llc::astsc_t<96> IndexProgressText = {};
	::llc::string IndexDatabasePath = {};
	::llc::aobj<::llc::string> IndexPaths = {};
	::llc::string IndexRoot = {};
	::llc::u2_t IndexUnreadable = 0;
	::llc::u2_t IndexTruncated = 0;
	::llc::u2_t IndexSkippedFolders = 0;
	::llc::u2_t IndexSkippedBinary = 0;
	::std::shared_ptr<SFileIndexWork> PendingIndex = {};
	::llc::aobj<::llc::SWordIndices> IndexWords = {};
	::llc::au2_t IndexShardStarts = {};
	::llc::u3_t IndexShardGeneration = 0;
	::std::shared_ptr<::llc::au3_t> IndexChars = {};
	::llc::u3_t RetireIndexGeneration = 0;
	::llc::u2_t RetireIndexShards = 0;
	::std::shared_ptr<SFileSearchWork> PendingSearch = {};
	::llc::string SearchQueued = {};
	::llc::u2_t SearchShardsShown = 0;
	::llc::u2_t SearchResultsShown = 0;
	::llc::au2_t FileResults = {};
	::llc::u2_t ResultOffset = 0;
	bool IndexReady = false;
	bool IndexPartial = false;
	::llc::acid PreviewControls = {};
	::llc::string PreviewBytes = {};
	::llc::aobj<::llc::vcst_t> PreviewLines = {};
	::llc::u2_t ProcessOffset = 0;
	::llc::u2_t ServiceOffset = 0;
	::llc::u2_t InventoryFrame = 0;
	::llc::SInputBox ConsoleInput = {};
	::llc::acid ConsoleControls = {};
	::llc::acid ProcessableControls = {};
	::llc::aobj<::llc::string> ConsoleLines = {};
	::std::shared_ptr<SConsoleWork> PendingConsole = {};
	::llc::u2_t			FileOffset			= 0;
	::llc::u2_t			FileColumnOffset	= 0;
	::llc::u2_t			FileLineWidthMax	= 0;
	::llc::u2_t			VisibleColumns		= 0;
	bool					WordWrap		= false;
	bool					SelectAll		= false;
	::llc::astatic<::llc::SViewport, SAMPLE_GUI_VIEWPORT_COUNT>
						Viewports		= {};
	::llc::astatic<::llc::astcid<::llc::GUI_VIEWPORT_ACTION_COUNT>, SAMPLE_GUI_VIEWPORT_COUNT>
						ViewportActions = {};
	::llc::astatic<::llc::n2s1_t, SAMPLE_GUI_VIEWPORT_COUNT>
						UnfoldedSize		= {};
	::llc::astatic<bool, SAMPLE_GUI_VIEWPORT_COUNT>
						Folded			= {};
};

::llc::err_t	sampleGUIViewportSetup				(SSampleGUIViewport & app);
::llc::err_t	sampleGUIViewportUpdate				(SSampleGUIViewport & app, bool systemRequestedExit, double secondsLastFrame);
::llc::err_t	sampleGUIViewportDraw				(SSampleGUIViewport & app, ::llc::img8bgra (&clientImages)[SAMPLE_GUI_VIEWPORT_COUNT], ::llc::u2_t frameIndex, ::llc::g8bgra target);

#endif // LLC_BLITTER_SEARCH_H_23627
