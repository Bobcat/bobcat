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

struct perft_result {
	perft_result() {
		nodes = enpassants = castles = captures = promotions = 0;
	}
	uint64 nodes;
	uint enpassants;
	uint castles;
	uint captures;
	uint promotions;
};

class Test {
public:
	Test(Game* game, int flags = LEGALMOVES) {
		this->game = game;
		this->flags = flags;
	}

	void perft(int depth) {
		printf("\nDepth         Nodes\n-------------------\n");
		perft_result result;
		for (int i = 1; i <= depth; i++) {
			clock_t t1, t2;
			t1 = millis();
			perft_result result;
			perft_(i, result);
			t2 = millis();
			double diff = (t2-t1)/(double)1000;
			printf("%5d%14llu  %f\n", i, result.nodes, diff);
		}
	}

	void perft_divide(int depth) {
		printf("depth is %d\n", depth);
		printf("  move    positions\n");

		perft_result result;
		Position* pos = game->pos;
		char buf[12];

		pos->generateMoves(0, 0, flags);
		while (const MoveData* move_data = pos->nextMove()) {
			const Move* m = &move_data->move;
			if (!game->makeMove(*m, flags == 0 ? true : false, true) ) {
				continue;
			}
			uint64 nodes_start = result.nodes;
			perft_(depth - 1, result);
			game->unmakeMove();
			printf("%7s %11llu\n", moveToString(*m, buf), result.nodes - nodes_start);
		}
		printf("total positions is %llu\n", result.nodes);
	}

	void timeToDepth(Search* search, ProtocolListener* app) {
		this->app = app;
		this->search = search;
		int saved_verbosity = search->verbosity;
		search->verbosity = 0;
		total_time = 0;
		// nunn.epd
		timeToDepth("r2qkb1r/pp1n1ppp/2p2n2/3pp2b/4P3/3P1NPP/PPPN1PB1/R1BQ1RK1 b kq - 0 8");
		timeToDepth("r2q1rk1/ppp1bppp/1nn1b3/4p3/1P6/P1NP1NP1/4PPBP/R1BQ1RK1 b - - 0 10");
		timeToDepth("rnbq1rk1/p3ppbp/1p1p1np1/2p5/3P1B2/2P1PN1P/PP2BPP1/RN1QK2R w KQ - 0 8");
		timeToDepth("r1b1qrk1/ppp1p1bp/n2p1np1/3P1p2/2P5/2N2NP1/PP2PPBP/1RBQ1RK1 b - - 2 9");
		timeToDepth("rnbq1rk1/1p2ppbp/2pp1np1/p7/P2PP3/2N2N1P/1PP1BPP1/R1BQ1RK1 b - - 0 8");
		timeToDepth("2kr3r/ppqn1pp1/2pbp2p/7P/3PQ3/5NP1/PPPB1P2/2KR3R w - - 1 16");
		timeToDepth("r3kb1r/pp2pppp/1nnqb3/8/3p4/NBP2N2/PP3PPP/R1BQ1RK1 b kq - 3 10");
		timeToDepth("r2qk2r/5pbp/p1npb3/3Np2Q/2B1Pp2/N7/PP3PPP/R4RK1 b kq - 0 15");
		timeToDepth("r1bqk2r/4bpp1/p2ppn1p/1p6/3BPP2/2N5/PPPQ2PP/2KR1B1R w kq - 0 12");
		timeToDepth("r1b1k2r/1pq3pp/p1nbpn2/3p4/3P4/2NB1N2/PP3PPP/R1BQ1RK1 w kq - 0 13");
		timeToDepth("rn1q1rk1/pp3ppp/3b4/3p4/3P2b1/2PB1N2/P4PPP/1RBQ1RK1 b - - 2 12");
		timeToDepth("r1b1kb1r/ppp2ppp/2p5/4Pn2/8/2N2N2/PPP2PPP/R1B2RK1 w - - 2 10");
		timeToDepth("r2qrbk1/1b1n1p1p/p2p1np1/1p1Pp3/P1p1P3/2P2NNP/1PB2PP1/R1BQR1K1 w - - 0 17");
		timeToDepth("r2q1rk1/pp1n1ppp/2p1pnb1/8/PbBPP3/2N2N2/1P2QPPP/R1B2RK1 w - - 1 11");
		timeToDepth("r1bq1rk1/1p2bppp/p1n1pn2/8/P1BP4/2N2N2/1P2QPPP/R1BR2K1 b - - 2 11");
		timeToDepth("r1b2rk1/pp3ppp/2n1pn2/q1bp4/2P2B2/P1N1PN2/1PQ2PPP/2KR1B1R b - - 2 10");
		timeToDepth("rnbq1rk1/2pnppbp/p5p1/1p2P3/3P4/1QN2N2/PP3PPP/R1B1KB1R w KQ - 1 10");
		timeToDepth("rnb2rk1/ppp1qppp/3p1n2/3Pp3/2P1P3/5NP1/PP1N1PBP/R2Q1RK1 w - - 1 11");
		timeToDepth("r2q1rk1/pbpn1pp1/1p2pn1p/3p4/2PP3B/P1Q1PP2/1P4PP/R3KBNR w KQ - 1 11");
		timeToDepth("r3qrk1/1ppb1pbn/n2p2pp/p2Pp3/2P1P2B/P1N5/1P1NBPPP/R2Q1RK1 w - - 1 13");
		printf("%f\n", total_time);
		search->verbosity = saved_verbosity;
	}

private:
	double total_time;
	void timeToDepth(const char* fen, int depth = 11) {
		app->newGame();
		app->setFEN(fen);
		char sdepth[12];
		sprintf(sdepth, "%d", depth);
		char* p[3] = {"go", "depth", sdepth};
		clock_t t1, t2;
		t1 = millis();
		search->getProtocol()->handleInput(p, 3);
		t2 = millis();
		double seconds = (t2-t1)/(double)1000;
		printf("%f\n", seconds);
		total_time += seconds;
	}

	int perft_(int depth, perft_result& result) {
		if (depth == 0) { 
			result.nodes++; 
			return 0; 
		}
		Position* pos = game->pos;
		pos->generateMoves(0, 0, flags);

		if ((flags & STAGES) == 0 && depth == 1) { 
			result.nodes += pos->moveCount(); 
		}
		else {
			while (const MoveData* move_data = pos->nextMove()) {
				const Move* m = &move_data->move;
				if (!game->makeMove(*m, (flags & LEGALMOVES) ? false : true, true)) {
					continue;
				}
				Test(game).perft_(depth - 1, result);
				game->unmakeMove();
			}
		}
		return 0;
	}

	Game* game;
	Search* search;
	ProtocolListener* app;
	int flags;
};

void printSearchData() {
#ifdef DEBUG
		printf("\nBF(ply) = nodes[ply]/nodes[ply-1]\n");
		printf("------");
		for (int id = 1; id <= search->search_depth - 1; id++) {
			printf("--------");
		}
		printf("\n");
		printf("PLxID|");
		for (int id = 1; id <= search->search_depth - 1; id++) {
			printf(" %5d |", id);
		}
		printf("\n");
		printf("------");
		for (int id = 1; id <= search->search_depth - 1; id++) {
			printf("--------");
		}
		printf("\n");
		for (int ply = 0; ply < search->MplyMax; ply++) {
			printf(" %3d |", ply);
			for (int id = 1; id <= search->search_depth - 1; id++) {
				__int64 n1 = search->MnumInteriorNodes[id][ply + 1] + search->MnumLeafNodes[id][ply + 1];
				__int64 n2 = search->MnumInteriorNodes[id][ply] + search->MnumLeafNodes[id][ply];
				if (n1 && n2) {
					double bf = (double) n1 / n2;
					printf(" %5.2f |", bf);
				} 
				else
					printf("       |");
			}
			printf("\n");
		}
		printf("------");
		for (int id =1; id <= search->search_depth - 1; id++) {
			printf("--------");
		}
		printf("\n");
#endif
}
