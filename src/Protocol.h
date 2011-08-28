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

const int FIXED_MOVE_TIME = 1;
const int FIXED_DEPTH = 2;
const int INFINITE_MOVE_TIME = 4;
const int PONDER_SEARCH = 8;

class ProtocolListener {
public:
	virtual int newGame() = 0; 
	virtual int setFEN(const char* fen) = 0;
	virtual int go(int wtime, int btime, int movestogo, int winc, int binc, int movetime) = 0;
	virtual int ponderHit() = 0;
	virtual int stop() = 0;
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

	virtual int handleInput(char* params[], int num_params) = 0;
	virtual void checkInput() = 0;
	virtual void postMoves(const char* bestmove, const char* pondermove) = 0;

	virtual void postInfo(const int depth, int selective_depth, uint64 node_count, 
		uint64 nodes_per_sec, uint64 time, int hash_full) = 0;

	virtual void postInfo(const Move curr_move, int curr_move_number) = 0;
	
	virtual void postPV(const int depth, int max_ply, uint64 node_count, uint64 nodes_per_second, uint64 time, int hash_full, 
		int score, const char* pv) = 0; 

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
		if (stricmp(trim(line), "stop") == 0) {
			flags &= ~(INFINITE_MOVE_TIME | PONDER_SEARCH);
			callback->stop();
		}
		else if (stricmp(trim(line), "ponderhit") == 0) {
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

	virtual void postInfo(const int depth, int selective_depth, uint64 node_count, uint64 nodes_per_sec, uint64 time, 
		int hash_full) 
	{
		char buf[1024];
		snprintf(buf, sizeof(buf), 
			"info depth %d " \
			"seldepth %d " \
			"hashfull %d " \
			"nodes %llu " \
			"nps %llu " \
			"time %llu", 
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
			moveToString(curr_move, move_buf), 
			curr_move_number);

		output->writeLine(buf);
	}

	virtual void postPV(const int depth, int max_ply, uint64 node_count, uint64 nodes_per_second, uint64 time, int hash_full, 
				int score, const char* pv) 
	{
		char buf[1024];
		snprintf(buf, sizeof(buf), 
			"info depth %d "
			"seldepth %d "
			"score cp %d "
			"hashfull %d "
			"nodes %llu "
			"nps %llu "
			"time %llu "
			"pv %s", 
			depth, 
			max_ply, 
			score, 
			hash_full, 
			node_count, 
			nodes_per_second, 
			time, 
			pv);

		output->writeLine(buf);
	}

	virtual int handleInput(char* params[], int num_params) {
		if (num_params < 1) {
			return -1;
		}
		if (stricmp(params[0], "uci") == 0) {
			char buf[2048];
			snprintf(buf, sizeof(buf),  
				"id name Bobcat 2.75 (1)\n" \
				"id author Gunnar Harms\n" \
				"option name Hash type spin default 256 min 8 max 1024\n" \
				"option name Ponder type check default true\n" \
				"option name Threads type spin default 1 min 1 max 4\n" \
				"uciok");

			output->writeLine(buf);
		}
		else if (stricmp(params[0], "isready") == 0) {
			printf("readyok\n");
		}
		else if (stricmp(params[0], "ucinewgame") == 0) {
			callback->newGame();
			printf("readyok\n");
		}
		else if (stricmp(params[0], "position") == 0) {
			handlePosition(params, num_params);
		}
		else if (stricmp(params[0], "go") == 0) {
			handleGo(params, num_params, callback);
		}
		else if (stricmp(params[0], "setoption") == 0) {
			handleSetOption(params, num_params);
		}
		else if (stricmp(params[0], "quit") == 0) {
			return 1;
		}
		return 0;
	}

	int handleGo(char* params[], int num_params, ProtocolListener* callback) {
		int wtime = 0;
		int btime = 0;
		int winc = 0;
		int binc = 0;
		int movetime = 0;
		int movestogo = 0;

		flags = 0;
		for (int param = 1; param < num_params; param++) {
			if (stricmp(params[param], "movetime") == 0) {
				flags |= FIXED_MOVE_TIME;
				if (++param < num_params) {
					movetime = strtol(params[param], NULL, 10);
				}
			}
			else if (stricmp(params[param], "depth") == 0) {
				flags |= FIXED_DEPTH;
				if (++param < num_params) {
					depth = strtol(params[param], NULL, 10);
				}
			}
			else if (stricmp(params[param], "wtime") == 0) {
				if (++param < num_params) {
					wtime = strtol(params[param], NULL, 10);
				}
			}
			else if (stricmp(params[param], "movestogo") == 0) {
				if (++param < num_params) {
					movestogo = strtol(params[param], NULL, 10);
				}
			}
			else if (stricmp(params[param], "btime") == 0) {
				if (++param < num_params) {
					btime = strtol(params[param], NULL, 10);
				}
			}
			else if (stricmp(params[param], "winc") == 0) {
				if (++param < num_params) {
					winc = strtol(params[param], NULL, 10);
				}
			}
			else if (stricmp(params[param], "binc") == 0) {
				if (++param < num_params) {
					binc = strtol(params[param], NULL, 10);
				}
			}
			else if (stricmp(params[param], "infinite") == 0) {
				flags |= INFINITE_MOVE_TIME;
			}
			else if (stricmp(params[param], "ponder") == 0) {
				flags |= PONDER_SEARCH;
			}
		}
		callback->go(wtime, btime, movestogo, winc, binc, movetime);
		return 0;
	}

	int handlePosition(char* params[], int num_params) {
		int param = 1;
		if (num_params < param + 1) {
			return -1;
		}
		char fen[128];
		if (stricmp(params[param], "startpos") == 0) {
			strcpy(fen, game->START_POSITION);
			param++;
		}
		else if (stricmp(params[param], "fen") == 0) {
			if (!FENfromParams(params, num_params, param, fen)) {
				return -1;
			}
			param++;
		}
		else {
			return -1;
		}
		callback->setFEN(fen);
		if ((num_params > param) && (stricmp(params[param++], "moves") == 0)) {
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

	bool handleSetOption(char* params[], int num_params) {
		int param = 1;
		char* option_name;
		char* option_value;
		if (parseOptionName(param, params, num_params, &option_name) && 
				parseOptionValue(param, params, num_params, &option_value)) 
		{
			return callback->setOption(option_name, option_value) == 0;
		}
		return false;
	}

	bool parseOptionName(int& param, char* params[], int num_params, char** option_name) {
		*option_name = NULL;
		while (param < num_params) {
			if (stricmp(params[param++], "name") == 0) {
				break;
			}
		}
		if (param < num_params) {
			*option_name = params[param++];
		}
		return *option_name != NULL;
	}

	bool parseOptionValue(int& param, char* params[], int num_params, char** option_value) {
		*option_value = NULL;
		while (param < num_params) {
			if (stricmp(params[param++], "value") == 0) {
				break;
			}
		}
		if (param < num_params) {
			*option_value = params[param++];
		}
		return true;
	}
};
