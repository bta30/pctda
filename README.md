# pctda
Proof of Concept of Targetted Dynamic Analysis

## Example program
This comes with an example program in `sum_program/` which, given a number of threads and some file names, summarises the files using that many threads. Each file is in a CSV format for columns of:
  - tag (unsigned integer)
  - values (float)

Any lines not in this format are ignored.

The program returns the mean value for each tag across each file on unique lines of the form `tag,mean`, e.g. `1,3.5`.

To execute the sample program, using two threads and the given example files `table1.csv`, `table2.csv`, `table3.csv` and `table4.csv`, perform:

```
cd sum_program/
make
./sum 2 table1.csv table2.csv table3.csv table4.csv
```

## Proof of concept JSON code tracer client for DynamoRIO
This performs a trace of a given executable, generating log files for each thread of one entry per instruction, containing:
  - Program counter
  - Opcode
  - Instruction name
  - Some operand values from immediately before executing the instruction:
    - General purpose register operands (register name and value)
    - Immediate operands (value)
    - Absolute memory address operands (near/far, address, value)
    - Relative memory address operands (near/far, base register name/value, displacement, value)
  - Debugging information
    - Variables (name, type, size, local/global)
    - Functions (on call)
    - Source file and line for each instruction

To get debugging information, the traced executable must be compiled with no optimisation and maximum DWARF4 debug information.
In GCC, this is flags `-O0 -g3 -gdwarf-4`.

To build and execute using the given example program, using `drrun` as in your DynamoRIO installation, perform:
```
cd json_tracer/
mkdir build
cd build
cmake ..
make
drrun -c libjsontracer.so -- ../../sum_program/sum 2 ../../sum_program/table*
```

## Dependencies
The code tracer requires `libdwarf` to be installed at build time
