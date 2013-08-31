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

class Search : public Runnable, MoveSorter {
public:
	Search(Protocol* protocol, Game* game, Eval* eval, SEE* see, TranspositionTable* transt) {
		initialise(protocol, game, eval, see, transt, false);
	}

	Search(Game* game, Eval* eval, SEE* see, TranspositionTable* transt) {
		initialise(NULL, game, eval, see, transt, true);
	}

    virtual ~Search() {
    }

	int go(int wtime, int btime, int movestogo, int winc, int binc, int movetime) {
		initialiseSearch(wtime, btime, movestogo, winc, binc, movetime);

		Score alpha = -MAXSCORE;
		Score beta = MAXSCORE;

		while (!stop_search && search_depth < MAXDEPTH) {
			try {
				search_depth++;
				max_plies = std::max(32, search_depth + 16);

				Score score = searchRoot(search_depth, alpha, beta);

				storePV();

				if (moveIsEasy(score)) {
					break;
				}
				alpha = std::max(-MAXSCORE, score - 50);
				beta = std::min(MAXSCORE, score + 50);
			}
			catch (const int) {
				while (ply) {
					unmakeMove();
				}
				storePV();
			}
		}
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

	virtual Protocol* getProtocol() {
		return protocol;
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
		verbosity = 1;
	}

	void postInfo(int depth, int selective_depth, uint64 node_count, uint64 nodes_per_sec, uint64 time, int hash_full) {
		if (!worker) {
			protocol->postInfo(search_depth, max_ply_reached, node_count, nodesPerSecond(), timeUsed(),	transt->getLoad());
		}
	}

	Score searchRoot(const Depth depth, Score alpha, Score beta) {
		do { // Stay in this loop until an exact score is found.
			pv_length[0] = 0;
			pos->eval_score = eval->evaluate(alpha, beta);
			findTranspositionRefineEval(depth, alpha, beta);
			sortRootMoves(false);

			Move best_move = 0;
			Score best_score = -MAXSCORE;
			int move_count = 0;

			while (const MoveData* move_data = pos->nextMove()) {
				const Move m = move_data->move;

				if (makeMoveAndEvaluate(m, alpha, beta)) {
                    ++move_count;

					Depth next_depth = getNextDepth(0, move_count == 1, depth, move_count, move_data, alpha, best_score);

					if (search_depth > 10 && (search_time > 5000 || isAnalysing()) && protocol) {
						protocol->postInfo(m, move_count);
					}
					Score score;

					if (move_count == 1) {
						score = searchNextDepthPV(next_depth, -beta, -alpha);
					}
					else {
						score = searchNextDepthNotPV(next_depth, -alpha);

						if (score > alpha && depth > 1 && next_depth < depth - 1) {
							score = searchNextDepthNotPV(depth - 1, -alpha);
						}

						if (score > alpha) {
							score = searchNextDepthPV(std::max(depth - 1, next_depth), -beta, -alpha);
						}
					}
					unmakeMove();

					if (score > best_score) {
						best_score = score;

						if (score > alpha) {
							best_move = m;

							if (score >= beta) {
								updatePV(best_move, best_score, depth, BETA);
								break;
							}
							updatePV(best_move, best_score, depth, EXACT);
							alpha = score;
						}
						else {
							updatePV(m, best_score, depth, ALPHA);
						}
					}
				}
			}
			pos->gotoMove(0);

			if (move_count == 0) {
				return pos->in_check ? searchNodeScore(-MAXSCORE) : drawScore();
			}
			else if (best_move) {
				updateHistoryScores(best_move, depth);

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
			if (use_root_move_scores) { // Always false for now.
				move_data->score = root_move_score[move_count++];
			}
			else { // Always take this branch for the moment.
				sortMove(*move_data);
			}
		}
		pos->gotoMove(0);
	}

	__forceinline Score searchNextDepthPV(const Depth depth, const Score alpha, const Score beta) {
		if (pos->isDraw() || game->isRepetition()) {
			if (!isNullMove(pos->last_move)) {
				return drawScore();
			}
		}
		return depth <= 0 ? -searchQuiesce(alpha, beta, 0, true) : -searchPV(depth, alpha, beta);
	}

	__forceinline Score searchNextDepthNotPV(const Depth depth, const Score beta) {
		if (pos->isDraw() || game->isRepetition()) {
			if (!isNullMove(pos->last_move)) {
				return drawScore();
			}
		}
		return depth <= 0 ? -searchQuiesce(beta - 1, beta, 0, false) : -searchNotPV(depth, beta);
	}

	__forceinline void generateMoves() {
		if (pos->in_check) {
			pos->generateMoves(this, 0, LEGALMOVES);
		}
		else {
			pos->generateMoves(this, pos->transp_move, STAGES);
		}
    }

 	Score searchNotPV(const Depth depth, const Score beta) {
		findTranspositionRefineEval(depth, beta - 1, beta);

		if (pos->transp_score_valid && pos->transp_depth_valid) {
			return searchNodeScore(pos->transp_score);
		}

		if (ply >= max_plies - 1) {
			return searchNodeScore(pos->eval_score);
		}
		Score score;

		if (okToTryNullMove(depth, beta)) {
			makeMoveAndEvaluate(0, beta - 1, beta);
			score = searchNextDepthNotPV(depth - nullMoveReduction(depth), -beta + 1);
			unmakeMove();

			if (score >= beta) {
				return searchNodeScore(score);
			}
		}

		if (depth <= 3
			&& pos->eval_score + razor_margin[depth] < beta)
		{
			score = searchQuiesce(beta - 1, beta, 0, false);
			if (score < beta) {
				return std::max(score, pos->eval_score + razor_margin[depth]);
			}
		}
		generateMoves();

		Move best_move = 0;
		Score best_score = -MAXSCORE;
		int move_count = 0;

		while (const MoveData* move_data = pos->nextMove()) {
			const Move m = move_data->move;

			if (makeMoveAndEvaluate(m, beta - 1, beta)) {
				Depth next_depth = getNextDepth(0, false, depth, ++move_count, move_data, beta - 1, best_score);

				if (next_depth == -999) {
					unmakeMove();
					continue;
				}
				score = searchNextDepthNotPV(next_depth, -beta + 1);

				if (score >= beta && depth > 1 && next_depth < depth - 1) {
					score = searchNextDepthNotPV(depth - 1, -beta + 1);
				}
				unmakeMove();

				if (score > best_score) {
					best_score = score;

					if (score >= beta) {
						best_move = m;
						break;
					}
				}
			}
		}

		if (move_count == 0) {
			return pos->in_check ? searchNodeScore(-MAXSCORE + ply) : drawScore();
		}
		else if (pos->reversible_half_move_count >= 100) {
			return drawScore();
		}
		else if (best_move) {
			updateHistoryScores(best_move, depth);
			updateKillerMoves(best_move);
		}
		return storeSearchNodeScore(best_score, depth, nodeType(best_score, beta, best_move), best_move);
	}

 	Score searchPV(const Depth depth, Score alpha, const Score beta) {
		findTranspositionRefineEval(depth, alpha, beta);

		if (ply >= max_plies - 1) {
			return searchNodeScore(pos->eval_score);
		}
		Score score;

		if (okToTryNullMove(depth, beta)) {
			makeMoveAndEvaluate(0, alpha, beta);
			score = searchNextDepthNotPV(depth - nullMoveReduction(depth), -beta + 1);
			unmakeMove();

			if (score >= beta) {
				return searchNodeScore(score);
			}
		}
		Move singular_move = getSingularMove(depth, true);

		if (!pos->transp_move && depth >= 4) {
			searchPV(depth - 2, alpha, beta);
			findTranspositionRefineEval(depth, alpha, beta);
		}
		generateMoves();

		Move best_move = 0;
		Score best_score = -MAXSCORE;
		int move_count = 0;

		while (const MoveData* move_data = pos->nextMove()) {
			const Move m = move_data->move;

			if (makeMoveAndEvaluate(m, alpha, beta)) {
				++move_count;

				Depth next_depth = getNextDepth(singular_move, move_count == 1, depth, move_count, move_data, alpha,
													best_score);

				if (next_depth == -999) {
					unmakeMove();
					continue;
				}

				if (move_count == 1) {
					score = searchNextDepthPV(next_depth, -beta, -alpha);
				}
				else {
					score = searchNextDepthNotPV(next_depth, -alpha);

					if (score > alpha && depth > 1 && next_depth < depth - 1) {
						score = searchNextDepthNotPV(depth - 1, -alpha);
					}

					if (score > alpha) {
						score = searchNextDepthPV(std::max(depth - 1, next_depth), -beta, -alpha);
					}
				}
				unmakeMove();

				if (score > best_score) {
					best_score = score;

					if (best_score > alpha) {
						best_move = m;

						if (score >= beta) {
							updatePV(best_move, best_score, depth, BETA);
							break;
						}
						updatePV(best_move, best_score, depth, EXACT);
						alpha = best_score;
					}
					else {
						updatePV(m, best_score, depth, ALPHA);
					}
				}
			}
		}

		if (move_count == 0) {
			return pos->in_check ? searchNodeScore(-MAXSCORE + ply) : drawScore();
		}
		else if (pos->reversible_half_move_count >= 100) {
			return drawScore();
		}
		else if (best_move) {
			updateHistoryScores(best_move, depth);

			if (best_score >= beta) {
				updateKillerMoves(best_move);
			}
		}
		return storeSearchNodeScore(best_score, depth, nodeType(best_score, beta, best_move), best_move);
	}

	__forceinline Move getSingularMove(const Depth depth, const bool is_pv_node) {
		if (is_pv_node
			&& (pos->transp_flags & EXACT)
			&& depth >= 4
			&& pos->transp_move)
		{
			if (searchFailLow(depth/2, std::max(-MAXSCORE, pos->eval_score - 75), pos->transp_move)) {
				return pos->transp_move;
			}
        }
		return 0;
	}

	bool searchFailLow(const Depth depth, Score alpha, const Move exclude_move) {
		generateMoves();

		Score best_score = -MAXSCORE;
		int move_count = 0;

		while (const MoveData* move_data = pos->nextMove()) {
			const Move m = move_data->move;

			if (m == exclude_move) {
				continue;
			}

			if (makeMoveAndEvaluate(m, alpha, alpha + 1)) {
				Depth next_depth = getNextDepth(0, false, depth, ++move_count, move_data, alpha, best_score);

				if (next_depth == -999) {
					unmakeMove();
					continue;
				}
				Score score = searchNextDepthNotPV(next_depth, -alpha);

				if (score > alpha && depth > 1 && next_depth < depth - 1) {
					score = searchNextDepthNotPV(depth - 1, -alpha);
				}
				unmakeMove();

				if (score > alpha) {
					return false;
				}
			}
		}

		if (move_count == 0) {
			return false;
		}
		return true;
	}

	__forceinline bool okToTryNullMove(const Depth depth, const Score beta) const {
		return !pos->in_check
			&& pos->null_moves_in_row < 1
			&& !pos->material.isKx(pos->side_to_move)
			&& pos->eval_score >= beta;
	}

	__forceinline Depth getNextDepth(Move singular_move, bool is_pv_node, const Depth depth, const int move_count,
		const MoveData* move_data, const Score alpha, Score& best_score) const
	{
		const Move m = move_data->move;

		if (m == singular_move && singular_move != 0) {
			return depth;
		}
		bool reduce = true;

		if (pos->in_check) {
			if (see->seeLastMove(m) >= 0) {
				return depth;
			}
			reduce = false;
		}

        if (isPassedPawnMove(m)) {
			if (see->seeLastMove(m) >= 0) {
                int r = rank(moveTo(m));

                if (r == 1 || r == 6) {
                    return depth;
                }
				reduce = false;
        }
        }

		if (((pos-1)->in_check && (pos-1)->moveCount() == 1)) {
			// Single repy extension is a way to find the mate in 9 in
			// 3r1r2/pppb1p1k/2npqP1p/2b1p3/8/2NP2PP/PPPQN1BK/R4R2 w - - 0 1
			if (is_pv_node) {
				return depth;
			}
			reduce = false;
		}

		if (reduce
			&& !isQueenPromotion(m)
			&& !isCapture(m)
			&& !isKillerMove(m, ply - 1)
			&& move_count >= 3)
		{
			if ((pos-1)->last_move != 0
				&& counter_moves[movePiece((pos-1)->last_move)][moveTo((pos-1)->last_move)] == m)
			{
				return depth - 2;
			}
			Depth next_depth = depth - 2 - depth/8 - (move_count-6)/12;

			if (next_depth <= 3
				&& -pos->eval_score + futility_margin[std::max(0, next_depth)] < alpha)
			{
				best_score = std::max(best_score, -pos->eval_score + futility_margin[std::max(0, next_depth)]);
				return -999;
			}
            return next_depth;
		}
		return depth - 1;
	}

	__forceinline int nullMoveReduction(const Depth depth) const {
		return 4 + depth/8;
	}

	Score searchQuiesce(Score alpha, const Score beta, int qs_ply, bool search_pv) {
		findTransposition(0, alpha, beta);

		if (!search_pv && pos->transp_score_valid && pos->transp_depth_valid) {
			return searchNodeScore(pos->transp_score);
		}

		if (pos->eval_score >= beta) {
			return searchNodeScore(pos->eval_score);
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

			if (!isPromotion(m)) {
				if (move_data->score < 0) {
					break;
				}
				else if (pos->eval_score + piece_value(moveCaptured(m)) + 150 < alpha) {
					best_score = std::max(best_score, pos->eval_score + piece_value(moveCaptured(m)) + 150);
					continue;
				}
			}

			if (makeMoveAndEvaluate(m, alpha, beta)) {
				++move_count;

				Score score;

				if (pos->isDraw()) {
					score = drawScore();
				}
				else {
					score = -searchQuiesce(-beta, -alpha, qs_ply + 1, (search_pv && move_count == 1));
				}
				unmakeMove();

				if (score > best_score) {
					best_score = score;

					if (best_score > alpha) {
						best_move = m;

						if (score >= beta) {
							break;
						}
						updatePV(best_move, best_score, 0, EXACT);
						alpha = best_score;
					}
				}
			}
		}

		if (move_count == 0) {
			return searchNodeScore(best_score);
		}
		return storeSearchNodeScore(best_score, 0, nodeType(best_score, beta, best_move), best_move);
	}

	__forceinline bool makeMoveAndEvaluate(const Move m, int alpha, int beta) {
		if (game->makeMove(m, true, true)) {
			pos = game->pos;
			++ply;
			pv_length[ply] = ply;
			node_count++;

			pos->eval_score = eval->evaluate(-beta, -alpha);

			if (ply > max_ply_reached) {
				max_ply_reached = ply;
			}
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
		if ((node_count & 0x3FFF) == 0) {//!!!!
			if (!isAnalysing() && !protocol->isFixedDepth()) {
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
			postInfo(search_depth, max_ply_reached, node_count, nodesPerSecond(), timeUsed(), transt->getLoad());
		}
	}

	__forceinline int isAnalysing() const {
		return protocol ? protocol->isAnalysing() : true;
	}

	__forceinline uint64 timeUsed() const {
		return std::max((uint64)1, millis() - start_time);
	}

	__forceinline void updatePV(const Move move, const Score score, const Depth depth, int node_type) {
		PVEntry* entry = &pv[ply][ply];

		entry->score = score;
		entry->depth = depth;
		entry->key = pos->key;
		entry->move = move;

		pv_length[ply] = pv_length[ply + 1];

		for (int i = ply + 1; i < pv_length[ply]; i++) {
			pv[ply][i] = pv[ply + 1][i];
		}
		if (ply == 0) {
			char buf[2048], buf2[16]; buf[0] = 0;
			for (int i = ply; i < pv_length[ply]; i++) {
				snprintf(&buf[strlen(buf)], sizeof(buf) - strlen(buf), "%s ",
					game->moveToString(pv[ply][i].move, buf2));
			}
			if (!worker && verbosity > 0) {
				protocol->postPV(search_depth, max_ply_reached, node_count, nodesPerSecond(),
					timeUsed(), transt->getLoad(), score, buf, node_type);
			}
		}
	}

	__forceinline void storePV() {
		if (pv_length[0]) {
			pos->pv_length = pv_length[0];
			for (int i = 0; i < pos->pv_length; i++) {
				const PVEntry& entry = pv[0][i];
				transt->insert(entry.key, entry.depth, entry.score, EXACT, entry.move);
			}
		}
	}

	uint64 nodesPerSecond() const {
		return (uint64)(node_count*1000/std::max((uint64)1, (millis() - start_time)));
	}

	__forceinline void updateHistoryScores(const Move move, const Depth depth) {
        history_scores[movePiece(move)][moveTo(move)] += depth*depth;

        if (history_scores[movePiece(move)][moveTo(move)] > PROMOTIONMOVESCORE) {
            for (int i = 0; i < 16; i++) for (int k = 0; k < 64; k++) {
                history_scores[i][k] >>= 2;
            }
        }
	}

	__forceinline void updateKillerMoves(const Move move) {
		// Same move can be stored twice for a ply.
		if (!isCapture(move) && !isPromotion(move)) {
			if (pos->last_move != 0) {
				counter_moves[movePiece(pos->last_move)][moveTo(pos->last_move)] = move;
			}

			if (move != killer_moves[0][ply]) {
				killer_moves[2][ply] = killer_moves[1][ply];
				killer_moves[1][ply] = killer_moves[0][ply];
				killer_moves[0][ply] = move;
			}
		}
	}

	__forceinline bool isKillerMove(const Move m, int ply) const {
        return m == killer_moves[0][ply] || m == killer_moves[1][ply] || m == killer_moves[2][ply];
	}

	__forceinline Score codecTTableScore(Score score, Score ply) const {
		if (abs((int)score) < MAXSCORE - 128) {
			return score;
		}
		return score < 0 ? score - ply : score + ply;
	}

	void initialiseSearch(int wtime, int btime, int movestogo, int winc, int binc, int movetime) {
		pos = game->pos; // Updated in makeMove and unmakeMove from here on.

		if (!worker) {
			if (protocol->isFixedTime()) {
				search_time = (int)(0.9*(double)movetime);
			}
			else {
				int moves_left = movestogo;
				if (moves_left == 0) {
					moves_left = 30;
				}

				time_left = pos->side_to_move == 0 ? wtime : btime;
				time_inc = pos->side_to_move == 0 ? winc : binc;

				search_time = time_left/moves_left + time_inc;

				if (movestogo == 0 && search_time > time_left/4) {
					search_time = time_inc ? time_left/10 : time_left/30;
				}
				else if (movestogo == 1) {
					search_time = (int)(search_time*0.5);
				}
				else if (movestogo > 1) {
					search_time = (int)(search_time*0.95);
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
		memset(killer_moves, 0, sizeof(killer_moves));
		memset(history_scores, 0, sizeof(history_scores));
		memset(counter_moves, 0, sizeof(counter_moves));
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
			move_data.score = KILLERMOVESCORE + 20;
		}
		else if (m == killer_moves[1][ply]) {
			move_data.score = KILLERMOVESCORE + 19;
		}
		else if (m == killer_moves[2][ply]) {
			move_data.score = KILLERMOVESCORE + 18;
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

	__forceinline Score drawScore() const {
		return 0;
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
				pos->eval_score = pos->transp_score;
				break;
			case BETA:
				pos->transp_score_valid = pos->transp_score >= beta;
				pos->eval_score = std::max(pos->eval_score, pos->transp_score);
				break;
			case ALPHA:
				pos->transp_score_valid = pos->transp_score <= alpha;
				pos->eval_score = std::min(pos->eval_score, pos->transp_score);
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

	bool moveIsEasy(const Score score) const {
		if (!worker) {
			if ((pos->moveCount() == 1 && search_depth > 9)
				|| (protocol->isFixedDepth() && protocol->getDepth() == search_depth)
				|| (score == MAXSCORE - 1))
			{
				return true;
			}
		}
		return false;
	}

	__forceinline bool isPassedPawnMove(const Move m) const {
		return movePieceType(m) == Pawn && board->isPawnPassed(moveTo(m), moveSide(m));
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
	int verbosity;

protected:
	Depth search_depth;
	bool worker;
	Move killer_moves[4][128];
	int history_scores[16][64];
	Move counter_moves[16][64];
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

	static int futility_margin[4];
	static int razor_margin[4];
};

uint64 Search::node_count;
const int Search::EXACT;
const int Search::ALPHA;
const int Search::BETA;
const int Search::MAXSCORE;

int Search::futility_margin[4] = { 150, 150, 150 ,400 };
int Search::razor_margin[4] = { 0, 125, 125, 400 };

//vanaf depth 36 vreemde matscores (32298 of zo)
//7k/8/6KN/7B/8/8/8/8 w - - 5 1
