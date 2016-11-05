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

#pragma pack(1)
struct Transposition {
  uint32_t key;
  uint16_t age; // 7 bits left
  uint8_t depth;
  uint8_t flags; // 5 bits left
  int16_t score;
  Move move;
  int16_t eval;
};
#pragma pack()

class TranspositionTable {
public:
  TranspositionTable(uint64_t size_mb) : table(NULL), size_mb(0) {
    if (sizeof(Transposition) != 16) {
      printf("error sizeof(Transposition) == %d\n", static_cast<int>(sizeof(Transposition)));
      exit(0);
    }
    initialise(size_mb);
  }

  void initialise(uint64_t new_size_mb) {
    new_size_mb = pow2(log2(new_size_mb));
    if (new_size_mb == size_mb) {
      return;
    }
    size_mb = new_size_mb;
    size = 1024*1024*size_mb/sizeof(Transposition);
    mask = size - 1;
    size += NUMBER_SLOTS - 1;
    delete [] table;
    table = new Transposition[size];
    clear();
  }

  __forceinline void clear() {
    memset(table, 0, size*sizeof(Transposition));
    occupied = 0;
    age = 0;
  }

  void initialiseSearch() {
    age++;
  }

  __forceinline Transposition* find(const uint64_t key) {
    Transposition* transp = table + (key & mask);
    for (int i = 0; i < NUMBER_SLOTS; i++, transp++) {
      if (transp->key == key32(key) && transp->flags) {
        return transp;
      }
    }
    return 0;
  }

  __forceinline Transposition* insert(const uint64_t key, const int depth, const int score,
                                      const int type, const int move, int eval)
  {
    Transposition* transp = getEntryToReplace(key, depth);
    if (transp->flags == 0) {
      occupied++;
    }
    transp->move = transp->key != key32(key) || move != 0 ? move : transp->move;
    transp->key = key32(key);
    transp->score = (int16_t)score;
    transp->depth = (uint8_t)depth;
    transp->flags = (uint8_t)type;
    transp->age = (uint16_t)age;
    transp->eval = (int16_t)eval;
    return transp;
  }

  __forceinline Transposition* getEntryToReplace(uint64_t key, int depth) {
    Transposition* transp = table + (key & mask);
    if (transp->flags == 0 || transp->key == key32(key)) {
      return transp;
    }
    Transposition* replace = transp++;
    int replace_score = (replace->age << 9) + replace->depth;
    for (int i = 1; i < NUMBER_SLOTS; i++, transp++) {
      if (transp->flags == 0 || transp->key == key32(key)) {
        return transp;
      }
      int score = (transp->age << 9) + transp->depth;
      if (score < replace_score) {
        replace_score = score;
        replace = transp;
      }
    }
    return replace;
  }

  __forceinline int getLoad() {
    return (int)((double)occupied/size*1000);
  }

  int getSizeMb() {
    return (int)size_mb;
  }

  __forceinline static uint32_t key32(const uint64_t key) {
    return key >> 32;
  }

protected:
  Transposition* table;
  uint64_t mask;
  uint64_t occupied;
  uint64_t size_mb;
  uint64_t size;
  int age;

  static const int NUMBER_SLOTS = 4;
};

#pragma pack(1)
struct PawnEntry {
  uint64_t zkey;
  int16_t eval_mg;
  int16_t eval_eg;
  uint8_t passed_pawn_files[2];
  int16_t unused;
};
#pragma pack()

class PawnStructureTable {
public:
  PawnStructureTable(uint64_t  size_mb) : table(NULL) {
    if (sizeof(PawnEntry) != 16) {
      printf("error sizeof(PawnEntry) == %d\n", static_cast<int>(sizeof(PawnEntry)));
      exit(0);
    }
    initialise(size_mb);
  }

  void initialise(uint64_t size_mb) {
    size = 1024*1024*pow2(log2(size_mb))/sizeof(PawnEntry);
    mask = size - 1;
    delete [] table;
    table = new PawnEntry[size];
    clear();
  }

  __forceinline void clear() {
    memset(table, 0, size*sizeof(PawnEntry));
  }

  __forceinline PawnEntry* find(const uint64_t key) {
    PawnEntry* pawnp = table + (key & mask);
    if (pawnp->zkey != key || pawnp->zkey == 0) {
      return 0;
    }
    return pawnp;
  }

  __forceinline PawnEntry* insert(const uint64_t key, int score_mg, int score_eg, int* passed_pawn_files) {
    PawnEntry* pawnp = table + (key & mask);
    pawnp->zkey = key;
    pawnp->eval_mg = (int16_t)score_mg;
    pawnp->eval_eg = (int16_t)score_eg;
    pawnp->passed_pawn_files[0] = (uint8_t)passed_pawn_files[0];
    pawnp->passed_pawn_files[1] = (uint8_t)passed_pawn_files[1];
    return pawnp;
  }

protected:
  PawnEntry* table;
  uint64_t size;
  uint64_t mask;
};

typedef TranspositionTable TTable;
typedef PawnStructureTable PSTable;
