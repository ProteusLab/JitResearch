set(PROT_NOP_AMOUNT 1000000)
configure_file(nopbench.s.in nopbench.s @ONLY)

prot_add_bench(nopbench nopbench.s)
