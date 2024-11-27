#!/bin/bash

# Function to check and install nlohmann/json.hpp (via package manager or download)
install_nlohmann_json() {
    if ! dpkg -l | grep -q "nlohmann-json3-dev"; then
        echo "nlohmann/json.hpp not found. Installing..."
        sudo apt-get update
        sudo apt-get install -y nlohmann-json3-dev
    else
        echo "nlohmann/json.hpp is already installed."
    fi
}

# Function to check and install curl
install_curl() {
    if ! command -v curl &> /dev/null; then
        echo "curl not found. Installing..."
        sudo apt-get update
        sudo apt-get install -y curl
    else
        echo "curl is already installed."
    fi
}

# Function to check and install ollama
install_ollama() {
    if ! command -v ollama &> /dev/null; then
        echo "ollama not found. Installing..."
        # Assuming Ollama can be installed using curl for Linux
        curl -fsSL https://ollama.com/download/ollama-linux.tar.gz -o ollama-linux.tar.gz
        tar -xzvf ollama-linux.tar.gz
        sudo mv ollama /usr/local/bin/
        rm ollama-linux.tar.gz
        echo "ollama installed."
    else
        echo "ollama is already installed."
    fi
}

# Install dependencies
install_nlohmann_json
install_curl
install_ollama

echo "Installation check complete."