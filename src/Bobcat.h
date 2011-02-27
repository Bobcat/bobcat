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

class Bobcat : public ProtocolListener {
public:
	Bobcat() : num_threads(2) {
	}

	virtual ~Bobcat() {
	}

	virtual int newGame() {
		game->newGame(game->START_POSITION);
		search->newGame();
		eval->newGame();
		pawnt->clear();
		transt->clear();
		return 0;
	}

	virtual int setFen(const char* fen) {
		return game->newGame(fen);
	}

	virtual int go(int wtime = 0, int btime = 0, int movestogo = 0, 
				int winc = 0, int binc = 0, int movetime = 5000) 
	{
		game->pos->pv_length = 0;
		if (!protocol->isAnalysing()) {
			goBook();
		}
		if (game->pos->pv_length == 0) {
			goSearch(wtime, btime, movestogo, winc, binc, movetime);
		}
		if (game->pos->pv_length) {
			char best_move[12], ponder_move[12];
			protocol->postMoves(moveToString(search->pv[0][0].move, best_move), 
				game->pos->pv_length > 1 ? moveToString(search->pv[0][1].move, ponder_move) : 0);
			game->makeMove(search->pv[0][0].move);
		}
		return 0;
	}

	virtual int ponderHit() {
		if (protocol->isFixedTime()) {
			search->start_time = millis();
		}
		else {
			if (search->time_left > 30000 || search->time_inc > 0) {
				search->search_time = min(search->search_time * 1.5, 
					search->search_time + millis() - search->start_time);
			}
		}
		return 0;
	}

	virtual int stop() {
		search->stop_search = true;
		return 0;
	}

	virtual bool makeMove(const char* m) {
		const Move* move;
		if (move = game->pos->stringToMove(m)) {
			return game->makeMove(*move);
		}
		return false;
	}

	int goBook() {
		char fen[128];
		game->getFen(fen);
		uint64 key = book->hash(fen);
		char m[6];
		if (book->find(key, m) == 0) {
			if (const Move* move = game->pos->stringToMove(m)) {
				game->pos->pv_length = 1;
				search->pv[0][0].move = *move;
			}
		}
		return 0;
	}

	int goSearch(int wtime, int btime, int movestogo, int winc, 
		int binc, int movetime) 
	{
		if (num_threads > 1) {
			// Simple MT with shared transposition table.
			startWorkers();
			search->go(wtime, btime, movestogo, winc, binc, movetime);
			stopWorkers();
		}
		else {
			search->go(wtime, btime, movestogo, winc, binc, movetime);
		}
		printSearchData();
		return 0;
	}

	void startWorkers() {
		for (int i = 0; i < num_threads - 1; i++) {
			workers[i].start(config,  game, transt, pawnt);
		}
	}
	
	void stopWorkers() {
		for (int i = 0; i < num_threads - 1; i++) {
			workers[i].stop();
		}
	}

	virtual int setOption(const char* name, const char* value) {
		char buf[128];
		strcpy(buf, "");;
		if (value != NULL) {
			if (stricmp("Hash", name) == 0) {
				transt->initialise(min(1024, max(8, strtol(value, NULL, 10))));
				snprintf(buf, sizeof(buf), "Hash table size is %d Mb\n", transt->getSizeMb());
			}
			else if (stricmp("Threads", name) == 0) {
				num_threads = min(4, max(1, strtol(value, NULL, 10)));
				snprintf(buf, sizeof(buf), "Number of threads is %d\n", num_threads);
			}
		}
		if (strlen(buf)) {
			logger->log(buf);
		}
		return 0;
	}

	int run(int argc, char* argv[]) {
		setbuf(stdout, NULL);

		logger = new Logger();
		config = new Config(argc > 1 ? argv[1] : "bobcat.ini");

		if (config->getBool("Bobcat", "log-file", false)) {
			logger->open(config->getString("Logging", "filename", "bobcat.log"));
		}

		char* buf;
		if ((buf = _getcwd(NULL, 0)) == NULL) {
			perror("_getcwd error");
		}
		else {
			logger->log(buf);
			free(buf);
		}

		Bitmanip_h_initialise();
		Bitboard_h_initialise();
		Magic_h_initialise();
		Zobrist_h_initialise();
		Square_h_initialise();

		game = new Game(config);
		input = new StdIn();
		output = new StdOut();
		protocol = new UCIProtocol(this, game, input, output);
		book = new Book(config, logger);
		pawnt = new PawnStructureTable(8);
		eval = new Eval(game, pawnt);
		see	= new SEE(game);
		transt = new TranspositionTable(256);
		search = new Search(protocol, game, eval, see, transt);
		
		newGame();

		bool console_mode = true;
		game->pos->board->print_board(cout);
		int exit = 0;

		while (exit == 0) {
			game->pos->generateMoves();

			if (console_mode) {
				cout << ("> ");
			}

			char line[16384];
			input->getLine(true, line);

			char* tokens[1024];
			int num_tokens = tokenize(trim(line), tokens, 1024);
			
			if (num_tokens == 0) {
				continue;
			}
			if (stricmp(tokens[0], "uci") == 0 || !console_mode) {
				exit = protocol->handleInput(tokens, num_tokens);
				console_mode = false;
			}
			else if (stricmp(tokens[0], "x") == 0) {
				exit = 1;
			}
			else if (stricmp(tokens[0], "d") == 0) {
				game->pos->board->print_board(cout);
				cout << endl;
				cout << "board key calculated  : " << game->calculateKey() << endl;
				cout << "board key incremental : " << game->pos->key << endl;
				cout << "pawn structure key    : " << game->pos->pawn_structure_key << endl;
			}
			else if (stricmp(tokens[0], "m") == 0) {
				game->pos->print_moves(cout);
			}
			else if (stricmp(tokens[0], "perft") == 0) {
				Test(game).perft(6);
			}
			else if (stricmp(tokens[0], "divide") == 0) {
				Test(game).perft_divide(5);
			}
			else if (stricmp(tokens[0], "t") == 0) {
				game->unmakeMove();
			}
			else if (stricmp(tokens[0], "go") == 0) {
				protocol->setFlags(FIXED_MOVE_TIME);
				go();
			}
			else if (stricmp(tokens[0], "analyse") == 0 || stricmp(tokens[0], "a") == 0) {
				protocol->setFlags(INFINITE_MOVE_TIME);
				go();
			}
			else if (stricmp(tokens[0], "new") == 0) {
				newGame();
			}
			else if (stricmp(tokens[0], "fen") == 0) {
				if (num_tokens == 1) {
					char fen[128];
					game->getFen(fen);
					cout <<  fen << endl;
				}
				else {
					int token = 0;
					char fen[128];
					if (fenFromParams(tokens, num_tokens, token, fen)) {
						setFen(fen);
					}
				}
			}
			else if (stricmp(tokens[0], "book") == 0) {
				char fen[128];
				game->getFen(fen);
				BB key = book->hash(fen);
				char move[6];
				if (book->find(key, move) == 0) {
					cout <<  move << endl;
				}
				else {
					cout <<  "no book move" << endl;
				}
			}
			else if (stricmp(tokens[0], "see") == 0 && num_tokens > 0) {
				const Move* m;
				if (m = game->pos->stringToMove(tokens[1])) {
					printf("SEE score for %s is %d\n", tokens[1], see->seeMove(*m));
				}
			}
			else if (makeMove(tokens[0])) {
			}
			else if (strlen(tokens[0])) {
				printf("Input is not a move or a command\n");
			}
			for (int i = 0; i < num_tokens; i++) {
				delete [] tokens[i];
			}
		}
		delete logger;
		delete config;
		delete game;
		delete input;
		delete output;
		delete protocol;
		delete pawnt;
		delete eval;
		delete see;
		delete transt;
		delete search;
		return 0;
	}

public:
	Config*		config;
	StdIn*		input;
	StdOut*		output;
	Logger*		logger;
	Game*		game;
	Eval*		eval;
	SEE*		see;
	Search*		search;
	Protocol*	protocol;
	Book*		book;
	TTable*		transt;
	PSTable*	pawnt;
	Worker		workers[3];
	int			num_threads;
};
