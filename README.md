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
