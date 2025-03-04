# Contributing to FakeClock

Thank you for your interest in contributing to FakeClock! This document provides guidelines and instructions for contributing to this project.

## Code of Conduct

Please be respectful and considerate of others when contributing to this project. We expect all contributors to follow basic principles of open-source collaboration.

## Getting Started

1. Fork the repository
2. Clone your fork: `git clone https://github.com/peper0/fakeclock.git`
3. Create a new branch for your feature or bugfix: `git checkout -b your-feature-name`
4. Make your changes
5. Build and test your changes

## Development Environment Setup

To set up your development environment:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

## Coding Standards

When contributing code, please follow these guidelines:

- Use consistent formatting with the rest of the codebase
- Follow C++17 standards or later
- Write clear, descriptive commit messages
- Add appropriate unit tests for new functionality
- Document public APIs using comments

## Pull Request Process

1. Ensure your code builds and passes all tests
2. Update documentation if necessary
3. Submit a pull request to the main repository
4. Address any feedback from code reviews

## Testing

All new features should include unit tests. Run the test suite before submitting your PR:

```bash
cd build
ctest
```

## Reporting Issues

If you find a bug or have a feature request:

1. Check if the issue already exists in the GitHub issue tracker
2. If not, create a new issue with a clear description
3. Include steps to reproduce bugs or clear descriptions of proposed features
4. Add relevant labels

## License

By contributing to FakeClock, you agree that your contributions will be licensed under the project's MIT License.
