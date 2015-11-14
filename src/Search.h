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
typedef int Depth;
typedef int Score;

class Search : public MoveSorter {
public:
  Search(Protocol* protocol, Game* game, Eval* eval, See* see, TranspositionTable* transt, Logger* logger) {
    initialise(protocol, game, eval, see, transt, logger);
  }

  Search(Game* game, Eval* eval, See* see, TranspositionTable* transt) {
    initialise(0, game, eval, see, transt, 0);
    stop_search = false;
  }

  virtual ~Search() {
  }

  int go(int wtime, int btime, int movestogo, int winc, int binc, int movetime) {
    initialiseSearch(wtime, btime, movestogo, winc, binc, movetime);

    auto alpha = -MAXSCORE;
    auto beta = MAXSCORE;

    while (!stop_search && search_depth < MAXDEPTH) {
      try {
        search_depth++;

        do {
          pv_length[0] = 0;

          if (!getTransposition()) {
            pos->eval_score = eval->evaluate(alpha, beta);
          }
          auto score = searchPV(search_depth, alpha, beta);

          if (score > alpha && score < beta) {
            break;
          }
          alpha = -MAXSCORE;
          beta = MAXSCORE;
        } while (true);

        storePV();

        if (moveIsEasy()) {
          break;
        }
        alpha = std::max(-MAXSCORE, pv[0][0].score - 50);
        beta = std::min(MAXSCORE, pv[0][0].score + 50);
      }
      catch (const int) {
        while (ply) {
          unmakeMove();
        }

        if (pv_length[0]) {
          storePV();
        }
      }
    }
    return 0;
  }

  void stop() {
    stop_search = true;
  }

  virtual void run() {
    go(0, 0, 0, 0, 0, 0);
  }

  virtual void newGame() {
  }

  virtual Protocol* getProtocol() {
    return protocol;
  }

  __forceinline uint64_t timeUsed() const {
    return std::max(static_cast<uint64_t>(1), millis() - start_time);
  }

protected:
  void initialise(Protocol* protocol, Game* game, Eval* eval, See* see, TranspositionTable* transt, Logger* logger) {
    this->protocol = protocol;
    this->game = game;
    this->eval = eval;
    this->see = see;
    this->transt = transt;
    this->logger = logger;
    board = game->pos->board;
    verbosity = 1;
    lag_buffer = -1;
  }

  /*void postInfo(int depth, int selective_depth, uint64 node_count, uint64 nodes_per_sec, uint64 time, int hash_full) {
    if (!worker) {
      protocol->postInfo(search_depth, max_ply_reached, node_count, nodesPerSecond(), timeUsed(), transt->getLoad());
    }
  }*/

  Score searchPV(const Depth depth, Score alpha, const Score beta) {
    if (ply >= MAXDEPTH - 1) {
      return searchNodeScore(pos->eval_score);
    }
    Score score;

    if (okToTryNullMove(depth, beta)) {
      makeMoveAndEvaluate(0, alpha, beta);
      score = searchNextDepthNotPV(depth - nullMoveReduction(depth), -beta + 1, 128);
      unmakeMove();

      if (score >= beta) {
        return searchNodeScore(score);
      }
    }
    Move singular_move = getSingularMove(depth, true);

    if (!pos->transp_move && depth >= 4) {
      searchPV(depth - 2, alpha, beta);
      getTransposition();
    }
    generateMoves();

    auto best_move = 0;
    auto best_score = -MAXSCORE;
    auto move_count = 0;

    while (const auto move_data = pos->nextMove()) {
      if (makeMoveAndEvaluate(move_data->move, alpha, beta)) {
        //if (ply == 0 && search_depth > 10 && (search_time > 5000 || isAnalysing()) && protocol) {
        //  protocol->postInfo(m, move_count);
        //}
        Score score;

        if (++move_count == 1) {
          score = searchNextDepthPV(nextDepthPV(singular_move, depth, move_data), -beta, -alpha);
        }
        else {
          auto next_depth = nextDepthNotPV(depth, move_count, move_data, alpha, best_score, EXACT);

          if (next_depth == -999) {
            unmakeMove();
            continue;
          }
          score = searchNextDepthNotPV(next_depth, -alpha, BETA);

          if (score > alpha && depth > 1 && next_depth < depth - 1) {
            score = searchNextDepthNotPV(depth - 1, -alpha, BETA);
          }

          if (score > alpha) {
            score = searchNextDepthPV(nextDepthPV(0, depth, move_data), -beta, -alpha);
          }
        }
        unmakeMove();

        if (score > best_score) {
          best_score = score;

          if (best_score > alpha) {
            best_move = move_data->move;

            if (score >= beta) {
              updatePV(best_move, best_score, depth, BETA);
              break;
            }
            updatePV(best_move, best_score, depth, EXACT);
            alpha = best_score;
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

  __forceinline Score searchNextDepthPV(const Depth depth, const Score alpha, const Score beta) {
    if (pos->isDraw() || game->isRepetition()) {
      if (!isNullMove(pos->last_move)) {
        return drawScore();
      }
    }
    return depth <= 0 ? -searchQuiesce(alpha, beta, 0, true) : -searchPV(depth, alpha, beta);
  }

  bool givesCheck(const Move m) {
    board->makeMove(m);
    auto ret = board->isAttacked(board->king_square[pos->side_to_move^1], pos->side_to_move);
    board->unmakeMove(m);
    return ret;
  }

  Score searchNotPV(const Depth depth, const Score beta, const int expectedNodeType) {
    if (isTranspositionScoreValid(depth, beta - 1, beta)) {
      return pos->transp_score;
    }

    if (ply >= MAXDEPTH - 1) {
      return searchNodeScore(pos->eval_score);
    }

    if (okToTryNullMove(depth, beta)) {
      makeMoveAndEvaluate(0, beta - 1, beta);
      auto score = searchNextDepthNotPV(depth - nullMoveReduction(depth), -beta + 1, ALPHA);
      unmakeMove();

      if (score >= beta) {
        return searchNodeScore(score);
      }
    }

    if (depth <= 3
        && pos->eval_score + razor_margin[depth] < beta)
    {
      auto score = searchQuiesce(beta - 1, beta, 0, false);

      if (score < beta) {
        return std::max(score, pos->eval_score + razor_margin[depth]);
      }
    }
    generateMoves();

    auto best_move = 0;
    auto best_score = -MAXSCORE;
    auto move_count = 0;

    auto nextExpectedNodeType = expectedNodeType == ALPHA ? BETA : ALPHA;

    while (const auto move_data = pos->nextMove()) {
      if (makeMoveAndEvaluate(move_data->move, beta - 1, beta)) {
        auto next_depth = nextDepthNotPV(depth, ++move_count, move_data, beta - 1, best_score, expectedNodeType);

        if (next_depth == -999) {
          unmakeMove();
          continue;
        }
        auto score = searchNextDepthNotPV(next_depth, -beta + 1, nextExpectedNodeType);

        if (score >= beta && depth > 1 && next_depth < depth - 1) {
          score = searchNextDepthNotPV(depth - 1, -beta + 1, nextExpectedNodeType);
        }
        unmakeMove();

        if (score > best_score) {
          best_score = score;

          if (score >= beta) {
            best_move = move_data->move;
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

  __forceinline Score searchNextDepthNotPV(const Depth depth, Score beta, int expectedNodeType) {
    if (pos->isDraw() || game->isRepetition()) {
      if (!isNullMove(pos->last_move)) {
        return drawScore();
      }
    }
    return depth <= 0 ? -searchQuiesce(beta - 1, beta, 0, false) : -searchNotPV(depth, beta, expectedNodeType);
  }

  __forceinline void generateMoves() {
    if (pos->in_check) {
      pos->generateMoves(this, 0, LEGALMOVES);
    }
    else {
      pos->generateMoves(this, pos->transp_move, STAGES);
    }
  }

  __forceinline Move getSingularMove(const Depth depth, const bool is_pv_node) {
    if (is_pv_node
        && (pos->transp_type == EXACT)
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

    auto move_count = 0;

    while (const auto move_data = pos->nextMove()) {
      if (move_data->move == exclude_move) {
        continue;
      }
      auto best_score = -MAXSCORE; //dummy

      if (makeMoveAndEvaluate(move_data->move, alpha, alpha + 1)) {
        Depth next_depth = nextDepthNotPV(depth, ++move_count, move_data, alpha, best_score, ALPHA);

        if (next_depth == -999) {
          unmakeMove();
          continue;
        }
        auto score = searchNextDepthNotPV(next_depth, -alpha, BETA);

        if (score > alpha && depth > 1 && next_depth < depth - 1) {
          score = searchNextDepthNotPV(depth - 1, -alpha, BETA);
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

  __forceinline Depth nextDepthNotPV(const Depth depth, const int move_count,
                                     const MoveData* move_data, const Score alpha, Score& best_score,
                                     int expectedNodeType)
  {
    const auto m = move_data->move;
    auto reduce = true;

    if (pos->in_check) {
      if (see->seeLastMove(m) >= 0) {
        return depth;
      }
      //reduce = false;
    }

    if (reduce
        && !isQueenPromotion(m)
        && !isCapture(m)
        && !isKillerMove(m, ply - 1)
        && move_count >= 3
        && depth > 1)
    {
      Depth next_depth = depth - 2 - depth/8 - (move_count-6)/12;

      if (next_depth <= 3
          && -pos->eval_score + futility_margin[std::max(0, next_depth)] < alpha)
      {
        best_score = std::max(best_score, -pos->eval_score + futility_margin[std::max(0, next_depth)]);
        return -999;
      }

      if (expectedNodeType == BETA) {
        next_depth -= 2;
      }
      return next_depth;
    }
    return depth - 1;
  }

  __forceinline Depth nextDepthPV(Move singular_move, const Depth depth, const MoveData* move_data) const {
    const auto m = move_data->move;

    if (m == singular_move) {
      return depth;
    }

    if (pos->in_check || isPassedPawnMove(m)) {
      if (see->seeLastMove(m) >= 0) {
        return depth;
      }
    }

    if (((pos-1)->in_check && (pos-1)->moveCount() == 1)) {
      return depth;
    }
    return depth - 1;
  }

  __forceinline int nullMoveReduction(const Depth depth) const {
    return 4 + depth/4;
  }

  Score searchQuiesce(Score alpha, const Score beta, int qs_ply, bool search_pv) {
    if (!search_pv && isTranspositionScoreValid(0, alpha, beta)) {
      return searchNodeScore(pos->transp_score);
    }

    if (pos->eval_score >= beta) {
      if (!pos->transposition || pos->transp_depth <= 0) {
        return storeSearchNodeScore(pos->eval_score, 0, BETA, 0);
      }
      return searchNodeScore(pos->eval_score);
    }

    if (ply >= MAXDEPTH - 1 || qs_ply > 6) {
      return searchNodeScore(pos->eval_score);
    }
    auto best_move = 0;
    auto best_score = pos->eval_score;
    auto move_count = 0;

    if (best_score > alpha) {
      alpha = best_score;
    }
    pos->generateCapturesAndPromotions(this);

    while (const auto move_data = pos->nextMove()) {
      if (!isPromotion(move_data->move)) {
        if (move_data->score < 0) {
          break;
        }
        else if (pos->eval_score + piece_value(moveCaptured(move_data->move)) + 150 < alpha) {
          best_score = std::max(best_score, pos->eval_score + piece_value(moveCaptured(move_data->move)) + 150);
          continue;
        }
      }

      if (makeMoveAndEvaluate(move_data->move, alpha, beta)) {
        ++move_count;

        Score score;

        if (pos->isDraw()) {
          score = drawScore();
        }
        else {
          score = -searchQuiesce(-beta, -alpha, qs_ply + 1, search_pv && move_count == 1);
        }
        unmakeMove();

        if (score > best_score) {
          best_score = score;

          if (best_score > alpha) {
            best_move = move_data->move;

            if (score >= beta) {
              break;
            }
            updatePV(best_move, best_score, 0, EXACT);
            alpha = best_score;
          }
        }
      }
    }
    if (!pos->transposition || pos->transp_depth <= 0) {
      return storeSearchNodeScore(best_score, 0, nodeType(best_score, beta, best_move), best_move);
    }
    return searchNodeScore(best_score);
  }

  __forceinline bool makeMoveAndEvaluate(const Move m, int alpha, int beta) {
    if (game->makeMove(m, true, true)) {
      pos = game->pos;
      ++ply;
      pv_length[ply] = ply;
      ++total_node_count;
      ++node_count;

      if (!getTransposition()) {
        pos->eval_score = eval->evaluate(-beta, -alpha);
      }
      max_ply = std::max(max_ply, ply);
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
    if ((node_count & 0x1fff) == 0) {
      if (protocol) {
        if (!isAnalysing() && !protocol->isFixedDepth()) {
          stop_search = search_depth > 1 && timeUsed() > search_time;
        }
        else {
          protocol->checkInput();
        }
      }

      if (stop_search) {
        throw 1;
      }
    }
    /*if ((node_count & 0x3FFFFF) == 0) {
      postInfo(search_depth, max_ply_reached, node_count, nodesPerSecond(), timeUsed(), transt->getLoad());
    }*/
  }

  __forceinline int isAnalysing() const {
    return protocol ? protocol->isAnalysing() : true;
  }

  __forceinline void updatePV(const Move move, const Score score, const Depth depth, int node_type) {
    PVEntry* entry = &pv[ply][ply];

    entry->score = score;
    entry->depth = depth;
    entry->key = pos->key;
    entry->move = move;
    entry->node_type = node_type;
    entry->eval = pos->eval_score;

    pv_length[ply] = pv_length[ply + 1];

    for (auto i = ply + 1; i < pv_length[ply]; ++i) {
      pv[ply][i] = pv[ply + 1][i];
    }
    if (ply == 0) {
      pos->pv_length = pv_length[0];

      char buf[2048], buf2[16];
      buf[0] = 0;

      for (auto i = ply; i < pv_length[ply]; ++i) {
        snprintf(&buf[strlen(buf)], sizeof(buf) - strlen(buf), "%s ",
                 game->moveToString(pv[ply][i].move, buf2));
      }

      if (protocol && verbosity > 0) {
        protocol->postPV(search_depth, max_ply, total_node_count, nodesPerSecond(),
                         timeUsed(), transt->getLoad(), score, buf, node_type);
      }
    }
  }

  __forceinline void storePV() {
    assert(pv_length[0] > 0);
    for (auto i = 0; i < pv_length[0]; ++i) {
      const auto& entry = pv[0][i];
      transt->insert(entry.key, entry.depth, entry.score, entry.node_type, entry.move, entry.eval);
    }
  }

  uint64_t nodesPerSecond() const {
    return (uint64_t)(total_node_count*1000/std::max((uint64_t)1, (millis() - start_time)));
  }

  __forceinline void updateHistoryScores(const Move move, const Depth depth) {
    history_scores[movePiece(move)][moveTo(move)] += depth*depth;

    if (history_scores[movePiece(move)][moveTo(move)] > PROMOTIONMOVESCORE - 100) {
      for (auto i = 0; i < 16; ++i)
        for (auto k = 0; k < 64; ++k) {
          history_scores[i][k] >>= 2;
        }
    }
  }

  __forceinline void updateKillerMoves(const Move move) {
    // Same move can be stored twice for a ply.
    if (!isCapture(move) && !isPromotion(move)) {
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

    if (protocol) {
      if (protocol->isFixedTime()) {
        search_time = 900*movetime/1000;
      }
      else {
        int moves_left = std::min(movestogo, movestogo < 80 ? 30 : movestogo/2);

        if (moves_left == 0) {
          moves_left = 30;
        }

        if (lag_buffer == -1) {
          lag_buffer = wtime > 5000 ? 200 : 50;
        }
        time_left = pos->side_to_move == 0 ? wtime : btime;
        time_inc = pos->side_to_move == 0 ? winc : binc;

        search_time = 2*(time_left/(moves_left + 1) + time_inc);

        // This helps in very fast controls such as 1000ms/80ms
        if (movestogo == 0 && search_time > time_left/4) {
          search_time = time_left/4;
        }
        search_time = std::max(0, std::min((int)search_time, (int)time_left - lag_buffer));
      }
      start_time = millis();
      transt->initialiseSearch();
      total_node_count = 1;
      stop_search = false;
    }
    ply = 0;
    search_depth = 0;
    node_count = 1;
    max_ply = 0;
    pos->pv_length = 0;
    memset(pv, 0, sizeof(pv));
    memset(killer_moves, 0, sizeof(killer_moves));
    memset(history_scores, 0, sizeof(history_scores));
  }

  double perc(int x1, int x2) {
    return (x1 + x2 == 0) ? 0 : 100*((double)x1/(x1 + x2));
  }

  virtual void sortMove(MoveData& move_data) {
    const auto m = move_data.move;

    if (pos->transp_move == m) {
      move_data.score = 890010;
    }
    else if (isQueenPromotion(m)) {
      move_data.score = 890000;
    }
    else if (isCapture(m)) {
      auto value_captured = piece_value(moveCaptured(m));
      auto value_piece = piece_value(movePiece(m));

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

  __forceinline void storeTransposition(const Depth depth, Score score, const int node_type, const Move move) {
    score = codecTTableScore(score, ply);

    if (node_type == BETA) {
      pos->eval_score = std::max(pos->eval_score, score);
    }
    else if (node_type == ALPHA) {
      pos->eval_score = std::min(pos->eval_score, score);
    }
    else if (node_type == EXACT) {
      pos->eval_score = score;
    }
    pos->transposition = transt->insert(pos->key, depth, score, node_type, move, pos->eval_score);
  }

  __forceinline bool getTransposition() {
    if ((pos->transposition = transt->find(pos->key)) == 0) {
      pos->transp_type = 0;
      pos->transp_move = 0;
      return false;
    }
    pos->transp_score = codecTTableScore(pos->transposition->score, -ply);
    pos->eval_score = codecTTableScore(pos->transposition->eval, -ply);
    pos->transp_depth = pos->transposition->depth;
    pos->transp_type = pos->transposition->flags & 7;
    pos->transp_move = pos->transposition->move;
    pos->flags = 0;
    return true;
  }

  __forceinline bool isTranspositionScoreValid(const Depth depth, const Score alpha, const Score beta) {
    if (pos->transposition && pos->transp_depth >= depth) {
      switch (pos->transp_type) {
      case EXACT:
        return true;
      case BETA:
        return pos->transp_score >= beta;
      case ALPHA:
        return pos->transp_score <= alpha;
      default://error
        break;
      }
    }
    return false;
  }

  __forceinline int nodeType(const Score score, const Score beta, const Move move) const {
    return move ? (score >= beta ? BETA : EXACT) : ALPHA;
  }

  bool moveIsEasy() const {
    if (protocol) {
      if ((pos->moveCount() == 1 && search_depth > 9)
          || (protocol->isFixedDepth() && protocol->getDepth() == search_depth)
          || (pv[0][0].score == MAXSCORE - 1))
      {
        return true;
      }

      if (!isAnalysing() && !protocol->isFixedDepth() && search_time < timeUsed()*2.5) {
        return true;
      }
    }
    return false;
  }

  __forceinline bool isPassedPawnMove(const Move m) const {
    return movePieceType(m) == Pawn && board->isPawnPassed(moveTo(m), moveSide(m));
  }

  struct PVEntry {
    uint64_t key;
    Depth depth;
    Score score;
    Move move;
    int node_type;
    int eval;
  };

public:
  Depth ply;
  Depth max_ply;
  PVEntry pv[128][128];
  int pv_length[128];
  uint64_t start_time;
  uint64_t search_time;
  uint64_t time_left;
  uint64_t time_inc;
  int lag_buffer;
  volatile bool stop_search;
  int verbosity;

protected:
  Depth search_depth;
  Move killer_moves[4][128];
  int history_scores[16][64];
  Protocol* protocol;
  Game* game;
  Eval* eval;
  Board* board;
  See* see;
  Position* pos;
  TTable* transt;
  Logger* logger;

  uint64_t node_count;
  static uint64_t total_node_count;

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

uint64_t Search::total_node_count;

const int Search::EXACT;
const int Search::ALPHA;
const int Search::BETA;
const int Search::MAXSCORE;

int Search::futility_margin[4] = { 150, 150, 150, 400 };
int Search::razor_margin[4] = { 0, 125, 125, 400 };
