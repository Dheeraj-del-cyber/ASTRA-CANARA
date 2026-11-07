# AI Prompts & Interaction Log

Chronological record of AI prompts used during project development for the 24-hour engineering hackathon.

---

### Prompt 1
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 14:24 IST
- **Prompt:**
```text
You are working on a 24-hour engineering hackathon project.

PROJECT:
Build a single-process embedded key-value database from scratch using C++17.

The database must store data in a single file and support:
- PUT
- GET
- DELETE
- BATCH
- OPEN
- CLOSE

Important requirements:
- Support at least 1 million keys.
- Support batches containing up to 10,000 operations.
- Recover in under 2 seconds.
- Support concurrent readers and writers.
- Guarantee atomic batches.
- Guarantee durability for every operation acknowledged as successful.
- Recover safely after unexpected process termination.
- Implement compaction.
- Support configurable maximum file size.
- Handle disk-full conditions safely.
- Provide a terminal-based CLI.
- Provide a torture-testing tool.

STRICT RESTRICTIONS:
- Do not use SQLite, RocksDB, LevelDB, Redis or any existing database engine.
- Do not use existing WAL, B-tree or LSM storage libraries.
- Implement the storage engine ourselves.
- Use C++17 and standard libraries wherever possible.

YOUR FIRST TASK IS REPOSITORY SETUP AND UNDERSTANDING.

Follow these steps:

1. Inspect the complete GitHub repository.
2. Read README.md and CONTRACT.md.
3. Understand the problem statement and all requirements.
4. Inspect the existing directory structure and available source files.
5. Identify the build system and check whether the project compiles.
6. Understand the proposed architecture and module boundaries.
7. Identify dependencies between your assigned module and other modules.
8. Identify any unclear or conflicting decisions in CONTRACT.md.

IMPORTANT:
- Do not start implementing the database yet.
- Do not modify another member's assigned module.
- Do not invent a different binary file format.
- Do not silently change shared interfaces.
- Do not overwrite existing files without understanding them.
- Do not introduce unnecessary dependencies.

FIRST RESPONSE:
Give me:
1. Your understanding of the project.
2. Your understanding of the current repository.
3. Your proposed implementation plan.
4. Files you expect to create or modify.
5. Interfaces you need from other modules.
6. Potential technical risks.
7. Questions that need team-level decisions.

Wait for the shared architecture and interfaces to be agreed upon before implementing.
```

---

### Prompt 2
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 14:35 IST
- **Prompt:**
```text
Before assigning individual implementation roles, establish the foundational project directory structure, CMake build configuration, and shared C++17 header contracts so all four team members can work simultaneously without interface conflicts.
```

---

### Prompt 3 (Storage Engine)
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 15:02 IST
- **Prompt:**
```text
You are responsible for the STORAGE ENGINE of our C++17 embedded key-value database.

YOUR RESPONSIBILITIES:

1. Design and implement the core storage engine.
2. Implement PUT, GET and DELETE operations.
3. Implement binary serialization and deserialization.
4. Maintain an in-memory index using suitable C++ data structures.
5. Support at least 1 million keys.
6. Handle large keys and values safely.
7. Implement efficient file read and write operations.
8. Provide clean interfaces for the durability and recovery modules.

BEFORE CODING:

- Read CONTRACT.md.
- Understand the agreed binary file format.
- Coordinate with Member 2 about transaction records and checksums.
- Coordinate with Member 3 about index synchronization.
- Do not independently change the file format.

EXPECTED OUTPUT:

- Storage engine header and implementation files.
- Data structures for indexing.
- Serialization/deserialization logic.
- Unit tests for PUT, GET and DELETE.
- Tests for large values and 1 million keys.
- Documentation explaining your implementation.

CODE QUALITY:

- Use clean C++17.
- Avoid unnecessary memory copies.
- Handle invalid input and file errors.
- Keep functions modular.
- Add meaningful comments around critical storage logic.

Work only on your assigned branch.

Do not claim an operation is complete until its required durability guarantees are implemented through the agreed interface.
```

---

### Prompt 4 (Team Lead & Integration)
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 15:06 IST
- **Prompt:**
```text
Assume the role of Team Lead and Integration Owner for our C++17 embedded key-value database hackathon project. I am responsible for coordinating module integration, the terminal CLI, and the independent torture-testing tool.

YOUR RESPONSIBILITIES:

1. Maintain the overall project architecture.
2. Coordinate the three other developers.
3. Establish and maintain shared interfaces.
4. Review pull requests before merging.
5. Integrate all modules into the main branch.
6. Implement or coordinate the terminal CLI.
7. Implement or coordinate the independent torture-testing tool.
8. Maintain project documentation.
9. Run integration and regression tests.
10. Prepare the final demonstration.

CLI REQUIREMENTS:

Support commands:
- kvdb open <path> [--max-bytes N]
- kvdb put <path> <key> <value>
- kvdb get <path> <key>
- kvdb del <path> <key>
- kvdb batch <path> <file>
- kvdb dump <path>
- kvdb check <path>
- kvdb serve <path>

TORTURE TESTING REQUIREMENTS:

Build a testing tool that:
- Generates reproducible random database workloads.
- Executes concurrent multi-threaded writers under heavy lock contention.
- Independently records acknowledged operations in an external journal.
- Randomly terminates the database process via ungraceful signals (SIGKILL / TerminateProcess).
- Restarts the database and verifies recovered data against the independent ACK oracle.
- Enforces all-or-nothing batch atomicity and verifies absence of unacknowledged data.
- Supports reproducible seeds and long-running stress runs.

GITHUB RULES:
- main is the integration branch.
- Each developer works on their assigned feature branch.
- Every change must come through a pull request.
- Review the implementation and tests before merging.
- Never merge code that breaks the build.
- Avoid merging conflicting changes to shared interfaces.
```

---

### Prompt 5
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 16:46 IST
- **Prompt:**
```text
Continue the implementation according to the approved architecture and module boundaries.
```

---

### Prompt 6
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 16:47 IST
- **Prompt:**
```text
Outline the phased development roadmap for the terminal CLI and independent torture-testing tool. Detail the milestones for implementing command parsing, oracle persistence, concurrent load testing, and crash-recovery verification.
```

---

### Prompt 7
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 16:50 IST
- **Prompt:**
```text
Continue with the execution of the planned phase.
```

---

### Prompt 8
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 16:52 IST
- **Prompt:**
```text
Proceed with the development of Phase 1: Implement the core CLI command structure and the initial torture-testing scaffold.
```

---

### Prompt 9
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 17:13 IST
- **Prompt:**
```text
Continue implementing Phase 1 components and tests.
```

---

### Prompt 10
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 17:14 IST
- **Prompt:**
```text
Implement Phase 2: Develop the independent persistent ACK oracle and sequential workload generator for torture testing.
```

---

### Prompt 11
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 17:27 IST
- **Prompt:**
```text
Continue implementation and validation of Phase 2.
```

---

### Prompt 12
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 17:28 IST
- **Prompt:**
```text
Create and submit a pull request for the completed CLI and torture-testing Phase 2 changes.
```

---

### Prompt 13
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 17:41 IST
- **Prompt:**
```text
Proceed with Phase 3: Implement the remaining CLI subcommands (dump with lexicographical sorting, check, and serve with interactive ACK protocol) and multi-threaded stress testing.
```

---

### Prompt 14
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 18:30 IST
- **Prompt:**
```text
We received feedback from our hackathon mentors.

They appreciated our choice of C++ and recommended that we pay special attention to database metadata and backend architecture.

We have already started implementation. Do not blindly rewrite existing code.

Your new task is to REVIEW AND VALIDATE THE COMPLETE BACKEND ARCHITECTURE.

STEP 1: Inspect the existing repository.
STEP 2: Understand the complete data flow.
STEP 3: METADATA DESIGN
STEP 4: BINARY FORMAT REVIEW
STEP 5: TRANSACTION AND DURABILITY REVIEW
STEP 6: RECOVERY REVIEW
STEP 7: INTEGRATION REVIEW

IMPORTANT RESTRICTIONS:
- Do not independently modify the shared binary format.
- Do not introduce a second transaction protocol.
- Do not rewrite other members' code.
- Do not delete existing working functionality.
- Do not claim crash safety without evidence.
```

---

### Prompt 15
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 22:04 IST
- **Prompt:**
```text
Commit all current changes and push the integration branch to the remote repository.
```

---

### Prompt 16
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 22:30 IST
- **Prompt:**
```text
Review the merged branches across the codebase to ensure there are no remaining merge conflicts or regressions, and provide instructions on building and running the full test suite.
```

---

### Prompt 17
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 22:44 IST
- **Prompt:**
```text
Consolidate all changes from the feature/crash-recovery branch into the main branch, delete the feature branch, and maintain a single unified branch policy.
```

---

### Prompt 18
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 23:10 IST
- **Prompt:**
```text
Generate a high-resolution system architecture diagram illustrating the CLI, Database facade, in-memory index, lock manager, WAL recovery engine, and native file storage layout.
```

---

### Prompt 19
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-07 05:55 IST
- **Prompt:**
```text
Inspect and resolve merge conflicts in Pull Request #8 submitted by my teammate (Phase 1 Log Scanner), verify that all test suites pass, and complete the merge into main.
```

---

### Prompt 20
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-07 06:17 IST
- **Prompt:**
```text
Inspect and resolve merge conflicts in Pull Request #9, integrate the remaining updates into main, and verify clean compilation across all build targets.
```

---

### Prompt 21
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-07 06:29 IST
- **Prompt:**
```text
Audit the codebase and commit history to verify how each team member's merged contributions (CLI, recovery scanner, compactor, storage engine, and torture harness) are integrated and actively utilized in the core database library and test suites.
```

---

### Prompt 22
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-07 06:42 IST
- **Prompt:**
```text
Clean up all temporary runtime artifacts, test databases, and intermediate logs from the repository root, verify the working tree, and push the clean state to origin/main.
```

---

### Prompt 23
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-07 06:52 IST
- **Prompt:**
```text
Verify whether the storage engine utilizes third-party serialization libraries such as Protocol Buffers or MessagePack, or adheres to a custom zero-dependency binary implementation.
```

---

### Prompt 24
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-07 06:53 IST
- **Prompt:**
```text
Explain the database's garbage collection and memory management mechanisms, detailing on-disk log compaction and RAM management for 1,000,000 keys.
```

---

### Prompt 25
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-07 07:05 IST
- **Prompt:**
```text
Compare our torture-testing harness against the common criteria for weak test tools (lack of mid-operation kills, graceful vs ungraceful termination, lack of independent oracles, and absence of batch/concurrency testing) to verify resilience.
```

---

### Prompt 26
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-07 08:48 IST
- **Prompt:**
```text
Resolve merge conflicts in Pull Request #11 (feat(recovery): Phase 2 - harden durable WAL commits and sync) from teammate dhanushpoojary078-svg, reconcile Win32 thread-safe file operations with failure injection test hooks, verify all test suites pass, and complete the merge into main.
```

---

### Prompt 27
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-07 09:33 IST
- **Prompt:**
```text
Update README.md comprehensively to production quality, emphasizing the system architecture, component data flow, Bitcask indexing design, zero-dependency binary formats, exact CLI command grammar, independent torture harness usage, and build/verification instructions.
```

---

### Prompt 28
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-07 10:07 IST
- **Prompt:**
```text
Review newly opened pull requests on the repository (PR #14: atomic recovery hardening and PR #15: subprocess and child process hardening), resolve merge conflicts and cleanse test batch artifacts from the commit history, verify the full test suite and build targets, and complete the merge into the main branch.
```

---

### Prompt 29
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-07 11:02 IST
- **Prompt:**
```text
Verify and update ARCHITECTURE.md and BREAKING.md to incorporate the complete 32 failure and crash scenarios specified across pages 5, 6, 7, and 8 of the hackathon problem statement (Groups A through F), ensuring exact disk byte states, code function references, and verification tooling match the judging rubric.
```


