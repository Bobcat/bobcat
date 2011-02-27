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

#pragma pack(1)
struct Transposition {
	uint32 key; 
	uint16 age; // 7 bits left
	uint8 depth;
	uint8 flags; // 5 bits left
	int16 score;
	Move move; 
	uint16 unused;
};
#pragma pack()

class TranspositionTable {
public:
	TranspositionTable(int size_mb) : table(NULL), size_mb(-1) {
		if (sizeof(Transposition) != 16) {
			cout << "error sizeof(Transposition) == " << sizeof(Transposition) << endl;
			exit(0);
		}
		initialise(size_mb);
	}

	void initialise(int new_size_mb) {
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

	__forceinline Transposition* find(const uint64 key) {
		Transposition* transp = table + (key & mask);
		for (int i = 0; i < NUMBER_SLOTS; i++, transp++) {
			if (transp->key == key32(key) && transp->flags) {
				return transp;
			}
		}
		return 0;
	}

	__forceinline Transposition* insert(const uint64 key, const int depth, const int score, const int type, const int move) {
		Transposition* transp = getEntryToReplace(key, depth);
		if (transp->flags == 0) {
			occupied++;
		}
		transp->move = transp->key != key32(key) || move != 0 ? move : transp->move;
		transp->key = key32(key);
		transp->score = (int16)score;
		transp->depth = (uint8)depth;
		transp->flags = (uint8)type;
		transp->age = (uint16)age;
		return transp;
	}

	__forceinline Transposition* getEntryToReplace(uint64 key, int depth) {
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
		return size_mb;
	}

	__forceinline static uint32 key32(const uint64 key) {
		return key >> 32;
	}

protected:
	Transposition* table;
	int mask;
	int occupied;
	int size_mb;
	int size;
	int age;

	static const int NUMBER_SLOTS = 4;
};

#pragma pack(1)
struct PawnEntry {
	uint64 zkey;
	int16 eval_mg;
	int16 eval_eg;
	uint8 passed_pawn_files[2];
	int16 unused;
};
#pragma pack()

class PawnStructureTable {
public:
	PawnStructureTable(int size_mb) : table(NULL) {
		if (sizeof(PawnEntry) != 16) {
			cout << "error sizeof(PawnEntry) == " << sizeof(PawnEntry) << endl;
			exit(0);
		}
		initialise(size_mb);
	}

	void initialise(int size_mb) {
		size = 1024*1024*pow2(log2(size_mb))/sizeof(PawnEntry);
		mask = size - 1;
		delete [] table;
		table = new PawnEntry[size];
		clear();
	}

	__forceinline void clear() {
		memset(table, 0, size*sizeof(PawnEntry));
	}

	__forceinline PawnEntry* find(const uint64 key) {
		PawnEntry* pawnp = table + (key & mask);
		if (pawnp->zkey != key || pawnp->zkey == 0) {
			return 0;
		}
		return pawnp;
	}

	__forceinline PawnEntry* insert(const uint64 key, int score_mg, int score_eg, int* passed_pawn_files) {
		PawnEntry* pawnp = table + (key & mask);
		pawnp->zkey = key;
		pawnp->eval_mg = (int16)score_mg;
		pawnp->eval_eg = (int16)score_eg;
		pawnp->passed_pawn_files[0] = (uint8)passed_pawn_files[0];
		pawnp->passed_pawn_files[1] = (uint8)passed_pawn_files[1];
		return pawnp;
	}

protected:
	PawnEntry* table;
	int size;
	int mask;
};

typedef TranspositionTable TTable;
typedef PawnStructureTable PSTable;
