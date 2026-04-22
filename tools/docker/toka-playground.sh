#!/bin/bash
set -e

# Get the absolute path of the directory containing this script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( dirname "$( dirname "$SCRIPT_DIR" )" )"

IMAGE_NAME="tokalang/playground:latest"

echo "================================================"
echo "🚀 Preparing the Toka User Playground..."
echo "================================================"

# Check if the image exists, build it if it doesn't
if ! docker image inspect "$IMAGE_NAME" > /dev/null 2>&1; then
    echo "📦 Building the Docker image (this will download the latest Toka binaries)..."
    docker build -t "$IMAGE_NAME" -f "$SCRIPT_DIR/Dockerfile.playground" "$SCRIPT_DIR"
else
    echo "✅ Found a cached local image. To fetch the latest version, run: docker rmi $IMAGE_NAME"
fi

echo "✨ Ready! Entering the sandbox environment..."
echo "💡 Tip: Inside the container, you can freely create directories, write a toka.json, and run 'toka fetch' or 'toka build'."
echo "💡 Tip: Press Ctrl+D to exit and destroy the container environment."

# Run the container in interactive mode
# We deliberately do not mount the host source code to ensure this simulates a clean external developer machine.
docker run --rm -it "$IMAGE_NAME"
