import ShellTypes::*;
import AMITypes::*;
import AOSF1Types::*;

module CascadeWrapper
(
	// User clock and reset
	input  clk,
	input  rst, 

	// Simplified Memory interface
	output AMIRequest  mem_reqs        [1:0],
	input              mem_req_grants  [1:0],
	input  AMIResponse mem_resps       [1:0],
	output             mem_resp_grants [1:0],

	// PCIe Slot DMA interface
	input  PCIEPacket  pcie_packet_in,
	output             pcie_full_out,

	output PCIEPacket pcie_packet_out,
	input             pcie_grant_in,

	// Soft register interface
	input  SoftRegReq   softreg_req,
	output SoftRegResp  softreg_resp
);
	parameter app_num = 0;
	
	// Disable DRAM
	assign mem_reqs[0] = '{valid: '0, isWrite: '0, addr: '0, data: '0, size: '0};
	assign mem_resp_grants[0] = 1'b0;
	assign mem_reqs[1] = '{valid: '0, isWrite: '0, addr: '0, data: '0, size: '0};
	assign mem_resp_grants[1] = 1'b0;
	
	// Disable pcie
	assign pcie_full_out = 1'd1;
	assign pcie_packet_out = '{valid: '0, data: '0, slot: '0, pad: '0, last: '0};
	
	program_logic
	#(
		.app_num(app_num)
	) pl (
		.clk(clk),
		.reset(rst),

		.softreg_req_valid(softreg_req.valid),
		.softreg_req_isWrite(softreg_req.isWrite),
		.softreg_req_addr(softreg_req.addr),
		.softreg_req_data(softreg_req.data),

		.softreg_resp_valid(softreg_resp.valid),
		.softreg_resp_data(softreg_resp.data)
	);

endmodule
