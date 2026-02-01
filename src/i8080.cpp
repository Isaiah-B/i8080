#include <fstream>
#include <vector>

#include "i8080.h"

#define PROGRAM_START 0x100

#if _DEBUG
	#define DEBUG_PRINT(s, ...) printf(s, __VA_ARGS__)
#else
	#define DEBUG_PRINT(s, ...)
#endif


i8080::i8080(Memory* _memory, CPM* CPM)
	: m_Memory(_memory), m_CPM(CPM)
{
	PC = PROGRAM_START;
	SP = 0xFFFF;

	m_flags.reset();
}

i8080::~i8080()
{
	m_Memory = nullptr;
	m_CPM = nullptr;
}


///////////////////////////////////
//////////////UTILS///////////////
/////////////////////////////////

static char GetRegisterFromIndex(uint8_t index)
{
	switch (index)
	{
		case 0x0: return 'B';
		case 0x1: return 'C';
		case 0x2: return 'D';
		case 0x3: return 'E';
		case 0x4: return 'H';
		case 0x5: return 'L';
		case 0x7: return 'A';

		case 0x6: return 'M';
	}

	fprintf(stderr, "Invalid register access 0x%02X", index);
	exit(1);
}


uint16_t i8080::add(const uint8_t v1, const uint8_t v2)
{
	uint16_t res = v1 + v2;
	
	setZSP(res);
	m_flags.setCY((res >> 8) & 0x1);
	m_flags.setAC(((v1 ^ v2 ^ res) & 0x10) >> 4);

	return res;
}

uint16_t i8080::subtract(const uint8_t v1, const uint8_t v2)
{
	uint8_t tc = (~v2) + 1;
	uint16_t res = v1 + tc;

	setZSP(res);
	m_flags.setCY(!((res >> 8) & 0x1));
	m_flags.setAC(((v1 ^ v2 ^ res) & 0x10) >> 4);

	return res;
}

uint8_t i8080::compare(const uint8_t value)
{
	uint8_t tc = (~value) + 1;
	uint16_t res = registers[A] + tc;

	if (value < registers[A]) {
		m_flags.setCY(((res >> 8) & 0x1));
	}
	else {
		m_flags.setCY(!((res >> 8) & 0x1));
	}

	setZSP(res);
	setACF(registers[A], value, res);

	return res;
}

void i8080::setACF(uint8_t v1, uint8_t v2, uint8_t v3)
{
	uint8_t res = ((v1 ^ v2 ^ v3) & 0x1) >> 4;
	m_flags.setAC(res);
}

void i8080::setZSP(const uint8_t value)
{
	m_flags.setZ(value == 0x00);
	m_flags.setS((value >> 7) & 0x1);
	
	uint8_t count = 0;

	for (uint8_t i{ 0 }; i < 8; i++) {
		count += (value >> i);
	}

	m_flags.setP(count % 2 == 0);
}

uint8_t i8080::LoadByte()
{
	return m_Memory->Read(PC++);
}

uint16_t i8080::LoadWord()
{
	return m_Memory->Read(PC++) | (m_Memory->Read(PC++) << 8);
}

uint16_t i8080::LoadRegisterPair(uint8_t rhIdx, uint8_t rlIdx)
{
	return (registers[rhIdx] << 8) | registers[rlIdx];
}


///////////////////////////////////
//////////////CYCLE///////////////
/////////////////////////////////
void i8080::Cycle()
{
	DEBUG_PRINT("0x%04X - ", PC);

	uint8_t opcode = LoadByte();

	DEBUG_PRINT("0x%02X ", opcode);

	switch (opcode)
	{
		case 0x00: DEBUG_PRINT("NOOP\n");	break;

		case 0x2F: CMA();	break;

		case 0x27: DAA();	break;
		case 0x37: STC();	break;
		case 0x3F: CMC();	break;

		case 0xE9: PCHL();  break;

		case 0xE3: XTHL();	break;
		case 0xEB: XCHG();	break;
		case 0xF9: SPHL();	break;

		default: {
			if (((opcode ^ 0xFB) & 0xC7) == 0xC7) {
				INR(opcode);
				return;
			}

			if (((opcode ^ 0xFC) & 0xCF) == 0xCF) {
				INX(opcode);
				return;
			}

			if (((opcode ^ 0xF4) & 0xCF) == 0xCF) {
				DCX(opcode);
				return;
			}

			if (((opcode ^ 0xFA) & 0xC7) == 0xC7) {
				DCR(opcode);
				return;
			}

			if (((opcode ^ 0xBF) & 0xC0) == 0xC0) {
				MOV(opcode);
				return;
			}

			if (((opcode ^ 0xF6) & 0xCF) == 0xCF) {
				DAD(opcode);
				return;
			}

			if (((opcode ^ 0xFE) & 0xCF) == 0xCF) {
				LXI(opcode);
				return;
			}

			if (((opcode ^ 0xF9) & 0xC7) == 0xC7) {
				MVI(opcode);
				return;
			}

			if (((opcode ^ 0xF8) & 0xE7) == 0xE7) {
				ProcessRotateAcc(opcode);
				return;
			}

			// LDAX/STAX
			if (((opcode ^ 0xFD) & 0xE7) == 0xE7) {
				ProcessAccTransfer(opcode);
				return;
			}

			if (((opcode ^ 0xDD) & 0xE7) == 0xE7) {
				ProcessDirectAddressing(opcode);
				return;
			}

			if (((opcode ^ 0x39) & 0xC7) == 0xC7) {
				ProcessImmediate(opcode);
				return;
			}

			if (((opcode ^ 0x7F) & 0xC0) == 0xC0) {
				ProcessRegisterToAcc(opcode);
				return;
			}

			if (((opcode ^ 0x3A) & 0xCF) == 0xCF) {
				ProcessPUSH(opcode);
				return;
			}

			if (((opcode ^ 0x3E) & 0xCF) == 0xCF) {
				ProcessPOP(opcode);
				return;
			}

			if (((opcode ^ 0x3D) & 0xC6) == 0xC6) {
				ProcessJMP(opcode);
				return;
			}

			if (((opcode ^ 0x3B) & 0xC6) == 0xC6) {
				ProcessCALL(opcode);
				return;
			}

			if (((opcode ^ 0x3F) & 0xC6) == 0xC6) {
				ProcessRET(opcode);
				return;
			}

			fprintf(stderr, "INVALID OPERATION\nExiting...\n");
			exit(1);
		}
	}

}


///////////////////////////////////////
//////////////OPERATIONS//////////////
/////////////////////////////////////
void i8080::RET(bool cond)
{
	if (cond) {
		uint16_t addr = m_Memory->Read(SP++) | (m_Memory->Read(SP++) << 8);
		PC = addr;

		DEBUG_PRINT(" 0x%04X\n", addr);
		return;
	}

	DEBUG_PRINT(" -- NO RET\n");
}

void i8080::JMP(bool cond)
{
	uint16_t addr = LoadWord();

	if (cond) {
		DEBUG_PRINT(" 0x%04X\n", addr);

		if (addr == 0x0) {
			m_CPM->WBOOT();
		}

		PC = addr;
		return;
	}

	DEBUG_PRINT(" -- NO JMP\n");
}

void i8080::CALL(bool cond)
{
	uint16_t addr = LoadWord();

	// Make CPM BDOS function call
	// C = Function code
	// DE = data address
	if (addr == 0x0005) {
		DEBUG_PRINT(" 0x%04X\n", addr);

		m_CPM->Call(registers[C],
					LoadRegisterPair(D, E));
		return;
	}

	if (cond) {
		m_Memory->Write(--SP, (PC & 0xFF00) >> 8);
		m_Memory->Write(--SP, PC & 0x00FF);

		PC = addr;

		DEBUG_PRINT(" PC -> 0x%04X\n", PC);
		return;
	}

	DEBUG_PRINT(" -- NO CALL\n");
}

void i8080::PCHL()
{
	PC = LoadRegisterPair(H, L);

	DEBUG_PRINT("PCHL PC -> 0x%04X\n", PC);
}

void i8080::POP(uint8_t rhIdx, uint8_t rlIdx)
{
	registers[rlIdx] = m_Memory->Read(SP++);
	registers[rhIdx] = m_Memory->Read(SP++);

	DEBUG_PRINT("POP 0x%02X(%c), 0x%02X(%c)\n",
		registers[rhIdx], GetRegisterFromIndex(rhIdx),
		registers[rlIdx], GetRegisterFromIndex(rlIdx));
}

void i8080::POP_PSW()
{
	uint8_t data = m_Memory->Read(SP);

	m_flags.reg = data;

	registers[A] = m_Memory->Read(++SP);
	SP++;

	DEBUG_PRINT("POP PSW\n\tFLAGS = 0x%02X\n\tA = 0x%02X\n", data, m_Memory->Read(SP-2));
}

void i8080::PUSH(uint8_t rhIdx, uint8_t rlIdx)
{
	m_Memory->Write(--SP, registers[rhIdx]);
	m_Memory->Write(--SP, registers[rlIdx]);

	DEBUG_PRINT("PUSH - 0x%02X(%c), 0x%02X(%c)\n",
		registers[rhIdx], GetRegisterFromIndex(rhIdx),
		registers[rlIdx], GetRegisterFromIndex(rlIdx));
}

void i8080::PUSH_PSW()
{
	m_Memory->Write(--SP, registers[A]);
	m_Memory->Write(--SP, m_flags.reg);

	DEBUG_PRINT("PUSH_PSW\n");
}

void i8080::STC()
{
	m_flags.setCY(0x1);
	DEBUG_PRINT("STC\n");
}

void i8080::CMC()
{
	uint8_t val = !m_flags.cy();
	m_flags.setCY(val);

	DEBUG_PRINT("CMC cy = %d\n", val);
}

void i8080::MVI(uint8_t opcode)
{
	uint8_t regIdx = (opcode & 0x38) >> 3;

	uint8_t byte = LoadByte();

	if (regIdx == MEMORY_REF)
		m_Memory->Write(LoadRegisterPair(H, L), byte);
	else
		registers[regIdx] = byte;

	DEBUG_PRINT("MVI 0x%02X -> %c\n", byte, GetRegisterFromIndex(regIdx));
}

void i8080::MOV(uint8_t opcode)
{
	uint8_t dstIndex = (opcode & 0x38) >> 3;
	uint8_t srcIndex = opcode & 0x7;

	if (dstIndex == MEMORY_REF) {
		uint16_t addr = LoadRegisterPair(H, L);
		m_Memory->Write(addr, registers[srcIndex]);

		DEBUG_PRINT("MOV 0x%02X(%c) -> 0x%04X(M)\n",
			registers[srcIndex], GetRegisterFromIndex(srcIndex), addr);

		return;
	}
	else if (srcIndex == MEMORY_REF) {
		uint16_t addr = LoadRegisterPair(H, L);
		uint8_t byte = m_Memory->Read(addr);
		registers[dstIndex] = byte;

		DEBUG_PRINT("MOV 0x%02X(M) -> %c\n",
			byte, GetRegisterFromIndex(dstIndex));

		return;
	}

	uint8_t dstReg = registers[dstIndex];
	uint8_t srcReg = registers[srcIndex];

	registers[dstIndex] = registers[srcIndex];

	DEBUG_PRINT("MOV 0x%02X(%c) -> 0x%02X(%c)\n",
		srcReg, GetRegisterFromIndex(srcIndex),
		dstReg, GetRegisterFromIndex(dstIndex));
}

// Exchange HL with DE
void i8080::XCHG()
{
	uint8_t tmp = registers[H];
	registers[H] = registers[D];
	registers[D] = tmp;

	tmp = registers[L];
	registers[L] = registers[E];
	registers[E] = tmp;

	DEBUG_PRINT("XCHG 0x%02X(H) <-> 0x%02X(D) - 0x%02X(L) <-> 0x%02X(E)\n",
		registers[D], registers[H],
		registers[E], registers[L]);
}

void i8080::XTHL()
{
	uint8_t temp = registers[L];
	registers[L] = m_Memory->Read(SP);
	m_Memory->Write(SP, temp);

	temp = registers[H];
	registers[H] = m_Memory->Read(SP + 1);
	m_Memory->Write(SP + 1, temp);

	DEBUG_PRINT("XTHL H(0x%02X), L(0x%02X)\n", registers[H], registers[L]);
}

void i8080::SPHL()
{
	uint16_t data = LoadRegisterPair(H, L);
	SP = data;

	DEBUG_PRINT("SPHL 0x%04X(HL) -> SP\n", data);
}

void i8080::STAX(uint16_t addr)
{
	m_Memory->Write(addr, registers[A]);

	DEBUG_PRINT("STAX 0x%02X(A) -> 0x%04X(M)\n", registers[A], addr);

}

void i8080::LDAX(uint16_t addr)
{
	uint8_t data = m_Memory->Read(addr);
	registers[A] = data;

	DEBUG_PRINT("LDAX 0x%02X(0x%04X) -> A\n", data, addr);
}

void i8080::SHLD(uint16_t addr)
{
	m_Memory->Write(addr, registers[L]);
	m_Memory->Write(addr+1, registers[H]);

	DEBUG_PRINT("SHLD 0x%02X(L) -> 0x%04X, 0x%02X(H) -> 0x%04X\n", registers[L], addr, registers[H], addr+1);
}

void i8080::LHLD(uint16_t addr)
{
	uint8_t loByte = m_Memory->Read(addr);
	uint8_t hiByte = m_Memory->Read(addr + 1);

	registers[L] = loByte;
	registers[H] = hiByte;

	DEBUG_PRINT("LHLD 0x%02X(0x%04X) -> L, 0x%02X(0X%04X) -> H\n", loByte, addr, hiByte, addr+1);
}

void i8080::LDA(uint16_t addr)
{
	uint8_t data = m_Memory->Read(addr);
	registers[A] = data;

	DEBUG_PRINT("LDA 0x%02X(0x%04X) -> A\n", data, addr);
}

void i8080::STA(uint16_t addr)
{
	m_Memory->Write(addr, registers[A]);

	DEBUG_PRINT("STA 0x%02X(A) -> 0x%04X(M)\n", registers[A], addr);
}

void i8080::DAD(uint8_t opcode)
{
	uint8_t rpIdx = (opcode & 0x30) >> 4;

	uint16_t rpValue{};

	switch (rpIdx) {
		case 0x0: rpValue = LoadRegisterPair(B, C); break;
		case 0x1: rpValue = LoadRegisterPair(D, E); break;
		case 0x2: rpValue = LoadRegisterPair(H, L); break;
		case 0x3: rpValue = SP;
	}

	uint16_t HLValue = LoadRegisterPair(H, L);

	uint32_t res = rpValue + HLValue;
	m_flags.setCY(res >> 16);

	registers[L] = res & 0xFF;
	registers[H] = (res & 0xFF00) >> 8;

// ugly debug print block
#ifdef _DEBUG
	if (rpIdx == 0x3) {
		DEBUG_PRINT("DAD 0x%04X(SP) + 0x%04X(HL) -> 0x%04X(HL)\n", rpValue, HLValue, res);
	}
	else {

		uint8_t rpHi{}, rpLo{};
		switch (rpIdx) {
			case 0x0: rpHi = B; rpLo = C; break;
			case 0x1: rpHi = D; rpLo = E; break;
			case 0x2: rpHi = H; rpLo = L; break;
		}
		DEBUG_PRINT("DAD 0x%04X(%c%c) + 0x%04X(HL) -> 0x%04X(HL)\n",
			rpValue,
			GetRegisterFromIndex(rpHi), GetRegisterFromIndex(rpLo),
			HLValue, res);
	}
#endif
}

void i8080::DAA()
{
	uint8_t initialA = registers[A];

	uint8_t accLo = registers[A] & 0x0F;
	if ((accLo > 0x9) || m_flags.ac() == 0x1) {
		uint16_t res = registers[A] + 6;

		setACF(initialA, 0x6, res);
		accLo = res & 0xF;
		registers[A] = res;
	}

	uint8_t accHi = registers[A] >> 4;
	if ((accHi > 0x9) || m_flags.cy() == 0x1) {
		accHi += 6;

		m_flags.setCY((accHi >> 4) & 0x1);
	}

	uint8_t res = (accHi << 4) | accLo;
	registers[A] = res;

	setZSP(res);

	DEBUG_PRINT("DAA 0x%02X(A) -> 0x%02X(A)\n", initialA, registers[A]);
}

void i8080::CMA()
{
	uint8_t a = registers[A];
	registers[A] = ~a;

	DEBUG_PRINT("CMA 0x%02X(A) -> 0x%02X(A)\n", a, registers[A]);
}

void i8080::INR(uint8_t opcode)
{
	uint8_t regIndex = (opcode & 0x38) >> 3;

	// INC byte at memory[HL]
	if (regIndex == 0x6) {
		uint16_t addr = LoadRegisterPair(H, L);
		uint8_t byte = m_Memory->Read(addr);
		uint8_t res = add(byte, 1);

		m_Memory->Write(addr, res);

		DEBUG_PRINT("INR 0x%02X(M) + 1 -> 0x%02X\n", byte, res);
		return;
	}

	uint8_t reg = registers[regIndex];
	uint8_t res = add(reg, 1);
	registers[regIndex] = res;

	DEBUG_PRINT("INR 0x%02X(%c) + 1 -> 0x%02X\n", reg, GetRegisterFromIndex(regIndex), res);
}

void i8080::DCR(uint8_t opcode)
{
	uint8_t regIndex = (opcode & 0x38) >> 3;

	// DEC byte at memory[HL]
	if (regIndex == 0x6) {
		uint16_t addr = LoadRegisterPair(H, L);
		uint8_t byte = m_Memory->Read(addr);
		uint8_t res = subtract(byte, 1);

		m_Memory->Write(addr, res);

		DEBUG_PRINT("DCR 0x%02X(M) - 1 -> 0x%02X\n", byte, res);
		return;
	}

	uint8_t reg = registers[regIndex];
	uint8_t res = subtract(reg, 1);
	registers[regIndex] = res;

	DEBUG_PRINT("DCR 0x%02X(%c) - 1 -> 0x%02X\n", reg, GetRegisterFromIndex(regIndex), res);
}

void i8080::INX(uint8_t opcode)
{
	uint8_t rpIndex = (opcode & 0x30) >> 4;

	if (rpIndex == 0x3) {
		SP++;
		DEBUG_PRINT("INX SP + 1 -> 0x%04X\n", SP);
		return;
	}

	uint8_t regHiIndex{};
	uint8_t regLoIndex{};

	switch (rpIndex) {
		case 0x0: regHiIndex = B; regLoIndex = C; break;
		case 0x1: regHiIndex = D; regLoIndex = E; break;
		case 0x2: regHiIndex = H; regLoIndex = L; break;
	}

	uint16_t rpValue = LoadRegisterPair(regHiIndex, regLoIndex);
	rpValue++;

	registers[regHiIndex] = rpValue >> 8;
	registers[regLoIndex] = rpValue & 0xFF;

	DEBUG_PRINT("INX %c%c + 1 -> 0x%04X\n",
		GetRegisterFromIndex(regHiIndex),
		GetRegisterFromIndex(regLoIndex),
		rpValue);
}

void i8080::DCX(uint8_t opcode)
{
	uint8_t rpIndex = (opcode & 0x30) >> 4;

	if (rpIndex == 0x3) {
		SP--;
		DEBUG_PRINT("DCX SP - 1 -> 0x%04X\n", SP);
		return;
	}

	uint8_t regHiIndex{};
	uint8_t regLoIndex{};

	switch (rpIndex) {
		case 0x0: regHiIndex = B; regLoIndex = C; break;
		case 0x1: regHiIndex = D; regLoIndex = E; break;
		case 0x2: regHiIndex = H; regLoIndex = L; break;
	}

	uint16_t rpValue = LoadRegisterPair(regHiIndex, regLoIndex);
	rpValue--;

	registers[regHiIndex] = rpValue >> 8;
	registers[regLoIndex] = rpValue & 0xFF;

	DEBUG_PRINT("DCX %c%c - 1 -> 0x%04X\n",
		GetRegisterFromIndex(regHiIndex),
		GetRegisterFromIndex(regLoIndex),
		rpValue);
}

void i8080::LXI(uint8_t opcode)
{
	uint8_t regIdx = (opcode & 0x30) >> 4;

	// Load into SP
	if (regIdx == 0x3) {
		uint16_t data = LoadWord();
		SP = data;
		DEBUG_PRINT("LXI 0x%04X -> SP\n", data);
		return;
	}

	// Load into register pair
	uint8_t rpHiIdx{};
	uint8_t rpLoIdx{};

	switch (regIdx) {
		case 0x0: rpHiIdx = B; rpLoIdx = C; break;
		case 0x1: rpHiIdx = D; rpLoIdx = E; break;
		case 0x2: rpHiIdx = H; rpLoIdx = L; break;
	}

	registers[rpLoIdx] = LoadByte();
	registers[rpHiIdx] = LoadByte();

	DEBUG_PRINT("LXI 0x%02X -> %c, 0x%02X -> %c\n",
		registers[rpHiIdx], GetRegisterFromIndex(rpHiIdx),
		registers[rpLoIdx], GetRegisterFromIndex(rpLoIdx));
}

/////////////////////////////////////////
///////////IMMEDIATE TO ACC/////////////
///////////////////////////////////////
void i8080::ADI(uint8_t value)
{
	uint8_t a = registers[A];
	uint16_t res = add(a, value);
	registers[A] = res;

	DEBUG_PRINT("ADI 0x%02X(A) + 0x%02X -> 0x%02X\n", a, value, res);
}

void i8080::ACI(uint8_t value)
{
	uint8_t a = registers[A];
	uint8_t carry = m_flags.cy();

	uint16_t res = add(a, value + carry);
	registers[A] = res;

	DEBUG_PRINT("ACI 0x%02X(A) + 0x%02X + 0x%02X -> 0x%02X\n", a, value, carry, res);
}

void i8080::SUI(uint8_t value)
{
	uint8_t a = registers[A];
	uint16_t res = subtract(a, value);
	registers[A] = res;

	DEBUG_PRINT("SUI 0x%02X(A) - 0x%02X -> 0x%02X\n", a, value, res);
}

void i8080::SBI(uint8_t value)
{
	uint8_t a = registers[A];
	uint8_t carry = m_flags.cy();

	uint16_t res = subtract(a, value + carry);
	registers[A] = res;

	DEBUG_PRINT("SUI 0x%02X(A) - (0x%02X + 0x%02X) -> 0x%02X\n", a, value, carry, res);
}

void i8080::ANI(uint8_t value)
{
	uint8_t a = registers[A];
	uint8_t res = registers[A] & value;
	registers[A] = res;

	setZSP(res);
	m_flags.setCY(0);
	m_flags.setAC(0);

	DEBUG_PRINT("ANI 0x%02X(A) AND 0x%02X -> 0x%02X\n", a, value, res);
}

void i8080::XRI(uint8_t value)
{
	uint8_t a = registers[A];

	uint8_t res = registers[A] ^ value;
	registers[A] = res;

	setZSP(res);
	m_flags.setCY(0);

	DEBUG_PRINT("XRI 0x%02X(A) XOR 0x%02X -> 0x%02X\n", a, value, res);
}

void i8080::ORI(uint8_t value)
{
	uint8_t a = registers[A];

	uint8_t res = registers[A] | value;
	registers[A] = res;

	setZSP(res);
	m_flags.setCY(0);

	DEBUG_PRINT("ORI 0x%02X(A) OR 0x%02X -> 0x%02X\n", a, value, res);
}

void i8080::CPI(uint8_t value)
{
	uint8_t a = registers[A];
	uint8_t res = compare(value);

	DEBUG_PRINT("CPI 0x%02X(A), 0x%02X\n", a, value);
}

////////////////////////////////////////
///////////REGISTER TO ACC/////////////
//////////////////////////////////////
void i8080::ADD(uint8_t value, uint8_t regIdx)
{
	uint8_t a = registers[A];
	uint8_t res = add(a, value);
	registers[A] = res;

	DEBUG_PRINT("ADD 0x%02X(A) + 0x%02X(%c) -> 0x%02X\n", a, value, GetRegisterFromIndex(regIdx), res);
}

void i8080::SUB(uint8_t value, uint8_t regIdx)
{
	uint8_t a = registers[A];
	uint8_t res = subtract(a, value);
	registers[A] = res;

	DEBUG_PRINT("SUB 0x%02X(A) - 0x%02X(%c) -> 0x%02X\n", a, value, GetRegisterFromIndex(regIdx), res);
}

void i8080::ADC(uint8_t value, uint8_t regIdx)
{
	uint8_t a = registers[A];
	uint8_t carry = m_flags.cy();

	uint8_t res = add(a, value + carry);
	registers[A] = res;

	DEBUG_PRINT("ADC 0x%02X(A) + 0x%02X(%c) + 0x%02X -> 0x%02X\n", a, value, GetRegisterFromIndex(regIdx), carry, res);
}

void i8080::SBB(uint8_t value, uint8_t regIdx)
{
	uint8_t a = registers[A];
	uint8_t carry = m_flags.cy();

	uint8_t res = subtract(a, value + carry);
	registers[A] = res;

	DEBUG_PRINT("SBB 0x%02X(A) - (0x%02X(%c) + 0x%02X) -> 0x%02X\n", a, value, GetRegisterFromIndex(regIdx), carry, res);
}

void i8080::ANA(uint8_t value, uint8_t regIdx)
{
	uint8_t a = registers[A];
	uint8_t res = registers[A] & value;
	registers[A] = res;

	setZSP(res);
	m_flags.setCY(0);

	DEBUG_PRINT("ANA 0x%02X(A) AND 0x%02X -> 0x%02X\n", a, value, res);
}

void i8080::XRA(uint8_t value, uint8_t regIdx)
{
	uint8_t a = registers[A];
	uint8_t res = registers[A] ^ value;
	registers[A] = res;

	setZSP(res);
	m_flags.setCY(0);
	setACF(a, value, res);

	DEBUG_PRINT("XRA 0x%02X(A) XOR 0x%02X(%c) -> 0x%02X\n", a, value, GetRegisterFromIndex(regIdx), res);
}

void i8080::ORA(uint8_t value, uint8_t regIdx)
{
	uint8_t a = registers[A];
	uint8_t res = registers[A] | value;
	registers[A] = res;

	setZSP(res);
	m_flags.setCY(0);

	DEBUG_PRINT("ORA 0x%02X(A) OR 0x%02X -> 0x%02X\n", a, value, res);
}

void i8080::CMP(uint8_t value, uint8_t regIdx)
{
	uint8_t a = registers[A];
	uint8_t res = compare(value);

	DEBUG_PRINT("CMP 0x%02X(A), 0x%02X(%c)\n", registers[A], value, GetRegisterFromIndex(regIdx));
}

void i8080::RLC()
{
	m_flags.setCY(registers[A] >> 7);

	registers[A] <<= 1;
	registers[A] |= m_flags.cy();

	DEBUG_PRINT("RLC A<< -> 0x%02X\n", registers[A]);
}

void i8080::RRC()
{
	m_flags.setCY(registers[A] & 0x1);

	registers[A] >>= 1;
	registers[A] |= (m_flags.cy() << 7);

	DEBUG_PRINT("RRC A>> -> 0x%02X\n", registers[A]);
}

void i8080::RAL()
{
	uint8_t aHiBit = registers[A] >> 7;
	uint8_t c = m_flags.cy();

	m_flags.setCY(aHiBit);

	registers[A] <<= 1;
	registers[A] |= c;

	DEBUG_PRINT("RAL c<<A -> 0x%02X\n", registers[A]);
}

void i8080::RAR()
{
	uint8_t aLowBit = registers[A] & 0x1;
	uint8_t c = m_flags.cy();

	m_flags.setCY(aLowBit);

	registers[A] >>= 1;
	registers[A] |= (c << 7);

	DEBUG_PRINT("RAR A>>c -> 0x%02X\n", registers[A]);
}

void i8080::ProcessPUSH(uint8_t opcode)
{
	uint8_t rpIdx = (opcode & 0x30) >> 4;

	switch (rpIdx) {
		case 0x0: PUSH(B, C); break;
		case 0x1: PUSH(D, E); break;
		case 0x2: PUSH(H, L); break;
		case 0x3: PUSH_PSW();
	}
}

void i8080::ProcessPOP(uint8_t opcode)
{
	uint8_t rpIdx = (opcode & 0x30) >> 4;

	switch (rpIdx) {
		case 0x0: POP(B, C); break;
		case 0x1: POP(D, E); break;
		case 0x2: POP(H, L); break;
		case 0x3: POP_PSW();
	}
}

void i8080::ProcessJMP(uint8_t opcode)
{
	bool cond = 0;
	uint8_t code = (opcode & 0x38) >> 3;

	// Bit determines whether JMP or JNZ is called
	// when code = 0x0
	uint8_t jmpBit = opcode & 0x1;

	switch (code)
	{
		case 0x0:
			if (jmpBit == 0x1) {
				cond = true;
				DEBUG_PRINT("JMP");
			}
			else {
				cond = m_flags.z() == 0;
				DEBUG_PRINT("JNZ");
			}
			break;

		case 0x1: cond = m_flags.z() == 1;	DEBUG_PRINT("JZ");	break;
		case 0x2: cond = m_flags.cy() == 0; DEBUG_PRINT("JNC");	break;
		case 0x3: cond = m_flags.cy() == 1; DEBUG_PRINT("JC");	break;
		case 0x4: cond = m_flags.p() == 0;	DEBUG_PRINT("JPO");	break;
		case 0x5: cond = m_flags.p() == 1;	DEBUG_PRINT("JPE");	break;
		case 0x6: cond = m_flags.s() == 0;	DEBUG_PRINT("JP");	break;
		case 0x7: cond = m_flags.s() == 1;	DEBUG_PRINT("JM");	break;
	}

	JMP(cond);
}

void i8080::ProcessCALL(uint8_t opcode)
{
	bool cond = 0;
	uint8_t code = (opcode & 0x38) >> 3;

	// Bit determines whether CALL or CZ is called
	// when code = 0x1
	uint8_t callBit = opcode & 0x1;

	switch (code)
	{
		case 0x1:
			if (callBit == 0x1) {
				cond = true;
				DEBUG_PRINT("CALL");
			}
			else {
				cond = m_flags.z() == 1;
				DEBUG_PRINT("CZ");
			}
			break;

		case 0x0: cond = m_flags.z() == 0;	DEBUG_PRINT("CNZ");	break;
		case 0x2: cond = m_flags.cy() == 0; DEBUG_PRINT("CNC");	break;
		case 0x3: cond = m_flags.cy() == 1; DEBUG_PRINT("CC");	break;
		case 0x4: cond = m_flags.p() == 0;	DEBUG_PRINT("CPO");	break;
		case 0x5: cond = m_flags.p() == 1;	DEBUG_PRINT("CPE");	break;
		case 0x6: cond = m_flags.s() == 0;	DEBUG_PRINT("CP");	break;
		case 0x7: cond = m_flags.s() == 1;	DEBUG_PRINT("CM");	break;
	}

	CALL(cond);
}

void i8080::ProcessRET(uint8_t opcode)
{
	bool cond = 0;
	uint8_t code = (opcode & 0x38) >> 3;

	// Bit determines whether RET or RZ is called
	// when code = 0x1
	uint8_t retBit = opcode & 0x1;

	switch (code)
	{
		case 0x1:
			if (retBit == 0x1) {
				cond = true;
				DEBUG_PRINT("RET");
			}
			else {
				cond = m_flags.z() == 1;
				DEBUG_PRINT("RZ");
			}
			break;

		case 0x0: cond = m_flags.z() == 0;	DEBUG_PRINT("RNZ");	break;
		case 0x2: cond = m_flags.cy() == 0; DEBUG_PRINT("RNC");	break;
		case 0x3: cond = m_flags.cy() == 1; DEBUG_PRINT("RC");	break;
		case 0x4: cond = m_flags.p() == 0;	DEBUG_PRINT("RPO");	break;
		case 0x5: cond = m_flags.p() == 1;	DEBUG_PRINT("RPE");	break;
		case 0x6: cond = m_flags.s() == 0;	DEBUG_PRINT("RP");	break;
		case 0x7: cond = m_flags.s() == 1;	DEBUG_PRINT("RM");	break;
	}

	RET(cond);
}

void i8080::ProcessRotateAcc(uint8_t opcode)
{
	uint8_t operationIdx = (opcode & 0x18) >> 3;

	switch (operationIdx) {
		case 0x0: RLC(); break;
		case 0x1: RRC(); break;
		case 0x2: RAL(); break;
		case 0x3: RAR(); break;
	}
}

void i8080::ProcessAccTransfer(uint8_t opcode)
{
	uint8_t rpIdx = (opcode & 0x10) >> 4;
	uint8_t operationIdx = (opcode & 0x8) >> 3;

	uint8_t rpHi{}, rpLo{};

	switch (rpIdx) {
		case 0x0: rpHi = B; rpLo = C; break;
		case 0x1: rpHi = D; rpLo = E; break;
	}

	uint16_t addr = LoadRegisterPair(rpHi, rpLo);

	switch (operationIdx) {
		case 0x0: STAX(addr); break;
		case 0x1: LDAX(addr); break;
	}
}

void i8080::ProcessImmediate(uint8_t opcode)
{
	uint8_t operationIdx = (opcode & 0x38) >> 3;
	uint8_t val = LoadByte();

	switch (operationIdx)
	{
		case 0x0: ADI(val); break;
		case 0x1: ACI(val); break;
		case 0x2: SUI(val); break;
		case 0x3: SBI(val); break;
		case 0x4: ANI(val); break;
		case 0x5: XRI(val); break;
		case 0x6: ORI(val); break;
		case 0x7: CPI(val); break;
		default:
			DEBUG_PRINT("ProcessImmediate() - INVALID OPERATION\n");
			exit(1);
	}
}

void i8080::ProcessRegisterToAcc(uint8_t opcode)
{
	uint8_t operationIdx = (opcode & 0x38) >> 3;
	uint8_t regIdx = opcode & 0x7;

	uint8_t val{};
	if (regIdx == MEMORY_REF)
		val = m_Memory->Read(LoadRegisterPair(H, L));
	else
		val = registers[regIdx];

	switch (operationIdx)
	{
		case 0x0: ADD(val, regIdx); break;
		case 0x1: ADC(val, regIdx); break;
		case 0x2: SUB(val, regIdx); break;
		case 0x3: SBB(val, regIdx); break;
		case 0x4: ANA(val, regIdx); break;
		case 0x5: XRA(val, regIdx); break;
		case 0x6: ORA(val, regIdx); break;
		case 0x7: CMP(val, regIdx); break;
		default:
			DEBUG_PRINT("ProcessRegisterToAcc() - INVALID OPERATION\n");
			exit(1);
	}
}

void i8080::ProcessDirectAddressing(uint8_t opcode)
{
	uint8_t operationIdx = (opcode & 0x18) >> 3;

	uint16_t addr = LoadWord();

	switch (operationIdx)
	{
		case 0x0: SHLD(addr); break;
		case 0x1: LHLD(addr); break;
		case 0x2: STA(addr);  break;
		case 0x3: LDA(addr);  break;
	}
}
