reg[7:0] packed;
wire valid = packed[7];
wire write = packed[6];
wire[5:0] slot = packed[5:0];

reg[7:0] vid[1:0];
wire[31:0] big_addr = {17'd0,vid[1][3:0],vid[0],3'd0};

reg[7:0] data_in[7:0];
wire[63:0] big_data;
assign big_data[7:0] = data_in[0];
assign big_data[15:8] = data_in[1];
assign big_data[23:16] = data_in[2];
assign big_data[31:24] = data_in[3];
assign big_data[39:32] = data_in[4];
assign big_data[47:40] = data_in[5];
assign big_data[55:48] = data_in[6];
assign big_data[63:56] = data_in[7];

wire clk = clock.val;

genvar i;
generate
for (i = 0; i < NUM_APPS; i = i+1) begin: modules
  wire valid_ = (slot == i) ? valid : 0;
  wire write_ = valid_ ? write : 0;
  wire[31:0] addr_ = valid_ ? big_addr : 0;
  wire[63:0] data_ = valid_ ? big_data : 0;
  wire valid_out;
  wire[63:0] data_out;
  
  program_logic #(
    .app_num(i)
  ) pl (
    .clk(clk),
    .reset(0),
    .softreg_req_valid(valid_),
    .softreg_req_isWrite(write_),
    .softreg_req_addr(addr_),
    .softreg_req_data(data_),
    .softreg_resp_valid(valid_out),
    .softreg_resp_data(data_out)
  );
  
  always @(posedge clock.val) begin
    if (valid_out) begin
      $fwrite(ofd, "%c%c%c%c%c%c%c%c", data_out[7:0], data_out[15:8], data_out[23:16],
        data_out[31:24], data_out[39:32], data_out[47:40], data_out[55:48], data_out[63:56]);
      //$display("%d", data_out);
    end
  end
end
endgenerate

always @(posedge clock.val) begin
  // Read in next operation
  $fflush(ifd);
  $fscanf(ifd, "%c%c%c", packed, vid[0], vid[1]);
  if ($feof(ifd)) begin
    packed = 0;
  end else begin
    if (packed[6])  // write
      $fscanf(ifd, "%c%c%c%c%c%c%c%c", data_in[0], data_in[1], data_in[2],
        data_in[3], data_in[4], data_in[5], data_in[6], data_in[7]);
    //$display("%d %d", packed, {vid[1][3:0],vid[0]});
  end
end
