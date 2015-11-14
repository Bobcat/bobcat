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
#include <thread>

class Worker {
public:
  void start(Game* master, TTable* transt, PSTable* pawnt) {
    game_ = new Game();
    game_->copy(master);
    see_ = new See(game_);
    eval_ = new Eval(*game_, pawnt);
    search_ = new Search(game_, eval_, see_, transt);
    thread_ = new std::thread(&Search::run, search_);
  }

  void stop() {
    search_->stop();
    thread_->join();
    delete thread_;
    delete search_;
    delete eval_;
    delete see_;
    delete game_;
  }

private:
  Game* game_;
  Eval* eval_;
  See* see_;
  Search* search_;
  std::thread* thread_;
};
