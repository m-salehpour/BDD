# Knowledge Graph (KG) Querying using Binary Decision Diagrams (BDDs)

Welcome! This is the codebase related to executing statement-based queries over KGs using BDDs.

# How to compile

Each realtion set is implemented in a separated C file, e.g., bdd_SPO.c, btree_SOP.c, etc.

We used this command for the compilation of each C file:

gcc -O0 -I/usr/local/include  [filename] -o [executablename] -L/usr/local/lib -lbdd  -lm -Werror=vla -Wextra -Wall -Wshadow -Wswitch-default -DDEBUG=1 -g && ./[executablename]
(Note that we assume that BuDDy is installed in `/usr/local/include` and `/usr/local/lib`)

# Benchmark Datasets
| Datasets  | Link |
| ------------- | ------------- |
| BSBM, LUBM, SP2Bench  |  https://zenodo.org/record/4445454/files/datasets.zip?download=1 |



# Authors
* Masoud Salehpour (University of Sydney)
* Joseph G. Davis  (University of Sydney) 

