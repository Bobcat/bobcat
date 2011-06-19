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

typedef int Score; 
typedef int Depth;

class Search : public Runnable, public MoveSorter {
public:
	Search(Protocol* protocol, Game* game, Eval* eval, SEE* see, TranspositionTable* transt) {
		initialise(protocol, game, eval, see, transt, false);
	}

	Search(Game* game, Eval* eval, SEE* see, TranspositionTable* transt) {
		initialise(NULL, game, eval, see, transt, true);
	}

	int go(int wtime, int btime, int movestogo, int winc, int binc, int movetime) {

		initialiseSearch(wtime, btime, movestogo, winc, binc, movetime);

		Score alpha = -MAXSCORE;
		Score beta = MAXSCORE;

		while (!stop_search && search_depth < MAXDEPTH) {
			try {
				search_depth += 1;
				max_plies = max(32, search_depth + 16);
				
				Score score = searchRoot(search_depth, alpha, beta);
			
				savePV();
				if (isEasyMove()) {
					break;
				}
				alpha = max(-MAXSCORE, score - 50);
				beta = min(MAXSCORE, score + 50);
			}
			catch (const int) {
				while (ply) {
					unmakeMove();
				}
				savePV();
			}
		}
		postInfo(0, -1, search_depth, max_ply_reached, node_count, nodesPerSecond(), timeUsed(), transt->getLoad());
		stopped = true;
		return 0;
	}

	void stop() {
		stop_search = true;
	}

	bool isStopped() {
		return stopped;
	}

	virtual void run() {
		go(0, 0, 0, 0, 0, 0);
	}

	virtual void newGame() {
	}

protected:
	void initialise(Protocol* protocol, Game* game, Eval* eval, SEE* see, TranspositionTable* transt, bool worker) {
		this->protocol = protocol;
		this->game = game;
		this->eval = eval;
		this->see = see;
		this->transt = transt;
		board = game->pos->board;
		this->worker = worker;
		stopped = false;
	}

	void postInfo(const Move curr_move, int curr_move_number, int depth, int selective_depth, uint64 node_count, 
		uint64 nodes_per_sec, uint64 time, int hash_full)
	{
		if (!worker) {
			protocol->postInfo(0, -1, search_depth, max_ply_reached, node_count, nodesPerSecond(), timeUsed(), transt->getLoad());
		}
	}

	Score searchRoot(const Depth depth, Score alpha, Score beta) {
		do { // stay in this loop until an exact score is found
			pv_length[0] = 0;
			pos->eval_score = eval->evaluate();
			findTranspositionRefineEval(depth, alpha, beta);
			sortRootMoves(false);
			
			Move best_move = 0;
			Score best_score = -MAXSCORE;
			int move_count = 0;

			while (const MoveData* move_data = pos->nextMove()) {
				const Move m = move_data->move;

				if (makeMove(m)) {
					Depth next_depth = getNextDepth(true, depth, ++move_count, move_data);

					if (search_depth > 10 && search_time > 5000) {
						postInfo(m, move_count, search_depth, max_ply_reached, node_count, nodesPerSecond(), 
							timeUsed(), transt->getLoad());
					}

					Score score;
					if (move_count == 1) {
						score = searchNextDepth(next_depth, -beta, -alpha);
					}
					else {
						score = searchNextDepth(next_depth, -alpha - 1, -alpha);
						if (score > alpha && score < beta) {
							score = searchNextDepth(next_depth, -beta, -alpha);
						}
					}
					if (score > alpha && next_depth < depth - 1) {
						score = searchNextDepth(depth - 1, -beta, -alpha);
					}

					unmakeMove();

					if (score > best_score) {
						best_score = score;
						if (best_score > alpha) { 
							best_move = m;
							if (score >= beta) {
								break;
							}
							updatePV(m, best_score, depth);
							alpha = best_score; 
						}
					}
				}
			}
			pos->gotoMove(0);
			if (move_count == 0) { 
				return searchNodeScore(pos->in_check ? -MAXSCORE : 0);
			}
			else if (best_move) {
				updateHistoryScores(best_move, depth, move_count);
				if (best_score >= beta) {
					updateKillerMoves(best_move);
				}
				else {
					return searchNodeScore(best_score);
				}
			}
			storeTransposition(depth, best_score, nodeType(best_score, beta, best_move), best_move);
			alpha = -MAXSCORE;
			beta = MAXSCORE;
		} while (true);
	}

	void sortRootMoves(bool use_root_move_scores) {
		int move_count = 0;
		while (MoveData* move_data = pos->nextMove()) {
			if (use_root_move_scores) { // always false
				move_data->score = root_move_score[move_count++];
			}
			else { // always take this branch for the moment
				sortMove(*move_data);
			}
		}
		pos->gotoMove(0);
	}

	__forceinline Score searchNextDepth(const Depth depth, const Score alpha, const Score beta) {
		if (pos->isDraw()) {
			return pos->eval_score;
		}
		return depth <= 0 ? -searchQuiesce(alpha, beta, 0) : -searchAll(depth, alpha, beta);
	}

 	Score searchAll(const Depth depth, Score alpha, const Score beta) {
		if (game->isRepetition()) {
			return searchNodeScore(0);
		}
		
		findTranspositionRefineEval(depth, alpha, beta);

		bool is_pv_node = beta - alpha > 1;

		if (!is_pv_node && pos->transp_score_valid && pos->transp_depth_valid) { 
			return searchNodeScore(pos->transp_score);
		} 
		
		if (ply >= max_plies - 1) {
			return searchNodeScore(pos->eval_score);
		}

		Score score;
		if (okToTryNullMove(depth, beta)) {
			makeMove(0);
			score = searchNextDepth(depth - nullMoveReduction(depth), -beta, -beta + 1);
			unmakeMove();
			if (score >= beta) {
				return searchNodeScore(score);
			}
		}

		if (!is_pv_node && !pos->in_check && depth <= 3) {// && !pos->material.isEndgame()) {
			if (pos->eval_score + 125 < alpha) {
				if (depth <= 1) {
					score = searchQuiesce(alpha, beta, 0);
					return max(score, pos->eval_score + 125);
				}
				else {
					if (pos->eval_score + 400 < alpha) {
						if ((score = searchQuiesce(alpha, beta, 0)) < alpha) {
							return max(score, pos->eval_score + 400);
						}
					}
				}
			}
		}

		if (!pos->transp_move && pos->eval_score >= beta - 100) {
			if (is_pv_node) {
				if (depth >= 4) {
					score = searchAll(depth - 2, alpha, beta);
					findTranspositionRefineEval(depth, alpha, beta);
				}
			}
			else if (depth >= 8) {
				score = searchAll(depth/2, alpha, beta);
				findTranspositionRefineEval(depth, alpha, beta);
			}
		}

		pos->generateMoves(this, pos->transp_move, STAGES);

		Move best_move = 0;
		Score best_score = -MAXSCORE;
		int move_count = 0;

		while (const MoveData* move_data = pos->nextMove()) {
			const Move m = move_data->move;

			if (makeMove(m)) {

				Depth next_depth = getNextDepth(is_pv_node, depth, ++move_count, move_data);

				if (okToPruneLastMove(is_pv_node, depth, next_depth, alpha, best_score, move_data)) {
					unmakeMove();
					continue;
				}

				if (move_count == 1) {
					score = searchNextDepth(next_depth, -beta, -alpha);
				}
				else {
					score = searchNextDepth(next_depth, -alpha - 1, -alpha);
					if (score > alpha && score < beta) {
						score = searchNextDepth(next_depth, -beta, -alpha);
					}
				}
				if (score > alpha && next_depth < depth - 1) {
					score = searchNextDepth(depth - 1, -beta, -alpha);
				}

				unmakeMove();

				if (score > best_score) {
					best_score = score;
					if (best_score > alpha) { 
						best_move = m;
						if (score >= beta) {
							break;
						}
						updatePV(m, best_score, depth);
						alpha = best_score; 
					}
				}
			}
		}
		if (move_count == 0) { 
			return searchNodeScore(pos->in_check ? -MAXSCORE + ply : 0);
		}
		else if (pos->reversible_half_move_count >= 100) { 
			return searchNodeScore(0);
		}
		else if (best_move) {
			updateHistoryScores(best_move, depth, move_count);
			if (best_score >= beta) {
				updateKillerMoves(best_move);
			}
		}
		return storeSearchNodeScore(best_score, depth, nodeType(best_score, beta, best_move), best_move);
	}

	__forceinline bool okToTryNullMove(const Depth depth, const Score beta) const {
		return !pos->in_check 
			&& pos->null_moves_in_row < 1 
			&& depth > 1 
			&& !pos->material.hasPawnsOnly(pos->side_to_move) 
			&& pos->eval_score >= beta; 
	}

	__forceinline Depth getNextDepth(bool is_pv_node, const Depth depth, const int move_count, 
		const MoveData* move_data) const
	{
		if (is_pv_node && pos->in_check) {
			return depth;
		}
		const Move m = move_data->move;
		if (!pos->in_check 
			&& !isPassedPawnMove(m) 
			&& !isQueenPromotion(m) 
			&& !isCapture(m) 
			&& move_count >= 5)
		{
			return depth - 2 - depth/16 - max(0, (move_count-6)/12);
			//return depth - 2 - depth/16 - (is_pv_node ? 0 : maxint(0, (move_count-6)/12));
		}
		return depth - 1;
	}

	__forceinline bool okToPruneLastMove(const bool is_pv_node, const Depth depth, const Depth next_depth, const Score alpha, 
		Score& best_score, const MoveData* move_data) const 
	{
		if (-pos->eval_score >= alpha) {
			return false;
		}
		int margin;
		//const Side them = pos->side_to_move;
		if (next_depth < 2) {
			// an opponent pieces could be hanging
			//margin = pos->material.highestAttackedPieceValue(eval->attacks(them ^ 1), board, them);
			margin = 150;
		}
		else {
			// with a move left for ourselves even more is possible 
			//margin = max(400, pos->material.highestPieceValue(them) + 100);
			margin = 400;
		}
		//const Move m = move_data->move;

		if (next_depth < 4 // maximum of two moves per side  
			&& next_depth > 0 // prefer qs search in case <= 0
			&& -pos->eval_score + margin < alpha
			&& next_depth < depth - 1 // already reduced
			//&& next_depth < depth // not extended
			//&& !isCapture(m)
			//&& !isPassedPawnMove(m) 
			//&& !is_pv_node
			//&& !pos->in_check
			//&& move_data->score < KILLERMOVESCORE 
			//&& best_score > -MAXSCORE // move_count > 1
			)
		{
			best_score = max(best_score, -pos->eval_score + margin);
			return true;
		}
		return false;
	}

	__forceinline int nullMoveReduction(const Depth depth) const {
		return 4 + depth/16;
	}

	Score searchQuiesce(Score alpha, const Score beta, int qs_ply) {
		findTransposition(0, alpha, beta);

		if (beta - alpha == 1 && pos->transp_score_valid && pos->transp_depth_valid) { 
			return searchNodeScore(pos->transp_score);
		} 

		if (pos->eval_score >= beta && !pos->in_check) {
			return storeSearchNodeScore(pos->eval_score, 0, BETA, 0);
		}
		
		if (ply >= max_plies - 1 || qs_ply > 6) {
			return searchNodeScore(pos->eval_score);
		}

		Move best_move = 0;
		Score best_score = pos->eval_score;
		int move_count = 0;

		if (best_score > alpha) {
			alpha = best_score; 
		}

		pos->generateCapturesAndPromotions(this);

		while (const MoveData* move_data = pos->nextMove()) {
			const Move m = move_data->move;

			if (!pos->in_check && !isPromotion(m)) {// && !pos->material.isEndgame()) {
				if (move_data->score < 0) { 
					continue;
				}
				else if (pos->eval_score + piece_value(moveCaptured(m)) + 150 < alpha) {
					best_score = max(best_score, pos->eval_score + piece_value(moveCaptured(m)) + 150);
					continue;
				}
			}
			if (makeMove(m)) {
				++move_count;

				Score score;
				if (pos->isDraw()) {
					score = pos->eval_score;
				}
				else {
					score = -searchQuiesce(-beta, -alpha, qs_ply + 1);
				}

				unmakeMove();
				
				if (score > best_score) {
					best_score = score;
					if (best_score > alpha) { 
						best_move = m;
						if (score >= beta) {  
							break;
						}
						updatePV(m, best_score, 0);
						alpha = best_score; 
					}
				}
			}
		}
		if (move_count == 0) { 
			return best_score;
		}
		return storeSearchNodeScore(best_score, 0, nodeType(best_score, beta, best_move), best_move);
	}

	__forceinline bool makeMove(const Move m) {
		if (game->makeMove(m, false, false)) {
			pos = game->pos;

			Side us = pos->side_to_move;
			Side them = us ^ 1;	

			pos->eval_score = eval->evaluate();

			if (eval->attacks(us) & bbSquare(board->king_square[them])) {
				game->unmakeMove();
				pos = game->pos;
				return false;
			}
			pos->in_check = (eval->attacks(them) & bbSquare(board->king_square[us])) != 0;

			ply++;
			if (ply > max_ply_reached) {
				max_ply_reached = ply;
			}
			pv_length[ply] = ply;
			node_count++;
			checkTime();
			return true;
		}
		return false;
	}

	__forceinline void unmakeMove() {
		game->unmakeMove();
		pos = game->pos;
		ply--;
	}

	__forceinline void checkTime() {
		if ((node_count & 0x3FFF) == 0) {
			if (!isAnalysing()) {
				stop_search = search_depth > 1 && timeUsed() > search_time;
			}
			else {
				if (protocol) {
					protocol->checkInput();
				}
			}	
			if (stop_search) {
				throw 1; 
			}
		}
		if ((node_count & 0x3FFFFF) == 0) {
			postInfo(0, -1, search_depth, max_ply_reached, node_count, nodesPerSecond(), timeUsed(), transt->getLoad());
		}
	}

	__forceinline int isAnalysing() const {
		return protocol ? protocol->isAnalysing() : true;
	}

	__forceinline uint64 timeUsed() const {
		return max(1, millis() - start_time);
	}

	__forceinline void updatePV(const Move m, Score score, const Depth depth) {
		PVEntry* entry = &pv[ply][ply];

		entry->score = score;
		entry->depth = depth;
		entry->key = pos->key;
		entry->move = m;

		pv_length[ply] = pv_length[ply + 1];

		for (int i = ply + 1; i < pv_length[ply]; i++) { 
			pv[ply][i] = pv[ply + 1][i];
		}  
		if (ply == 0) {
			char buf[2048], buf2[16]; buf[0] = 0;
			for (int i = ply; i < pv_length[ply]; i++) { 
				snprintf(&buf[strlen(buf)], sizeof(buf) - strlen(buf), "%s ", 
					moveToString(pv[ply][i].move, buf2));
			}
			if (!worker) {
				protocol->postPV(search_depth, max_ply_reached, node_count, nodesPerSecond(), 
					timeUsed(), transt->getLoad(), score, buf);
			}
		}
	}

	__forceinline void savePV() {
		if (pv_length[0]) {
			pos->pv_length = pv_length[0];
			for (int i = 0; i < pos->pv_length; i++) {
				const PVEntry& entry = pv[0][i];
				transt->insert(entry.key, entry.depth, entry.score, EXACT, entry.move);
			}
		}
	}

	uint64 nodesPerSecond() const {
		return (uint64)(node_count*1000/max(1, (millis() - start_time)));
	}

	__forceinline void updateHistoryScores(const Move m, const Depth depth, int move_count) {
		history_scores[movePiece(m)][moveTo(m)] += depth*depth;
		if (history_scores[movePiece(m)][moveTo(m)] > PROMOTIONMOVESCORE) {
			for (int i = 0; i < 16; i++) for (int k = 0; k < 64; k++) {
				history_scores[i][k] >>= 2; 
			}
		}
	}

	__forceinline void updateKillerMoves(const Move m) {
		if (!isCapture(m) && !isPromotion(m) && m != killer_moves[0][ply]) {
			killer_moves[2][ply] = killer_moves[1][ply];
			killer_moves[1][ply] = killer_moves[0][ply];
			killer_moves[0][ply] = m;
		}
	}

	__forceinline Score codecTTableScore(Score score, Score ply) const {
		if (abs((int)score) < MAXSCORE - 128) {
			return score;
		}
		return score < 0 ? score - ply : score + ply;
	}

	void initialiseSearch(int wtime, int btime, int movestogo, int winc, int binc, int movetime) {
		pos = game->pos; // updated in makeMove and unmakeMove from here on

		if (!worker) {
			if (protocol->isFixedTime()) {
				search_time = (int)(0.9*(double)movetime);
			}
			else {
				int movesLeft = movestogo;
				if (movesLeft == 0) {
					movesLeft = 30;
				}

				time_left = pos->side_to_move == 0 ? wtime : btime;
				time_inc = pos->side_to_move == 0 ? winc : binc;

				search_time = time_left/movesLeft + time_inc;

				if (movestogo != 0) {
					search_time = (int)(search_time*0.95);
				}

				if (movestogo == 0 && search_time > time_left/4) {
					search_time = time_inc ? time_left/10 : time_left/30;
				}
				else if (movestogo > 0 && search_time > time_left/2) {
					search_time = time_left/4;
				}
			}
			start_time = millis();
			transt->initialiseSearch();
		}
		ply = 0;
		search_depth = 0;
		pos->pv_length = 0;
		node_count = 1; 
		stop_search = false;
		max_ply_reached = 0;
		memset(pv, 0, sizeof(pv));
		memset(root_move_score, 0, sizeof(root_move_score));
		memset(history_scores, 0, sizeof(history_scores)); 
		memset(killer_moves, 0, sizeof(killer_moves));
		pos->generateMoves(this, 0, LEGALMOVES);
	}

	virtual void sortMove(MoveData& move_data) {
		const Move m = move_data.move;
		if (pos->transp_move == m) {
			move_data.score = 890010;
		}
		else if (isQueenPromotion(m)) {
			move_data.score = 890000;
		}
		else if (isCapture(m)) {
			Score value_captured = piece_value(moveCaptured(m));
			Score value_piece = piece_value(movePiece(m));
			if (value_piece == 0) {
				value_piece = 1800;
			}
			if (value_piece <= value_captured) {
				move_data.score = 300000 + value_captured*20 - value_piece;
			}
			else if (see->seeMove(m) >= 0) {
				move_data.score = 160000 + value_captured*20 - value_piece;
			}
			else { 
				move_data.score = -100000 + value_captured*20 - value_piece;
			}
		}
		else if (isPromotion(m)) {
			move_data.score = PROMOTIONMOVESCORE + piece_value(movePromoted(m));
		}
		else if (m == killer_moves[0][ply]) {
			move_data.score = KILLERMOVESCORE + 2;
		}
		else if (m == killer_moves[1][ply]) {
			move_data.score = KILLERMOVESCORE + 1;
		}
		else if (m == killer_moves[2][ply]) {
			move_data.score = KILLERMOVESCORE;
		}
		else {
			move_data.score = history_scores[movePiece(m)][moveTo(m)];
		}
	}

	__forceinline Score searchNodeScore(const Score score) const {
		return score;
	}

	__forceinline Score storeSearchNodeScore(const Score score, const Depth depth, const int node_type, const Move move) {
		storeTransposition(depth, score, node_type, move);
		return searchNodeScore(score);
	}

	__forceinline void storeTransposition(const Depth depth, const Score score, const int node_type, const Move move) {
		pos->transposition = transt->insert(pos->key, depth, codecTTableScore(score, ply), node_type, move);
	}

	__forceinline void findTranspositionRefineEval(const Depth depth, const Score alpha, const Score beta) {
		if (!pos->transposition || pos->transposition->key != transt->key32(pos->key)) {
			if ((pos->transposition = transt->find(pos->key)) == 0) {
				pos->transp_depth_valid = false;
				pos->transp_score_valid = false;
				pos->transp_flags = 0;
				pos->transp_move = 0;
				return;
			}
		}
		pos->transp_depth_valid = pos->transposition->depth >= depth;
		pos->transp_score = codecTTableScore(pos->transposition->score, -ply);
		pos->transp_depth = pos->transposition->depth;
		pos->transp_flags = pos->transposition->flags;

		switch (pos->transposition->flags & 7) {
		case EXACT:
			pos->transp_score_valid = true;
			if (pos->transposition->depth) {
				pos->eval_score = pos->transp_score;
			}
			break;
		case BETA:
			if ((pos->transp_score_valid = pos->transp_score >= beta) && pos->transp_depth) {
				pos->eval_score = max(pos->eval_score, pos->transp_score);
			}
			break;
		case ALPHA:
			if ((pos->transp_score_valid = pos->transp_score <= alpha) && pos->transp_depth) {
				pos->eval_score = min(pos->eval_score, pos->transp_score);
			}
			break;
		default://error
			pos->transp_score_valid = false;
			break;
		}
		pos->transp_move = pos->transposition->move;
	}

	__forceinline void findTransposition(const Depth depth, const Score alpha, const Score beta) {
		return findTranspositionRefineEval(depth, alpha, beta);
	}

	__forceinline int nodeType(const Score score, const Score beta, const Move move) const {
		return move ? (score >= beta ? BETA : EXACT) : ALPHA;
	}

	bool isEasyMove() const {
		if (pos->moveCount() == 1 && search_depth > 4) {
			return true;
		}
		return false;
	}

	__forceinline bool isPassedPawnMove(const Move m) const {
		return (movePiece(m) & 7) == Pawn && board->isPawnPassed(moveTo(m), side(m));
	}

	struct PVEntry {
		uint64 key;
		Depth depth;
		Score score;
		Move move;
	};

public:
	Depth ply;
	Depth max_ply_reached;
	PVEntry pv[128][128];
	int pv_length[128];
	uint64 start_time;
	uint64 search_time; 
	uint64 time_left;
	uint64 time_inc;
	bool stop_search;
	int max_plies;

protected:	
	Depth search_depth;
	bool worker;
	Move killer_moves[4][128];
	int history_scores[16][64];
	Score root_move_score[256];
	bool stopped;
	Protocol* protocol;
	Game* game;
	Eval* eval;
	Board* board;
	SEE* see;
	Position* pos;
	TTable* transt;

	static uint64 node_count;

	static const int EXACT = 1;
	static const int BETA = 2;
	static const int ALPHA = 4;

	static const int MAXSCORE = 0x7fff;
	static const int MAXDEPTH = 96;

	static const int KILLERMOVESCORE = 124900;
	static const int PROMOTIONMOVESCORE = 50000;
};

uint64 Search::node_count;
