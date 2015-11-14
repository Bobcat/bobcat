/*
  This file is part of Bobcat.
  Copyright 2008-2015 Gunnar Harms

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

  void logts(const char* text) {
    char buf1[4096], buf2[64];
    snprintf(buf1, sizeof(buf1), "%s %s", timeString(buf2), text);
    writeLine(buf1);
  }

  void logts(const char* text1, const char* text2) {
    char buf1[4096], buf2[64];
    snprintf(buf1, sizeof(buf1), "%s %s%s", timeString(buf2), text1, text2);
    writeLine(buf1);
  }

protected:
  void write(const char* text) {
    if (file == NULL) {
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
    if (file != NULL) {
      fclose(file);
    }
    file = 0;
    delete [] logfile;
    logfile = NULL;
  }

  FILE* file;
  char* logfile;
};

class StdIn {
public:
  StdIn(Logger* logger) {
    initialise();
    this->logger = logger;
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
  StdIn() {
  }

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
    if (logger) {
      logger->logts("<", line);
    }
    return len;
  }

  HANDLE handle;
  bool is_pipe;
  Logger* logger;
};

class StdOut {
public:
  StdOut(Logger* logger) {
    this->logger = logger;
  }

  void writeLine(const char* line) {
    printf("%s\n", line);
    if (logger) {
      logger->logts(">", line);
    }
  }

protected:
  Logger* logger;
};
