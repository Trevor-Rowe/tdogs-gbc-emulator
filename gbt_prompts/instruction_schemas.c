{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        cpu_log(DEBUG, "");
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}

/* 3 CYCLE SCHEMA */

{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        cpu_log(DEBUG, "");
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        cpu_log(DEBUG, "");
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}

/* 4 CYCLE SCHEMA */

{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        cpu_log(DEBUG, "");
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        cpu_log(DEBUG, "");
        return false;
    }

    if (ins->duration == 4) // Fourth Cycle
    {
        cpu_log(DEBUG, "");
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}

/* 5 CYCLE SCHEMA */

{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        cpu_log(DEBUG, "");
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        cpu_log(DEBUG, "");
        return false;
    }

    if (ins->duration == 4) // Fourth Cycle
    {
        cpu_log(DEBUG, "");
        return false;
    }

    if (ins->duration == 5) // Fifth Cycle
    {
        cpu_log(DEBUG, "");
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}

/* 6 CYCLE SCHEMA */

{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        cpu_log(DEBUG, "");
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        cpu_log(DEBUG, "");
        return false;
    }

    if (ins->duration == 4) // Fourth Cycle
    {
        cpu_log(DEBUG, "");
        return false;
    }

    if (ins->duration == 5) // Fifth Cycle
    {
        cpu_log(DEBUG, "");
        return false;
    }

    if (ins->duration == 6) // Sixth Cycle
    {
        cpu_log(DEBUG, "");
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}