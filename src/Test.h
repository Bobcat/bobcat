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
	Test(Game* game, int flags = LegalMoves) {
		this->game = game;
		this->flags = flags;
	}

	void perft(int depth) {
		printf("\nDepth         Nodes\n-------------------\n");
		perft_result result;
		for (int i = 1; i <= depth; i++) {
			clock_t t1, t2;
			t1 = clock();
			perft_result result;
			perft_(i, result);
			t2 = clock();
			float diff = ((float)t2-(float)t1)/CLOCKS_PER_SEC;
			cout << setw(5) << i << setw(14) << result.nodes << "  " << diff << endl;
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
			if (!game->makeMove(*m, flags == 0 ? true : false)) {
				continue;
			}
			uint64 nodes_start = result.nodes;
			perft_(depth - 1, result);
			game->unmakeMove();
			printf("%7s %11ld\n", moveToString(*m, buf), result.nodes - nodes_start);
		}
		printf("total positions is %ld\n", result.nodes);
	}

private:
	int perft_(int depth, perft_result& result) {
		if (depth == 0) { 
			result.nodes++; 
			return 0; 
		}
		Position* pos = game->pos;
		pos->generateMoves(0, 0, flags);

		if ((flags & Stages) == 0 && depth == 1) { 
			result.nodes += pos->moveCount(); 
		}
		else {
			while (const MoveData* move_data = pos->nextMove()) {
				const Move* m = &move_data->move;
				if (!game->makeMove(*m, (flags & LegalMoves) ? false : true, true)) {
					continue;
				}
				Test(game).perft_(depth - 1, result);
				game->unmakeMove();
			}
		}
		return 0;
	}

	Game* game;
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
