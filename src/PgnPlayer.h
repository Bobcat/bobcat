#include <iostream>
using namespace std;

namespace pgn {

class PGNPlayer : public PGNFileReader
{
public:
	PGNPlayer(const char* path, bool check_legal = true) : PGNFileReader(path),
		game_(nullptr)
	{
		game_ = new Game();
	}

	virtual void readPGNDatabase() {
		PGNFileReader::readPGNDatabase();
		cout << game_count_ << endl;
	}

	virtual void readPGNGame() {
		game_->newGame(Game::START_POSITION);
		PGNFileReader::readPGNGame();
		if (game_count_ % 100 == 0)
			cout << game_count_ << endl;
	}

	virtual void readTagSection() {
		PGNFileReader::readTagSection();

		if (strieq(tag_name, "FEN")) {
			game_->setFen(std::string(tag_value).substr(1, strlen(tag_value) - 2).c_str());
		}
	}

	virtual void readSANMove() {
		PGNFileReader::readSANMove();

		int piece = (side_to_move << 3);

		if (pawn_move_) {
			piece |= Pawn;
			game_->pos->generatePawnMoves(capture_, bbSquare(to_square));
		}
		else if (castle_move_) {
			piece |= King;
			game_->pos->generateMoves();
		}
		else if (piece_move_) {
				switch (from_piece) {
				case 'N':
					piece |= Knight;
					break;
				case 'B':
					piece |= Bishop;
					break;
				case 'R':
					piece |= Rook;
					break;
				case 'Q':
					piece |= Queen;
					break;
				case 'K':
					piece |= King;
					break;
				default:
					cout<<"default ["<<token_str << "]"<<endl;
					exit(0);
			}
			game_->pos->generateMoves(piece, bbSquare(to_square));
		}
		else {
			cout<<"else"<<endl;
			exit(0);
		}
		Piece promoted = (side_to_move << 3);

		if (promoted_to != -1) {
			switch (promoted_to) {
			case 'N':
				promoted |= Knight;
				break;
			case 'B':
				promoted |= Bishop;
				break;
			case 'R':
				promoted |= Rook;
				break;
			case 'Q':
				promoted |= Queen;
				break;
			default:
				cout<<"promoted_to error ["<<token_str << "]"<<endl;
				exit(0);
			}
		}
		bool found = false;
		int move_count = game_->pos->moveCount();

		for (int i = 0; i < move_count; ++i) {
			Move m = game_->pos->move_list[i].move;

			if ((movePiece(m) != piece)
				|| ((int)moveTo(m) != to_square)
				|| (promoted_to != -1 && movePromoted(m) != promoted)
				|| (capture_ && !isCapture(m))
				|| (from_file != -1 && ::file(moveFrom(m)) != from_file)
				|| (from_rank != -1 && ::rank(moveFrom(m)) != from_rank))
			{
				continue;
			}

			if (!game_->makeMove(m, true, true)) {
					continue;
			}
			found = true;
			break;
		}

		if (!found) {
			cout<<"!found ["<<token_str << "]"<<endl;
			cout << "to_square:"<<to_square<<endl;
			cout<<"piece:"<<piece<<endl;
			cout<<"from_file:"<<from_file<<endl;
			cout<<"from_rank:"<<from_rank<<endl;
			cout<<"pawn_move_:"<<pawn_move_<<endl;
			cout<<"castle_move_:"<<castle_move_<<endl;
			cout<<"side_to_move:"<<side_to_move<<endl;
			cout<<"pos->in_check:"<<game_->pos->in_check<<endl;
			cout<<"game_count_:"<<game_count_<<endl;
			game_->board.print();
			exit(0);
		}
	}

protected:
	Game* game_;

private:
	Move move_;
};

} // namespace pgn
