# Breaking Points, Failure Modes & Engineering Trade-Offs

## 1. Identified Crash Points & Failure Modes

1. **Abrupt Process Termination (SIGKILL / Power Loss):**
   - *Risk:* Writes in flight may result in torn frames at the physical end of the file.
   * *Mitigation:* Tail truncation up to the last known durable `COMMIT` frame.
2. **Crash During Compaction File Swap:**
   - *Risk:* Crash while replacing the primary database file with the compacted copy.
   * *Mitigation:* Temporary file written and fully synced (`FlushFileBuffers`/`fsync`) before any rename. Atomic replacement semantics (`MoveFileEx` / `rename`) ensure zero window where no valid file exists.
3. **Mid-Log Corruption vs. Torn Tail:**
   - *Risk:* Mistaking historical bit rot for a crash tail.
   * *Mitigation:* Only frames past the last valid commit at EOF are eligible for tail truncation. Checksum failures before EOF trigger immediate `Status::Corruption`.

## 2. Weaknesses & Trade-Offs

1. **Write-Through Synchronization Overhead:**
   - Calling OS sync on every single individual `put()` operation limits single-thread write throughput to disk rotational/flush speeds.
   * *Optimization:* Batching amortizes sync costs by issuing a single flush for up to 10,000 operations.
2. **In-Memory Hash Index Scaling:**
   - Indexing 1M keys in RAM requires careful memory management to avoid high heap overhead.
   * *Design Decision:* Future iterations can store offset-pointers rather than cached string values to reduce resident memory below 50 MB for 1M keys.

## 3. Failed Attempts & Retrospective
- **Dynamic library linking:** Early MinGW builds depended on dynamic runtime libraries (`libstdc++-6.dll`). Resolved by enforcing static linking (`-static -static-libgcc -static-libstdc++`) in `CMakeLists.txt`.

