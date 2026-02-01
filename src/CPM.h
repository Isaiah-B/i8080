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
			case 0x0: WBOOT(); break;
			case 0x9: C_WRITESTR(addr); break;

			default:
				fprintf(stderr, "INVALID CPM FUNCTION CALL\nExiting...\n");
				exit(1);
		}
	}

	void WBOOT()
	{
		printf("CPM WBOOT\n");
		exit(0);
	}

	void C_WRITESTR(uint16_t addr)
	{
		uint8_t c = memory->Read(addr++);

		while (c != '$') {
			printf("%c", c);
			c = memory->Read(addr++);
		}
		printf("\n");
	}

private:
	Memory* memory;
};