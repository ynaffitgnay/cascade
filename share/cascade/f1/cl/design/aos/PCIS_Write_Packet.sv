// Translates PCIS interface from AXI to AOSPacket
// XDMA source recommended, BAR4 could cause issues
// Note: data path is NOT registered

import AMITypes::*;
import AOSF1Types::*;

module PCIS_Write_Packet (

    // General Signals
    input clk,
    input rst,

    // Write Address Channel
    input[5:0]   sh_cl_dma_pcis_awid,    // tag for the write address group
    input[63:0]  sh_cl_dma_pcis_awaddr,  // address of first transfer in write burst
    input[7:0]   sh_cl_dma_pcis_awlen,   // number of transfers in a burst (+1 to this value)
    input[2:0]   sh_cl_dma_pcis_awsize,  // size of each transfer in the burst
    input        sh_cl_dma_pcis_awvalid, // write address valid, signals the write address and control info is correct
    output logic cl_sh_dma_pcis_awready,

    // Write Data Channel
    input[511:0] sh_cl_dma_pcis_wdata,   // write data
    input[63:0]  sh_cl_dma_pcis_wstrb,   // write strobes, indicates which byte lanes hold valid data, 1 strobe bit per 8 bits to write
    input        sh_cl_dma_pcis_wlast,   // indicates the last transfer
    input        sh_cl_dma_pcis_wvalid,  // indicates the write data and strobes are valid
    output logic cl_sh_dma_pcis_wready,  // indicates the slave can accept write data

    // Write Response Channel
    output logic[5:0] cl_sh_dma_pcis_bid,    // response id tag
    output logic[1:0] cl_sh_dma_pcis_bresp,  // write response indicating the status of the transaction
    output logic      cl_sh_dma_pcis_bvalid, // indicates the write response is valid
    input             sh_cl_dma_pcis_bready, // indicates the master can accept a write response
    
    // AOSPacket out
    input  logic                   packet_out_ready, // indicates the master can accept a packet
    output logic[AMI_APP_BITS-1:0] packet_out_app,   // app num for packet routing
    output AOSPacket               packet_out        // valid, data, and slot information
);

// State
typedef enum {
    IDLE,   // wait for start
    RECV,   // receive data
    RESP   // acknowledge transaction
} state_t;
state_t state;
state_t next_state;

// Transaction ID
reg   [5:0] id;
logic [5:0] next_id;

// Transaction app num
reg   [AMI_APP_BITS-1:0] app_num;
logic [AMI_APP_BITS-1:0] next_app_num;

// Error tracking
//reg   [31:0] err;
//logic [31:0] next_err;

// State machine logic
always_comb begin
    next_state = state;
    next_id = id;
    next_app_num = app_num;
    
    cl_sh_dma_pcis_awready = 0;
    cl_sh_dma_pcis_wready = 0;
    cl_sh_dma_pcis_bid = id;
    cl_sh_dma_pcis_bresp = 2'b00;
    cl_sh_dma_pcis_bvalid = 0;
    
    packet_out_app = app_num;
    packet_out.valid = 0;
    packet_out.data = sh_cl_dma_pcis_wdata;
    packet_out.slot = id[1:0];

    case (state)
    IDLE: begin
        cl_sh_dma_pcis_awready = 1;
        next_id = sh_cl_dma_pcis_awid;
        next_app_num = sh_cl_dma_pcis_awaddr[13+:AMI_APP_BITS];
        if (sh_cl_dma_pcis_awvalid) begin
            // check that sh_cl_dma_pcis_awaddr[26] is set?
            // set unaligned error if (sh_cl_dma_pcis_awaddr[12:0] != '0)
            // set width error if (sh_cl_dma_pcis_awsize != 3'b110)
            next_state = RECV;
        end
    end
    RECV: begin
        cl_sh_dma_pcis_wready = packet_out_ready;
        //cl_sh_dma_pcis_wready = 1;
        packet_out.valid = sh_cl_dma_pcis_wvalid;
        // set mask error if (sh_cl_dma_pcis_wvalid && (sh_cl_dma_pcis_wstrb != '1))
        if (packet_out_ready && sh_cl_dma_pcis_wvalid && sh_cl_dma_pcis_wlast) begin
        //if (sh_cl_dma_pcis_wvalid && sh_cl_dma_pcis_wlast) begin
            next_state = RESP;
        end
    end
    RESP: begin
        cl_sh_dma_pcis_bvalid = 1;
        if (sh_cl_dma_pcis_bready) begin
            next_state = IDLE;
        end
    end
    endcase
end

// State machine update
always_ff @(posedge clk) begin
    if (rst) begin
        state <= IDLE;
        id <= 0;
        app_num <= 0;
        //err <= 0;
    end else begin
        state <= next_state;
        id <= next_id;
        app_num <= next_app_num;
        //err <= next_err;
    end
end

endmodule
