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

struct KeyValue {
	~KeyValue() {
		delete [] key;
		delete [] value;
	}
	char* key;
	char* value;
};

class ConfigSection {
public:
	ConfigSection(const char* section_name) : size(0) {
		key = new char[strlen(section_name) + 1];
		strcpy(key, section_name);
	}

	~ConfigSection() {
		for (int i = 0; i < size; i++) {
			delete pairs[i];
		}
		delete [] key;
	}

	void add(const char* key, const char* value) {
		if (size < 32) {
			pairs[size] = new KeyValue();
			pairs[size]->key = new char[strlen(key) + 1];
			strcpy(pairs[size]->key, key);
			pairs[size]->value = new char[strlen(value) + 1];
			strcpy(pairs[size]->value, value);
			size++;
		}
	}

	const char* getString(const char* key, const char* def = 0) const {
		for (int i = 0; i < size; i++) {
			if (stricmp(pairs[i]->key, key) == 0) {
				return pairs[i]->value;
			}
		}
		return def;
	}
	char* key;
	KeyValue* pairs[32];
	int size;
};

class Config {
public:
	Config(const char* filename) :
		line_number(0),
		empty_section(new ConfigSection("empty")),
		size(0),
		status(0)
	{
		if (filename == NULL) {
			status = 1;
			return;
		}

		FILE* fp = fopen(filename, "r");
		if (fp == NULL) {
			status = 2;
			return;
		}
		char line[256];

		while (fgets(line, 256, fp) && size < 16) {
			line_number++;
			if (strlen(trim(line)) == 0 || line[0] == '#') {
				continue;
			}
			char key[256];
			if (sectionNameFromLine(line, key)) {
				sections[size++] = new ConfigSection(key);
			}
			else if (size > 0) {
				keyValueFromLine(sections[size - 1], line);
			}
		}
	}

	~Config() {
		for (int i = 0; i < size; i++) {
			delete sections[i];
		}
		delete empty_section;
	}

	const ConfigSection* getSection(const char* key) const {
		for (int i = 0; i < size; i++) {
			if (stricmp(sections[i]->key, key) == 0) {
				return sections[i];
			}
		}
		return empty_section;
	}

	const char* getString(const char* section, const char* key, const char* def = 0) const {
		return getSection(section)->getString(key, def);
	}

	bool getBool(const char* section, const char* key, const bool def = false) const {
		const char* s = getSection(section)->getString(key);
		return s ? stricmp(s, "true") == 0 : def;// 1
	}

	bool sectionNameFromLine(const char* line, char* section_name) {
		if (strlen(line) > 2 && line[0] == '[' && line[strlen(line) - 1] == ']') {
			strncpy(section_name, &line[1], strlen(line) - 2);
			section_name[strlen(line) - 2] = 0;
			return true;
		}
		return false;
	}

	bool keyValueFromLine(ConfigSection* section, const char* line) {
		int pos = -1;
		for (int i = 0; pos == -1 && i < (int)strlen(line); i++) {
			if (line[i] == '=') {
				pos = i;
			}
		}
		if (pos != -1) {
			char key[256];
			strncpy(key, line, pos);
			key[pos] = 0;
			if (strlen(trim(key)) > 0) {
				char value[256];
				trim(skipComment(trim(strncpy(value, line + pos + 1, strlen(line) - pos))));
				section->add(key, value);
				return true;
			}
			throw 1; // empty key not allowed
		}
		throw 1; // line is not a key/value pair
	}

	char* skipComment(char* line) {
		for (size_t i = 0; i < strlen(line); i++) {
			if (line[i] == '#') {
				line[i] = 0;
			}
		}
		return line;
	}
	int line_number;
	ConfigSection* sections[16];
	ConfigSection* empty_section;
	int size;
	int status;
};
