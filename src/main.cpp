#include <iostream>

#include "i8080.h"
#include "Memory.h"
#include "CPM.h"

int main()
{
	Memory* memory = new Memory();
	memory->LoadROM("roms/TST8080.COM");
	//memory->LoadROM("roms/8080PRE.COM");
	
	CPM* cpm = new CPM(memory);

	i8080* cpu = new i8080(memory, cpm);

	while (1) {
		cpu->Cycle();
	}

	delete cpu;
	delete cpm;
	delete memory;

	return 0;
}