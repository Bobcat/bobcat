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
	Bobcat() : num_threads(1) {
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

	virtual int setFEN(const char* fen) {
		return game->newGame(fen);
	}

	virtual int go(int wtime = 0, int btime = 0, int movestogo = 0, int winc = 0, int binc = 0, int movetime = 5000) {
		game->pos->pv_length = 0;

		if (!protocol->isAnalysing() && !protocol->isFixedDepth()) {
			goBook();
		}

		if (game->pos->pv_length == 0) {
			goSearch(wtime, btime, movestogo, winc, binc, movetime);
		}

		if (game->pos->pv_length) {
			char best_move[12];
			char ponder_move[12];

			protocol->postMoves(game->moveToString(search->pv[0][0].move, best_move),
				game->pos->pv_length > 1 ? game->moveToString(search->pv[0][1].move, ponder_move) : 0);

			game->makeMove(search->pv[0][0].move, true, true);
		}
		return 0;
	}

	virtual int ponderHit() {
		if (protocol->isFixedTime()) {
			search->start_time = millis();
		}
		else {
			if (search->timeUsed() < search->search_time) {
				search->search_time += (search->search_time - search->timeUsed());
			}
			search->search_time += search->search_time*0.5; // not too sophisticated
		}
		return 0;
	}

	virtual int stop() {
		search->stop_search = true;
		return 0;
	}

	virtual bool makeMove(const char* m) {
		const Move* move = game->pos->stringToMove(m);

		if (move) {
			return game->makeMove(*move, true, true);
		}
		return false;
	}

	void goBook() {
		char fen[128];
		game->getFEN(fen);
		uint64 key = book->hash(fen);
		char m[6];
		if (book->find(key, m) == 0) {
			if (const Move* move = game->pos->stringToMove(m)) {
				game->pos->pv_length = 1;
				search->pv[0][0].move = *move;
			}
		}
	}

	void goSearch(int wtime, int btime, int movestogo, int winc, int binc, int movetime) {
		// Simple MT with shared transposition table.
		startWorkers();
		search->go(wtime, btime, movestogo, winc, binc, movetime);
		stopWorkers();
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
		char buf[1024];
		strcpy(buf, "");;
		if (value != NULL) {
			if (strieq("Hash", name)) {
				transt->initialise(std::min(1024, std::max(8, (int)strtol(value, NULL, 10))));
				_snprintf(buf, sizeof(buf), "Hash ", transt->getSizeMb());
			}
			else if (strieq("Threads", name)) {
				num_threads = std::min(16, std::max(1, (int)strtol(value, NULL, 10)));
				_snprintf(buf, sizeof(buf), "Threads %d.", num_threads);
			}
			else if (strieq("UCI_Chess960", name)) {
				if (strieq(value, "true")) {
					game->chess960 = true;
				}
				_snprintf(buf, sizeof(buf), "UCI_Chess960 ", game->chess960 ? on : off);
			}
			else if (strieq("UCI_Chess960_Arena", name)) {
				if (strieq(value, "true")) {
					game->chess960 = true;
					game->xfen = true;
				//	game->arena = true;
				}
			//	snprintf(buf, sizeof(buf), "UCI_Chess960_Arena ", game->arena ? on : off);
			}
		}
		if (strlen(buf)) {
			logger->logts(buf);
		}
		return 0;
	}

	int run(int argc, char* argv[]) {
		setbuf(stdout, NULL);
		setbuf(stdin, NULL);

		logger = new Logger();
		config = new Config(argc > 1 ? argv[1] : "bobcat.ini");

		logTimeAndCwd();

		Bitboard_h_initialise();
		Magic_h_initialise();
		Zobrist_h_initialise();
		Square_h_initialise();

		game = new Game(config);
		input = new StdIn(logger);
		output = new StdOut(logger);
		protocol = new UCIProtocol(this, game, input, output);
		book = new Book(config, logger);
		transt = new TranspositionTable(256);
		pawnt = new PawnStructureTable(8);
		see = new SEE(game);
		eval = new Eval(game, pawnt, see);
		search = new Search(protocol, game, eval, see, transt, logger);

//		print_bb(bbSquare(flip[0][a2]), "bbSquare(flip[0][a2])");
	//	print_bb(bbSquare(flip[1][a2]), "bbSquare(flip[1][a2])");
		//print_bb(bbSquare(a2), "bbSquare(a2)");

		newGame();

		bool console_mode = true;
		int exit = 0;

		while (exit == 0) {
			game->pos->generateMoves();

			char line[16384];
			input->getLine(true, line);

			char* tokens[1024];
			int num_tokens = tokenize(trim(line), tokens, 1024);

			if (num_tokens == 0) {
				continue;
			}
			if (strieq(tokens[0], "uci") || !console_mode) {
				exit = protocol->handleInput((const char**)tokens, num_tokens);
				console_mode = false;
			}
			else if (strieq(tokens[0], "x")) {
				exit = 1;
			}
			else if (strieq(tokens[0], "d")) {
				game->pos->board->print();
				printf("\n");
				printf("board key calculated  : %" PRIu64 "\n", game->calculateKey());
				printf("board key incremental : %" PRIu64 "\n", game->pos->key);
				printf("pawn structure key    : %" PRIu64 "\n", game->pos->pawn_structure_key);
			}
			else if (strieq(tokens[0], "m")) {
				game->print_moves();
			}
			else if (strieq(tokens[0], "perft")) {
				Test(game).perft(6);
			}
			else if (strieq(tokens[0], "timetodepth") || strieq(tokens[0], "ttd")) {
				Test(game).timeToDepth(search, this);
			}
			else if (strieq(tokens[0], "divide")) {
				Test(game).perft_divide(5);
			}
			else if (strieq(tokens[0], "t")) {
				game->unmakeMove();
			}
			else if (strieq(tokens[0], "go")) {
				protocol->setFlags(FIXED_MOVE_TIME);
				go();
			}
			else if (strieq(tokens[0], "analyse") || strieq(tokens[0], "a")) {
				protocol->setFlags(INFINITE_MOVE_TIME);
				go();
			}
			else if (strieq(tokens[0], "eval") || strieq(tokens[0], "e")) {
				printf("eval->evaluate() returns %d cp\n", eval->evaluate(-100000, 100000));
			}
			else if (strieq(tokens[0], "new")) {
				newGame();
			}
			else if (strieq(tokens[0], "fen")) {
				if (num_tokens == 1) {
					char fen[128];
					game->getFEN(fen);
					printf("%s\n", fen);
				}
				else {
					int token = 0;
					char fen[128];
					if (FENfromParams((const char**)tokens, num_tokens, token, fen)) {
						setFEN(fen);
					}
				}
			}
			else if (strieq(tokens[0], "book")) {
				char fen[128];
				game->getFEN(fen);
				BB key = book->hash(fen);
				char move[6];
				if (book->find(key, move) == 0) {
					printf("%s\n", move);
				}
				else {
					printf("no book move\n");
				}
			}
			else if (strieq(tokens[0], "see") && num_tokens > 0) {
				const Move* m = game->pos->stringToMove(tokens[1]);
				if (m) {
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

	void logTimeAndCwd() {
		char buf1[2048];
		char buf2[2048];

		if (true/*!!!!*/||config->getBool("Bobcat", "log-file", false)) {
			logger->open(config->getString("Logging", "filename", "bobcat.log"));
		}
		snprintf(buf1, sizeof(buf1), "Time is %s.", dateAndTimeString(buf2));
		logger->logts(buf1);
		snprintf(buf1, sizeof(buf1), "Working directory is %s.", _getcwd(buf2, 2048));
		logger->logts(buf1);
        printf("%s\n",buf1);
	}

public:
	Config* config;
	StdIn* input;
	StdOut* output;
	Logger* logger;
	Game* game;
	Eval* eval;
	SEE* see;
	Search* search;
	Protocol* protocol;
	Book* book;
	TTable* transt;
	PSTable* pawnt;
	Worker workers[16];
	int num_threads;

	static const char* on;
	static const char* off;
};

const char* Bobcat::on = "ON";
const char* Bobcat::off = "OFF";
