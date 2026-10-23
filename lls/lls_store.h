#include "llc_file.h"
#include "llc_json.h"
#include "llc_runtime.h"
#include "llc_string_compose.h"
#ifndef LLS_STORE_H
#define LLS_STORE_H
namespace llc {
#ifdef LLC_WINDOWS
inline err_t llsStoreDirectory(vcst_t path) {
	string name = path;
	cnst DWORD attributes = GetFileAttributesA(name.begin());
	if(attributes != INVALID_FILE_ATTRIBUTES) {
		if_true_fe(!(attributes & FILE_ATTRIBUTE_DIRECTORY) || (attributes & FILE_ATTRIBUTE_REPARSE_POINT));
		rtrn 0;
	}
	if_zero_fe(CreateDirectoryA(name.begin(), 0));
	rtrn 0;
}
// Each update replaces one complete JSON file on the same volume. No in-place edits.
inline err_t llsStoreSave(vcst_t path, vcst_t data, bool replace = true) {
	string target = path, pending;
	if_fail_fe(append_strings(pending, path, ".pending"));
	cnst DWORD attributes = GetFileAttributesA(target.begin());
	if_true_fe(attributes != INVALID_FILE_ATTRIBUTES && (attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)));
	HANDLE file = CreateFileA(pending.begin(), GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
	if_true_fe(file == INVALID_HANDLE_VALUE);
	DWORD written = 0;
	cnst bool complete = WriteFile(file, data.begin(), data.size(), &written, 0) && written == data.size() && FlushFileBuffers(file);
	CloseHandle(file);
	if(!complete) { DeleteFileA(pending.begin()); rtrn -1; }
	if(!MoveFileExA(pending.begin(), target.begin(), MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0))) {
		DeleteFileA(pending.begin());
		rtrn -1;
	}
	rtrn 0;
}
inline err_t llsStoreLoad(vcst_t path, string & data, SJSONReader & reader, u2_t max = 2 * 1024 * 1024) {
	string file = path;
	cnst DWORD attributes = GetFileAttributesA(file.begin());
	if_true_fe(attributes == INVALID_FILE_ATTRIBUTES || (attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)));
	cnst s3_t size = fileSize(path);
	if_true_fe(size < 0 || size > max);
	if_fail_fe(fileToMemory(path, data, max));
	rtrn jsonParse(reader, data, JSON_PARSE_STRICT);
}
#else
inline err_t llsStoreDirectory(vcst_t) { rtrn -1; }
inline err_t llsStoreSave(vcst_t, vcst_t, bool = true) { rtrn -1; }
inline err_t llsStoreLoad(vcst_t, string &, SJSONReader &, u2_t = 2 * 1024 * 1024) { rtrn -1; }
#endif

}
#endif
