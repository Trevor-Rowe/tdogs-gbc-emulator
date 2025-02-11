opcode_table = 
{
    [NOP]      = /* no_op()   , */ // 0x00 (Length: 1, Cycles: 1)
    [LD_BC_NN] = /* ld_bc_nn(), */ // 0x01 (Length: 3, Cycles: 3)
    [LD_BC_A]  = /* ld_bc()   , */ // 0x02 (Length: 1, Cycles: 2)
    // Cotinue this pattern for ALL 256 opcodes.
};