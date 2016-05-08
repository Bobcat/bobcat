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

class Search : public MoveSorter
{
public:
  Search(Protocol* protocol, Game* game, Eval* eval, See* see, TranspositionTable* transt, Logger* logger)
  {
    initialise(protocol, game, eval, see, transt, logger);
  }

  Search(Game* game, Eval* eval, See* see, TranspositionTable* transt)
  {
    initialise(0, game, eval, see, transt, 0);
    stop_search = false;
  }

  virtual ~Search() {
  }

  int go(int wtime, int btime, int movestogo, int winc, int binc, int movetime, int num_workers)
  {
    num_workers_ = num_workers;
    initialiseSearch(wtime, btime, movestogo, winc, binc, movetime);

    drawScore_[pos->side_to_move] = 0;//-25;
    drawScore_[!pos->side_to_move] = 0;//25;

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
          auto score = search(true, search_depth, alpha, beta, EXACT);

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

  void stop()
  {
    stop_search = true;
  }

  virtual void run()
  {
    go(0, 0, 0, 0, 0, 0, 0);
  }

  __forceinline uint64_t millisUsed() const {
    return start_time.millisElapsed();
  }

protected:
  void initialise(Protocol* protocol, Game* game, Eval* eval, See* see, TranspositionTable* transt, Logger* logger)
  {
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

  Score search(bool pv, const Depth depth, Score alpha, const Score beta, int expectedNodeType)
  {
    if (!pv && isTranspositionScoreValid(depth, alpha, beta)) {
      return pos->transp_score;
    }

    if (ply >= MAXDEPTH - 1) {
      return searchNodeScore(pos->eval_score);
    }

    if (!pv && shouldTryNullMove(depth, beta)) {
      makeMoveAndEvaluate(0, alpha, beta);
      auto score = searchNextDepth(false, depth - nullMoveReduction(depth), -beta, -beta + 1, ALPHA);
      unmakeMove();

      if (score >= beta) {
        return searchNodeScore(score);
      }
    }

    if (!pv && depth <= 3
        && pos->eval_score + razor_margin[depth] < beta)
    {
      auto score = searchQuiesce(beta - 1, beta, 0, false);

      if (score < beta) {
        return std::max(score, pos->eval_score + razor_margin[depth]);
      }
    }

    Move singular_move = pv ? getSingularMove(depth, true) : 0;

    pos->generateMoves(this, pos->transp_move, STAGES);

    auto best_move = 0;
    auto best_score = -MAXSCORE;
    auto move_count = 0;

    while (const auto move_data = pos->nextMove()) {
      Score score;

      if (makeMoveAndEvaluate(move_data->move, alpha, beta)) {
        ++move_count;

        if (move_count == 1 && pv) {
          score = searchNextDepth(true, nextDepthPV(singular_move, depth, move_data), -beta, -alpha, EXACT);
        }
        else {
          auto next_depth = nextDepthNotPV(pv, depth, move_count, move_data, alpha, best_score);
          auto nextExpectedNodeType = (expectedNodeType & (EXACT|ALPHA)) ? BETA : ALPHA;

          if (next_depth == -999) {
            unmakeMove();
            continue;
          }
          score = searchNextDepth(false, next_depth, -alpha - 1, -alpha, nextExpectedNodeType);

          if (score > alpha && depth > 1 && next_depth < depth - 1) {
            score = searchNextDepth(false, depth - 1, -alpha - 1, -alpha, nextExpectedNodeType);
          }

          if (score > alpha && score < beta) {
            score = searchNextDepth(true, nextDepthPV(0, depth, move_data), -beta, -alpha, EXACT);
          }
        }
        unmakeMove();

        if (score > best_score) {
          best_score = score;

          if (best_score > alpha) {
            best_move = move_data->move;

            if (score >= beta) {
              if (ply == 0) {
                updatePV(best_move, best_score, depth, BETA);
              }
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
    return storeSearchNodeScore(best_score, depth, nodeType(best_score, beta, best_move), best_move);
  }

  __forceinline Score searchNextDepth(bool pv, const Depth depth, Score alpha, Score beta, int expectedNodeType)
  {
    if (pos->isDraw() || game->isRepetition()) {
      if (!isNullMove(pos->last_move)) {
        return -drawScore();
      }
    }
    return depth <= 0 ? -searchQuiesce(alpha, beta, 0, pv) : -search(pv, depth, alpha, beta, expectedNodeType);
  }

  __forceinline Move getSingularMove(const Depth depth, const bool is_pv_node)
  {
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

  bool searchFailLow(const Depth depth, Score alpha, const Move exclude_move)
  {
    pos->generateMoves(this, pos->transp_move, STAGES);

    auto move_count = 0;

    while (const auto move_data = pos->nextMove()) {
      if (move_data->move == exclude_move) {
        continue;
      }
      auto best_score = -MAXSCORE; //dummy

      if (makeMoveAndEvaluate(move_data->move, alpha, alpha + 1)) {
        Depth next_depth = nextDepthNotPV(true, depth, ++move_count, move_data, alpha, best_score);

        if (next_depth == -999) {
          unmakeMove();
          continue;
        }
        auto score = searchNextDepth(false, next_depth, -alpha - 1, -alpha, BETA);

        if (score > alpha && depth > 1 && next_depth < depth - 1) {
          score = searchNextDepth(false, depth - 1, -alpha - 1, -alpha, BETA);
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

  __forceinline bool shouldTryNullMove(const Depth depth, const Score beta) const
  {
    return !pos->in_check
           && pos->null_moves_in_row < 1
           && !pos->material.isKx(pos->side_to_move)
           && pos->eval_score >= beta;
  }

  __forceinline Depth nextDepthNotPV(bool pv,
                                     const Depth depth,
                                     const int move_count,
                                     const MoveData* move_data,
                                     const Score alpha,
                                     Score& best_score)
  {
    const auto m = move_data->move;

    if (pos->in_check) {
      if (see->seeLastMove(m) >= 0) {
        return depth;
      }
    }

    if (!isQueenPromotion(m)
        && !isCapture(m)
        && !isKillerMove(m, ply - 1)
        && move_count >= (pv ? 5 : 3))
    {
      Depth next_depth = depth - 2 - depth/8 - (move_count - 6)/10;

      if (next_depth <= 3) {
        auto score = -pos->eval_score + futility_margin[std::min(3, std::max(0, next_depth))];

        if (score < alpha) {
          best_score = std::max(best_score, score);
          return -999;
        }
      }
      return next_depth;
    }
    return depth - 1;
  }

  __forceinline Depth nextDepthPV(Move singular_move, const Depth depth, const MoveData* move_data) const
  {
    const auto m = move_data->move;

    if (m == singular_move) {
      return depth;
    }

    if (pos->in_check || isPassedPawnMove(m)) {
      if (see->seeLastMove(m) >= 0) {
        return depth;
      }
    }
    return depth - 1;
  }

  __forceinline int nullMoveReduction(const Depth depth) const
  {
    return 4 + depth/4;
  }

  Score searchQuiesce(Score alpha, const Score beta, int qs_ply, bool search_pv)
  {
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
          score = -drawScore();
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

  __forceinline bool makeMoveAndEvaluate(const Move m, int alpha, int beta)
  {
    if (game->makeMove(m, true, true)) {
      pos = game->pos;
      ++ply;
      pv_length[ply] = ply;
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

  __forceinline void unmakeMove()
  {
    game->unmakeMove();
    pos = game->pos;
    ply--;
  }

  __forceinline void checkTime()
  {
    if ((node_count & 0x1fff) == 0) {
      if (protocol) {
        if (!isAnalysing() && !protocol->isFixedDepth()) {
          stop_search = search_depth > 1 && millisUsed() > search_time;
        }
        else {
          protocol->checkInput();
        }
      }

      if (stop_search) {
        throw 1;
      }
    }
  }

  __forceinline int isAnalysing() const
  {
    return protocol ? protocol->isAnalysing() : true;
  }

  __forceinline void updatePV(const Move move, const Score score, const Depth depth, int node_type)
  {
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
        protocol->postPV(search_depth, max_ply, node_count*num_workers_, nodesPerSecond(),
                         std::max(1ull, start_time.millisElapsed()), transt->getLoad(), score, buf, node_type);
      }
    }
  }

  __forceinline void storePV()
  {
    assert(pv_length[0] > 0);
    for (auto i = 0; i < pv_length[0]; ++i) {
      const auto& entry = pv[0][i];
      transt->insert(entry.key, entry.depth, entry.score, entry.node_type, entry.move, entry.eval);
    }
  }

  uint64_t nodesPerSecond() const
  {
    uint64_t micros = start_time.microsElapsedHighRes();
    return micros == 0 ? node_count*num_workers_ : node_count*num_workers_*1000000/micros;
  }

  __forceinline void updateHistoryScores(const Move move, const Depth depth)
  {
    history_scores[movePiece(move)][moveTo(move)] += depth*depth;

    if (history_scores[movePiece(move)][moveTo(move)] > PROMOTIONMOVESCORE - 100) {
      for (auto i = 0; i < 16; ++i)
        for (auto k = 0; k < 64; ++k) {
          history_scores[i][k] >>= 2;
        }
    }
  }

  __forceinline void updateKillerMoves(const Move move)
  {
    // Same move can be stored twice for a ply.
    if (!isCapture(move) && !isPromotion(move)) {
      if (move != killer_moves[0][ply]) {
        killer_moves[2][ply] = killer_moves[1][ply];
        killer_moves[1][ply] = killer_moves[0][ply];
        killer_moves[0][ply] = move;
      }
    }
  }

  __forceinline bool isKillerMove(const Move m, int ply) const
  {
    return m == killer_moves[0][ply] || m == killer_moves[1][ply] || m == killer_moves[2][ply];
  }

  __forceinline Score codecTTableScore(Score score, Score ply) const
  {
    if (abs((int)score) < MAXSCORE - 128) {
      return score;
    }
    return score < 0 ? score - ply : score + ply;
  }

  void initialiseSearch(int wtime, int btime, int movestogo, int winc, int binc, int movetime)
  {
    pos = game->pos; // Updated in makeMove and unmakeMove from here on.

    if (protocol) {
      if (protocol->isFixedTime()) {
        search_time = 950*movetime/1000;
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
      transt->initialiseSearch();
      stop_search = false;
      start_time.start();
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

  virtual void sortMove(MoveData& move_data)
  {
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

  __forceinline Score searchNodeScore(const Score score) const
  {
    return score;
  }

  __forceinline Score storeSearchNodeScore(const Score score, const Depth depth, const int node_type, const Move move)
  {
    if (move) {
      updateHistoryScores(move, depth);
      updateKillerMoves(move);
    }
    storeTransposition(depth, score, node_type, move);
    return searchNodeScore(score);
  }

  __forceinline Score drawScore() const
  {
    return drawScore_[pos->side_to_move];
  }

  __forceinline void storeTransposition(const Depth depth, Score score, const int node_type, const Move move)
  {
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

  __forceinline bool getTransposition()
  {
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

  __forceinline bool isTranspositionScoreValid(const Depth depth, const Score alpha, const Score beta)
  {
      return  pos->transposition &&
              pos->transp_depth >= depth &&
              ((pos->transp_type & EXACT) ||
                ((pos->transp_type & BETA) && pos->transp_score >= beta) ||
                ((pos->transp_type & ALPHA) && pos->transp_score <= alpha));
  }

  __forceinline int nodeType(const Score score, const Score beta, const Move move) const {
    return move ? (score >= beta ? BETA : EXACT) : ALPHA;
  }

  bool moveIsEasy() const
  {
    if (protocol) {
      if ((pos->moveCount() == 1 && search_depth > 9)
          || (protocol->isFixedDepth() && protocol->getDepth() == search_depth)
          || (pv[0][0].score == MAXSCORE - 1))
      {
        return true;
      }

      if (!isAnalysing() && !protocol->isFixedDepth() && search_time < millisUsed()*2.5) {
        return true;
      }
    }
    return false;
  }

  __forceinline bool isPassedPawnMove(const Move m) const
  {
    return movePieceType(m) == Pawn && board->isPawnPassed(moveTo(m), moveSide(m));
  }

public:
  struct PVEntry
  {
    uint64_t key;
    Depth depth;
    Score score;
    Move move;
    int node_type;
    int eval;
  };

  Depth ply;
  Depth max_ply;
  PVEntry pv[128][128];
  int pv_length[128];
  Stopwatch start_time;
  uint64_t search_time;
  uint64_t time_left;
  uint64_t time_inc;
  int lag_buffer;
  volatile bool stop_search;
  int verbosity;
  Protocol* protocol;

protected:
  Depth search_depth;
  Move killer_moves[4][128];
  int history_scores[16][64];
  int drawScore_[2];
  Game* game;
  Eval* eval;
  Board* board;
  See* see;
  Position* pos;
  TTable* transt;
  Logger* logger;

  uint64_t node_count;
  uint64_t num_workers_;

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

const int Search::EXACT;
const int Search::ALPHA;
const int Search::BETA;
const int Search::MAXSCORE;

int Search::futility_margin[4] = { 150, 150, 150, 400 };
int Search::razor_margin[4] = { 0, 125, 125, 400 };
