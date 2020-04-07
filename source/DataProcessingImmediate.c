#include <stdio.h>
#include <stdlib.h>

#include "adefs.h"
#include "bits.h"
#include "common.h"
#include "instruction.h"
#include "utils.h"
#include "strext.h"

static int DisassemblePCRelativeAddressingInstr(struct instruction *i,
        struct ad_insn *out){
    unsigned op = bits(i->opcode, 31, 31);
    unsigned immlo = bits(i->opcode, 29, 30);
    unsigned immhi = bits(i->opcode, 5, 23);
    unsigned Rd = bits(i->opcode, 0, 4);
    uint64 imm = 0;

    const char *instr_s = NULL;

    if(Rd > AD_RTBL_GEN_64_SZ)
        return 1;

    ADD_FIELD(out, op);
    ADD_FIELD(out, immlo);
    ADD_FIELD(out, immhi);
    ADD_FIELD(out, Rd);

    if(op == 0){
        instr_s = "adr";

        imm = (immhi << 2) | immlo;
        imm = sign_extend((unsigned long)imm, 21);
        imm += i->PC;

        SET_INSTR_ID(out, AD_INSTR_ADR);
    }
    else{
        instr_s = "adrp";

        imm = ((immhi << 2) | immlo) << 12;
        imm = sign_extend((unsigned long)imm, 32);
        imm += (i->PC & ~0xfff);

        SET_INSTR_ID(out, AD_INSTR_ADRP);
    }

    ADD_REG_OPERAND(out, Rd, _SZ(_64_BIT), NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(AD_RTBL_GEN_64));
    ADD_IMM_OPERAND(out, AD_IMM_ULONG, /*(unsigned long *)&*/imm);
    {
      const char *Rd_s = GET_GEN_REG(AD_RTBL_GEN_64, Rd, NO_PREFER_ZR);
#ifdef _MSC_VER
      concat(DECODE_STR(out), "%s %s, %I64x", instr_s, Rd_s, imm);
#else
      concat(DECODE_STR(out), "%s %s, %#lx", instr_s, Rd_s, imm);
#endif /* _MSC_VER */
    }
    return 0;
}

static int DisassembleAddSubtractImmediateInstr(struct instruction *i,
        struct ad_insn *out){
    unsigned sf = bits(i->opcode, 31, 31);
    unsigned op = bits(i->opcode, 30, 30);
    unsigned S = bits(i->opcode, 29, 29);
    unsigned sh = bits(i->opcode, 22, 22);
    unsigned imm12 = bits(i->opcode, 10, 21);
    unsigned Rn = bits(i->opcode, 5, 9);
    unsigned Rd = bits(i->opcode, 0, 4);

    const char *const *registers = AD_RTBL_GEN_32;
    size_t len = AD_RTBL_GEN_32_SZ;

    if(sf == 1){
        registers = AD_RTBL_GEN_64;
        len = AD_RTBL_GEN_64_SZ;
    }

    if(Rn > len || Rd > len)
        return 1;

    ADD_FIELD(out, sf);
    ADD_FIELD(out, op);
    ADD_FIELD(out, S);
    ADD_FIELD(out, sh);
    ADD_FIELD(out, imm12);
    ADD_FIELD(out, Rn);
    ADD_FIELD(out, Rd);
    {
    unsigned long imm = imm12;

    const char *Rd_s = GET_GEN_REG(registers, Rd, NO_PREFER_ZR);
    const char *Rn_s = GET_GEN_REG(registers, Rn, NO_PREFER_ZR);

    int instr_id = AD_NONE;
    int sz = (registers == AD_RTBL_GEN_64 ? _64_BIT : _32_BIT);
    int shift = sh ? 12 : 0;

    if(S == 0 && op == 0){
        if(sh == 0 && imm12 == 0 && (Rd == 0x1f || Rn == 0x1f)){
            instr_id = AD_INSTR_MOV;

            ADD_REG_OPERAND(out, Rd, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_REG_OPERAND(out, Rn, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));

            concat(DECODE_STR(out), "mov %s, %s", Rd_s, Rn_s);
        }
        else{
            instr_id = AD_INSTR_ADD;

            ADD_REG_OPERAND(out, Rd, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_REG_OPERAND(out, Rn, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, AD_IMM_ULONG, /*(unsigned long *)&*/imm);

            concat(DECODE_STR(out), "add %s, %s, #%#lx", Rd_s, Rn_s, imm);

            if(sh){
                ADD_SHIFT_OPERAND(out, AD_SHIFT_LSL, shift);

                concat(DECODE_STR(out), ", lsl #%d", shift);
            }
        }
    }
    else if(S == 1 && op == 0){
        if(Rd == 0x1f){
            instr_id = AD_INSTR_CMN;

            ADD_REG_OPERAND(out, Rd, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_REG_OPERAND(out, Rn, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, AD_IMM_ULONG, *(unsigned long *)&imm);

            concat(DECODE_STR(out), "cmn %s, #%#lx", Rn_s, imm);

            if(sh){
                ADD_SHIFT_OPERAND(out, AD_SHIFT_LSL, shift);

                concat(DECODE_STR(out), ", lsl #%d", shift);
            }
        }
        else{
            instr_id = AD_INSTR_ADDS;

            ADD_REG_OPERAND(out, Rd, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_REG_OPERAND(out, Rn, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, AD_IMM_ULONG, *(unsigned long *)&imm);

            concat(DECODE_STR(out), "adds %s, %s, #%#lx", Rd_s, Rn_s, imm);

            if(sh){
                ADD_SHIFT_OPERAND(out, AD_SHIFT_LSL, shift);

                concat(DECODE_STR(out), ", lsl #%d", shift);
            }
        }
    }
    else if(S == 0 && op == 1){
        instr_id = AD_INSTR_SUB;

        ADD_REG_OPERAND(out, Rd, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
        ADD_REG_OPERAND(out, Rn, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
        ADD_IMM_OPERAND(out, AD_IMM_ULONG, *(unsigned long *)&imm);

        concat(DECODE_STR(out), "sub %s, %s, #%#lx", Rd_s, Rn_s, imm);

        if(sh){
            ADD_SHIFT_OPERAND(out, AD_SHIFT_LSL, shift);

            concat(DECODE_STR(out), ", lsl #%d", shift);
        }
    }
    else if(S == 1 && op == 1){
        if(Rd == 0x1f){
            instr_id = AD_INSTR_CMP;

            ADD_REG_OPERAND(out, Rn, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, AD_IMM_ULONG, *(unsigned long *)&imm);

            concat(DECODE_STR(out), "cmp %s, #%#lx", Rn_s, imm);

            if(sh){
                ADD_SHIFT_OPERAND(out, AD_SHIFT_LSL, shift);

                concat(DECODE_STR(out), ", lsl #%d", shift);
            }
        }
        else{
            instr_id = AD_INSTR_SUBS;

            ADD_REG_OPERAND(out, Rd, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_REG_OPERAND(out, Rn, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, AD_IMM_ULONG, *(unsigned long *)&imm);

            concat(DECODE_STR(out), "subs %s, %s, #%#lx", Rd_s, Rn_s, imm);

            if(sh){
                ADD_SHIFT_OPERAND(out, AD_SHIFT_LSL, shift);

                concat(DECODE_STR(out), ", lsl #%d", shift);
            }
        }
    }

    SET_INSTR_ID(out, instr_id);
    }
    return 0;
}

static int DisassembleAddSubtractImmediateWithTagsInstr(struct instruction *i,
        struct ad_insn *out){
    unsigned sf = bits(i->opcode, 31, 31);
    unsigned op = bits(i->opcode, 30, 30);
    unsigned S = bits(i->opcode, 29, 29);
    unsigned o2 = bits(i->opcode, 22, 22);
    unsigned uimm6 = bits(i->opcode, 16, 21);
    unsigned op3 = bits(i->opcode, 14, 15);
    unsigned uimm4 = bits(i->opcode, 10, 13);
    unsigned Rn = bits(i->opcode, 5, 9);
    unsigned Rd = bits(i->opcode, 0, 4);
    const char *instr_s = NULL;
    int instr_id = AD_NONE;

    if(sf == 0)
        return 1;

    if(sf == 1 && S == 1)
        return 1;

    if(Rn > AD_RTBL_GEN_64_SZ || Rd > AD_RTBL_GEN_64_SZ)
        return 1;

    ADD_FIELD(out, sf);
    ADD_FIELD(out, op);
    ADD_FIELD(out, S);
    ADD_FIELD(out, o2);
    ADD_FIELD(out, uimm6);
    ADD_FIELD(out, op3);
    ADD_FIELD(out, uimm4);
    ADD_FIELD(out, Rn);
    ADD_FIELD(out, Rd);

    if(op == 0){
        instr_s = "addg";
        instr_id = AD_INSTR_ADDG;
    }
    else{
        instr_s = "subg";
        instr_id = AD_INSTR_SUBG;
    }
    {
    const char *Rn_s = GET_GEN_REG(AD_RTBL_GEN_64, Rn, NO_PREFER_ZR);
    const char *Rd_s = GET_GEN_REG(AD_RTBL_GEN_64, Rd, NO_PREFER_ZR);

    uimm6 <<= 4;

    ADD_REG_OPERAND(out, Rd, _SZ(_64_BIT), NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(AD_RTBL_GEN_64));
    ADD_REG_OPERAND(out, Rn, _SZ(_64_BIT), NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(AD_RTBL_GEN_64));
    ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&uimm6);
    ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&uimm4);

    concat(DECODE_STR(out), "%s %s, %s, #%#x, #%#x", instr_s, Rd_s,
            Rn_s, uimm6, uimm4);

    SET_INSTR_ID(out, instr_id);
    }
    return 0;
}

static int DisassembleLogicalImmediateInstr(struct instruction *i,
        struct ad_insn *out){
    unsigned sf = bits(i->opcode, 31, 31);
    unsigned opc = bits(i->opcode, 29, 30);
    unsigned N = bits(i->opcode, 22, 22);
    unsigned immr = bits(i->opcode, 16, 21);
    unsigned imms = bits(i->opcode, 10, 15);
    unsigned Rn = bits(i->opcode, 5, 9);
    unsigned Rd = bits(i->opcode, 0, 4);
    unsigned long imm = 0;
    const char *const *registers = AD_RTBL_GEN_32;
    size_t len = AD_RTBL_GEN_32_SZ;

    if(sf == 0 && N == 1)
        return 1;

    DecodeBitMasks(N, imms, immr, 1, &imm);

    ADD_FIELD(out, sf);
    ADD_FIELD(out, opc);
    ADD_FIELD(out, N);
    ADD_FIELD(out, immr);
    ADD_FIELD(out, imms);
    ADD_FIELD(out, Rn);
    ADD_FIELD(out, Rd);

    if(sf == 1){
        registers = AD_RTBL_GEN_64;
        len = AD_RTBL_GEN_64_SZ;
    }

    if(Rn > len || Rd > len)
        return 1;
    {
    const char *Rn_s = GET_GEN_REG(registers, Rn, NO_PREFER_ZR);
    const char *Rd_s = GET_GEN_REG(registers, Rd, NO_PREFER_ZR);

    int instr_id = AD_NONE;
    int sz = (registers == AD_RTBL_GEN_64 ? _64_BIT : _32_BIT);
    int imm_type = sz == _64_BIT ? AD_IMM_LONG : AD_IMM_INT;

    if(opc == 0){
        instr_id = AD_INSTR_AND;

        ADD_REG_OPERAND(out, Rd, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
        ADD_REG_OPERAND(out, Rn, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
        ADD_IMM_OPERAND(out, imm_type, imm_type == AD_IMM_LONG ? *(long *)&imm : *(int *)&imm);

        concat(DECODE_STR(out), "and %s, %s", Rd_s, Rn_s);

        if(imm_type == AD_IMM_LONG)
            concat(DECODE_STR(out), ", #"S_LX"", S_LA(imm));
        else
            concat(DECODE_STR(out), ", #"S_X"", S_A(imm));
    }
    else if(opc == 1){
        if(Rn == 0x1f && !MoveWidePreferred(sf, N, imms, immr)){
            instr_id = AD_INSTR_MOV;

            ADD_REG_OPERAND(out, Rd, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, imm_type, imm_type == AD_IMM_LONG ? *(long *)&imm : *(int *)&imm);

            concat(DECODE_STR(out), "mov %s", Rd_s);

            if(imm_type == AD_IMM_LONG)
                concat(DECODE_STR(out), ", #"S_LX"", S_LA(imm));
            else
                concat(DECODE_STR(out), ", #"S_X"", S_A(imm));
        }
        else{
            instr_id = AD_INSTR_ORR;

            ADD_REG_OPERAND(out, Rd, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_REG_OPERAND(out, Rn, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, imm_type, imm_type == AD_IMM_LONG ? *(long *)&imm : *(int *)&imm);

            concat(DECODE_STR(out), "orr %s, %s", Rd_s, Rn_s);

            if(imm_type == AD_IMM_LONG)
                concat(DECODE_STR(out), ", #"S_LX"", S_LA(imm));
            else
                concat(DECODE_STR(out), ", #"S_X"", S_A(imm));
        }
    }
    else if(opc == 2){
        instr_id = AD_INSTR_EOR;

        ADD_REG_OPERAND(out, Rd, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
        ADD_REG_OPERAND(out, Rn, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
        ADD_IMM_OPERAND(out, imm_type, imm_type == AD_IMM_LONG ? *(long *)&imm : *(int *)&imm);

        concat(DECODE_STR(out), "eor %s, %s", Rd_s, Rn_s);

        if(imm_type == AD_IMM_LONG)
            concat(DECODE_STR(out), ", #"S_LX"", S_LA(imm));
        else
            concat(DECODE_STR(out), ", #"S_X"", S_A(imm));
    }
    else if(opc == 3){
        if(Rd == 0x1f){
            instr_id = AD_INSTR_TST;

            ADD_REG_OPERAND(out, Rn, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, imm_type, imm_type == AD_IMM_LONG ? *(long *)&imm : *(int *)&imm);

            concat(DECODE_STR(out), "tst %s", Rn_s);

            if(imm_type == AD_IMM_LONG)
                concat(DECODE_STR(out), ", #"S_LX"", S_LA(imm));
            else
                concat(DECODE_STR(out), ", #"S_X"", S_A(imm));
        }
        else{
            instr_id = AD_INSTR_ANDS;

            ADD_REG_OPERAND(out, Rd, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_REG_OPERAND(out, Rn, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, imm_type, imm_type == AD_IMM_LONG ? *(long *)&imm : *(int *)&imm);

            concat(DECODE_STR(out), "ands %s, %s", Rd_s, Rn_s);

            if(imm_type == AD_IMM_LONG)
                concat(DECODE_STR(out), ", #"S_LX"", S_LA(imm));
            else
                concat(DECODE_STR(out), ", #"S_X"", S_A(imm));
        }
    }

    SET_INSTR_ID(out, instr_id);
    }
    return 0;
}

static int DisassembleMoveWideImmediateInstr(struct instruction *i,
        struct ad_insn *out){
    unsigned sf = bits(i->opcode, 31, 31);
    unsigned opc = bits(i->opcode, 29, 30);
    unsigned hw = bits(i->opcode, 21, 22);
    unsigned imm16 = bits(i->opcode, 5, 20);
    unsigned Rd = bits(i->opcode, 0, 4);
    const char *const *registers = AD_RTBL_GEN_32;
    size_t len = AD_RTBL_GEN_32_SZ;

    if(opc == 1)
        return 1;

    if(sf == 0 && (hw >> 1) == 1)
        return 1;

    if(sf == 1){
        registers = AD_RTBL_GEN_64;
        len = AD_RTBL_GEN_64_SZ;
    }

    if(Rd > len)
        return 1;

    ADD_FIELD(out, sf);
    ADD_FIELD(out, opc);
    ADD_FIELD(out, hw);
    ADD_FIELD(out, imm16);
    ADD_FIELD(out, Rd);
    {
    const char *Rd_s = GET_GEN_REG(registers, Rd, NO_PREFER_ZR);

    int instr_id = AD_NONE;
    int sz = (registers == AD_RTBL_GEN_64 ? _64_BIT : _32_BIT);

    unsigned shift = hw << 4;

    if(opc == 0){
        int alias = !(IsZero(imm16) && hw != 0);

        if(sf == 0)
            alias = (alias && !IsOnes(imm16, 16));

        if(alias){
            int imm_type = sz == _64_BIT ? AD_IMM_LONG : AD_IMM_INT;
            long imm = ~((long)imm16 << shift);

            instr_id = AD_INSTR_MOV;
            ADD_REG_OPERAND(out, Rd, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, imm_type, imm_type == AD_IMM_LONG ? *(long *)&imm : *(int *)&imm);

            concat(DECODE_STR(out), "mov %s", Rd_s);

            if(imm_type == AD_IMM_LONG)
                concat(DECODE_STR(out), ", #"S_LX"", S_LA(imm));
            else
                concat(DECODE_STR(out), ", #"S_X"", S_A(imm));
        }
        else{
            instr_id = AD_INSTR_MOVN;

            ADD_REG_OPERAND(out, Rd, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&imm16);

            concat(DECODE_STR(out), "movn %s, #%#x", Rd_s, imm16);

            if(shift != 0){
                ADD_SHIFT_OPERAND(out, AD_SHIFT_LSL, shift);

                concat(DECODE_STR(out), ", lsl #%d", shift);
            }
        }
    }
    else if(opc == 2){
        if(!(IsZero(imm16) && hw != 0)){
            int imm_type = sz == _64_BIT ? AD_IMM_LONG : AD_IMM_INT;
            long imm = (long)imm16 << shift;

            instr_id = AD_INSTR_MOV;
            ADD_REG_OPERAND(out, Rd, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, imm_type, imm_type == AD_IMM_LONG ? *(long *)&imm : *(int *)&imm);

            concat(DECODE_STR(out), "mov %s", Rd_s);

            if(imm_type == AD_IMM_LONG)
                concat(DECODE_STR(out), ", #"S_LX"", S_LA(imm));
            else
                concat(DECODE_STR(out), ", #"S_X"", S_A(imm));
        }
        else{
            instr_id = AD_INSTR_MOVZ;

            ADD_REG_OPERAND(out, Rd, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, AD_IMM_INT, *(unsigned int *)&imm16);

            concat(DECODE_STR(out), "movz %s, #%#lx", Rd_s, imm16);

            if(shift != 0){
                ADD_SHIFT_OPERAND(out, AD_SHIFT_LSL, shift);

                concat(DECODE_STR(out), ", lsl #%d", shift);
            }
        }
    }
    else if(opc == 3){
        instr_id = AD_INSTR_MOVK;

        ADD_REG_OPERAND(out, Rd, sz, NO_PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
        ADD_IMM_OPERAND(out, AD_IMM_INT, *(unsigned int *)&imm16);

        concat(DECODE_STR(out), "movk %s, #%#lx", Rd_s, imm16);

        if(shift != 0){
            ADD_SHIFT_OPERAND(out, AD_SHIFT_LSL, shift);

            concat(DECODE_STR(out), ", lsl #%d", shift);
        }
    }

    SET_INSTR_ID(out, instr_id);
    }
    return 0;
}

static int DisassembleBitfieldInstr(struct instruction *i,
        struct ad_insn *out){
    unsigned sf = bits(i->opcode, 31, 31);
    unsigned opc = bits(i->opcode, 29, 30);
    unsigned N = bits(i->opcode, 22, 22);
    unsigned immr = bits(i->opcode, 16, 21);
    unsigned imms = bits(i->opcode, 10, 15);
    unsigned Rn = bits(i->opcode, 5, 9);
    unsigned Rd = bits(i->opcode, 0, 4);
    const char *const *registers = AD_RTBL_GEN_32;
    size_t len = AD_RTBL_GEN_32_SZ;

    if(opc == 3)
        return 1;

    if(sf == 0 && N == 1)
        return 1;

    if(sf == 1 && N == 0)
        return 1;

    if(sf == 1){
        registers = AD_RTBL_GEN_64;
        len = AD_RTBL_GEN_64_SZ;
    }

    if(Rn > len || Rd > len)
        return 1;

    ADD_FIELD(out, sf);
    ADD_FIELD(out, opc);
    ADD_FIELD(out, N);
    ADD_FIELD(out, immr);
    ADD_FIELD(out, imms);
    ADD_FIELD(out, Rn);
    ADD_FIELD(out, Rd);
    {
    const char *Rn_s = GET_GEN_REG(registers, Rn, PREFER_ZR);
    const char *Rd_s = GET_GEN_REG(registers, Rd, PREFER_ZR);

    int instr_id = AD_NONE;
    int sz = (registers == AD_RTBL_GEN_64 ? _64_BIT : _32_BIT);

    if(opc == 0){
        if((sf == 1 && imms == 0x3f) || (sf == 0 && imms == 0x1f)){
            instr_id = AD_INSTR_ASR;

            ADD_REG_OPERAND(out, Rd, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_REG_OPERAND(out, Rn, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&immr);

            concat(DECODE_STR(out), "asr %s, %s, #%#x", Rd_s, Rn_s, immr);
        }
        else if(imms < immr){
            unsigned lsb = (sz - immr) & (sz - 1);
            unsigned width = imms + 1;

            instr_id = AD_INSTR_SBFIZ;
            ADD_REG_OPERAND(out, Rd, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_REG_OPERAND(out, Rn, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&lsb);
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&width);

            concat(DECODE_STR(out), "sbfiz %s, %s, #%#x, #%#x", Rd_s, Rn_s, lsb, width);
        }
        else if(BFXPreferred(sf, (opc >> 1), imms, immr)){
            unsigned lsb = immr;
            unsigned width = imms + 1 - lsb;

            instr_id = AD_INSTR_SBFX;
            ADD_REG_OPERAND(out, Rd, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_REG_OPERAND(out, Rn, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&lsb);
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&width);

            concat(DECODE_STR(out), "sbfx %s, %s, #%#x, #%#x", Rd_s, Rn_s, lsb, width);
        }
        else if(immr == 0){
            const char *instr_s = "sxtb";
            instr_id = AD_INSTR_SXTB;
            if(imms == 0xf){
                instr_id = AD_INSTR_SXTH;
                instr_s = "sxth";
            }
            else if(imms == 0x1f){
                instr_id = AD_INSTR_SXTW;
                instr_s = "sxtw";
            }

            ADD_REG_OPERAND(out, Rd, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_REG_OPERAND(out, Rn, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));

            concat(DECODE_STR(out), "%s %s, %s", instr_s, Rd_s, Rn_s);
        }
        else{
            instr_id = AD_INSTR_SBFM;

            ADD_REG_OPERAND(out, Rd, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_REG_OPERAND(out, Rn, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&immr);
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&imms);

            concat(DECODE_STR(out), "sbfm %s, %s, #%#x, #%#x", Rd_s, Rn_s, immr, imms);
        }
    }
    else if(opc == 1){
        if(imms < immr){
            unsigned lsb = (sz - immr) & (sz - 1);
            unsigned width = imms + 1;
            const char *instr_s = "bfc";

            instr_id = AD_INSTR_BFC;
            if(Rn != 0x1f){
                instr_id = AD_INSTR_BFI;
                instr_s = "bfi";
            }

            ADD_REG_OPERAND(out, Rd, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));

            /* in this case, BFC will always have the zero register as Rn, so we omit it */
            if(instr_id == AD_INSTR_BFI)
                ADD_REG_OPERAND(out, Rn, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));

            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&lsb);
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&width);

            concat(DECODE_STR(out), "%s %s,", instr_s, Rd_s);

            if(instr_id == AD_INSTR_BFI)
                concat(DECODE_STR(out), " %s,", Rn_s);

            concat(DECODE_STR(out), " #%#x, #%#x", lsb, width);
        }
        else if(imms >= immr){
            unsigned lsb = immr;
            unsigned width = imms + 1 - lsb;

            instr_id = AD_INSTR_BFXIL;
            ADD_REG_OPERAND(out, Rd, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_REG_OPERAND(out, Rn, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&lsb);
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&width);

            concat(DECODE_STR(out), "bfxil %s, %s, #%#x, #%#x", Rd_s, Rn_s, lsb, width);
        }
        else{
            instr_id = AD_INSTR_BFM;

            ADD_REG_OPERAND(out, Rd, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_REG_OPERAND(out, Rn, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&immr);
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&imms);

            concat(DECODE_STR(out), "bfm %s, %s, #%#x, #%#x", Rd_s, Rn_s, immr, imms);
        }
    }
    else if(opc == 2){
        if(imms + 1 == immr){
            if((sf == 0 && imms != 0x1f) || (sf == 1 && imms != 0x3f)){
                unsigned shift = (sz - 1) - imms;

                instr_id = AD_INSTR_LSL;
                ADD_REG_OPERAND(out, Rd, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
                ADD_REG_OPERAND(out, Rn, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
                ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&shift);

                concat(DECODE_STR(out), "lsl %s, %s, #%#x", Rd_s, Rn_s, shift);
            }
            else{
                return 1;
            }
        }
        else if((sf == 0 && imms == 0x1f) || (sf == 1 && imms == 0x3f)){
            instr_id = AD_INSTR_LSR;

            ADD_REG_OPERAND(out, Rd, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_REG_OPERAND(out, Rn, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&immr);

            concat(DECODE_STR(out), "lsr %s, %s, #%#x", Rd_s, Rn_s, immr);
        }
        else if(imms < immr){
            unsigned lsb = sz - immr & (sz - 1);
            unsigned width = imms + 1;

            instr_id = AD_INSTR_UBFIZ;

            ADD_REG_OPERAND(out, Rd, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_REG_OPERAND(out, Rn, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&lsb);
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&width);

            concat(DECODE_STR(out), "ubfiz %s, %s, #%#x, #%#x", Rd_s, Rn_s, lsb, width);
        }
        else if(BFXPreferred(sf, (opc >> 1), imms, immr)){
            unsigned lsb = immr;
            unsigned width = imms + 1 - lsb;

            instr_id = AD_INSTR_UBFX;
            ADD_REG_OPERAND(out, Rd, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_REG_OPERAND(out, Rn, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&lsb);
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&width);

            concat(DECODE_STR(out), "ubfx %s, %s, #%#x, #%#x", Rd_s, Rn_s, lsb, width);
        }
        else if(immr == 0){
            const char *instr_s = "uxtb";
            instr_id = AD_INSTR_UXTB;
            if(imms == 0xf){
                instr_id = AD_INSTR_UXTH;
                instr_s = "uxth";
            }

            ADD_REG_OPERAND(out, Rd, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_REG_OPERAND(out, Rn, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));

            concat(DECODE_STR(out), "%s %s, %s", instr_s, Rd_s, Rn_s);
        }
        else{
            instr_id = AD_INSTR_UBFM;

            ADD_REG_OPERAND(out, Rd, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_REG_OPERAND(out, Rn, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&immr);
            ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&imms);

            concat(DECODE_STR(out), "ubfm %s, %s, #%#x, #%#x", Rd_s, Rn_s, immr, imms);
        }
    }

    SET_INSTR_ID(out, instr_id);
    }
    return 0;
}

static int DisassembleExtractInstr(struct instruction *i,
        struct ad_insn *out){
    unsigned sf = bits(i->opcode, 31, 31);
    unsigned op21 = bits(i->opcode, 29, 30);
    unsigned N = bits(i->opcode, 22, 22);
    unsigned o0 = bits(i->opcode, 21, 21);
    unsigned Rm = bits(i->opcode, 16, 20);
    unsigned imms = bits(i->opcode, 10, 15);
    unsigned Rn = bits(i->opcode, 5, 9);
    unsigned Rd = bits(i->opcode, 0, 4);
    const char *const *registers = AD_RTBL_GEN_32;
    size_t len = AD_RTBL_GEN_32_SZ;

    if((op21 << 1) == 1 || (op21 >> 1) == 1)
        return 1;

    if(op21 == 0 && o0 == 1)
        return 1;

    if(sf == 0 && ((imms >> 5) == 1 || N == 1))
        return 1;

    if(sf == 1 && N == 0)
        return 1;

    if(sf == 1){
        registers = AD_RTBL_GEN_64;
        len = AD_RTBL_GEN_64_SZ;
    }

    if(Rm > len || Rn > len || Rd > len)
        return 1;

    ADD_FIELD(out, sf);
    ADD_FIELD(out, op21);
    ADD_FIELD(out, N);
    ADD_FIELD(out, o0);
    ADD_FIELD(out, Rm);
    ADD_FIELD(out, imms);
    ADD_FIELD(out, Rn);
    ADD_FIELD(out, Rd);
    {
    const char *Rm_s = GET_GEN_REG(registers, Rm, PREFER_ZR);
    const char *Rn_s = GET_GEN_REG(registers, Rn, PREFER_ZR);
    const char *Rd_s = GET_GEN_REG(registers, Rd, PREFER_ZR);

    int instr_id = AD_INSTR_EXTR;
    int sz = (registers == AD_RTBL_GEN_64 ? _64_BIT : _32_BIT);

    const char *instr_s = "extr";

    if(Rn == Rm){
        instr_id = AD_INSTR_ROR;
        instr_s = "ror";
    }

    ADD_REG_OPERAND(out, Rd, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
    ADD_REG_OPERAND(out, Rn, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));
    
    if(Rd != Rm)
        ADD_REG_OPERAND(out, Rm, sz, PREFER_ZR, _SYSREG(AD_NONE), _RTBL(registers));

    ADD_IMM_OPERAND(out, AD_IMM_UINT, *(unsigned int *)&imms);

    concat(DECODE_STR(out), "%s %s, %s", instr_s, Rd_s, Rn_s);

    if(instr_id == AD_INSTR_EXTR)
        concat(DECODE_STR(out), ", %s", Rm_s);

    concat(DECODE_STR(out), ", #%#x", imms);

    SET_INSTR_ID(out, instr_id);
    }
    return 0;
}

int DataProcessingImmediateDisassemble(struct instruction *i,
        struct ad_insn *out){
    unsigned op0 = bits(i->opcode, 23, 25);

    int result = 0;

    if((op0 >> 1) == 0)
        result = DisassemblePCRelativeAddressingInstr(i, out);
    else if(op0 == 2)
        result = DisassembleAddSubtractImmediateInstr(i, out);
    else if(op0 == 3)
        result = DisassembleAddSubtractImmediateWithTagsInstr(i, out);
    else if(op0 == 4)
        result = DisassembleLogicalImmediateInstr(i, out);
    else if(op0 == 5)
        result = DisassembleMoveWideImmediateInstr(i, out);
    else if(op0 == 6)
        result = DisassembleBitfieldInstr(i, out);
    else if(op0 == 7)
        result = DisassembleExtractInstr(i, out);
    else
        result = 1;

    return result;
}
