#!/bin/bash

# Install dependencies
sudo apt-get update
sudo apt-get -y install build-essential git libopus-dev


# Clone FFmpeg source code
git clone https://github.com/FFmpeg/FFmpeg.git
cd FFmpeg/

# Configure FFmpeg with Opus support
./configure --enable-libopus

# Build and install FFmpeg
make -j$(nproc)
sudo make install
