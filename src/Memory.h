#pragma once

#include <cstdint>
#include <fstream>
#include <vector>

class Memory
{
public:
	Memory() {}

	void LoadROM(const char* filename)
	{
		std::ifstream file(filename, std::ifstream::binary | std::ifstream::ate);
		auto filesize = file.tellg();
		file.seekg(std::ifstream::beg);

		std::vector<char> buf(filesize);

		file.read(buf.data(), filesize);

		if (filesize > 65536) {
			fprintf(stderr, "File is too large");
			exit(1);
		}

		for (unsigned int i = 0; i < filesize; i++) {
			m_Memory[i + 0x100] = buf[i];
		}
	}

	uint8_t Read(uint16_t addr) const { return m_Memory[addr]; }
	void Write(uint16_t addr, uint8_t val) { m_Memory[addr] = val; }

public:
	uint8_t m_Memory[655356]{};
};