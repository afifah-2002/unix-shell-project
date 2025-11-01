# Unix Shell Project

CSCE 5640 - Operating System Design
Project 1, Fall 2025

## Team Members
- Afifah Khan (ID: 11800954)
- Nanditha Aitha [ Id: 11759521]
- Bhavana Bayyapu [ ID :11680072]
- Deepika Thakur [ID: 11751906]

## Project Description
Implementation of a Unix/Linux shell with the following features:
- Interactive and batch modes
- Command execution with fork/exec
- Input/output redirection
- Pipe support
- Command history

## How to Compile
```bash
gcc -Wall -g shell.c -o myshell
```

## How to Run

### Interactive Mode
```bash
./myshell
```

### Batch Mode
```bash
./myshell test_commands.txt
```

## Project Structure
```
.
├── shell.c              # Main shell implementation
├── Makefile            # Build instructions
├── test_commands.txt   # Test batch file
└── README.md          # This file
```



