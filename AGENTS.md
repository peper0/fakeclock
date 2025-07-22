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

Updating these notes
--------------------

If a missing dependency or useful tool slows down your workflow, document the fix
here so future runs can skip troubleshooting.

Working via GitHub Issues
-------------------------

All tasks and discussions happen in GitHub issues. You can list issues with:
```bash
curl -sL https://api.github.com/repos/peper0/fakeclock/issues | jq '.[].title'
```
Use the issue page to ask questions or propose changes.

The `scripts/github_activity.py` helper script prints open issues and
comments from open pull requests using the GitHub API. Run it with:
```bash
scripts/github_activity.py
```
