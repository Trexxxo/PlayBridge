/* Experimental PS5 10.xx AVCap2 request layout, reconstructed from
 * PBS-Payload-0.3.0-beta.3-ps5.elf.
 *
 * pbs_ps5_find_offsets (ELF +0xdaa0) maps 10.20 to the 10.01-family
 * table (ELF +0xd3130). 10.60 (+0xd3178) has the same request layout.
 * The tables identify field offsets, NOT an entire OpenVideo template.
 * Non-table fields below are adapted from the tested 13.60 request;
 * they remain hypotheses until verified on 10.xx hardware.
 */
#ifndef PLAYBRIDGE_FW10_CAPTURE_H
#define PLAYBRIDGE_FW10_CAPTURE_H

#define FW10_REQUEST_SIZE 0x2b8u
#define FW10_HEADER_SIZE  0x188u
#define FW10_RESOURCE_PROFILE 12
#define FW10_FRAME_BUDGET 0x100000u

static int fw10_capture_layout(uint32_t fw){
 uint32_t family=fw&0xffff0000u;
 return family>=0x10010000u&&family<=0x10600000u;
}

/* 10.xx AVCap2 request translated from the 13.60 template using
 * the offsets recorded in PBS's firmware tables.
 * u32/u64 and memset are provided by stream-av3.c. */
static uint32_t fw10_prepare_video_request(unsigned char*p,uint32_t fps,uint32_t bitrate,uint32_t output_mode){
 uint32_t rate=bitrate<=30000000u?bitrate:30000000u;
 memset(p,0,0x2e8);
 u64(p,0,FW10_REQUEST_SIZE);
 u64(p,8,0x28);
 u32(p,0x14,2);
 u32(p,0x18,4);
 u32(p,0x1c,1);
 u32(p,0x24,0);
 u32(p,0x28,FW10_RESOURCE_PROFILE);
 /* Common early request fields from 13.60; verify on 10.xx. */
 u64(p,0x30,0x58);
 u64(p,0x38,0x18);
 p[0x40]=1;
 u32(p,0x44,1);
 u32(p,0x48,1);
 u32(p,0x4c,0x200000);
 u64(p,0x50,0x10);
 p[0x58]=1;
 u32(p,0x5c,9);
 u64(p,0x60,0x18);
 p[0x68]=1;
 /* The first nested section moves 0x10 bytes earlier. */
 u64(p,0x78,0x30);
 u32(p,0x84,fps);
 u64(p,0xa8,0xd8);
 u64(p,0xb0,0x20);
 u32(p,0xb8,rate);
 u32(p,0xbc,rate);
 u32(p,0xc0,(uint32_t)((uint64_t)rate*1001/(fps*1000)));
 u32(p,0xc8,0x33);
 u64(p,0xd0,0x20);
 u64(p,0xf0,0x20);
 p[0xf8]=1;
 u32(p,0xfc,FW10_FRAME_BUDGET);
 u64(p,0x110,0x18);
 p[0x118]=1;
 u32(p,0x11c,25);
 u32(p,0x120,fps);
 /* The second nested section moves 0x20 bytes earlier. */
 u64(p,0x168,0x10);
 p[0x170]=1;
 u32(p,0x174,1);
 u64(p,0x180,0x50);
 u64(p,0x1d0,0xe8);
 u64(p,0x1d8,0x28);
 /* These are encoder image-buffer parameters, not 1920x1080 pixels. */
 u32(p,0x1ec,384);
 u32(p,0x1f0,320);
 u32(p,0x1f4,512);
 u64(p,0x1f8,0x3c000);
 u64(p,0x200,0x88);
 memset(p+0x208,0xff,0x80);
 /* PBS's 10.xx table maps the output mode to +0x288, and the
  * following format selector to +0x28c. Do not erase the mode. */
 u32(p,0x288,output_mode);
 u32(p,0x28c,2);
 u64(p,0x290,0x10);
 u32(p,0x298,1);
 p[0x29c]=1;
 u64(p,0x2a0,0x10);
 p[0x2a8]=1;
 u32(p,0x2ac,110);
 return rate;
}
#endif
