#include <vector>
#include <memory>

namespace et {

struct Node {
	std::string fen_;
	double result_;
};

class PGNPlayer : public pgn::PGNPlayer
{
public:
	PGNPlayer(const char* path) : pgn::PGNPlayer(path) {}

	virtual ~PGNPlayer() {}

	virtual void readSANMove() {
		pgn::PGNPlayer::readSANMove();
		if ((game_->pos - game_->position_list) > 10 && (game_->pos - game_->position_list) < 40) {
			node_.fen_ = game_->getFen();
			game_nodes_.push_back(node_);
		}
	}

	virtual void readGameTermination() {
		pgn::PGNPlayer::readGameTermination();

		for (auto& node : game_nodes_) {
			node.result_ = result_ == pgn::WhiteWin ? 1 : result_ == pgn::Draw ? 0.5 : 0;
		}
		all_nodes_.insert(all_nodes_.end(), game_nodes_.begin(), game_nodes_.end());
		game_nodes_.clear();
	}

	std::vector<Node> all_nodes_;

private:
	std::vector<Node> game_nodes_;
	Node node_;
};

struct Param {
	Param(std::string name, int* value, int initial_value, int step) :
		name_(name), value_(value), step_(step)
	{
			*value = initial_value;
	}
	Param(std::string name, int* value, int step) :
		name_(name), value_(value), step_(step)
	{
	}
	std::string name_;
	int* value_;
	int step_;
};

class Tune : public MoveSorter {
public:
	Tune(Game& game, See& see, Eval& eval) : game_(game), see_(see), eval_(eval) {
		PGNPlayer pgn("C:\\chess\\lb\\test.pgn");

		pgn.read();

		// Tune as described in from https://chessprogramming.wikispaces.com/Texel%27s+Tuning+Method

		std::vector<Param> initial_param_values;

		initial_param_values.push_back(Param("knightMobMg_", &eval_.knightMobMg_, 1));
		initial_param_values.push_back(Param("knightMobEg_", &eval_.knightMobEg_, 1));
		initial_param_values.push_back(Param("bishopMobMg_", &eval_.bishopMobMg_, 1));
		initial_param_values.push_back(Param("bishopMobEg_", &eval_.bishopMobEg_, 1));

		double K = bestK();
		double bestE = E(pgn.all_nodes_, initial_param_values, K);
		std::vector<Param> best_param_values = initial_param_values;
		bool improved = true;

		while (improved) {
			printBestValues(bestE, best_param_values);
			improved = false;

			for (size_t i = 0; i < initial_param_values.size(); ++i) {
				std::vector<Param> new_param_values = best_param_values;

				*new_param_values[i].value_ += new_param_values[i].step_;

				double newE = E(pgn.all_nodes_, new_param_values, K);

				if (newE < bestE) {
					bestE = newE;
					best_param_values = new_param_values;
					improved = true;
				}
				else {
					*new_param_values[i].value_ -= 2*new_param_values[i].step_;
					newE = E(pgn.all_nodes_, new_param_values, K);

					if (newE < bestE) {
						bestE = newE;
						best_param_values = new_param_values;
						improved = true;
					}
				}
			}
		}
	}

	virtual ~Tune() {}

	double E(const std::vector<Node>& nodes, const std::vector<Param>& param_values, double K) {
		double x = 0;

		for (auto node : nodes) {
			game_.setFen(node.fen_.c_str());
			x += pow(node.result_ - sigmoid(getScore(0), K), 2);
		}
		x /= nodes.size();

		cout << "x:" << x;
		for (size_t i = 0; i < param_values.size(); ++i) {
			cout << " pv[" << i << "]:" << *param_values[i].value_;
		}
		cout << endl;
		return x;
	}

	Score getScore(int side) {
		Score score = getQuiesceScore(-32768, 32768);
		return game_.pos->side_to_move == side ? score : -score;
	}

	Score getQuiesceScore(Score alpha, const Score beta) {
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

			if (game_.makeMove(move_data->move, true, true)) {
				auto score = -getQuiesceScore(-beta, -alpha);

				game_.unmakeMove();

				if (score > best_score) {
					best_score = score;

					if (best_score > alpha) {
						if (score >= beta) {
							break;
						}
						alpha = best_score;
					}
				}
			}
		}
		return best_score;
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

	void printBestValues(double E, const std::vector<Param> param_values) {
			cout << "E:" << E << "  " << endl;
			for (size_t i = 0; i < param_values.size(); ++i) {
				cout << i << ":" << param_values[i].name_ << " = " << *param_values[i].value_ << endl;
			}
	}

	double bestK() {
		/*double smallestE;
		double bestK = -1;
		for (double K = 1.10; K < 1.15; K += 0.01) {
				double _E = E(pgn.all_nodes_, initial_param_values, K);
				if (bestK < 0 || _E < smallestE) {
					bestK = K;
					smallestE = _E;
				}
				std::cout << "K:" << K << "  _E:" << _E << endl;
		}
		std::cout << "bestK:" << bestK << "smallestE:" << smallestE << endl;*/
		return 1.12;
	}

private:
	Game& game_;
	See& see_;
	Eval& eval_;
};

}
