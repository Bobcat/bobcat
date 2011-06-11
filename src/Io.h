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

class Logger {
public:
	Logger() : file(NULL), logfile(NULL) {
	}

	~Logger() {
		cleanup();
	}

	void open(const char* filename) {
		cleanup();
		if (filename) {
			logfile = new char[strlen(filename) + 1];
			strcpy(logfile, filename);
			file = fopen(logfile, "w+t");
		}
	}

	void log(const char* text) {
		writeLine(text);
	}

protected:
	void write(const char* text) {
		if (file <= 0) {
			//printf(text);
			return;
		}
		fwrite(text, 1, strlen(text), file);
		fflush(file);
	}

	void writeLine(const char* text) {
		write(text);
		write("\n");
	}

	void cleanup() {
		if (file > 0) {
			fclose(file);
		}
		file = 0;
		delete [] logfile;
		logfile = NULL;
	}

	char* logfile;
	FILE* file;
};

class StdIn {
public:
	StdIn() {
		initialise();
	}

	int getLine(bool blocking, char* line) { 
		line[0] = '\0';
		if (!blocking) {
			if (!isAvailable()) {
				return 0; 
			}
		}
		getLine(line, 16384, stdin);
		return 1;
	} 

protected:
	void initialise() { 
		DWORD dw; 
		handle = GetStdHandle(STD_INPUT_HANDLE); 
		is_pipe = !GetConsoleMode(handle, &dw); 
	}

	int isAvailable() { 
		DWORD bytes_read; 
		if (is_pipe) { 
			if (!PeekNamedPipe(handle, NULL, 0, NULL, &bytes_read, NULL)) {
				return 0; 
			}
			return bytes_read; 
		} 
		return _kbhit(); 
	} 

	int getLine(char* line, int size, FILE* stream) {
		int c;
		int len = 0;
		do {
			c = fgetc(stream);
			if (c == EOF) {
				exit(1);
			}
			else if (c == '\n') {
				break;
			}
			else if (len < size - 1) { 
				line[len++] = (char)c;
			}
		} while (true);
		line[len] = 0;
		return len;
	}

	HANDLE handle;
	bool is_pipe;
};

class StdOut {
public:
	void writeLine(const char* line) {
		printf("%s\n", line);
	}

	void write(const char* text) {
		printf("%s", text);
	}
};
