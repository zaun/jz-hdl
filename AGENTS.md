# Planning
- For tasks that touch more than 3 files, create a plan and get approval before writing code
- Break large tasks into steps and track progress
- Always use the Plan tool when making plans

# Decision Making
- Always fix compiler bugs, never work around them
- Don't guess, ask when uncertain
- Present options instead of choosing
- Explain what you're about to do before doing it
- When told something is wrong, stop and review all changes made
    - explain what is correct and what is not
    - ask how to proceed, do not immediately revert

# Communication
- Be concise — no filler, no restating what I said
- When I ask a question, answer it and stop
- Don't apologize, just fix it
- Don't explain obvious things
- Don't use platitudes
- Greet each new context with "Hello, I've read CLAUDE.md and am ready to procede."
  *Explain all the rules are must follow*

# Tools: These are **HARD CONSTRAINTS**
- Always use the Write tool to create files — Never use bash with cat, echo, or heredocs
- Always use the Edit tool to modify files — Never use sed or awk via bash
- Always use the Read tool to read files — Never use cat, head, or tail via bash
- Always use the Grep tool to search file contents — Never use grep or rg via bash
- Always use the Glob tool to find files — Never use find or ls via bash
- Always use the Plan tool to make plans - Never use a plain message
- Avoid using $() with the Bash tool
- This applies to ALL paths including /tmp

# Safety
- Never delete files without asking
- Never force-push
- Don't overwrite uncommitted work

# Information
- specification/chip-info-specification.md : Chip data file format (compiler/data/*.jzon)
- specification/jz-hdl-specification.md : The JZ-HDL specification
- specification/jzw.md : The jzw file format specification
- specification/simulation-specification.md : The JZ-HDL simulation specification
- specification/testbench-specification.md : The JZ-HDL testbench specification
- datasheets/ : Manufacturer fpga datasheets
