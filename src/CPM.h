#pragma once

#include <cstdint>
#include <cstdio>

#include "Memory.h"

class CPM
{
public:
	CPM(Memory* _memory)
		: memory(_memory) { }

	~CPM() {
		memory = nullptr;
	}

	void Call(uint8_t code, uint16_t addr)
	{
		switch (code)
		{
			case 9: C_WRITESTR(addr);
		}
	}

	void C_WRITESTR(uint16_t addr)
	{
		uint8_t c = memory->Read(addr++);

		while (c != '$') {
			printf("%c", c);
			c = memory->Read(addr++);
		}
	}

private:
	Memory* memory;
};