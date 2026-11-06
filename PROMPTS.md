# AI Prompts & Interaction Log

Chronological, unedited record of AI prompts used during project development.

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

Wait for the shared architecture and interfaces to be agreed upon before implementing. and i will also send the prompt of my role after this
```

---

### Prompt 2
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 14:35 IST
- **Prompt:**
```text
before y role we will do the strucure that my every team members will work on the same we are four in team of each of us have our own roles so we will do the structre first
```

---

### Prompt 3
- **Tool:** Antigravity IDE (Gemini 3.8 Flash)
- **Approximate Time:** 2026-11-06 15:11 IST
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


