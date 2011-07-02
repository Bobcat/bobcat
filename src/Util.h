/*
  This file is part of Bobcat.
  Copyright 2008-2011 Gunnar Harms

  Bobcat is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Bobcat is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Bobcat.  If not, see <http://www.gnu.org/licenses/>.
*/

bool iswhitespace(char c) {
	return c == ' ' || c == '\t' || (int)c == 10 || (int)c == 13;
}

char* rtrim(char* buf) {
	while (strlen(buf) && iswhitespace(buf[strlen(buf) - 1])) {
		buf[strlen(buf) - 1] = 0;
	}
	return buf;
}

char* ltrim(char* buf) {
	while (iswhitespace(buf[0])) {
		memcpy(buf, buf + 1, strlen(buf));
	}
	return buf;
}

char* trim(char* buf) {
	return ltrim(rtrim(buf));
}

int tokenize(char* input, char* tokens[], int max_tokens) {
	int num_tokens = 0;
	char* token = strtok(input, " ");
	while (token != NULL && num_tokens < max_tokens) {
		tokens[num_tokens] = new char[strlen(token) + 1];
		strcpy(tokens[num_tokens++], token);
		token = strtok(NULL, " ");
	}
	return num_tokens;
}

static uint64 rand64() {
	return genrand64_int64();
}

static int log2(unsigned int x) {
	return (int)(log((double)x)/log(2.0));
}

static int pow2(int x) {
	return (int)pow(2.0, x);
}

const char* FENfromParams(char* params[], int num_params, int& param, char* fen) {
	if ((num_params - param - 1) < 6) {
		return NULL;
	}
	fen[0] = 0;
	for (int i = 0; i < 6; i++) {
		if (strlen(fen) + strlen(params[++param]) + 1 >= 128) {
			return NULL;
		}
		sprintf(&fen[strlen(fen)], "%s ", params[param]);
	}
	return fen;
}

#if defined(_MSC_VER)

__forceinline uint64 millis() {
	return GetTickCount();
}

struct Runnable {
  virtual void run() = 0;
};

class Thread {
public:
	Thread(Runnable* runnable) : handle(NULL) {
		this->runnable = runnable;
	}

	~Thread() {
		close();
	}

	void start() {
		DWORD threadId;
		handle = ::CreateThread(0, 0, threadProc, runnable, 0, &threadId);
	}

	void close() {
		if (handle) {
			::CloseHandle(handle);
		}
	}
 
protected:
	static DWORD __stdcall threadProc(LPVOID param) {
		((Runnable*)param)->run();
		return 0;
	}

	HANDLE handle;
	Runnable* runnable; 
};

#endif // defined(_MSCVER)
