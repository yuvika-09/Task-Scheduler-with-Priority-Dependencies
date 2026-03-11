# Task Scheduler

## Overview
Concurrent scheduler that executes tasks respecting
dependencies and priorities.

## Design
- Kahn's algorithm for dependency resolution
- priority queue for task scheduling
- thread pool for concurrency

## Data Structures
unordered_map
priority_queue
adjacency list graph

## How to Run
g++ scheduler.cpp -std=c++17 -pthread
./scheduler --workers 4

## Tradeoffs
Focused on correctness and readability.