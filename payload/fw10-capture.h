/* Experimental PS5 10.xx AVCap2 request layout, reconstructed from
 * PBS-Payload-0.3.0-beta.3-ps5.elf.
 *
 * pbs_ps5_find_offsets (ELF +0xdaa0) maps 10.20 to the 10.01-family
 * table (ELF +0xd3130). 10.60 (+0xd3178) has the same request layout.
 * This header does not include or redistribute the PBS executable.
 *
 * These fields are statically observed; execution in PlayBridge's own
 * process has NOT been tested on a 10.xx console.
 */
#ifndef PLAYBRIDGE_FW10_CAPTURE_H
#define PLAYBRIDGE_FW10_CAPTURE_H

#define FW10_REQUEST_SIZE 0x2b8u
#define FW10_HEADER_SIZE  0x188u
#define FW10_RESOURCE_PROFILE 12

/* SDK firmware versions have build-specific low bits: select the
 * exact 10.01..10.60 family the PBS offsets table maps, not 10.xx+
 * indiscriminately. 10.20 is covered by this family. */
static int fw10_capture_layout(uint32_t fw){
 uint32_t family=fw&0xffff0000u;
 return family>=0x10010000u&&family<=0x10600000u;
}

/* Only 1080p is enabled for the first live test.
 * u32/u64 and memset are provided by stream-av3.c. */
static uint32_t fw10_prepare_video_request(unsigned char*p,uint32_t fps,uint32_t bitrate){
 uint32_t rate=bitrate<=30000000u?bitrate:30000000u;
 memset(p,0,0x2e8);
 u64(p,0,FW10_REQUEST_SIZE);
 u64(p,8,0x28);
 u32(p,0x14,2);
 u32(p,0x18,4);
 u32(p,0x1c,1);
 u32(p,0x28,FW10_RESOURCE_PROFILE);
 /* AVCap2 bitrate/encoder block: PBS table +0x18 and +0x1c. */
 u64(p,0xa8,0xd8);
 u64(p,0xb0,0x20);
 u32(p,0xb8,rate);
 u32(p,0xbc,rate);
 u32(p,0xc0,(uint32_t)((uint64_t)rate*1001/(fps*1000)));
 u32(p,0xc4,18);
 u32(p,0xc8,0x33);
 /* Layout-specific nested parameter and image dimensions. */
 u64(p,0x1d0,0xe8);
 u64(p,0x1d8,0x28);
 u32(p,0x1f0,1920);
 u32(p,0x1f4,1080);
 u64(p,0x200,0x88);
 memset(p+0x210,0xff,0x80);
 u32(p,0x288,2);
 u64(p,0x290,0x10);
 u32(p,0x298,1);
 p[0x29c]=1;
 return rate;
}
#endif
