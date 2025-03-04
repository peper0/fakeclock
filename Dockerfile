FROM ubuntu:latest

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    libgtest-dev \
    #boost asio: only for testing
    libboost-all-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

# Copy the project files
COPY . /build

# Set the working directory
WORKDIR /build

# Run tests
CMD cd build && ctest --output-on-failure
