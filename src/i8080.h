#pragma once

#include <cstdio>
#include <cstdint>

#include "Memory.h"
#include "CPM.h"

#define A 0b111
#define B 0b000
#define C 0b001
#define D 0b010
#define E 0b011
#define H 0b100
#define L 0b101
#define MEMORY_REF 0b110

class i8080
{
public:
	i8080(Memory* memory, CPM* cpm);
	~i8080();

	void Cycle();

private:
	struct flags {
		uint8_t reg{0};

		uint8_t cy() const { return (reg & 0x1); }
		uint8_t p()  const { return ((reg >> 2) & 0x1); }
		uint8_t ac() const { return ((reg >> 4) & 0x1); }
		uint8_t z()  const { return ((reg >> 6) & 0x1); }
		uint8_t s()  const { return ((reg >> 7) & 0x1); }

		void setBit(uint8_t bit, uint8_t val) {
			if (val)
				reg |= (val << bit);
			else
				reg &= ~(0x1 << bit);
		}

		void setCY(uint8_t val) { setBit(0, val); }
		void setP(uint8_t val)  { setBit(2, val); }
		void setAC(uint8_t val) { setBit(4, val); }
		void setZ(uint8_t val)  { setBit(6, val); }
		void setS(uint8_t val)	{ setBit(7, val); }

		void reset() { reg = 0; }
	} m_flags;

	uint8_t registers[8]{0};
	uint16_t PC{}, SP{};

	Memory* m_Memory = nullptr;
	CPM* m_CPM = nullptr;

private:
	uint16_t add(const uint8_t v1, const uint8_t v2);
	uint16_t subtract(const uint8_t v1, const uint8_t v2);
	uint8_t compare(const uint8_t value);
	void setACF(uint8_t v1, uint8_t v2, uint8_t v3);
	void setZSP(const uint8_t value);

	uint8_t LoadByte();
	uint16_t LoadWord();
	uint16_t LoadRegisterPair(uint8_t rhIdx, uint8_t rlIdx);

	void RET(bool cond);
	void JMP(bool cond);
	void CALL(bool cond);
	void PCHL();

	void POP(uint8_t rhIdx, uint8_t rlIdx);
	void POP_PSW();
	void PUSH(uint8_t rhIdx, uint8_t rlIdx);
	void PUSH_PSW();

	void STC();
	void CMC();

	void MOV(uint8_t opcode);

	void MVI(uint8_t opcode);
	void LXI(uint8_t opcode);
	void XCHG();
	void XTHL();
	void SPHL();

	void STAX(uint16_t addr);
	void LDAX(uint16_t addr);

	void SHLD(uint16_t addr);
	void LHLD(uint16_t addr);
	void STA(uint16_t addr);
	void LDA(uint16_t addr);

	void DAD(uint8_t opcode);
	void DAA();

	void CMA();
	void INR(uint8_t opcode);
	void DCR(uint8_t opcode);
	void INX(uint8_t opcode);
	void DCX(uint8_t opcode);

	void ADI(uint8_t value);
	void ACI(uint8_t value);
	void SUI(uint8_t value);
	void SBI(uint8_t value);
	void ANI(uint8_t value);
	void XRI(uint8_t value);
	void ORI(uint8_t value);
	void CPI(uint8_t value);

	void ADD(uint8_t value, uint8_t regIdx);
	void SUB(uint8_t value, uint8_t regIdx);
	void ADC(uint8_t value, uint8_t regIdx);
	void SBB(uint8_t value, uint8_t regIdx);
	void ANA(uint8_t value, uint8_t regIdx);
	void XRA(uint8_t value, uint8_t regIdx);
	void ORA(uint8_t value, uint8_t regIdx);
	void CMP(uint8_t value, uint8_t regIdx);

	void RLC();
	void RRC();
	void RAL();
	void RAR();

	void ProcessPUSH(uint8_t opcode);
	void ProcessPOP(uint8_t opcode);
	void ProcessJMP(uint8_t opcode);
	void ProcessCALL(uint8_t opcode);
	void ProcessRET(uint8_t opcode);

	void ProcessRotateAcc(uint8_t opcode);
	void ProcessAccTransfer(uint8_t opcode);
	void ProcessImmediate(uint8_t opcode);
	void ProcessRegisterToAcc(uint8_t opcode);
	void ProcessDirectAddressing(uint8_t opcode);
};