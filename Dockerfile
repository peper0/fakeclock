FROM ubuntu:20.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libgtest-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

# Install gtest
RUN apt-get update && apt-get install -y cmake && \
    cd /usr/src/gtest && \
    cmake CMakeLists.txt && \
    make && \
    cp *.a /usr/lib

# Set the working directory
WORKDIR /home/peper/src/fakeclock

# Copy the current directory contents into the container
COPY . .

# Create build directory
RUN mkdir build
WORKDIR /home/peper/src/fakeclock/build

# Build the project
RUN cmake .. && make

# Run tests
CMD ["./test_fakeclock"]
