`include "share/cascade/test/benchmark/bitcoin/bitcoin_yield.v"
Bitcoin#(.UNROLL(5), .DIFF(25)) bitcoin();
