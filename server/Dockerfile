# Use Ubuntu as base image
FROM ubuntu:latest

# Install dependencies
RUN apt update && apt install -y \
    g++ cmake git curl

# Set working directory
WORKDIR /app

# Copy project files
COPY . .

# Compile the server
# RUN g++ -o server main.cpp -pthread -std=c++11

# Expose port 8080
EXPOSE 8080

# Run the server
# CMD ["./server"]
CMD ["bash"]