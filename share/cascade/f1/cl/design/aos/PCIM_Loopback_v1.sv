// Translates PCIM interface from AXI to AOSPacket

import AMITypes::*;
import AOSF1Types::*;

typedef struct packed {
    logic[39:0] addr;
    logic[7:0]  len;
    logic[15:0] id;
} PCIM_Command;

module PCIM_Write(
    // General signals
    input clk,
    input rst,
    
    // Write address channel
    output logic[15:0]  cl_sh_pcim_awid,
    output logic[63:0]  cl_sh_pcim_awaddr,
    output logic[7:0]   cl_sh_pcim_awlen,
    output logic[2:0]   cl_sh_pcim_awsize,
    output logic[18:0]  cl_sh_pcim_awuser,   // RESERVED (not used)
    output logic        cl_sh_pcim_awvalid,
    input               sh_cl_pcim_awready,

    // Write data channel
    output logic[511:0] cl_sh_pcim_wdata,
    output logic[63:0]  cl_sh_pcim_wstrb,
    output logic        cl_sh_pcim_wlast,
    output logic        cl_sh_pcim_wvalid,
    input               sh_cl_pcim_wready,

	// Write response channel
    input logic[15:0]   sh_cl_pcim_bid,
    input logic[1:0]    sh_cl_pcim_bresp,
    input logic         sh_cl_pcim_bvalid,
    output logic        cl_sh_pcim_bready,

    // Other shell signals
    // Max payload size - 00:128B, 01:256B, 10:512B
    input[1:0]          cfg_max_payload,
    // Max read requst size - 000b:128B, 001b:256B, 010b:512B, 011b:1024B, 100b-2048B, 101b:4096B
    input[2:0]          cfg_max_read_req,
    
    // SoftReg control interface
    input SoftRegReq    softreg_req,
    output SoftRegResp  softreg_resp,

    // AOSPacket in
    output logic        packet_in_ready,
    input AOSPacket     packet_in
);

// Command FIFO signals
logic cfifo_full;   // not used
logic cfifo_empty;
logic cfifo_wrreq;
logic cfifo_rdreq;
PCIM_Command cfifo_in;
PCIM_Command cfifo_out;

// Length FIFO signals
logic lfifo_full;   // not used
logic lfifo_empty;
logic lfifo_wrreq;
logic lfifo_rdreq;
logic[7:0] lfifo_in;
logic[7:0] lfifo_out;

// Remaining length
logic[7:0] length;
logic[7:0] next_length;

// Returned ID LIFO shift register
// Holds up to 32 responses
// Response: {1'valid, 7'id}
logic[255:0] responses;
logic[255:0] temp_responses;
logic[255:0] next_responses;


// Command buffering
always_comb begin
    PCIM_Command temp_cmd;
    temp_cmd.addr = softreg_req.data[63:24];
    temp_cmd.len = softreg_req.data[23:16];
    temp_cmd.id = softreg_req.data[15:0];
    
    lfifo_wrreq = 0;
    lfifo_in = temp_cmd.len;
    cfifo_wrreq = 0;
    cfifo_in = temp_cmd;
    
    if (softreg_req.valid && softreg_req.isWrite && (softreg_req.addr == 32'h8)) begin
        // differentiate read / write commands by address
        lfifo_wrreq = 1;
        cfifo_wrreq = 1;
    end
end

// Address channel logic
always_comb begin
    cl_sh_pcim_awid = cfifo_out.id;
    cl_sh_pcim_awaddr = {18'h0, cfifo_out.addr, 6'h0};
    cl_sh_pcim_awlen = cfifo_out.len;
    cl_sh_pcim_awsize = 3'b110;
    cl_sh_pcim_awuser = 0;
    cl_sh_pcim_awvalid = !cfifo_empty;
    cfifo_rdreq = sh_cl_pcim_awready;
end

// Write data state machine
typedef enum logic[1:0] {
    WIDLE,   // wait for length
    WSEND   // write data
} wstate_t;
wstate_t wstate;
wstate_t next_wstate;

always_comb begin
    next_wstate = wstate;
    next_length = length;
    
    lfifo_rdreq = 0;
    
    cl_sh_pcim_wdata = packet_in.data;
    cl_sh_pcim_wstrb = {64{1'b1}};
    cl_sh_pcim_wlast = (length == 8'h0);
    cl_sh_pcim_wvalid = 0;
    packet_in_ready = 0;
    
    case(wstate)
    WIDLE: begin
        lfifo_rdreq = 1;
        if (!lfifo_empty) begin
            next_length = lfifo_out;
            next_wstate = WSEND;
        end
    end
    WSEND: begin
        cl_sh_pcim_wvalid = packet_in.valid;
        packet_in_ready = sh_cl_pcim_wready;
        
        if (packet_in.valid && sh_cl_pcim_wready) begin
            next_length = length - 8'h1;
            
            if (length == 8'h0) begin
                lfifo_rdreq = 1;
                
                if (lfifo_empty) begin
                    next_wstate = WIDLE;
                end else begin
                    next_length = lfifo_out;
                end
            end
        end
    end
    endcase
end

always_ff @(posedge clk) begin
    if (rst) begin
        wstate <= WIDLE;
        length <= 0;
    end else begin
        wstate <= next_wstate;
        length <= next_length;
    end
end

// Write response channel logic
// Response shift reg logic
// Softreg response logic
always_comb begin
    cl_sh_pcim_bready = 1;
    
    temp_responses = responses;
    if (softreg_req.valid && !softreg_req.isWrite && (softreg_req.addr == 32'h8)) begin
        temp_responses = {64'h0, responses[255:64]};
    end
    next_responses = temp_responses;
    if (sh_cl_pcim_bvalid) begin
        next_responses = {temp_responses[247:0], 1'b1, sh_cl_pcim_bresp, sh_cl_pcim_bid[4:0]};
    end
    
    softreg_resp.valid = softreg_req.valid && !softreg_req.isWrite && (softreg_req.addr == 32'h8);
    softreg_resp.data = responses[63:0];
end

always_ff @(posedge clk) begin
    if (rst) begin
        responses <= 0;
    end else begin
        responses <= next_responses;
    end
end


HullFIFO
#(
    .TYPE                   (0),
    .WIDTH                  (8),
    .LOG_DEPTH              (6)
)
LenFIFO
(
    .clock                  (clk),
    .reset_n                (~rst),
    .wrreq                  (lfifo_wrreq),
    .data                   (lfifo_in),
    .full                   (lfifo_full),
    .q                      (lfifo_out),
    .empty                  (lfifo_empty),
    .rdreq                  (lfifo_rdreq)
);

HullFIFO
#(
    .TYPE                   (0),
    .WIDTH                  ($bits(PCIM_Command)),
    .LOG_DEPTH              (6)
)
CommandFIFO
(
    .clock                  (clk),
    .reset_n                (~rst),
    .wrreq                  (cfifo_wrreq),
    .data                   (cfifo_in),
    .full                   (cfifo_full),
    .q                      (cfifo_out),
    .empty                  (cfifo_empty),
    .rdreq                  (cfifo_rdreq)
);

endmodule


module PCIM_Read(
    // General signals
    input clk,
    input rst,

    // Read address channel
    // Note max 32 outstanding txns are supported, width is larger to allow bits for AXI fabrics
    output logic[15:0]  cl_sh_pcim_arid,
    output logic[63:0]  cl_sh_pcim_araddr,
    output logic[7:0]   cl_sh_pcim_arlen,
    output logic[2:0]   cl_sh_pcim_arsize,
    output logic[18:0]  cl_sh_pcim_aruser,   // RESERVED (not used)
    output logic        cl_sh_pcim_arvalid,
    input               sh_cl_pcim_arready,

    // Read data channel
    input[15:0]         sh_cl_pcim_rid,
    input[511:0]        sh_cl_pcim_rdata,
    input[1:0]          sh_cl_pcim_rresp,
    input               sh_cl_pcim_rlast,
    input               sh_cl_pcim_rvalid,
    output logic        cl_sh_pcim_rready,

    // Other shell signals
    // Max payload size - 00:128B, 01:256B, 10:512B
    input[1:0]          cfg_max_payload,
    // Max read requst size - 000b:128B, 001b:256B, 010b:512B, 011b:1024B, 100b-2048B, 101b:4096B
    input[2:0]          cfg_max_read_req,
    
    // SoftReg control interface
    input SoftRegReq    softreg_req,
    output SoftRegResp  softreg_resp,

    // AOSPacket out
    input logic         packet_out_ready,
    output AOSPacket    packet_out
);

// Command FIFO signals
logic cfifo_full;   // not used
logic cfifo_empty;
logic cfifo_wrreq;
logic cfifo_rdreq;
PCIM_Command cfifo_in;
PCIM_Command cfifo_out;

// Returned ID LIFO shift register
// Holds up to 32 responses
// Response: {1'valid, 7'id}
logic[255:0] responses;
logic[255:0] temp_responses;
logic[255:0] next_responses;


// Command buffering
always_comb begin
    PCIM_Command temp_cmd;
    temp_cmd.addr = softreg_req.data[63:24];
    temp_cmd.len = softreg_req.data[23:16];
    temp_cmd.id = softreg_req.data[15:0];
    
    cfifo_wrreq = 0;
    cfifo_in = temp_cmd;
    
    if (softreg_req.valid && softreg_req.isWrite && (softreg_req.addr == 32'h0)) begin
        // differentiate read / write commands by address
        cfifo_wrreq = 1;
    end
end

// Address channel logic
always_comb begin
    cl_sh_pcim_arid = cfifo_out.id;
    cl_sh_pcim_araddr = {18'h0, cfifo_out.addr, 6'h0};
    cl_sh_pcim_arlen = cfifo_out.len;
    cl_sh_pcim_arsize = 3'b110;
    cl_sh_pcim_aruser = 0;
    cl_sh_pcim_arvalid = !cfifo_empty;
    cfifo_rdreq = sh_cl_pcim_arready;
end

// Read data channel logic
// Response shift reg logic
// Softreg response logic
always_comb begin
    packet_out.valid = sh_cl_pcim_rvalid;
    packet_out.data = sh_cl_pcim_rdata;
    cl_sh_pcim_rready = packet_out_ready;
    
    temp_responses = responses;
    if (softreg_req.valid && !softreg_req.isWrite && (softreg_req.addr == 32'h0)) begin
        temp_responses = {64'h0, responses[255:64]};
    end
    next_responses = temp_responses;
    if (sh_cl_pcim_rvalid && packet_out_ready && sh_cl_pcim_rlast) begin
        next_responses = {temp_responses[247:0], 1'b1, sh_cl_pcim_rresp, sh_cl_pcim_rid[4:0]};
    end
    
    softreg_resp.valid = softreg_req.valid && !softreg_req.isWrite && (softreg_req.addr == 32'h0);
    softreg_resp.data = responses[63:0];
end

always_ff @(posedge clk) begin
    if (rst) begin
        responses <= 0;
    end else begin
        responses <= next_responses;
    end
end


HullFIFO
#(
    .TYPE                   (0),
    .WIDTH                  ($bits(PCIM_Command)),
    .LOG_DEPTH              (6)
)
CommandFIFO
(
    .clock                  (clk),
    .reset_n                (~rst),
    .wrreq                  (cfifo_wrreq),
    .data                   (cfifo_in),
    .full                   (cfifo_full),
    .q                      (cfifo_out),
    .empty                  (cfifo_empty),
    .rdreq                  (cfifo_rdreq)
);

endmodule


module PCIM_Loopback(
    // General signals
    input clk,
    input rst,
    
    // Write address channel
    output logic[15:0]  cl_sh_pcim_awid,
    output logic[63:0]  cl_sh_pcim_awaddr,
    output logic[7:0]   cl_sh_pcim_awlen,
    output logic[2:0]   cl_sh_pcim_awsize,
    output logic[18:0]  cl_sh_pcim_awuser,   // RESERVED (not used)
    output logic        cl_sh_pcim_awvalid,
    input               sh_cl_pcim_awready,

    // Write data channel
    output logic[511:0] cl_sh_pcim_wdata,
    output logic[63:0]  cl_sh_pcim_wstrb,
    output logic        cl_sh_pcim_wlast,
    output logic        cl_sh_pcim_wvalid,
    input               sh_cl_pcim_wready,

	// Write response channel
    input logic[15:0]   sh_cl_pcim_bid,
    input logic[1:0]    sh_cl_pcim_bresp,
    input logic         sh_cl_pcim_bvalid,
    output logic        cl_sh_pcim_bready,

    // Read address channel
    // Note max 32 outstanding txns are supported, width is larger to allow bits for AXI fabrics
    output logic[15:0]  cl_sh_pcim_arid,
    output logic[63:0]  cl_sh_pcim_araddr,
    output logic[7:0]   cl_sh_pcim_arlen,
    output logic[2:0]   cl_sh_pcim_arsize,
    output logic[18:0]  cl_sh_pcim_aruser,   // RESERVED (not used)
    output logic        cl_sh_pcim_arvalid,
    input               sh_cl_pcim_arready,

    // Read data channel
    input[15:0]         sh_cl_pcim_rid,
    input[511:0]        sh_cl_pcim_rdata,
    input[1:0]          sh_cl_pcim_rresp,
    input               sh_cl_pcim_rlast,
    input               sh_cl_pcim_rvalid,
    output logic        cl_sh_pcim_rready,

    // Other shell signals
    // Max payload size - 00:128B, 01:256B, 10:512B
    input[1:0]          cfg_max_payload,
    // Max read requst size - 000b:128B, 001b:256B, 010b:512B, 011b:1024B, 100b-2048B, 101b:4096B
    input[2:0]          cfg_max_read_req,
    
    // SoftReg control interface
    input SoftRegReq softreg_req,
    output SoftRegResp softreg_resp
);

// SoftReg write signals
SoftRegReq softreg_req_write;
SoftRegResp softreg_resp_write;

// SoftReg write signals
SoftRegReq softreg_req_read;
SoftRegResp softreg_resp_read;

// AOSPacket write signals
logic packet_out_ready;
AOSPacket packet_out;

// AOSPacket read signals
logic packet_in_ready;
AOSPacket packet_in;

// FIFO signals
logic full;
logic empty;
logic wrreq;
logic rdreq;
AOSPacket data;
AOSPacket q;

// Connection logic
assign softreg_req_write = softreg_req;
assign softreg_req_read = softreg_req;
assign softreg_resp = softreg_resp_write.valid ? softreg_resp_write : softreg_resp_read;

assign packet_out_ready = !full;
assign wrreq = packet_out.valid && packet_out_ready;
assign data = packet_out;

assign rdreq = packet_in_ready && !empty;
assign packet_in.valid = !empty;
assign packet_in.data = q.data;
assign packet_in.slot = q.slot;

PCIM_Write PWD (
    // General signals
    .clk(clk),
    .rst(rst),
 
    // Write address channel
    .cl_sh_pcim_awid(cl_sh_pcim_awid),
    .cl_sh_pcim_awaddr(cl_sh_pcim_awaddr),
    .cl_sh_pcim_awlen(cl_sh_pcim_awlen),
    .cl_sh_pcim_awsize(cl_sh_pcim_awsize),
    .cl_sh_pcim_awuser(cl_sh_pcim_awuser),
    .cl_sh_pcim_awvalid(cl_sh_pcim_awvalid),
    .sh_cl_pcim_awready(sh_cl_pcim_awready),

    // Write data channel
    .cl_sh_pcim_wdata(cl_sh_pcim_wdata),
    .cl_sh_pcim_wstrb(cl_sh_pcim_wstrb),
    .cl_sh_pcim_wlast(cl_sh_pcim_wlast),
    .cl_sh_pcim_wvalid(cl_sh_pcim_wvalid),
    .sh_cl_pcim_wready(sh_cl_pcim_wready),

    // Write response channel
    .sh_cl_pcim_bid(sh_cl_pcim_bid),
    .sh_cl_pcim_bresp(sh_cl_pcim_bresp),
    .sh_cl_pcim_bvalid(sh_cl_pcim_bvalid),
    .cl_sh_pcim_bready(cl_sh_pcim_bready),
    
    // Other shell signals
    .cfg_max_payload(cfg_max_payload),
    .cfg_max_read_req(cfg_max_read_req),
    
    // SoftReg control interface
    .softreg_req(softreg_req_write),
    .softreg_resp(softreg_resp_write),

    // AOSPacket in
    .packet_in_ready(packet_in_ready),
    .packet_in(packet_in)
);

PCIM_Read PRD (
    // General Signals
    .clk(clk),
    .rst(rst),

    // Read address channel
    .cl_sh_pcim_arid(cl_sh_pcim_arid),
    .cl_sh_pcim_araddr(cl_sh_pcim_araddr),
    .cl_sh_pcim_arlen(cl_sh_pcim_arlen),
    .cl_sh_pcim_arsize(cl_sh_pcim_arsize),
    .cl_sh_pcim_aruser(cl_sh_pcim_aruser),
    .cl_sh_pcim_arvalid(cl_sh_pcim_arvalid),
    .sh_cl_pcim_arready(sh_cl_pcim_arready),

    // Read data channel
    .sh_cl_pcim_rid(sh_cl_pcim_rid),
    .sh_cl_pcim_rdata(sh_cl_pcim_rdata),
    .sh_cl_pcim_rresp(sh_cl_pcim_rresp),
    .sh_cl_pcim_rlast(sh_cl_pcim_rlast),
    .sh_cl_pcim_rvalid(sh_cl_pcim_rvalid),
    .cl_sh_pcim_rready(cl_sh_pcim_rready),
    
    // Other shell signals
    .cfg_max_payload(cfg_max_payload),
    .cfg_max_read_req(cfg_max_read_req),
    
    // SoftReg control interface
    .softreg_req(softreg_req_read),
    .softreg_resp(softreg_resp_read),

    // AOSPacket out
    .packet_out_ready(packet_out_ready),
    .packet_out(packet_out)
);

HullFIFO
#(
    .TYPE                   (0),
    .WIDTH                  ($bits(AOSPacket)),
    .LOG_DEPTH              (10)
)
AOSPacketFIFO
(
    .clock                  (clk),
    .reset_n                (~rst),
    .wrreq                  (wrreq),
    .data                   (data),
    .full                   (full),
    .q                      (q),
    .empty                  (empty),
    .rdreq                  (rdreq)
);

endmodule
