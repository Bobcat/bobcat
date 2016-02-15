#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <memory>
#include <iomanip>

namespace et {

struct Node {
  std::string fen_;
  double result_;
};

class PGNPlayer : public pgn::PGNPlayer
{
public:
  PGNPlayer() : pgn::PGNPlayer(), position_count_(0) {}

  virtual ~PGNPlayer() {}

  virtual void readPGNDatabase() {
    PGNFileReader::readPGNDatabase();
    printProgress(true);
  }

  virtual void readSANMove() {
    pgn::PGNPlayer::readSANMove();

    auto half_move_count = game_->pos - game_->position_list;

//    if ((game_->board.queens(0)|game_->board.queens(1))&&(half_move_count >=14) && (half_move_count % 7 == 0)) {
    if ((half_move_count >=14) && (half_move_count % 7 == 0)) {
      node_.fen_ = game_->getFen();
      game_nodes_.push_back(node_);
    }
    ++position_count_;
  }

  virtual void readGameTermination() {
    pgn::PGNPlayer::readGameTermination();

    for (auto& node : game_nodes_) {
      node.result_ = result_ == pgn::WhiteWin ? 1 : result_ == pgn::Draw ? 0.5 : 0;
    }
    all_nodes_.insert(all_nodes_.end(), game_nodes_.begin(), game_nodes_.end());
    game_nodes_.clear();

    printProgress(false);
  }

  virtual void readComment1() {
    pgn::PGNPlayer::readComment1();
    //cout << comment_ << endl;
  }

  void printProgress(bool force) {
    if (!force && game_count_ % 100 != 0) {
      return;
    }
    cout << "game_count_:" << game_count_
    << " position_count_:" << position_count_
    << " all_nodes_.size:" << all_nodes_.size()
    << endl;
  }

  std::vector<Node> all_nodes_;

private:
  std::vector<Node> game_nodes_;
  Node node_;
  int64_t position_count_;
};

struct Param {
  Param(std::string name, int& value, int initial_value, int step) :
    name_(name), initial_value_(initial_value), value_(value), step_(step)
  {
    //initial_value_ = value;
    //value = initial_value;
  }
  Param(std::string name, int& value, int step) :
    name_(name), initial_value_(value), value_(value), step_(step)
  {
  }
  std::string name_;
  int initial_value_;
  int& value_;
  int step_;
};

class Tune : public MoveSorter
{
public:
  Tune(Game& game, See& see, Eval& eval) : game_(game), see_(see), eval_(eval) {
    PGNPlayer pgn;

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

    //pgn.read("C:\\Program Files (x86)\\Arena\\Tournaments\\Arena tournament test11.pgn");
    // Tune as described in https://chessprogramming.wikispaces.com/Texel%27s+Tuning+Method

    std::vector<Param> params;

    makeQuiet(pgn.all_nodes_);

    initEval(params);

    double K = bestK();
    double bestE = E(pgn.all_nodes_, params, K);
    bool improved = true;

    ofstream out("C:\\chess\\test6.txt");
    out << "Old E:" << bestE << "\n";
    out << "Old Values:\n" << emitCode(params, true) << "\n";

    while (improved) {
      printBestValues(bestE, params);
      improved = false;

      for (size_t i = 0; i < params.size(); ++i) {
        auto& step = params[i].step_;
        auto& value = params[i].value_;

        if (step == 0) {
          continue;
        }
        value += step;

        std::cout << "Tuning prm[" << i << "] " << params[i].name_
        << " current:" << value - step << " trying:" << value << "..."
        << std::endl;

        double newE = E(pgn.all_nodes_, params, K);

        if (newE < bestE) {
          bestE = newE;
          improved = true;
          out << "E:" << bestE << "\n";
          out << emitCode(params, true);
        }
        else if (step > 0) {
          step = -step;
          value += 2*step;

          std::cout << "Tuning prm[" << i << "] " << params[i].name_
          << " current:" << value - step << " trying:" << value << "..."
          << std::endl;

          newE = E(pgn.all_nodes_, params, K);

          if (newE < bestE) {
            bestE = newE;
            improved = true;
            out << "E:" << bestE << "\n";
            out << emitCode(params, true);
          }
          else {
            value -= step;
            step = 0;
          }
        }
        else {
            value -= step;
            step = 0;
        }
      }
      //makeQuiet(pgn.all_nodes_);
    }
    printBestValues(bestE, params);
    out << "New E:" << bestE << "\n";
    out << "\nNew:\n" << emitCode(params, false) << "\n";
    std::cout << emitCode(params, true) << "\n";
  }

  virtual ~Tune() {}

  double E(const std::vector<Node>& nodes, const std::vector<Param>& param_values, double K) {
    double x = 0;

    for (auto node : nodes) {
      game_.setFen(node.fen_.c_str());
      x += pow(node.result_ - sigmoid(getScore(0), K), 2);
    }
    x /= nodes.size();

    cout << "x:" << std::setprecision(12) << x;

    for (size_t i = 0; i < param_values.size(); ++i) {
      if (param_values[i].step_) {
        cout << " prm[" << i << "]:" << param_values[i].value_;
      }
    }
    cout << endl;
    return x;
  }

  void makeQuiet(std::vector<Node>& nodes) {
    for (auto& node : nodes) {
      game_.setFen(node.fen_.c_str());
      pv_length[0] = 0;
      getQuiesceScore(-32768, 32768, true, 0);
      playPV();
      node.fen_ = game_.getFen();
    }
  }

  Score getScore(int side) {
  //  Score score = getQuiesceScore(-32768, 32768, false, 0);
    auto score = eval_.evaluate(-100000, 100000);
    return game_.pos->side_to_move == side ? score : -score;
  }

  Score getQuiesceScore(Score alpha, const Score beta, bool storePV, int ply) {
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

  bool makeMove(const Move m, int ply) {
    if (game_.makeMove(m, true, true)) {
      ++ply;
      pv_length[ply] = ply;
      return true;
    }
    return false;
  }

  void unmakeMove() {
    game_.unmakeMove();
  }

  void playPV() {
    for (auto i = 0; i < pv_length[0]; ++i) {
      game_.makeMove(pv[0][i].move, false, true);
    }
  }

  __forceinline void updatePV(const Move move, const Score score, int ply) {
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

  virtual void sortMove(MoveData& move_data) {
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

  std::string emitCode(const std::vector<Param> params0, bool hr) {
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

  void printBestValues(double E, const std::vector<Param> params) {
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

  double bestK() {
    /*double smallestE;
    double bestK = -1;
    for (double K = 1.10; K < 1.15; K += 0.01) {
        double _E = E(pgn.all_nodes_, params, K);
        if (bestK < 0 || _E < smallestE) {
          bestK = K;
          smallestE = _E;
        }
        std::cout << "K:" << K << "  _E:" << _E << endl;
    }
    std::cout << "bestK:" << bestK << "smallestE:" << smallestE << endl;*/
    return 1.12;
  }

  void initEval(std::vector<Param>& params) {
    //auto step0 = 0;
    auto step5 = 1;
    //auto step1 = 1;
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
    params.push_back(Param("knight_in_danger", eval_.knight_in_danger, 0, step5));
    params.push_back(Param("bishop_in_danger", eval_.bishop_in_danger, 0, step5));
    params.push_back(Param("rook_in_danger", eval_.rook_in_danger, 0, step5));
    params.push_back(Param("queen_in_danger", eval_.queen_in_danger, 0, step5));

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

  }

private:
  Game& game_;
  See& see_;
  Eval& eval_;

  Search::PVEntry pv[128][128];
  int pv_length[128];
};

}
