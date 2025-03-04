FROM ubuntu:latest

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libgtest-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

# Copy the project files
COPY . /build

# Set the working directory
WORKDIR /build

# Build the project
RUN mkdir build && cd build && cmake .. && cmake --build .

# Run tests
CMD cd build && ctest --output-on-failure
