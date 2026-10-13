FakeClock Development Notes
===========================

This project follows a **test-driven development (TDD)** approach. Always write a failing test before implementing a new feature or fixing a bug.

Environment setup
-----------------

1. Install dependencies (example for Ubuntu):
   ```bash
   sudo apt-get update
   sudo apt-get install -y build-essential cmake libgtest-dev libboost-dev
   ```
2. Build the project:
   ```bash
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Debug
   cmake --build .
   ```
3. Run the tests:
   ```bash
   ctest --output-on-failure
   ```

Tips for faster setup
---------------------

- Install all dependencies in one step to avoid repeated apt calls.
- Reuse the same `build` directory when possible; CMake will only rebuild changed files.
- Running tests directly after a successful build (`cmake --build . && ctest`) saves time.
- Older Boost releases may provide Process headers without a separate CMake
  component. Keep that component optional; the fork integration test checks
  header availability and reports a skip only when v2 headers are missing.
- Use `-DUSE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug` for sanitizer regressions.
  The `ClockRuntime.*` CTest cases run in separate processes with real-time
  timeouts, including initialization before `main`.

Updating these notes
--------------------

If a missing dependency or useful tool slows down your workflow, document the fix
here so future runs can skip troubleshooting.

Working via GitHub Issues
-------------------------

All tasks and discussions happen in GitHub issues. You can list issues with:
The `scripts/github_activity.py` helper script prints open issues and
comments from open pull requests using the GitHub API. Run it with:
```bash
scripts/github_activity.py
```

- When you're asked to fix PR comments:
    - Answer each comment on github. It may be simple "fixed" or "done". If you
      disagree with the comment, explain why. If you don't understand the comment, ask for clarification.
    - If you need to make changes, do so in the same branch as the PR.
    - After committing changes, check for new comments on the PR (and again after successive commits).
