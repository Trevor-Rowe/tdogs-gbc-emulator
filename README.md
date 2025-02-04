# tdogs-gbc-emulator
**Purpose**
Want to build an environemnt for Neural Networks to interact with my goal to improve in the following areas:
  * Getting further in touch with my hardware roots:
    * This is a great project to better master C, developing robust software from the ground up.
  * Specializing hardware for AI/ML training and development.
  * Data Science Skillet: Model architecture, hypothesis testing, data aquisition, and deployment.
  * Documentation of my thoughts and design choices through the development process.

**Research**
Before ANY code was written ~40 hours of research and note taking were completed to familiarize myself with
the GBC's architeture. Here are some of the critical resources:
  * https://gbdev.io/pandocs/                -> Holy Grail with comprehensive information about the GBC and its ROMs.
  * https://gbdev.io/gb-opcodes/optables/    -> CPU Opcode table, well organized documentation of SM83 instruction set.
  * https://youtu.be/e87qKixKFME?list=PLVxiWMqQvhg_yk4qy2cSC3457wZJga_e5 -> Part 1 of his series helped me get started!
  * ChatGPT -> Basically used as a search engine for the pandocs, to more closely examine and interpret information.

**Roadmap (High-Level)**
  1. Encapsulate meta-data and ROM Cartridge behavior. -CURRENT STEP-
  2. Program CPU registers, clock, stack, and OPCode behavior.
  3. Ensure correct memory mappings.
  4. Audio.
  5. Video.
  6. I/0 and interrupts.

**Project Structure**
  roms    -> contains ROM files to be read in and executed by the system.
  lib     -> external resources.
  src     -> .c files.
  include -> .h files.
  test    -> CUnit testing. 

**A Word About LLM Usage**
Generative LLM models like ChatGPT are great for expediting research and development when used cautiously. It should
almost never be the sole source of truth. As such, I prefer to use ChatGPT for tasks such as:
  * Answering basic syntax questions for the c programming language.
  * Boiler plate code whose functionality can be easily confirmed and tested.
    * EXAMPLE: get_publisher_name() in cartridge.c, it is just a switch statement table derived from https://gbdev.io/pandocs/The_Cartridge_Header.html
  
I am not against generated code but a developer should be responsible for verfication and testing.

I'll be posting progress on LinkedIn and maybe YouTube depending on time.
  
