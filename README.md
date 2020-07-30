# Ascia

Ascia is a C program used to split big files into several smaller ones, and join them again when necessary.

## Installation

Simply use the makefile by typing:

```bash
make
```

## Usage

To split the file _filename_ into blocks of size _X_ bytes use:
```bash
ascia split filename X
```

To join the files back use:
```bash
ascia join filename
```