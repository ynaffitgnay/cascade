module RegMem#(
  parameter ADDR_SIZE = 4,
  parameter BYTE_SIZE = 8
)(
  input  wire clock,
  input  wire wen,
  input  wire[ADDR_SIZE-1:0] raddr1,
  output wire[BYTE_SIZE-1:0] rdata1,
  input  wire[ADDR_SIZE-1:0] raddr2,
  output wire[BYTE_SIZE-1:0] rdata2,
  input  wire[ADDR_SIZE-1:0] waddr,
  input  wire[BYTE_SIZE-1:0] wdata
);
  (*non_volatile*)
  reg[BYTE_SIZE-1:0] mem[2**ADDR_SIZE-1:0];
  assign rdata1 = mem[raddr1];
  assign rdata2 = mem[raddr2];
  always @(posedge clock)
    if (wen) 
      mem[waddr] <= wdata;
endmodule
