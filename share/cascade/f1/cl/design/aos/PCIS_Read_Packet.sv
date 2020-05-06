// Translates PCIS interface from AXI to AOSPacket
// XDMA source recommended, BAR4 could cause issues
// Note: data path is NOT registered

import AMITypes::*;
import AOSF1Types::*;

module PCIS_Read_Packet(
    
    // General Signals
    input clk,
    input rst,

    // Read Address Channel
    input[5:0]   sh_cl_dma_pcis_arid,    // read address id for the read address group
    input[63:0]  sh_cl_dma_pcis_araddr,  // address of first transfer in a read burst transaction
    input[7:0]   sh_cl_dma_pcis_arlen,   // burst length, number of transfers in a burst (+1 to this value)
    input[2:0]   sh_cl_dma_pcis_arsize,  // burst size, size of each transfer in the burst
    input        sh_cl_dma_pcis_arvalid, // read address valid, signals the read address/control info is valid
    output logic cl_sh_dma_pcis_arready, // read address ready, signals the slave is ready to accept an address/control info

    // Read Data Channel
    output logic[5:0]   cl_sh_dma_pcis_rid,     // read id tag
    output logic[511:0] cl_sh_dma_pcis_rdata,   // read data
    output logic[1:0]   cl_sh_dma_pcis_rresp,   // status of the read transfer
    output logic        cl_sh_dma_pcis_rlast,   // indicates last transfer in a read burst
    output logic        cl_sh_dma_pcis_rvalid,  // indicates the read data is valid
    input               sh_cl_dma_pcis_rready,  // indicates the master (the host) can accept read data/response info

    // AOSPacket in
    output logic                   packet_in_ready, // indicates the slave can accept a packet
    output logic[AMI_APP_BITS-1:0] packet_in_app,   // app num for packet routing
    input AOSPacket                packet_in        // valid, data, and slot information
);

// State
typedef enum {
    IDLE,   // wait for start
    SEND   // send data
} state_t;
state_t state;
state_t next_state;

// Transaction ID
reg   [5:0] id;
logic [5:0] next_id;

// Transaction length remaining
reg   [7:0] len;
logic [7:0] next_len;

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
    next_len = len;
    next_app_num = app_num;
    
    cl_sh_dma_pcis_arready = 0;
    cl_sh_dma_pcis_rid = id;
    cl_sh_dma_pcis_rdata = packet_in.data;
    cl_sh_dma_pcis_rresp = 2'b00;
    cl_sh_dma_pcis_rlast = 0;
    cl_sh_dma_pcis_rvalid = 0;
    packet_in_ready = 0;
    packet_in_app = app_num;

    case (state)
    IDLE: begin
        cl_sh_dma_pcis_arready = 1;
        next_id = sh_cl_dma_pcis_arid;
        next_app_num = sh_cl_dma_pcis_araddr[13+:AMI_APP_BITS];
        next_len = sh_cl_dma_pcis_arlen;
	    if (sh_cl_dma_pcis_arvalid) begin
            // check that sh_cl_dma_pcis_awaddr[26] is set?
            // set unaligned error if (sh_cl_dma_pcis_araddr[12:0] != '0)
            // set width error if (sh_cl_dma_pcis_arsize != 3'b110)
            next_state = SEND;
        end
    end
    SEND: begin
        cl_sh_dma_pcis_rvalid = packet_in.valid;
        //cl_sh_dma_pcis_rvalid = 1;
        packet_in_ready = sh_cl_dma_pcis_rready;
        if (len == 0) begin
            cl_sh_dma_pcis_rlast = 1;
        end
        if (packet_in.valid && sh_cl_dma_pcis_rready) begin
        //if (sh_cl_dma_pcis_rready) begin
            next_len = len - 1;
            if (len == 0) begin
                next_state = IDLE;
            end
        end
    end
    endcase
end

// State machine update
always_ff @(posedge clk) begin
    if (rst) begin
        state <= IDLE;
        id <= 0;
        len <= 0;
        app_num <= 0;
        //err <= 0;
    end else begin
        state <= next_state;
        id <= next_id;
        len <= next_len;
        app_num <= next_app_num;
        //err <= next_err;
    end
end

endmodule
