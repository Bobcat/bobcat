#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <memory>
#include <iomanip>

namespace eval
{

struct Node
{
  Node(const char* fen) : fen_(fen) {}

  std::string fen_;
  double result_;
};

class PGNPlayer : public pgn::PGNPlayer
{
public:
  PGNPlayer() : pgn::PGNPlayer(), all_nodes_count_(0) {}

  virtual ~PGNPlayer() {}

  virtual void readPGNDatabase()
  {
    PGNFileReader::readPGNDatabase();
    printProgress(true);
  }

  virtual void readSANMove()
  {
    pgn::PGNPlayer::readSANMove();

    all_nodes_count_++;

    if (((game_->pos - game_->position_list) >= 14) && (all_nodes_count_ % 7 == 0)) {
      current_game_nodes_.push_back(Node(game_->getFen()));
    }
  }

  virtual void readGameTermination()
  {
    pgn::PGNPlayer::readGameTermination();

    for (auto& node : current_game_nodes_) {
      node.result_ = result_ == pgn::WhiteWin ? 1 : result_ == pgn::Draw ? 0.5 : 0;
    }
    all_selected_nodes_.insert(all_selected_nodes_.end(), current_game_nodes_.begin(), current_game_nodes_.end());
    current_game_nodes_.clear();

    printProgress(false);
  }

  virtual void readComment1()
  {
    pgn::PGNPlayer::readComment1();
    //cout << comment_ << endl;
  }

  void printProgress(bool force)
  {
    if (!force && game_count_ % 100 != 0) {
      return;
    }
    cout << "game_count_:" << game_count_
    << " position_count_:" << all_nodes_count_
    << " all_nodes_.size:" << all_selected_nodes_.size()
    << endl;
  }

  std::vector<Node> all_selected_nodes_;

private:
  std::vector<Node> current_game_nodes_;
  int64_t all_nodes_count_;
};
static bool x_;
struct Param
{
  Param(std::string name, int& value, int initial_value, int step) :
    name_(name), initial_value_(initial_value), value_(value), step_(step)
  {
    if (x_)value = initial_value;
  }

  std::string name_;
  int initial_value_;
  int& value_;
  int step_;
};

struct ParamIndexRecord
{
  size_t idx_;
  double improved_;
};

bool operator<(const ParamIndexRecord& lhs, const ParamIndexRecord& rhs)
{
  return lhs.improved_ >= rhs.improved_;
}

class Tune : public MoveSorter
{
public:
  Tune(Game& game, See& see, Eval& eval) :
    game_(game), see_(see), eval_(eval), score_static_(false)
  {
    PGNPlayer pgn;
    /*
    pgn.read("C:\\chess\\lb\\test20150827.pgn");
    pgn.read("C:\\chess\\lb\\test20151102.pgn");
    pgn.read("C:\\chess\\lb\\test20151118.pgn");
    pgn.read("C:\\chess\\lb\\test20151204.pgn");
    pgn.read("C:\\chess\\lb\\test20151208.pgn");
    pgn.read("C:\\chess\\lb\\test20151215.pgn");
    pgn.read("C:\\chess\\lb\\test20151230.pgn");
    pgn.read("C:\\chess\\lb\\test20160102.pgn");
    pgn.read("C:\\chess\\lb\\test20160105.pgn");
    pgn.read("C:\\chess\\lb\\test20160108.pgn");
    pgn.read("C:\\chess\\lb\\test20160109.pgn");
    pgn.read("C:\\chess\\lb\\test20160112.pgn");
    pgn.read("C:\\chess\\lb\\test20160118.pgn");
    pgn.read("C:\\chess\\lb\\test20160128.pgn");
    pgn.read("C:\\chess\\lb\\test20160129.pgn");
    pgn.read("C:\\chess\\lb\\test20160131.pgn");
    pgn.read("C:\\chess\\lb\\test20160203.pgn");
    pgn.read("C:\\chess\\lb\\test20160220.pgn");
    pgn.read("C:\\chess\\lb\\test20160221.pgn");
    pgn.read("C:\\chess\\lb\\test20160226.pgn");
    pgn.read("C:\\chess\\lb\\test20160227.pgn");
    pgn.read("C:\\chess\\lb\\test20160228.pgn");
    pgn.read("C:\\chess\\lb\\test20150827.pgn");
    pgn.read("C:\\chess\\lb\\test20151102.pgn");
    pgn.read("C:\\chess\\lb\\test20160302.pgn");
    pgn.read("C:\\chess\\lb\\test20160308.pgn");
    pgn.read("C:\\chess\\lb\\test20160314.pgn");
    pgn.read("C:\\chess\\lb\\test20160315.pgn");
    pgn.read("C:\\chess\\lb\\test20160316.pgn");
    pgn.read("C:\\chess\\lb\\test20160317.pgn");
    pgn.read("C:\\chess\\lb\\test20160318.pgn");
    pgn.read("C:\\chess\\lb\\test20160319.pgn");
    pgn.read("C:\\chess\\lb\\test20160320.pgn");
    pgn.read("C:\\chess\\lb\\test20160327.pgn");
    pgn.read("C:\\chess\\lb\\test20160328.pgn");
    pgn.read("C:\\chess\\lb\\test20160330.pgn");
    pgn.read("C:\\chess\\lb\\test20160331.pgn");
    pgn.read("C:\\chess\\lb\\test20160401.pgn");
    pgn.read("C:\\chess\\lb\\test20160516.pgn");
    pgn.read("C:\\chess\\lb\\test20160517.pgn");
    pgn.read("C:\\chess\\lb\\test20160518.pgn");
    pgn.read("C:\\chess\\lb\\test20160519.pgn");
    // to do add the missing
    pgn.read("C:\\chess\\lb\\test20160524.pgn");
    pgn.read("C:\\chess\\lb\\test20160525.pgn");
*/
    pgn.read("C:\\chess\\lb\\test20160526.pgn");
    pgn.read("C:\\chess\\lb\\test20160527.pgn");
    pgn.read("C:\\chess\\lb\\test20160525.pgn");
    pgn.read("C:\\chess\\lb\\test20160526.pgn");
    pgn.read("C:\\chess\\lb\\test20160527.pgn");

    // Tune as described in https://chessprogramming.wikispaces.com/Texel%27s+Tuning+Method

    eval_.tuning_ = true;
    score_static_ = true;

    std::vector<Param> params;

    initEval(params);

    if (score_static_) {
      makeQuiet(pgn.all_selected_nodes_);
    }
    std::vector<ParamIndexRecord> params_index;

    for (size_t i = 0; i < params.size(); ++i) {
      params_index.push_back(ParamIndexRecord{ i, 0 });
    }


    double K = bestK();
    double bestE = E(pgn.all_selected_nodes_, params, params_index, K);
    bool improved = true;

    ofstream out("C:\\chess\\test6.txt");
    out << "Old E:" << bestE << "\n";
    out << "Old Values:\n" << emitCode(params, true) << "\n";

    while (improved) {
      printBestValues(bestE, params);
      improved = false;

      for (size_t i = 0; i < params_index.size(); ++i) {
        size_t idx = params_index[i].idx_;
        auto& step = params[idx].step_;
        auto& value = params[idx].value_;

        if (step == 0) {
          continue;
        }
        value += step;

        std::cout << "Tuning prm[" << idx << "] " << params[idx].name_ << " i:" << i
        << " current:" << value - step << " trying:" << value << "..."
        << std::endl;

        double newE = E(pgn.all_selected_nodes_, params, params_index, K);

        if (newE < bestE) {
          params_index[i].improved_ = bestE - newE;
          bestE = newE;
          improved = true;
          out << "E:" << bestE << "\n";
          out << emitCode(params, true);
        }
        else if (step > 0) {
          step = -step;
          value += 2*step;

          std::cout << "Tuning prm[" << idx << "] " << params[idx].name_ << " i:" << i
          << " current:" << value - step << " trying:" << value << "..."
          << std::endl;

          newE = E(pgn.all_selected_nodes_, params, params_index, K);

          if (newE < bestE) {
            params_index[i].improved_ = bestE - newE;
            bestE = newE;
            improved = true;
            out << "E:" << bestE << "\n";
            out << emitCode(params, true);
          }
          else {
            params_index[i].improved_ = 0;
            value -= step;
            step = 0;
          }
        }
        else {
            params_index[i].improved_ = 0;
            value -= step;
            step = 0;
        }
      }

      if (improved) {
        //std::stable_sort(params_index.begin(), params_index.end());
      }
    }
    printBestValues(bestE, params);
    out << "New E:" << bestE << "\n";
    out << "\nNew:\n" << emitCode(params, false) << "\n";
    std::cout << emitCode(params, true) << "\n";
  }

  virtual ~Tune()
  {
    eval_.tuning_ = false;
  }

  void initEval(std::vector<Param>& params)
  {
    //auto step1 = 1;
    //auto step2 = 1;
    auto step5 = 1;
    x_ = 0;

    for (auto i = 0; i < 9; ++i) {
      params.push_back(Param("knight_mob_mg", eval_.knight_mob_mg[i], 0, step5));
      params.push_back(Param("knight_mob_eg", eval_.knight_mob_eg[i], 0, step5));
    }

/*
    for (auto i = 0; i < 9; ++i) {
      params.push_back(Param("knight_mob_mg", eval_.knight_mob_mg[i], 0, step5));
      params.push_back(Param("knight_mob_eg", eval_.knight_mob_eg[i], 0, step5));
    }

    for (auto i = 0; i < 14; ++i) {
      params.push_back(Param("bishop_mob_mg", eval_.bishop_mob_mg[i], 0, step5));
      params.push_back(Param("bishop_mob_eg", eval_.bishop_mob_eg[i], 0, step5));
    }

    for (auto i = 0; i < 15; ++i) {
      params.push_back(Param("rook_mob_mg", eval_.rook_mob_mg[i], 0, step5));
      params.push_back(Param("rook_mob_eg", eval_.rook_mob_eg[i], 0, step5));
    }

    for (auto i = 0; i < 28; ++i) {
      params.push_back(Param("queen_mob_mg", eval_.queen_mob_mg[i], 0, step5));
      params.push_back(Param("queen_mob_eg", eval_.queen_mob_eg[i], 0, step5));
    }

    for (auto i = 0; i < 9; ++i) {
      params.push_back(Param("knight_mob2_mg", eval_.knight_mob2_mg[i], 0, step5));
      params.push_back(Param("knight_mob2_eg", eval_.knight_mob2_eg[i], 0, step5));
    }

    for (auto i = 0; i < 14; ++i) {
      params.push_back(Param("bishop_mob2_mg", eval_.bishop_mob2_mg[i], 0, step5));
      params.push_back(Param("bishop_mob2_eg", eval_.bishop_mob2_eg[i], 0, step5));
    }

    for (auto i = 0; i < 64; ++i) {
      params.push_back(Param("king_pcsq_mg", eval_.king_pcsq_mg[i], 0, step5));
      params.push_back(Param("king_pcsq_eg", eval_.king_pcsq_eg[i], 0, step5));
    }

    for (auto i = 0; i < 64; ++i) {
      auto step = i > 7 && i < 56 ? step5 : 0;
      params.push_back(Param("pawn_pcsq_mg", eval_.pawn_pcsq_mg[i], 0, step));
      params.push_back(Param("pawn_pcsq_eg", eval_.pawn_pcsq_eg[i], 0, step));
    }

    for (auto i = 0; i < 64; ++i) {
      params.push_back(Param("knight_pcsq_mg", eval_.knight_pcsq_mg[i], 0, step5));
      params.push_back(Param("knight_pcsq_eg", eval_.knight_pcsq_eg[i], 0, step5));
    }

    for (auto i = 0; i < 64; ++i) {
      params.push_back(Param("bishop_pcsq_mg", eval_.bishop_pcsq_mg[i], 0, step5));
      params.push_back(Param("bishop_pcsq_eg", eval_.bishop_pcsq_eg[i], 0, step5));
    }

    for (auto i = 0; i < 64; ++i) {
      params.push_back(Param("rook_pcsq_mg", eval_.rook_pcsq_mg[i], 0, step5));
      params.push_back(Param("rook_pcsq_eg", eval_.rook_pcsq_eg[i], 0, step5));
    }

    for (auto i = 0; i < 64; ++i) {
      params.push_back(Param("queen_pcsq_mg", eval_.queen_pcsq_mg[i], 0, step5));
      params.push_back(Param("queen_pcsq_eg", eval_.queen_pcsq_eg[i], 0, step5));
    }

    params.push_back(Param("knight_in_danger", eval_.knight_in_danger, 0, step5));
    params.push_back(Param("bishop_in_danger", eval_.bishop_in_danger, 0, step5));
    params.push_back(Param("rook_in_danger", eval_.rook_in_danger, 0, step5));
    params.push_back(Param("queen_in_danger", eval_.queen_in_danger, 0, step5));
    params.push_back(Param("knight_in_danger2", eval_.knight_in_danger2, 0, step5));
    params.push_back(Param("bishop_in_danger2", eval_.bishop_in_danger2, 0, step5));
    params.push_back(Param("rook_in_danger2", eval_.rook_in_danger2, 0, step5));
    params.push_back(Param("queen_in_danger2", eval_.queen_in_danger2, 0, step5));

    for (auto i = 0; i < 2; ++i) params.push_back(Param("pawn_isolated_mg", eval_.pawn_isolated_mg[i], 0, step5));
    for (auto i = 0; i < 2; ++i) params.push_back(Param("pawn_isolated_eg", eval_.pawn_isolated_eg[i], 0, step5));
    for (auto i = 0; i < 2; ++i) params.push_back(Param("pawn_behind_mg", eval_.pawn_behind_mg[i], 0, step5));
    for (auto i = 0; i < 2; ++i) params.push_back(Param("pawn_behind_eg", eval_.pawn_behind_eg[i], 0, step5));
    for (auto i = 0; i < 2; ++i) params.push_back(Param("pawn_doubled_mg", eval_.pawn_doubled_mg[i], 0, step5));
    for (auto i = 0; i < 2; ++i) params.push_back(Param("pawn_doubled_eg", eval_.pawn_doubled_eg[i], 0, step5));

    params.push_back(Param("bishop_pair_mg", eval_.bishop_pair_mg, 0, step5));
    params.push_back(Param("bishop_pair_eg", eval_.bishop_pair_eg, 0, step5));

    params.push_back(Param("king_obstructs_rook", eval_.king_obstructs_rook, 0, step5));
    params.push_back(Param("rook_open_file", eval_.rook_open_file, 0, step5));

    for (auto i = 0; i < 8; ++i) params.push_back(Param("passed_pawn_mg", eval_.passed_pawn_mg[i], 0, step5));
    for (auto i = 0; i < 8; ++i) params.push_back(Param("passed_pawn_eg", eval_.passed_pawn_eg[i], 0, step5));
    for (auto i = 0; i < 8; ++i) params.push_back(Param("passed_pawn_no_us", eval_.passed_pawn_no_us[i], 0, step5));
    for (auto i = 0; i < 8; ++i) params.push_back(Param("passed_pawn_no_attacks", eval_.passed_pawn_no_attacks[i], 0, step5));
    for (auto i = 0; i < 8; ++i) params.push_back(Param("passed_pawn_no_them", eval_.passed_pawn_no_them[i], 0, step5));
    for (auto i = 0; i < 8; ++i) params.push_back(Param("passed_pawn_king_dist_us", eval_.passed_pawn_king_dist_us[i], 0, step5));
    for (auto i = 0; i < 8; ++i) params.push_back(Param("passed_pawn_king_dist_them", eval_.passed_pawn_king_dist_them[i], 0, step5));

    for (auto i = 0; i < 4; ++i) params.push_back(Param("king_pawn_shelter", eval_.king_pawn_shelter[i], 0, step5));
    for (auto i = 0; i < 4; ++i) params.push_back(Param("king_on_open", eval_.king_on_open[i], 0, step5));
    for (auto i = 0; i < 4; ++i) params.push_back(Param("king_on_half_open", eval_.king_on_half_open[i], 0, step5));

    params.push_back(Param("knight_attack_king", eval_.knight_attack_king, 0, step5));
    params.push_back(Param("bishop_attack_king", eval_.bishop_attack_king, 0, step5));
    params.push_back(Param("rook_attack_king", eval_.rook_attack_king, 0, step5));
    params.push_back(Param("queen_attack_king", eval_.queen_attack_king, 0, step5));
*/
  }

  double E(const std::vector<Node>& nodes,
           const std::vector<Param>& params,
           const std::vector<ParamIndexRecord>& params_index,
           double K)
  {
    double x = 0;

    for (auto node : nodes) {
      game_.setFen(node.fen_.c_str());
      x += pow(node.result_ - sigmoid(getScore(0), K), 2);
    }
    x /= nodes.size();

    cout << "x:" << std::setprecision(12) << x;

    for (size_t i = 0; i < params_index.size(); ++i) {
      if (params[params_index[i].idx_].step_) {
        cout << " prm[" << i << "]:" << params[params_index[i].idx_].value_;
      }
    }
    cout << endl;
    return x;
  }

  void makeQuiet(std::vector<Node>& nodes)
  {
    for (auto& node : nodes) {
      game_.setFen(node.fen_.c_str());
      pv_length[0] = 0;
      getQuiesceScore(-32768, 32768, true, 0);
      playPV();
      node.fen_ = game_.getFen();
    }
  }

  Score getScore(int side)
  {
    auto score = 0;

    if (score_static_) {
      score = eval_.evaluate(-100000, 100000);
    }
    else {
      score = getQuiesceScore(-32768, 32768, false, 0);
    }
    return game_.pos->side_to_move == side ? score : -score;
  }

  Score getQuiesceScore(Score alpha, const Score beta, bool storePV, int ply)
  {
    auto score = eval_.evaluate(-100000, 100000);

    if (score >= beta) {
      return score;
    }
    auto best_score = score;

    if (best_score > alpha) {
      alpha = best_score;
    }
    game_.pos->generateCapturesAndPromotions(this);

    while (const auto move_data = game_.pos->nextMove()) {
      if (!isPromotion(move_data->move)) {
        if (move_data->score < 0) {
          break;
        }
      }

      if (makeMove(move_data->move, ply)) {
        auto score = -getQuiesceScore(-beta, -alpha, storePV, ply + 1);

        game_.unmakeMove();

        if (score > best_score) {
          best_score = score;

          if (best_score > alpha) {
            if (score >= beta) {
              break;
            }

            if (storePV) {
              updatePV(move_data->move, best_score, ply);
            }
            alpha = best_score;
          }
        }
      }
    }
    return best_score;
  }

  bool makeMove(const Move m, int ply)
  {
    if (game_.makeMove(m, true, true)) {
      ++ply;
      pv_length[ply] = ply;
      return true;
    }
    return false;
  }

  void unmakeMove()
  {
    game_.unmakeMove();
  }

  void playPV()
  {
    for (auto i = 0; i < pv_length[0]; ++i) {
      game_.makeMove(pv[0][i].move, false, true);
    }
  }

  __forceinline void updatePV(const Move move, const Score score, int ply)
  {
    assert(ply<128);
    assert(pv_length[ply]<128);
    Search::PVEntry* entry = &pv[ply][ply];

    entry->score = score;
    entry->move = move;
    //entry->eval = game_.pos->eval_score;

    pv_length[ply] = pv_length[ply + 1];

    for (auto i = ply + 1; i < pv_length[ply]; ++i) {
      pv[ply][i] = pv[ply + 1][i];
    }
  }

  virtual void sortMove(MoveData& move_data)
  {
    const auto m = move_data.move;

    if (isQueenPromotion(m)) {
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
      else if (see_.seeMove(m) >= 0) {
        move_data.score = 160000 + value_captured*20 - value_piece;
      }
      else {
        move_data.score = -100000 + value_captured*20 - value_piece;
      }
    }
    else {
      exit(0);
    }
  }

  std::string emitCode(const std::vector<Param> params0, bool hr)
  {
    std::map<std::string, std::vector<Param> > params1;

    for (auto& param : params0) {
      params1[param.name_].push_back(param);
    }
    std::stringstream s;

    for (auto& params2 : params1) {
      auto n = params2.second.size();

      s << "int Eval::" << params2.first;

      if (n > 1) {
        s << "[" << n <<"] = { ";
      }
      else {
          s << " = ";
      }

      for (size_t i = 0; i < n; ++i) {
        if (hr && n == 64) {
            if (i % 8 == 0) {
              s << "\n ";
            }
            s << setw(4) << params2.second[i].value_;
        }
        else {
        s << params2.second[i].value_;
        }
        if (n > 1 && i < n - 1) {
          s << ", ";
        }
      }

      if (n > 1) {
        s << " }";
      }
      s << ";\n";
    }
    return s.str();
  }

  void printBestValues(double E, const std::vector<Param> params)
  {
    auto finished = 0;
    for (size_t i = 0; i < params.size(); ++i) {
      if (params[i].step_ == 0) {
          finished++;
      }
      cout << i << ":"
      << params[i].name_ << ":" << params[i].value_
      << "  step:" << params[i].step_
      << endl;
    }
    cout << "Best E:" << std::setprecision(12) << E << "  " << endl;
    cout << "Finished:" << finished*100.0/params.size() << "%" << endl;
  }

  double bestK()
  {
    /*
    double smallestE;
    double bestK = -1;
    for (double K = 1.10; K < 1.15; K += 0.01) {
        double _E = E(pgn.all_nodes_, params, K);
        if (bestK < 0 || _E < smallestE) {
          bestK = K;
          smallestE = _E;
        }
        std::cout << "K:" << K << "  _E:" << _E << endl;
    }
    std::cout << "bestK:" << bestK << "smallestE:" << smallestE << endl;
    */
    return 1.12;
  }

private:
  Game& game_;
  See& see_;
  Eval& eval_;

  Search::PVEntry pv[128][128];
  int pv_length[128];
  bool score_static_;
};

}//namespace eval
