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
#include <thread>

class Worker {
public:
	void start(Game* master, TTable* transt, PSTable* pawnt) {
		game = new Game();
		game->copy(master);
		see = new See(game);
		eval = new Eval(game, pawnt, see);
		search = new Search(game, eval, see, transt);
		thread_ = new std::thread(&Search::run, search);
	}

	void stop() {
		search->stop();
		thread_->join();
		delete thread_;
		delete search;
		delete eval;
		delete see;
		delete game;
	}

private:
	Game* game;
	Eval* eval;
	See* see;
	Search* search;
	std::thread* thread_;
};
