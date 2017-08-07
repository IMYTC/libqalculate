/*
    Qalculate (library)

    Copyright (C) 2003-2007, 2008, 2016  Hanna Knutsson (hanna.knutsson@protonmail.com)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "support.h"

#include "util.h"
#include <stdarg.h>
#include "Number.h"

#include <string.h>
#include <time.h>
#include <iconv.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_ICU
#	include <unicode/ucasemap.h>
#endif
#include <fstream>
#ifdef _WIN32
#	include <winsock2.h>
#	include <windows.h>
#	include <shlobj.h>
//#	include <shlwapi.h>
#	include <direct.h>
#	include <knownfolders.h>
#	include <initguid.h>
#	include <shlobj.h>
#else
#	include <utime.h>
#	include <unistd.h>
#	include <pwd.h>
#endif

bool eqstr::operator()(const char *s1, const char *s2) const {
	return strcmp(s1, s2) == 0;
}

#ifdef HAVE_ICU
	UCaseMap *ucm = NULL;
#endif

char buffer[20000];

void sleep_ms(int milliseconds) {
#ifdef _WIN32
	Sleep(milliseconds);
#elif _POSIX_C_SOURCE >= 199309L
	struct timespec ts;
	ts.tv_sec = milliseconds / 1000;
	ts.tv_nsec = (milliseconds % 1000) * 1000000;
	nanosleep(&ts, NULL);
#else
	usleep(milliseconds * 1000);
#endif
}

void now(int &hour, int &min, int &sec) {
	time_t t = time(NULL);
	struct tm *lt = localtime(&t);
	hour = lt->tm_hour;
	min = lt->tm_min;
	sec = lt->tm_sec;
}

string& gsub(const string &pattern, const string &sub, string &str) {
	size_t i = str.find(pattern);
	while(i != string::npos) {
		str.replace(i, pattern.length(), sub);
		i = str.find(pattern, i + sub.length());
	}
	return str;
}
string& gsub(const char *pattern, const char *sub, string &str) {
	size_t i = str.find(pattern);
	while(i != string::npos) {
		str.replace(i, strlen(pattern), string(sub));
		i = str.find(pattern, i + strlen(sub));
	}
	return str;
}

string& remove_blanks(string &str) {
	size_t i = str.find_first_of(SPACES, 0);
	while(i != string::npos) {
		str.erase(i, 1);
		i = str.find_first_of(SPACES, i);
	}
	return str;
}

string& remove_duplicate_blanks(string &str) {
	size_t i = str.find_first_of(SPACES, 0);
	while(i != string::npos) {
		if(i == 0 || is_in(SPACES, str[i - 1])) {
			str.erase(i, 1);
		} else {
			i++;
		}
		i = str.find_first_of(SPACES, i);
	}
	return str;
}

string& remove_blank_ends(string &str) {
	size_t i = str.find_first_not_of(SPACES);
	size_t i2 = str.find_last_not_of(SPACES);
	if(i != string::npos && i2 != string::npos) {
		if(i > 0 || i2 < str.length() - 1) {
			str = str.substr(i, i2 - i + 1);
		}
	} else {
		str.resize(0);
	}
	return str;
}
string& remove_parenthesis(string &str) {
	if(str[0] == LEFT_PARENTHESIS_CH && str[str.length() - 1] == RIGHT_PARENTHESIS_CH) {
		str = str.substr(1, str.length() - 2);
		return remove_parenthesis(str);
	}
	return str;
}

string d2s(double value, int precision) {
	// qgcvt(value, precision, buffer);
	sprintf(buffer, "%.*G", precision, value);
	string stmp = buffer;
	// gsub("e", "E", stmp);
	return stmp;
}

string p2s(void *o) {
	sprintf(buffer, "%p", o);
	string stmp = buffer;
	return stmp;
}
string i2s(int value) {
	// char buffer[10];
	sprintf(buffer, "%i", value);
	string stmp = buffer;
	return stmp;
}
string i2s(long int value) {
	sprintf(buffer, "%li", value);
	string stmp = buffer;
	return stmp;
}
string i2s(unsigned int value) {
	sprintf(buffer, "%u", value);
	string stmp = buffer;
	return stmp;
}
string i2s(unsigned long int value) {
	sprintf(buffer, "%lu", value);
	string stmp = buffer;
	return stmp;
}
const char *b2yn(bool b, bool capital) {
	if(capital) {
		if(b) return _("Yes");
		return _("No");
	}
	if(b) return _("yes");
	return _("no");
}
const char *b2tf(bool b, bool capital) {
	if(capital) {
		if(b) return _("True");
		return _("False");
	}
	if(b) return _("true");
	return _("false");
}
const char *b2oo(bool b, bool capital) {
	if(capital) {
		if(b) return _("On");
		return _("Off");
	}
	if(b) return _("on");
	return _("off");
}
int s2i(const string& str) {
	return strtol(str.c_str(), NULL, 10);
}
int s2i(const char *str) {
	return strtol(str, NULL, 10);
}
void *s2p(const string& str) {
	void *p;
	sscanf(str.c_str(), "%p", &p);
	return p;
}
void *s2p(const char *str) {
	void *p;
	sscanf(str, "%p", &p);
	return p;
}

size_t find_ending_bracket(const string &str, size_t start, int *missing) {
	int i_l = 1;
	while(true) {
		start = str.find_first_of(LEFT_PARENTHESIS RIGHT_PARENTHESIS, start);
		if(start == string::npos) {
			if(missing) *missing = i_l;
			return string::npos;
		}
		if(str[start] == LEFT_PARENTHESIS_CH) {
			i_l++;
		} else {
			i_l--;
			if(!i_l) {
				if(missing) *missing = i_l;
				return start;
			}
		}
		start++;
	}
}

char op2ch(MathOperation op) {
	switch(op) {
		case OPERATION_ADD: return PLUS_CH;
		case OPERATION_SUBTRACT: return MINUS_CH;		
		case OPERATION_MULTIPLY: return MULTIPLICATION_CH;		
		case OPERATION_DIVIDE: return DIVISION_CH;		
		case OPERATION_RAISE: return POWER_CH;		
		case OPERATION_EXP10: return EXP_CH;
		default: return ' ';		
	}
}

string& wrap_p(string &str) {
	str.insert(str.begin(), 1, LEFT_PARENTHESIS_CH);
	str += RIGHT_PARENTHESIS_CH;
	return str;
}

bool is_in(const char *str, char c) {
	for(size_t i = 0; i < strlen(str); i++) {
		if(str[i] == c)
			return true;
	}
	return false;
}
bool is_not_in(const char *str, char c) {
	for(size_t i = 0; i < strlen(str); i++) {
		if(str[i] == c)
			return false;
	}
	return true;
}
bool is_in(const string &str, char c) {
	for(size_t i = 0; i < str.length(); i++) {
		if(str[i] == c)
			return true;
	}
	return false;
}
bool is_not_in(const string &str, char c) {
	for(size_t i = 0; i < str.length(); i++) {
		if(str[i] == c)
			return false;
	}
	return true;
}

int sign_place(string *str, size_t start) {
	size_t i = str->find_first_of(OPERATORS, start);
	if(i != string::npos)
		return i;
	else
		return -1;
}

int gcd(int i1, int i2) {
	if(i1 < 0) i1 = -i1;
	if(i2 < 0) i2 = -i2;
	if(i1 == i2) return i2;
	int i3;
	if(i2 > i1) {
		i3 = i2;
		i2 = i1;
		i1 = i3;
	}
	while((i3 = i1 % i2) != 0) {
		i1 = i2;
		i2 = i3;
	}
	return i2;
}

size_t unicode_length(const string &str) {
	size_t l = str.length(), l2 = 0;
	for(size_t i = 0; i < l; i++) {
		if(str[i] > 0 || (unsigned char) str[i] >= 0xC2) {
			l2++;
		}
	}
	return l2;
}
size_t unicode_length(const char *str) {
	size_t l = strlen(str), l2 = 0;
	for(size_t i = 0; i < l; i++) {
		if(str[i] > 0 || (unsigned char) str[i] >= 0xC2) {
			l2++;
		}
	}
	return l2;
}

bool text_length_is_one(const string &str) {
	if(str.empty()) return false;
	if(str.length() == 1) return true;
	if(str[0] >= 0) return false;
	for(size_t i = 1; i < str.length(); i++) {
		if(str[i] > 0 || (unsigned char) str[i] >= 0xC2) {
			return false;
		}
	}
	return true;
}

bool equalsIgnoreCase(const string &str1, const string &str2) {
	if(str1.empty() || str2.empty()) return false;
	for(size_t i1 = 0, i2 = 0; i1 < str1.length() || i2 < str2.length(); i1++, i2++) {
		if(i1 >= str1.length() || i2 >= str2.length()) return false;
		if((str1[i1] < 0 && i1 + 1 < str1.length()) || (str2[i2] < 0 && i2 + 1 < str2.length())) {
			size_t iu1 = 1, iu2 = 1;
			if(str1[i1] < 0) {
				while(iu1 + i1 < str1.length() && str1[i1 + iu1] < 0) {
					iu1++;
				}
			}
			if(str2[i2] < 0) {
				while(iu2 + i2 < str2.length() && str2[i2 + iu2] < 0) {
					iu2++;
				}
			}
			bool isequal = (iu1 == iu2);
			if(isequal) {
				for(size_t i = 0; i < iu1; i++) {
					if(str1[i1 + i] != str2[i2 + i]) {
						isequal = false;
						break;
					}
				}
			}
			if(!isequal) {
				char *gstr1 = utf8_strdown(str1.c_str() + (sizeof(char) * i1));
				char *gstr2 = utf8_strdown(str2.c_str() + (sizeof(char) * i2));
				if(!gstr1 || !gstr2) return false;
				bool b = strcmp(gstr1, gstr2) == 0;
				free(gstr1);
				free(gstr2);
				return b;
			}
			i1 += iu1 - 1;
			i2 += iu2 - 1;
		} else if(str1[i1] != str2[i2] && !((str1[i1] >= 'a' && str1[i1] <= 'z') && str1[i1] - 32 == str2[i2]) && !((str1[i1] <= 'Z' && str1[i1] >= 'A') && str1[i1] + 32 == str2[i2])) {
			return false;
		}
	}
	return true;
}

bool equalsIgnoreCase(const string &str1, const char *str2) {
	if(str1.empty() || strlen(str2) == 0) return false;
	for(size_t i1 = 0, i2 = 0; i1 < str1.length() || i2 < strlen(str2); i1++, i2++) {
		if(i1 >= str1.length() || i2 >= strlen(str2)) return false;
		if((str1[i1] < 0 && i1 + 1 < str1.length()) || (str2[i2] < 0 && i2 + 1 < strlen(str2))) {
			size_t iu1 = 1, iu2 = 1;
			if(str1[i1] < 0) {
				while(iu1 + i1 < str1.length() && str1[i1 + iu1] < 0) {
					iu1++;
				}
			}
			if(str2[i2] < 0) {
				while(iu2 + i2 < strlen(str2) && str2[i2 + iu2] < 0) {
					iu2++;
				}
			}
			bool isequal = (iu1 == iu2);
			if(isequal) {
				for(size_t i = 0; i < iu1; i++) {
					if(str1[i1 + i] != str2[i2 + i]) {
						isequal = false;
						break;
					}
				}
			}
			if(!isequal) {
				char *gstr1 = utf8_strdown(str1.c_str() + (sizeof(char) * i1));
				char *gstr2 = utf8_strdown(str2 + (sizeof(char) * i2));
				if(!gstr1 || !gstr2) return false;
				bool b = strcmp(gstr1, gstr2) == 0;
				free(gstr1);
				free(gstr2);
				return b;
			}
			i1 += iu1 - 1;
			i2 += iu2 - 1;
		} else if(str1[i1] != str2[i2] && !((str1[i1] >= 'a' && str1[i1] <= 'z') && str1[i1] - 32 == str2[i2]) && !((str1[i1] <= 'Z' && str1[i1] >= 'A') && str1[i1] + 32 == str2[i2])) {
			return false;
		}
	}
	return true;
}

void parse_qalculate_version(string qalculate_version, int *qalculate_version_numbers) {
	for(size_t i = 0; i < 3; i++) {
		size_t dot_i = qalculate_version.find(".");
		if(dot_i == string::npos) {
			qalculate_version_numbers[i] = s2i(qalculate_version);
			break;
		}
		qalculate_version_numbers[i] = s2i(qalculate_version.substr(0, dot_i));
		qalculate_version = qalculate_version.substr(dot_i + 1, qalculate_version.length() - (dot_i + 1));
	}
}

#ifdef _WIN32
string utf8_encode(const wstring &wstr) {
	if(wstr.empty()) return string();
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int) wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int) wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}
#endif

string getOldLocalDir() {
#ifdef _WIN32
	char path[MAX_PATH];
	SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path);
	string str = path;
	return str + "\\Qalculate";
#else
	const char *homedir;
	if ((homedir = getenv("HOME")) == NULL) {
		homedir = getpwuid(getuid())->pw_dir;
	}
	return string(homedir) + "/.qalculate";
#endif
}
string getLocalDir() {
#ifdef _WIN32
	char path[MAX_PATH];
	SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, path);
	string str = path;
	return str + "\\Qalculate";
#else
	const char *homedir;
	if((homedir = getenv("XDG_CONFIG_HOME")) == NULL) {
		return string(getpwuid(getuid())->pw_dir) + "/.config/qalculate";
	}
	return string(homedir) + "/qalculate";
#endif
}
string getLocalDataDir() {
#ifdef _WIN32
	char path[MAX_PATH];
	SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, path);
	string str = path;
	return str + "\\Qalculate";
#else
	const char *homedir;
	if((homedir = getenv("XDG_DATA_HOME")) == NULL) {
		return string(getpwuid(getuid())->pw_dir) + "/.local/share/qalculate";
	}
	return string(homedir) + "/qalculate";
#endif
}
string getLocalTmpDir() {
#ifdef _WIN32
	char path[MAX_PATH];
	SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, path);
	string str = path;
	return str + "\\cache\\Qalculate";
#else
	const char *homedir;
	if((homedir = getenv("XDG_CACHE_HOME")) == NULL) {
		return string(getpwuid(getuid())->pw_dir) + "/.cache/qalculate";
	}
	return string(homedir) + "/qalculate";
#endif
}

bool move_file(const char *from_file, const char *to_file) {
#ifdef _WIN32
	return MoveFile(from_file, to_file) == 0;
#else
	ifstream source(from_file);
	if(source.fail()) {
		source.close();
		return false;
	}

	ofstream dest(to_file);
	if(dest.fail()) {
		source.close();
		dest.close();
		return false;
	}

	dest << source.rdbuf();

	source.close();
	dest.close();
	
	struct stat stats_from;
	if(stat(from_file, &stats_from) == 0) {
		struct utimbuf to_times;
		to_times.actime = stats_from.st_atime;
		to_times.modtime = stats_from.st_mtime;
		utime(to_file, &to_times);
	}
	
	remove(from_file);
	
	return true;
#endif
}

string getPackageDataDir() {
#ifndef WIN32
	return PACKAGE_DATA_DIR;
#else
	char exepath[MAX_PATH];
	GetModuleFileName(NULL, exepath, MAX_PATH);
	string datadir(exepath);
	datadir.resize(datadir.find_last_of('\\'));
	if (datadir.substr(datadir.length() - 4) == "\\bin") {
		datadir.resize(datadir.find_last_of('\\'));
		datadir += "\\share";
	} else if(datadir.substr(datadir.length() - 6) == "\\.libs") {
		datadir.resize(datadir.find_last_of('\\'));
		datadir.resize(datadir.find_last_of('\\'));
		return datadir;
	}
	return datadir;
#endif
}

string getGlobalDefinitionsDir() {
#ifndef WIN32
	return string(PACKAGE_DATA_DIR) + "/qalculate";
#else
	char exepath[MAX_PATH];
	GetModuleFileName(NULL, exepath, MAX_PATH);
	string datadir(exepath);
	bool is_qalc = datadir.substr(datadir.length() - 8) == "qalc.exe";
	datadir.resize(datadir.find_last_of('\\'));
	if(datadir.substr(datadir.length() - 4) == "\\bin") {
		datadir.resize(datadir.find_last_of('\\'));
		datadir += "\\share\\qalculate";
		return datadir;
	} else if(datadir.substr(datadir.length() - 6) == "\\.libs") {
		datadir.resize(datadir.find_last_of('\\'));
		datadir.resize(datadir.find_last_of('\\'));
		if(!is_qalc) {
			datadir.resize(datadir.find_last_of('\\'));
			datadir += "\\libqalculate";
			if(!dirExists(datadir)) {
				datadir += "-";
				datadir += VERSION;
			}
		}
		return datadir + "\\data";
	}
	return datadir + "\\definitions";
#endif
}

string getPackageLocaleDir() {
#ifndef WIN32
	return PACKAGE_LOCALE_DIR;
#else
	char exepath[MAX_PATH];
	GetModuleFileName(NULL, exepath, MAX_PATH);
	string datadir(exepath);
	datadir.resize(datadir.find_last_of('\\'));
	if (datadir.substr(datadir.length() - 4) == "\\bin" || datadir.substr(datadir.length() - 6) == "\\.libs") {
		datadir.resize(datadir.find_last_of('\\'));
		return datadir + "\\share\\locale";
	}
	return datadir + "\\locale";
#endif
}

string buildPath(string dir, string filename) {
#ifdef WIN32
	return dir + '\\' + filename;
#else
	return dir + '/' + filename;
#endif
}
string buildPath(string dir1, string dir2, string filename) {
#ifdef WIN32
	return dir1 + '\\' + dir2 + '\\' + filename;
#else
	return dir1 + '/' + dir2 + '/' + filename;
#endif
}
string buildPath(string dir1, string dir2, string dir3, string filename) {
#ifdef WIN32
	return dir1 + '\\' + dir2 + '\\' + dir3 + '\\' + filename;
#else
	return dir1 + '/' + dir2 + '/' + dir3 + '/' + filename;
#endif
}

bool dirExists(string dirpath) {
	return fileExists(dirpath);
}
bool fileExists(string filepath) {
#ifdef WIN32
	struct _stat info;
	return _stat(filepath.c_str(), &info) == 0;
#else
	struct stat info;
	return stat(filepath.c_str(), &info) == 0;
#endif
}
bool makeDir(string dirpath) {
#ifdef WIN32
	return _mkdir(dirpath.c_str()) == 0;
#else
	return mkdir(dirpath.c_str(), S_IRWXU) == 0;
#endif
}
bool removeDir(string dirpath) {
#ifdef WIN32
	return _rmdir(dirpath.c_str()) == 0;
#else
	return rmdir(dirpath.c_str()) == 0;
#endif
}

char *locale_from_utf8(const char *str) {
	iconv_t conv = iconv_open("", "UTF-8");
	if(conv == (iconv_t) -1) return NULL;
	size_t inlength = strlen(str);
	size_t outlength = inlength * 4;
	char *dest, *buffer;
	buffer = dest = (char*) malloc((outlength + 4) * sizeof(char));
	if(!buffer) return NULL;
	size_t err = iconv(conv, (char **) &str, &inlength, &buffer, &outlength);
	if(err != (size_t) -1) err = iconv(conv, NULL, &inlength, &buffer, &outlength);
	iconv_close(conv);
	memset(buffer, 0, 4);
	if(err == (size_t) -1) {free(dest); return NULL;}
	return dest;
}
char *locale_to_utf8(const char *str) {
	iconv_t conv = iconv_open("UTF-8", "");
	if(conv == (iconv_t) -1) return NULL;
	size_t inlength = strlen(str);
	size_t outlength = inlength * 4;
	char *dest, *buffer;
	buffer = dest = (char*) malloc((outlength + 4) * sizeof(char));
	if(!buffer) return NULL;
	size_t err = iconv(conv, (char**) &str, &inlength, &buffer, &outlength);
	if(err != (size_t) -1) err = iconv(conv, NULL, &inlength, &buffer, &outlength);
	iconv_close(conv);
	memset(buffer, 0, 4 * sizeof(char));
	if(err == (size_t) -1) {free(dest); return NULL;}
	return dest;
}
char *utf8_strdown(const char *str, int l) {
#ifdef HAVE_ICU
	if(!ucm) return NULL;
	UErrorCode err = U_ZERO_ERROR;
	size_t inlength = l <= 0 ? strlen(str) : (size_t) l;
	size_t outlength = inlength + 4;
	char *buffer = (char*) malloc(outlength * sizeof(char));
	int32_t length = ucasemap_utf8ToLower(ucm, buffer, outlength, str, inlength, &err);
	if(U_SUCCESS(err)) {
		return buffer;
	} else if(err == U_BUFFER_OVERFLOW_ERROR) {
		outlength = length + 4;
		buffer = (char*) realloc(buffer, outlength * sizeof(char));
		err = U_ZERO_ERROR;
		ucasemap_utf8ToLower(ucm, buffer, outlength, str, inlength, &err);
		if(U_SUCCESS(err)) {
			return buffer;
		}
	}
	cout << u_errorName(err) << endl;
	return NULL;
#else
	return NULL;
#endif
}

#ifdef _WIN32

Thread::Thread() : running(false), m_thread(NULL), m_threadReadyEvent(NULL), m_threadID(0) {
	m_threadReadyEvent = CreateEvent(NULL, false, false, NULL);
}

Thread::~Thread() {
	CloseHandle(m_threadReadyEvent);
}

void Thread::enableAsynchronousCancel() {}

DWORD WINAPI Thread::doRun(void *data) {
	// create thread message queue
	MSG msg;
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

	Thread *thread = (Thread *) data;
	SetEvent(thread->m_threadReadyEvent);
	thread->run();
	thread->running = false;
	return 0;
}

bool Thread::start() {
	m_thread = CreateThread(NULL, 0, Thread::doRun, this, 0, &m_threadID);
	running = (m_thread != NULL);
	if(!running) return false;
	WaitForSingleObject(m_threadReadyEvent, INFINITE);
	return running;
}

bool Thread::cancel() {
	// FIXME: this is dangerous
	int ret = TerminateThread(m_thread, 0);
	if (ret == 0) return false;
	CloseHandle(m_thread);
	m_thread = NULL;
	m_threadID = 0;
	running = false;
	return true;
}

#else

Thread::Thread() : running(false), m_pipe_r(NULL), m_pipe_w(NULL) {
	pthread_attr_init(&m_thread_attr);
	int pipe_wr[] = {0, 0};
	pipe(pipe_wr);
	m_pipe_r = fdopen(pipe_wr[0], "r");
	m_pipe_w = fdopen(pipe_wr[1], "w");
}

Thread::~Thread() {
	fclose(m_pipe_r);
	fclose(m_pipe_w);
	pthread_attr_destroy(&m_thread_attr);
}

void Thread::doCleanup(void *data) {
	Thread *thread = (Thread *) data;
	thread->running = false;
}

void Thread::enableAsynchronousCancel() {
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
}

void *Thread::doRun(void *data) {

	pthread_cleanup_push(&Thread::doCleanup, data);

	Thread *thread = (Thread *) data;
	thread->run();

	pthread_cleanup_pop(1);
	return NULL;

}

bool Thread::start() {
	if(running) return true;
	running = pthread_create(&m_thread, &m_thread_attr, &Thread::doRun, this) == 0;
	return running;
}

bool Thread::cancel() {
	if(!running) return true;
	running = pthread_cancel(m_thread) != 0;
	return !running;
}


#endif
