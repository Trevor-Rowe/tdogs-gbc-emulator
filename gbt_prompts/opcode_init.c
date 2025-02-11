/* ASSUME THAT FUNCTION POINTERS HAVE BEEN DECLARED*/
typedef void (*OpcodeHandler)();
OpcodeHandler prefix_opcode_table[256];
OpcodeHandler opcode_table[256] = 
{
    /*              ROW 1               */
    /* 0x00 */ [NOP]       = no_op,   
    /* 0x01 */ [LD_BC_NN]  = ld_bc_nn,
    /* 0x02 */ [LD_BC_A]   = ld_bc_mem_a,
    /* 0x03 */ [INC_BC]    = inc_bc,
    /* 0x04 */ [INC_B]     = inc_b,
    /* 0x05 */ [DEC_B]     = dec_b,
    /* 0x06 */ [LD_B_N]    = ld_b_n,
    /* 0x07 */ [RLCA]      = rlca,
    /* 0x08 */ [LD_NN_SP]  = ld_nn_mem_sp,
    /* 0x09 */ [ADD_HL_BC] = add_hl_bc,
    /* 0x0A */ [LD_A_BC]   = ld_a_bc_mem,
    /* 0x0B */ [DEC_BC]    = dec_bc,
    /* 0x0C */ [INC_C]     = inc_c,
    /* 0x0D */ [DEC_C]     = dec_c,
    /* 0x0E */ [LD_C_N]    = ld_c_n,
    /* 0x0F */ [RRCA]      = rrca,
    /*              ROW 2                */
    /* 0x10 */ [STOP]      = no_op,   
    /* 0x11 */ [LD_DE_NN]  = ld_de_nn,
    /* 0x12 */ [LD_DE_A]   = ld_de_mem_a,
    /* 0x13 */ [INC_DE]    = inc_de,
    /* 0x14 */ [INC_D]     = inc_d,
    /* 0x15 */ [DEC_D]     = dec_d,
    /* 0x16 */ [LD_D_N]    = ld_d_n,
    /* 0x17 */ [RLA]       = rla,
    /* 0x18 */ [JR_N]      = jr_n,
    /* 0x19 */ [ADD_HL_DE] = add_hl_de,
    /* 0x1A */ [LD_A_DE]   = ld_a_de_mem,
    /* 0x1B */ [DEC_DE]    = dec_de,
    /* 0x1C */ [INC_E]     = inc_e,
    /* 0x1D */ [DEC_E]     = dec_e,
    /* 0x1E */ [LD_E_N]    = ld_e_n,
    /* 0x1F */ [RRA]       = rra,
    /*              ROW 3                */
    /* 0x20 */ [JR_NZ_N]   = jr_nz_n,   
    /* 0x21 */ [LD_HL_NN]  = ld_hl_nn,
    /* 0x22 */ [LD_HLI_A]  = ld_hli_a,
    /* 0x23 */ [INC_HL]    = inc_hl,
    /* 0x24 */ [INC_H]     = inc_h,
    /* 0x25 */ [DEC_H]     = dec_h,
    /* 0x26 */ [LD_H_N]    = ld_h_n,
    /* 0x27 */ [DAA]       = daa,
    /* 0x28 */ [JR_Z_N]    = jr_z_n,
    /* 0x29 */ [ADD_HL_HL] = add_hl_hl,
    /* 0x2A */ [LD_A_HLI]  = ld_a_hli,
    /* 0x2B */ [DEC_HL]    = dec_hl,
    /* 0x2C */ [INC_L]     = inc_e,
    /* 0x2D */ [DEC_L]     = dec_l,
    /* 0x2E */ [LD_L_N]    = ld_l_n,
    /* 0x2F */ [CPL]       = cpl,
};
    /* 0xC0 */ []   = ,   
    /* 0xC1 */ []  = ,
    /* 0xC2 */ []  = ,
    /* 0xC3 */ []    = ,
    /* 0xC4 */ []     = ,
    /* 0xC5 */ []     = ,
    /* 0xC6 */ []    = ,
    /* 0xC7 */ []       = ,
    /* 0xC8 */ []    = ,
    /* 0xC9 */ [] = ,
    /* 0xCA */ []  = ,
    /* 0xCB */ []    = ,
    /* 0xCC */ []     = ,
    /* 0xCD */ []     = ,
    /* 0xCE */ []    = ,
    /* 0xCF */ []       = ,
    /*              ROW 14                */