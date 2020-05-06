// Translates PCIM interface from AXI to AOSPacket

import AMITypes::*;
import AOSF1Types::*;

typedef struct packed {
    logic[39:0] addr;
    logic[15:0] len;
    logic[7:0]  id;
} PCIM_Command;

typedef struct packed {
    logic[10:0] num;
    logic[7:0]  id;
} NID;

module PCIM_Write (
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
// Buffer unpacked SoftReg commands
logic cfifo_full;   // not used
logic cfifo_empty;
logic cfifo_wrreq;
logic cfifo_rdreq;
PCIM_Command cfifo_in;
PCIM_Command cfifo_out;

// Length FIFO signals
// Tracks # writes per txn
logic lfifo_full;
logic lfifo_empty;
logic lfifo_wrreq;
logic lfifo_rdreq;
logic[5:0] lfifo_in;
logic[5:0] lfifo_out;

// NID FIFO signals
// Tracks txns per ID
logic nidfifo_full;
logic nidfifo_empty;
logic nidfifo_wrreq;
logic nidfifo_rdreq;
NID nidfifo_in;
NID nidfifo_out;

// Current command
PCIM_Command cmd;
PCIM_Command next_cmd;

// Remaining txns in cmd
logic[10:0] num_txns;
logic[10:0] next_num_txns;

// Remaining writes in txn
logic[5:0] length;
logic[5:0] next_length;

// Remaining responses for ID
NID pending;
NID next_pending;

// Completed ID LIFO shift register
// Holds up to 32 ID responses
// Response: {1'valid, 2'status, 5'id}
logic[255:0] responses;
logic[255:0] next_responses;


// Command buffering
always_comb begin
    PCIM_Command temp_cmd;
    temp_cmd.addr = softreg_req.data[63:24];
    temp_cmd.len = softreg_req.data[23:8];
    temp_cmd.id = softreg_req.data[7:0];
    
    cfifo_wrreq = 0;
    cfifo_in = temp_cmd;
    
    if (softreg_req.valid && softreg_req.isWrite && (softreg_req.addr == 32'h8)) begin
        // differentiate read / write commands by address
        cfifo_wrreq = 1;
    end
end

// Address channel state machine
typedef enum logic[1:0] {
    AWAIT,   // wait for command
    ANID,   // generate NID
    AAXI,   // send AXI request
    ALEN   // send data length
} astate_t;
astate_t astate;
astate_t next_astate;

always_comb begin
    logic[16:0] aligned_len;
    logic[5:0] page_len;
    logic[5:0] temp_len;

    next_astate = astate;
    next_cmd = cmd;
    next_num_txns = num_txns;
    
    cfifo_rdreq = 0;
    
    nidfifo_wrreq = 0;
    aligned_len = cmd.addr[5:0] + cmd.len;
    // 1 less than the actual number of txns so always at least one txn
    nidfifo_in.num = aligned_len[16:6];
    nidfifo_in.id = cmd.id;
    
    page_len = 6'b111111 - cmd.addr[5:0];
    temp_len = (num_txns == 0) ? cmd.len[5:0] : page_len;
    
    cl_sh_pcim_awid = 0;
    cl_sh_pcim_awaddr = {18'h0, cmd.addr, 6'h0};
    cl_sh_pcim_awlen = {2'd0, temp_len};
    cl_sh_pcim_awsize = 3'b110;
    cl_sh_pcim_awuser = 0;
    cl_sh_pcim_awvalid = 0;
    
    lfifo_wrreq = 0;
    lfifo_in = temp_len;
    
    case (astate)
    AWAIT: begin
        cfifo_rdreq = 1;
        next_cmd = cfifo_out;
        if (!cfifo_empty) begin
            next_astate = ANID;
        end
    end
    ANID: begin
        nidfifo_wrreq = 1;
        next_num_txns = nidfifo_in.num;
        if (!nidfifo_full) begin
            next_astate = AAXI;
        end
    end
    AAXI: begin
        cl_sh_pcim_awvalid = 1;
        if (sh_cl_pcim_awready) begin
            next_astate = ALEN;
        end
    end
    ALEN: begin
        lfifo_wrreq = 1;
        if (!lfifo_full) begin
            next_num_txns = num_txns - 1;
            next_cmd.addr[39:6] = cmd.addr[39:6] + 1;
            next_cmd.addr[5:0] = 0;
            next_cmd.len = cmd.len - temp_len - 1;
            if (num_txns == 0) begin
                next_astate = AWAIT;
            end else begin
                next_astate = AAXI;
            end
        end
    end
    endcase
end

always_ff @(posedge clk) begin
    if (rst) begin
        astate <= AWAIT;
        cmd <= 0;
        num_txns <= 0;
    end else begin
        astate <= next_astate;
        cmd <= next_cmd;
        num_txns <= next_num_txns;
    end
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
        next_length = lfifo_out;
        if (!lfifo_empty) begin
            next_wstate = WSEND;
        end
    end
    WSEND: begin
        cl_sh_pcim_wvalid = packet_in.valid;
        packet_in_ready = sh_cl_pcim_wready;
        if (packet_in.valid && sh_cl_pcim_wready) begin
            next_length = length - 1;
            if (length == 0) begin
                lfifo_rdreq = 1;
                next_length = lfifo_out;
                if (lfifo_empty) begin
                    next_wstate = WIDLE;
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

// State machine and logic for:
// Write response channel
// Response shift reg
// Softreg response
typedef enum logic[1:0] {
    RIDLE,   // wait for NID
    RWAIT   // wait for last txn, then record ID
} rstate_t;
rstate_t rstate;
rstate_t next_rstate;

always_comb begin
    next_rstate = rstate;
    next_pending = pending;
    next_responses = responses;
    
    nidfifo_rdreq = 0;
    
    cl_sh_pcim_bready = 0;
    
    if (softreg_req.valid && !softreg_req.isWrite && (softreg_req.addr == 32'h8)) begin
        next_responses = {64'h0, responses[255:64]};
    end
    
    softreg_resp.valid = softreg_req.valid && !softreg_req.isWrite && (softreg_req.addr == 32'h8);
    softreg_resp.data = responses[63:0];
    
    case(rstate)
    RIDLE: begin
        nidfifo_rdreq = 1;
        next_pending = nidfifo_out;
        if (!nidfifo_empty) begin
            next_rstate = RWAIT;
        end
    end
    RWAIT: begin
        cl_sh_pcim_bready = 1;
        if (sh_cl_pcim_bvalid) begin
            next_pending.num = pending.num - 1;
            next_pending.id[6:5] = pending.id[6:5] | sh_cl_pcim_bresp;
            if (pending.num == 0) begin
                next_rstate = RIDLE;
                next_responses = {next_responses[247:0], 1'b1, next_pending.id[6:0]};
            end
        end
    end
    endcase
end

always_ff @(posedge clk) begin
    if (rst) begin
        rstate <= RIDLE;
        pending <= 0;
        responses <= 0;
    end else begin
        rstate <= next_rstate;
        pending <= next_pending;
        responses <= next_responses;
    end
end


HullFIFO
#(
    .TYPE                   (0),
    .WIDTH                  ($bits(PCIM_Command)),
    .LOG_DEPTH              (5)
)
CmdFIFO
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

HullFIFO
#(
    .TYPE                   (0),
    .WIDTH                  (6),
    .LOG_DEPTH              (5)
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
    .WIDTH                  ($bits(NID)),
    .LOG_DEPTH              (5)
)
NIdFIFO
(
    .clock                  (clk),
    .reset_n                (~rst),
    .wrreq                  (nidfifo_wrreq),
    .data                   (nidfifo_in),
    .full                   (nidfifo_full),
    .q                      (nidfifo_out),
    .empty                  (nidfifo_empty),
    .rdreq                  (nidfifo_rdreq)
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
// Buffer unpacked SoftReg commands
logic cfifo_full;   // not used
logic cfifo_empty;
logic cfifo_wrreq;
logic cfifo_rdreq;
PCIM_Command cfifo_in;
PCIM_Command cfifo_out;

// NID FIFO signals
// Tracks txns per ID
logic nidfifo_full;
logic nidfifo_empty;
logic nidfifo_wrreq;
logic nidfifo_rdreq;
NID nidfifo_in;
NID nidfifo_out;

// Response FIFO signals
// Buffers txn responses
logic rfifo_full;
logic rfifo_empty;
logic rfifo_wrreq;
logic rfifo_rdreq;
logic[1:0] rfifo_in;
logic[1:0] rfifo_out;

// Current command
PCIM_Command cmd;
PCIM_Command next_cmd;

// Remaining txns in cmd
logic[15:0] num_txns;
logic[15:0] next_num_txns;

// Remaining reads in txn
logic[7:0] length;
logic[7:0] next_length;

// Remaining responses for ID
NID pending;
NID next_pending;

// Completed ID LIFO shift register
// Holds up to 32 responses
// Response: {1'valid, 2'status, 5'id}
logic[255:0] responses;
logic[255:0] next_responses;


// Command buffering
always_comb begin
    PCIM_Command temp_cmd;
    temp_cmd.addr = softreg_req.data[63:24];
    temp_cmd.len = softreg_req.data[23:8];
    temp_cmd.id = softreg_req.data[7:0];
    
    cfifo_wrreq = 0;
    cfifo_in = temp_cmd;
    
    if (softreg_req.valid && softreg_req.isWrite && (softreg_req.addr == 32'h0)) begin
        // differentiate read / write commands by address
        cfifo_wrreq = 1;
    end
end

// Address channel state machine
typedef enum logic[1:0] {
    AWAIT,   // wait for command
    ANID,   // generate NID
    AAXI   // send AXI request
} astate_t;
astate_t astate;
astate_t next_astate;

always_comb begin
    logic[16:0] aligned_len;
    logic[5:0] page_len;
    logic[5:0] temp_len;
    
    next_astate = astate;
    next_cmd = cmd;
    next_num_txns = num_txns;
    
    cfifo_rdreq = 0;
    
    nidfifo_wrreq = 0;
    aligned_len = cmd.addr[5:0] + cmd.len;
    // 1 less than the actual number of txns so always one txn minimum
    nidfifo_in.num = aligned_len[16:6];
    nidfifo_in.id = cmd.id;
    
    page_len = 6'b111111 - cmd.addr[5:0];
    temp_len = (num_txns == 0) ? cmd.len[5:0] : page_len;
    
    cl_sh_pcim_arid = 0;
    cl_sh_pcim_araddr = {18'h0, cmd.addr, 6'h0};
    cl_sh_pcim_arlen = {2'd0, temp_len};
    cl_sh_pcim_arsize = 3'b110;
    cl_sh_pcim_aruser = 0;
    cl_sh_pcim_arvalid = 0;
    
    case (astate)
    AWAIT: begin
        cfifo_rdreq = 1;
        next_cmd = cfifo_out;
        if (!cfifo_empty) begin
            next_astate = ANID;
        end
    end
    ANID: begin
        nidfifo_wrreq = 1;
        next_num_txns = nidfifo_in.num;
        if (!nidfifo_full) begin
            next_astate = AAXI;
        end
    end
    AAXI: begin
        cl_sh_pcim_arvalid = 1;
        if (sh_cl_pcim_arready) begin
            next_num_txns = num_txns - 1;
            next_cmd.addr[39:6] = cmd.addr[39:6] + 1;
            next_cmd.addr[5:0] = 0;
            next_cmd.len = cmd.len - temp_len - 1;
            if (num_txns == 0) begin
                next_astate = AWAIT;
            end
        end
    end
    endcase
end

always_ff @(posedge clk) begin
    if (rst) begin
        astate <= AWAIT;
        cmd <= 0;
        num_txns <= 0;
    end else begin
        astate <= next_astate;
        cmd <= next_cmd;
        num_txns <= next_num_txns;
    end
end

// Read data channel logic
// Response enqueue logic
always_comb begin
    packet_out.valid = sh_cl_pcim_rvalid && !rfifo_full;
    packet_out.data = sh_cl_pcim_rdata;
    cl_sh_pcim_rready = packet_out_ready && !rfifo_full;
    
    rfifo_wrreq = 0;
    rfifo_in = sh_cl_pcim_rresp;
    if (sh_cl_pcim_rvalid && packet_out_ready && sh_cl_pcim_rlast) begin
        rfifo_wrreq = 1;
    end
end

// State machine and logic for:
// Response shift reg
// Softreg response
typedef enum logic[1:0] {
    RIDLE,   // wait for response
    RWAIT   // wait for last txn, then record ID
} rstate_t;
rstate_t rstate;
rstate_t next_rstate;

always_comb begin
    next_rstate = rstate;
    next_pending = pending;
    next_responses = responses;
    
    nidfifo_rdreq = 0;
    
    rfifo_rdreq = 0;
    
    if (softreg_req.valid && !softreg_req.isWrite && (softreg_req.addr == 32'h0)) begin
        next_responses = {64'h0, responses[255:64]};
    end
    
    softreg_resp.valid = softreg_req.valid && !softreg_req.isWrite && (softreg_req.addr == 32'h0);
    softreg_resp.data = responses[63:0];
    
    case(rstate)
    RIDLE: begin
        nidfifo_rdreq = 1;
        next_pending = nidfifo_out;
        if (!nidfifo_empty) begin
            next_rstate = RWAIT;
        end
    end
    RWAIT: begin
        rfifo_rdreq = 1;
        if (!rfifo_empty) begin
            next_pending.num = pending.num - 1;
            next_pending.id[6:5] = pending.id[6:5] | rfifo_out;
            if (pending.num == 0) begin
                next_rstate = RIDLE;
                next_responses = {next_responses[247:0], 1'b1, next_pending.id[6:0]};
            end
        end
    end
    endcase
end

always_ff @(posedge clk) begin
    if (rst) begin
        rstate <= RIDLE;
        pending <= 0;
        responses <= 0;
    end else begin
        rstate <= next_rstate;
        pending <= next_pending;
        responses <= next_responses;
    end
end


HullFIFO
#(
    .TYPE                   (0),
    .WIDTH                  ($bits(PCIM_Command)),
    .LOG_DEPTH              (5)
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

HullFIFO
#(
    .TYPE                   (0),
    .WIDTH                  ($bits(NID)),
    .LOG_DEPTH              (5)
)
NIdFIFO
(
    .clock                  (clk),
    .reset_n                (~rst),
    .wrreq                  (nidfifo_wrreq),
    .data                   (nidfifo_in),
    .full                   (nidfifo_full),
    .q                      (nidfifo_out),
    .empty                  (nidfifo_empty),
    .rdreq                  (nidfifo_rdreq)
);

HullFIFO
#(
    .TYPE                   (0),
    .WIDTH                  (2),
    .LOG_DEPTH              (3)
)
RespFIFO
(
    .clock                  (clk),
    .reset_n                (~rst),
    .wrreq                  (rfifo_wrreq),
    .data                   (rfifo_in),
    .full                   (rfifo_full),
    .q                      (rfifo_out),
    .empty                  (rfifo_empty),
    .rdreq                  (rfifo_rdreq)
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
assign wrreq = packet_out.valid;
assign data = packet_out;

assign rdreq = packet_in_ready;
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
    .LOG_DEPTH              (11)
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
