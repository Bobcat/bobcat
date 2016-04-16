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

const int FIXED_MOVE_TIME = 1;
const int FIXED_DEPTH = 2;
const int INFINITE_MOVE_TIME = 4;
const int PONDER_SEARCH = 8;

class ProtocolListener {
public:
  virtual int newGame() = 0;
  virtual int setFen(const char* fen) = 0;
  virtual int go(int wtime, int btime, int movestogo, int winc, int binc, int movetime) = 0;
  virtual void ponderHit() = 0;
  virtual void stop() = 0;
  virtual int setOption(const char* name, const char* value) = 0;
};

class Protocol {
public:
  Protocol(ProtocolListener* callback, Game* game, StdIn* input, StdOut* output) {
    this->callback = callback;
    this->game = game;
    this->input = input;
    this->output = output;
  }

  virtual ~Protocol() {
  }

  virtual int handleInput(const char* params[], int num_params) = 0;
  virtual void checkInput() = 0;
  virtual void postMoves(const char* bestmove, const char* pondermove) = 0;

  virtual void postInfo(const int depth, int selective_depth, uint64_t node_count, uint64_t nodes_per_sec, uint64_t time,
                        int hash_full) = 0;

  virtual void postInfo(const Move curr_move, int curr_move_number) = 0;

  virtual void postPV(const int depth, int max_ply, uint64_t node_count, uint64_t nodes_per_second, uint64_t time,
                      int hash_full, int score, const char* pv, int node_type) = 0;

  __forceinline int isAnalysing() {
    return flags & (INFINITE_MOVE_TIME | PONDER_SEARCH);
  }

  __forceinline int isFixedTime() {
    return flags & FIXED_MOVE_TIME;
  }

  __forceinline int isFixedDepth() {
    return flags & FIXED_DEPTH;
  }

  __forceinline int getDepth() {
    return depth;
  }

  __forceinline void setFlags(int flags) {
    this->flags = flags;
  }

protected:
  int flags;
  int depth;
  ProtocolListener* callback;
  StdIn* input;
  StdOut* output;
  Game* game;
};

class UCIProtocol : public Protocol {
public:
  UCIProtocol(ProtocolListener* callback, Game* game, StdIn* input, StdOut* output) :
    Protocol(callback, game, input, output)
  {
  }

  virtual void checkInput() {
    char line[16384];
    input->getLine(false, line);
    if (strieq(trim(line), "stop")) {
      flags &= ~(INFINITE_MOVE_TIME | PONDER_SEARCH);
      callback->stop();
    }
    else if (strieq(trim(line), "ponderhit")) {
      flags &= ~(INFINITE_MOVE_TIME | PONDER_SEARCH);
      callback->ponderHit();
    }
  }

  void postMoves(const char* bestmove, const char* pondermove) {
    while (flags & (INFINITE_MOVE_TIME | PONDER_SEARCH)) {
      Sleep(10);
      checkInput();
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "bestmove %s", bestmove);
    if (pondermove) {
      snprintf(&buf[strlen(buf)], sizeof(buf) - strlen(buf), " ponder %s", pondermove);
    }
    output->writeLine(buf);
  }

  virtual void postInfo(const int depth, int selective_depth, uint64_t node_count, uint64_t nodes_per_sec,
                        uint64_t time, int hash_full)
  {
    char buf[1024];
    _snprintf(buf, sizeof(buf),
              "info depth %d " \
              "seldepth %d " \
              "hashfull %d " \
              "nodes %" PRIu64 " " \
              "nps %" PRIu64 " " \
              "time %" PRIu64 "",
              depth,
              selective_depth,
              hash_full,
              node_count,
              nodes_per_sec,
              time);

    output->writeLine(buf);
  }

  virtual void postInfo(const Move curr_move, int curr_move_number) {
    char buf[1024];
    char move_buf[32];

    snprintf(buf, sizeof(buf),
             "info currmove %s " \
             "currmovenumber %d ",
             game->moveToString(curr_move, move_buf),
             curr_move_number);

    output->writeLine(buf);
  }

  virtual void postPV(const int depth, int max_ply, uint64_t node_count, uint64_t nodes_per_second,
                      uint64_t time, int hash_full, int score, const char* pv, int node_type)
  {
    char bound[24];

    if (node_type == 4) {
      strcpy(bound, "upperbound ");
    }
    else if (node_type == 2) {
      strcpy(bound, "lowerbound ");
    }
    else {
      bound[0] = 0;
    }
    char buf[1024];

    _snprintf(buf, sizeof(buf),
              "info depth %d "
              "seldepth %d "
              "score cp %d "
              "%s"
              "hashfull %d "
              "nodes %" PRIu64 " "
              "nps %" PRIu64 " "
              "time %" PRIu64 " "
              "pv %s",
              depth,
              max_ply,
              score,
              bound,
              hash_full,
              node_count,
              nodes_per_second,
              time,
              pv);

    output->writeLine(buf);
  }

  virtual int handleInput(const char* params[], int num_params) {
    if (num_params < 1) {
      return -1;
    }
    if (strieq(params[0], "uci")) {
      char buf[2048];
      snprintf(buf, sizeof(buf),
               "id name Bobcat 7.1\n" \
               "id author Gunnar Harms\n" \
               "option name Hash type spin default 256 min 8 max 65536\n" \
               "option name Ponder type check default true\n" \
               "option name Threads type spin default 1 min 1 max 64\n" \
               "option name UCI_Chess960 type check default false\n" \
               "uciok");

      output->writeLine(buf);
    }
    else if (strieq(params[0], "isready")) {
      printf("readyok\n");
    }
    else if (strieq(params[0], "ucinewgame")) {
      callback->newGame();
      printf("readyok\n");
    }
    else if (strieq(params[0], "position")) {
      handlePosition(params, num_params);
    }
    else if (strieq(params[0], "go")) {
      handleGo(params, num_params, callback);
    }
    else if (strieq(params[0], "setoption")) {
      handleSetOption(params, num_params);
    }
    else if (strieq(params[0], "quit")) {
      return 1;
    }
    return 0;
  }

  int handleGo(const char* params[], int num_params, ProtocolListener* callback) {
    int wtime = 0;
    int btime = 0;
    int winc = 0;
    int binc = 0;
    int movetime = 0;
    int movestogo = 0;

    flags = 0;
    for (int param = 1; param < num_params; param++) {
      if (strieq(params[param], "movetime")) {
        flags |= FIXED_MOVE_TIME;
        if (++param < num_params) {
          movetime = strtol(params[param], NULL, 10);
        }
      }
      else if (strieq(params[param], "depth")) {
        flags |= FIXED_DEPTH;
        if (++param < num_params) {
          depth = strtol(params[param], NULL, 10);
        }
      }
      else if (strieq(params[param], "wtime")) {
        if (++param < num_params) {
          wtime = strtol(params[param], NULL, 10);
        }
      }
      else if (strieq(params[param], "movestogo")) {
        if (++param < num_params) {
          movestogo = strtol(params[param], NULL, 10);
        }
      }
      else if (strieq(params[param], "btime")) {
        if (++param < num_params) {
          btime = strtol(params[param], NULL, 10);
        }
      }
      else if (strieq(params[param], "winc")) {
        if (++param < num_params) {
          winc = strtol(params[param], NULL, 10);
        }
      }
      else if (strieq(params[param], "binc")) {
        if (++param < num_params) {
          binc = strtol(params[param], NULL, 10);
        }
      }
      else if (strieq(params[param], "infinite")) {
        flags |= INFINITE_MOVE_TIME;
      }
      else if (strieq(params[param], "ponder")) {
        flags |= PONDER_SEARCH;
      }
    }
    callback->go(wtime, btime, movestogo, winc, binc, movetime);
    return 0;
  }

  int handlePosition(const char* params[], int num_params) {
    int param = 1;
    if (num_params < param + 1) {
      return -1;
    }
    char fen[128];
    if (strieq(params[param], "startpos")) {
      strcpy(fen, Game::kStartPosition);
      param++;
    }
    else if (strieq(params[param], "fen")) {
      if (!FENfromParams(params, num_params, param, fen)) {
        return -1;
      }
      param++;
    }
    else {
      return -1;
    }
    callback->setFen(fen);
    if ((num_params > param) && (strieq(params[param++], "moves"))) {
      while (param < num_params) {
        const Move* move = game->pos->stringToMove(params[param++]);
        if (move == 0) {
          return -1;
        }
        game->makeMove(*move, true, true);
      }
    }
    return 0;
  }

  bool handleSetOption(const char* params[], int num_params) {
    int param = 1;
    const char* option_name;
    const char* option_value;
    if (parseOptionName(param, params, num_params, &option_name) &&
        parseOptionValue(param, params, num_params, &option_value))
    {
      return callback->setOption(option_name, option_value) == 0;
    }
    return false;
  }

  bool parseOptionName(int& param, const char* params[], int num_params, const char** option_name) {
    while (param < num_params) {
      if (strieq(params[param++], "name")) {
        break;
      }
    }
    if (param < num_params) {
      *option_name = params[param++];
      return *option_name != '\0';
    }
    return false;
  }

  bool parseOptionValue(int& param, const char* params[], int num_params, const char** option_value) {
    *option_value = NULL;
    while (param < num_params) {
      if (strieq(params[param++], "value")) {
        break;
      }
    }

    if (param < num_params) {
      *option_value = params[param++];
      return *option_value != NULL;
    }
    return true;
  }
};
