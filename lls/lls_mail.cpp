#include "lls_store.h"
#include "lls_mail.h"
#include "../lcp/lcp_text.h"
#include "llc_file.h"
#include "llc_path.h"
#include "llc_runtime.h"
#include "llc_json_expression.h"
#include "llc_chrono.h"
#include <algorithm>

namespace {
using namespace llc;
stct SRule { string From, Contains, Folder; };
stct SBox {
	string Path, Task, Status, Next;
	aobj<string> Folders;
	aobj<SRule> Rules;
};
stct SMail {
	string Id, From, To, Subject, Text, Folder, Previous;
	bool Read = false;
};
err_t field(cnst SJSONReader & reader, u2_t node, vcst_t key, string & output, bool required = true) {
	cnst err_t found = jsonObjectValueGet(reader, node, key);
	if(found < 0) rtrn required ? -1 : 0;
	vcst_t value; string decoded;
	if_fail_fe(jsonObjectGetString(reader, found, value));
	if_fail_fe(lcpStringDecode(value, decoded));
	rtrn output.append(value);
}
bool safeName(vcst_t value) {
	if(!value.size() || value.size() > 48) rtrn false;
	for(cnst char c : value)
		if(!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) rtrn false;
	rtrn true;
}
bool folderExists(cnst SBox & box, vcst_t folder) {
	for(cnst auto & name : box.Folders) if(name == folder) rtrn true;
	rtrn false;
}
err_t pair(string & json, vcst_t key, vcst_t value, bool comma = true) {
	if(comma) { if_fail_fe(json.push_back(',')); }
	if_fail_fe(lcpJSONString(json, key));
	if_fail_fe(json.push_back(':'));
	rtrn lcpJSONString(json, value);
}
err_t boxJSON(cnst SBox & box, string & json) {
	if_fail_fe(json.append_string("{\"version\":1"));
	if_fail_fe(pair(json, "task", box.Task));
	if_fail_fe(pair(json, "status", box.Status));
	if_fail_fe(pair(json, "next", box.Next));
	if_fail_fe(json.append_string(",\"folders\":["));
	for(u2_t i = 0; i < box.Folders.size(); ++i) {
		if(i) { if_fail_fe(json.push_back(',')); }
		if_fail_fe(lcpJSONString(json, box.Folders[i]));
	}
	if_fail_fe(json.append_string("],\"filters\":["));
	for(u2_t i = 0; i < box.Rules.size(); ++i) {
		if(i) { if_fail_fe(json.push_back(',')); }
		if_fail_fe(json.push_back('{'));
		if_fail_fe(pair(json, "from", box.Rules[i].From, false));
		if_fail_fe(pair(json, "contains", box.Rules[i].Contains));
		if_fail_fe(pair(json, "folder", box.Rules[i].Folder));
		if_fail_fe(json.push_back('}'));
	}
	rtrn json.append_string("]}");
}
err_t saveBox(cnst SBox & box) {
	string json, path;
	if_fail_fe(boxJSON(box, json));
	if_fail_fe(append_strings(path, box.Path, "/metadata.json"));
	rtrn llsStoreSave(path, json);
}
err_t openBox(vcst_t name, SBox & box) {
	if_true_fe(!safeName(name));
	string root, parent;
	if_fail_fe(pathAbsolute("../.lls-mail", parent));
	if_fail_fe(llsStoreDirectory(parent));
#ifdef _DEBUG
	if_fail_fe(append_strings(root, parent, "/Debug"));
#else
	if_fail_fe(append_strings(root, parent, "/Release"));
#endif
	if_fail_fe(llsStoreDirectory(root));
	if_fail_fe(append_strings(box.Path, root, "/box_", name));
	if_fail_fe(llsStoreDirectory(box.Path));
	string messages, path;
	if_fail_fe(append_strings(messages, box.Path, "/messages"));
	if_fail_fe(llsStoreDirectory(messages));
	if_fail_fe(append_strings(path, box.Path, "/metadata.json"));
#ifdef LLC_WINDOWS
	if(GetFileAttributesA(path.begin()) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND) {
		for(vcst_t folder : {vcst_t{"inbox"}, vcst_t{"sent"}, vcst_t{"trash"}})
			if_fail_fe(box.Folders.push_back(string{folder}));
		rtrn saveBox(box);
	}
#endif
	string data; SJSONReader reader;
	if_fail_fe(llsStoreLoad(path, data, reader));
	s3_t version = 0;
	if_fail_fe(jsonObjectGetAsInteger(reader, 0, "version", version));
	if_true_fe(version != 1);
	if_fail_fe(field(reader, 0, "task", box.Task));
	if_fail_fe(field(reader, 0, "status", box.Status));
	if_fail_fe(field(reader, 0, "next", box.Next));
	cnst err_t folders = jsonObjectValueGet(reader, 0, "folders");
	cnst err_t count = folders < 0 ? -1 : jsonArraySize(reader, folders);
	if_true_fe(count < 3 || count > 128);
	for(err_t i = 0; i < count; ++i) {
		vcst_t value; string decoded;
		if_fail_fe(jsonObjectGetString(reader, jsonArrayValueGet(reader, folders, i), value));
		if_fail_fe(lcpStringDecode(value, decoded));
		if_true_fe(!safeName(value) || folderExists(box, value));
		if_fail_fe(box.Folders.push_back(string{value}));
	}
	if_true_fe(!folderExists(box, "inbox") || !folderExists(box, "sent") || !folderExists(box, "trash"));
	cnst err_t rules = jsonObjectValueGet(reader, 0, "filters");
	cnst err_t ruleCount = rules < 0 ? -1 : jsonArraySize(reader, rules);
	if_true_fe(ruleCount < 0 || ruleCount > 32);
	for(err_t i = 0; i < ruleCount; ++i) {
		SRule rule;
		cnst err_t node = jsonArrayValueGet(reader, rules, i);
		if_fail_fe(field(reader, node, "from", rule.From));
		if_fail_fe(field(reader, node, "contains", rule.Contains));
		if_fail_fe(field(reader, node, "folder", rule.Folder));
		if_true_fe(!folderExists(box, rule.Folder) || rule.Folder == "trash" || rule.Folder == "sent");
		if_fail_fe(box.Rules.push_back(rule));
	}
	rtrn 0;
}
err_t mailJSON(cnst SMail & mail, string & json, bool includeText = true) {
	if_fail_fe(json.push_back('{'));
	if_fail_fe(pair(json, "id", mail.Id, false));
	if_fail_fe(pair(json, "from", mail.From));
	if_fail_fe(pair(json, "to", mail.To));
	if_fail_fe(pair(json, "subject", mail.Subject));
	if(includeText) { if_fail_fe(pair(json, "text", mail.Text)); }
	if_fail_fe(pair(json, "folder", mail.Folder));
	if_fail_fe(pair(json, "previous", mail.Previous));
	rtrn append_strings(json, ",\"read\":", b82st(mail.Read), '}');
}
err_t mailPath(cnst SBox & box, vcst_t id, string & path) {
	if_true_fe(!safeName(id));
	rtrn append_strings(path, box.Path, "/messages/", id, ".json");
}
err_t readMail(cnst SBox & box, vcst_t id, SMail & mail) {
	string path, data; SJSONReader reader;
	if_fail_fe(mailPath(box, id, path));
	if_fail_fe(llsStoreLoad(path, data, reader));
	if_fail_fe(field(reader, 0, "id", mail.Id));
	if_true_fe(mail.Id != id);
	if_fail_fe(field(reader, 0, "from", mail.From));
	if_fail_fe(field(reader, 0, "to", mail.To));
	if_fail_fe(field(reader, 0, "subject", mail.Subject));
	if_fail_fe(field(reader, 0, "text", mail.Text));
	if_fail_fe(field(reader, 0, "folder", mail.Folder));
	if_fail_fe(field(reader, 0, "previous", mail.Previous));
	if_true_fe(!folderExists(box, mail.Folder));
	rtrn jsonObjectGetAsBoolean(reader, 0, "read", mail.Read);
}
err_t saveMail(cnst SBox & box, cnst SMail & mail, bool replace = true) {
	string path, json;
	if_fail_fe(mailPath(box, mail.Id, path));
	if_fail_fe(mailJSON(mail, json));
	rtrn llsStoreSave(path, json, replace);
}
err_t response(string & body, vcst_t text, vcst_t data) {
	if_fail_fe(body.append_string("\"result\":{\"isError\":false,\"content\":[{\"type\":\"text\",\"text\":"));
	if_fail_fe(lcpJSONString(body, text));
	rtrn append_strings(body, "}],\"structuredContent\":", data, '}');
}
// Explicit profile only, short single-line feedback; no guessed chat state.
err_t focus(cnst SBox & box, string & text) {
	if_fail_fe(text.append_string("Focus: "));
	if(!box.Task.size() && !box.Status.size() && !box.Next.size()) rtrn text.append_string("not set.");
	string source;
	if_fail_fe(append_strings(source, box.Task, " | ", box.Status, " | next: ", box.Next));
	u2_t length = llc::min((u2_t)144, source.size());
	while(length && length < source.size() && ((u0_t)source[length] & 0xC0) == 0x80) --length;
	for(u2_t i = 0; i < length; ++i) {
		char c = source[i]; if((u0_t)c < 32 || c == 127) c = ' ';
		if_fail_fe(text.push_back(c));
	}
	if(length < source.size()) { if_fail_fe(text.append_string("...")); }
	rtrn 0;
}
err_t call(cnst SJSONReader & reader, string & body) {
	cnst err_t args = jsonExpressionResolve("params.arguments", reader, 0);
	if_fail_fe(args);
	string action, owner;
	if_fail_fe(field(reader, args, "action", action));
	if_fail_fe(field(reader, args, "mailbox", owner));
	SBox box;
	if_fail_fe(openBox(owner, box));
	if(action == "send") {
		SMail mail;
		mail.From = owner;
		if_fail_fe(field(reader, args, "to", mail.To));
		if_fail_fe(field(reader, args, "subject", mail.Subject, false));
		if_fail_fe(field(reader, args, "text", mail.Text));
		if_true_fe(!mail.Text.size() || mail.Text.size() > 262144 || mail.Subject.size() > 512);
		if_true_fe(mail.To == owner); // Avoid ambiguous inbox/sent copies in one mailbox.
		SBox recipient;
		if_fail_fe(openBox(mail.To, recipient));
		sttc u3_t sequence = 0;
		if_fail_fe(append_strings(mail.Id, str((u3_t)timeCurrentInUs()), '-', str(++sequence)));
		mail.Folder = vcst_t{"inbox"};
		for(cnst auto & rule : recipient.Rules)
			if((!rule.From.size() || rule.From == owner) && (!rule.Contains.size() || mail.Subject.find(rule.Contains) >= 0 || mail.Text.find(rule.Contains) >= 0)) {
				mail.Folder = rule.Folder; break;
			}
		if_fail_fe(saveMail(recipient, mail, false));
		mail.Folder = vcst_t{"sent"}; mail.Read = true;
		cnst bool sentCopy = succeeded(saveMail(box, mail, false));
		string json, text;
		if_fail_fe(append_strings(json, "{\"delivered\":true,\"sentCopy\":", b82st(sentCopy)));
		if_fail_fe(pair(json, "id", mail.Id));
		if_fail_fe(json.push_back('}'));
		if_fail_fe(append_strings(text, "Delivered ", mail.Id, sentCopy ? vcst_t{".\n"} : vcst_t{". Sent-copy save failed; do not resend.\n"}));
		if_fail_fe(focus(box, text));
		rtrn response(body, text, json);
	}
	if(action == "profile" || action == "folders" || action == "folder_create" || action == "filter_set") {
		bool changed = false;
		if(action == "profile") {
			string * values[] = {&box.Task, &box.Status, &box.Next};
			vcst_t names[] = {"task", "status", "next"};
			for(u2_t i = 0; i < 3; ++i)
				if(jsonObjectValueGet(reader, args, names[i]) >= 0) {
					if_fail_fe(values[i]->clear());
					if_fail_fe(field(reader, args, names[i], *values[i]));
					if_true_fe(values[i]->size() > 512);
					changed = true;
				}
		}
		if(action == "folder_create") {
			string folder; if_fail_fe(field(reader, args, "folder", folder));
			if_true_fe(!safeName(folder) || box.Folders.size() >= 128);
			if(!folderExists(box, folder)) { if_fail_fe(box.Folders.push_back(folder)); changed = true; }
		}
		if(action == "filter_set") {
			cnst err_t array = jsonObjectValueGet(reader, args, "filters");
			cnst err_t count = array < 0 ? -1 : jsonArraySize(reader, array);
			if_true_fe(count < 0 || count > 32);
			if_fail_fe(box.Rules.clear());
			for(err_t i = 0; i < count; ++i) {
				cnst err_t node = jsonArrayValueGet(reader, array, i);
				SRule rule;
				if_fail_fe(field(reader, node, "from", rule.From, false));
				if_fail_fe(field(reader, node, "contains", rule.Contains, false));
				if_fail_fe(field(reader, node, "folder", rule.Folder));
				if_true_fe((rule.From.size() && !safeName(rule.From)) || rule.Contains.size() > 512 || !folderExists(box, rule.Folder) || rule.Folder == "trash" || rule.Folder == "sent");
				if_fail_fe(box.Rules.push_back(rule));
			}
			changed = true;
		}
		if(changed) { if_fail_fe(saveBox(box)); }
		string json, text;
		if_fail_fe(boxJSON(box, json));
		if_fail_fe(focus(box, text));
		rtrn response(body, text, json);
	}
	if(action == "list") {
		string folder, query;
		if_fail_fe(field(reader, args, "folder", folder, false));
		if(!folder.size()) folder = vcst_t{"inbox"};
		if_true_fe(!folderExists(box, folder));
		if_fail_fe(field(reader, args, "query", query, false));
		s3_t limit = 20, offset = 0;
		if(jsonObjectValueGet(reader, args, "limit") >= 0) { if_fail_fe(jsonObjectGetAsInteger(reader, args, "limit", limit)); }
		if(jsonObjectValueGet(reader, args, "offset") >= 0) { if_fail_fe(jsonObjectGetAsInteger(reader, args, "offset", offset)); }
		bool unread = false;
		if(jsonObjectValueGet(reader, args, "unread") >= 0) { if_fail_fe(jsonObjectGetAsBoolean(reader, args, "unread", unread)); }
		if_true_fe(limit < 1 || limit > 100 || offset < 0);
		string path; if_fail_fe(append_strings(path, box.Path, "/messages"));
		SPathTree tree;
		if_fail_fe(pathList(tree, path, [](bool folderEntry, vcst_c & entry)->err_t {
			if(folderEntry) rtrn 1;
			vcst_t name; if_fail_fe(pathFilename(entry, name));
			rtrn name.size() > 5 && vcst_t{name.begin() + name.size() - 5, 5} == ".json" ? 0 : 1;
		}));
		aobj<string> ids;
		for(cnst auto & name : tree.Name) {
			if_true_fe(name.size() <= 5);
			if_fail_fe(ids.push_back(string{vcst_t{name.begin(), name.size() - 5}}));
		}
		std::sort(ids.begin(), ids.end(), [](cnst string & a, cnst string & b) { rtrn strcmp(a.begin(), b.begin()) > 0; });
		string json = "{\"messages\":[";
		u3_t matches = 0, unreadCount = 0, returned = 0;
		for(cnst auto & id : ids) {
			SMail mail; if_fail_fe(readMail(box, id, mail));
			if(mail.Folder != folder) continue;
			unreadCount += !mail.Read;
			if(unread && mail.Read) continue;
			if(query.size() && mail.Subject.find(query) < 0 && mail.Text.find(query) < 0 && mail.From.find(query) < 0) continue;
			if(matches++ < (u3_t)offset || returned >= (u3_t)limit) continue;
			if(returned++) { if_fail_fe(json.push_back(',')); }
			if_fail_fe(mailJSON(mail, json, false));
		}
		if_fail_fe(append_strings(json, "],\"total\":", str(matches), ",\"unread\":", str(unreadCount), ",\"offset\":", str(offset), ",\"returned\":", str(returned), '}'));
		string text; if_fail_fe(append_strings(text, owner, "/", folder, ": ", str(matches), " matching; ", str(unreadCount), " unread."));
		rtrn response(body, text, json);
	}
	if(action == "read" || action == "mark" || action == "move" || action == "trash" || action == "restore") {
		string id; if_fail_fe(field(reader, args, "id", id));
		SMail mail; if_fail_fe(readMail(box, id, mail));
		if(action == "mark") { if_fail_fe(jsonObjectGetAsBoolean(reader, args, "read", mail.Read)); }
		if(action == "move") {
			string folder; if_fail_fe(field(reader, args, "folder", folder));
			if_true_fe(!folderExists(box, folder) || folder == "trash" || mail.Folder == "trash");
			mail.Folder = folder;
		}
		if(action == "trash" && mail.Folder != "trash") { mail.Previous = mail.Folder; mail.Folder = vcst_t{"trash"}; }
		if(action == "restore" && mail.Folder == "trash") {
			if_true_fe(!folderExists(box, mail.Previous) || mail.Previous == "trash");
			mail.Folder = mail.Previous; mail.Previous.clear();
		}
		if(action != "read") { if_fail_fe(saveMail(box, mail)); }
		string json; if_fail_fe(mailJSON(mail, json));
		rtrn response(body, action == "read" ? vcst_t{"Message read; read/unread state unchanged."} : vcst_t{"Message updated."}, json);
	}
	rtrn lcpTextBody(body, "Unknown inbox action. Open help inbox.", true);
}
}

llc::err_t llsMail(cnst llc::SJSONReader & reader, llc::string & body) {
	cnst llc::err_t result = call(reader, body);
	if(llc::succeeded(result)) rtrn result;
	if_fail_fe(body.clear());
	rtrn lcpTextBody(body, "Inbox operation failed: invalid fields, missing message, unsafe path, corrupt metadata, or storage error. Inspect service diagnostics. No retry was made; check delivery before resending.", true);
}
