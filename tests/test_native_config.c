/* SPDX-License-Identifier: GPL-2.0 */
/* Production C codec/scheduler. Fixed wire offsets below come from Legacy,
 * not from C layout assumptions. No fake radio completion or native peer. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "owly_awdl_sync.h"

#define REQUIRE(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static const uint8_t mac[6]={2,1,2,3,4,5};
static const uint8_t service[12]={6,9,0,0,0,0,0,0,0,0,0,0};
static uint32_t word(const uint8_t *p) { uint32_t x; memcpy(&x,p,4); return x; }

int main(void)
{
    struct owly_native_config cfg={0}, saved;
    struct owly_sync engine;
    struct owly_cfg_bssid b={0,{0,0x25,0,0xff,0x94,0x73},{0}};
    struct owly_cfg_u32 u={0,0};
    struct owly_cfg_rssi r={0,{0,0,0},-73};
    struct owly_cfg_sync sync={0};
    struct owly_cfg_params params={0,16,16,16,110,0};
    struct owly_cfg_rate rate={0,12};
    struct { uint8_t before[8]; struct owly_cfg_sequence sequence; uint8_t after[8]; } guarded;
    uint8_t frame[2048], bad[12], peer[6]={2,9,8,7,6,5};
    size_t length=0;
    unsigned groups=0,i;
    REQUIRE(sizeof(b)==16 && sizeof(r)==20 && sizeof(sync)==24 && sizeof(params)==24 && sizeof(rate)==5);
    REQUIRE(sizeof(guarded.sequence)==400 && offsetof(struct owly_cfg_sequence,channels)==12 && offsetof(struct owly_cfg_rssi,threshold)==16);++groups;

    /* Version-zero buffers match the configure() sequence in Legacy4C07538B. */
    REQUIRE(owly_config_request(&cfg,true,125,&b,mac,149)==0);
    u.value=0x12345678;REQUIRE(owly_config_request(&cfg,true,122,&u,mac,149)==0);
    u.value=149;REQUIRE(owly_config_request(&cfg,true,132,&u,mac,149)==0);
    REQUIRE(owly_config_request(&cfg,true,135,&r,mac,149)==0);
    sync.state=1;memcpy(sync.master,mac,6);sync.flags=1;
    REQUIRE(owly_config_request(&cfg,true,134,&sync,mac,149)==0);
    REQUIRE(cfg.committed==31 && cfg.election_id==0x12345678 && cfg.master_channel==149 && cfg.peer_rssi==-73 && cfg.sync.flags==1);++groups;
    saved=cfg;
    for (i=0;i<3;i++) {
        uint8_t ch=i==0 ? 6 : i==1 ? 44 : 149;
        memset(&guarded,0xa5,sizeof(guarded));owly_config_sequence(&guarded.sequence,ch);
        const uint8_t *wire=(const uint8_t *)&guarded.sequence;
        REQUIRE(word(wire)==1 && wire[5]==16 && wire[6]==1 && wire[7]==3 && wire[9]==255);
        for (unsigned j=0;j<16;j++) {
            REQUIRE(word(wire+12+12*j)==1 && word(wire+16+12*j)==ch);
            REQUIRE(word(wire+20+12*j)==(ch==6?10u:18u));
        }
        for (unsigned j=204;j<400;j++) REQUIRE(wire[j]==0);
        for (unsigned j=0;j<8;j++) REQUIRE(guarded.before[j]==0xa5 && guarded.after[j]==0xa5);
        REQUIRE(owly_config_request(&cfg,true,129,&guarded.sequence,mac,ch)==0);
    }++groups;
    cfg=saved;
    u.value=44;REQUIRE(owly_config_request(&cfg,true,132,&u,mac,149)==-EOPNOTSUPP && !memcmp(&cfg,&saved,sizeof(cfg)));++groups;
    for (unsigned n=0;n<8;n++) {
        unsigned number=(unsigned[]){116,122,125,129,132,134,135,213}[n];
        uint8_t payload[400]={2,0,0,0};
        REQUIRE(owly_config_request(&cfg,true,number,payload,mac,149)==-EINVAL && !memcmp(&cfg,&saved,sizeof(cfg)));
    }++groups;
    b.bssid[5]^=1;REQUIRE(owly_config_request(&cfg,true,125,&b,mac,149)==-EOPNOTSUPP);b.bssid[5]^=1;++groups;
    r.threshold=-129;REQUIRE(owly_config_request(&cfg,true,135,&r,mac,149)==-EINVAL);
    r.threshold=1;REQUIRE(owly_config_request(&cfg,true,135,&r,mac,149)==-EINVAL);
    r.threshold=-73;r.reserved[2]=1;REQUIRE(owly_config_request(&cfg,true,135,&r,mac,149)==-EOPNOTSUPP);
    REQUIRE(!memcmp(&cfg,&saved,sizeof(cfg)));r.reserved[2]=0;++groups;
    memcpy(sync.master,peer,6);REQUIRE(owly_config_request(&cfg,true,134,&sync,mac,149)==-EOPNOTSUPP);
    memcpy(sync.master,mac,6);sync.state=2;REQUIRE(owly_config_request(&cfg,true,134,&sync,mac,149)==-EOPNOTSUPP);
    sync.state=1;sync.flags=2;REQUIRE(owly_config_request(&cfg,true,134,&sync,mac,149)==-EOPNOTSUPP);
    sync.flags=1;sync.mode_flags=0x20;REQUIRE(owly_config_request(&cfg,true,134,&sync,mac,149)==-EOPNOTSUPP);
    REQUIRE(!memcmp(&cfg,&saved,sizeof(cfg)));sync.mode_flags=0;++groups;
    for (i=0;i<7;i++) {
        owly_config_sequence(&guarded.sequence,149);
        if (i==0)guarded.sequence.channels[0].channel=44;
        if (i==1)guarded.sequence.channels[1].flags|=0x2000;
        if (i==2)guarded.sequence.length=15;
        if (i==3)guarded.sequence.duplicate=1;
        if (i==4)guarded.sequence.channels[16].channel=149;
        if (i==5)guarded.sequence.tail[3]=1;
        if (i==6)guarded.sequence.channels[1].flags=8;
        REQUIRE(owly_config_request(&cfg,true,129,&guarded.sequence,mac,149)==-EOPNOTSUPP);
        REQUIRE(!memcmp(&cfg,&saved,sizeof(cfg)));
    }++groups;
    REQUIRE(owly_config_request(&cfg,true,213,&rate,mac,149)==0);
    rate.half_mbps=13;REQUIRE(owly_config_request(&cfg,true,213,&rate,mac,149)==-EOPNOTSUPP);++groups;
    uint8_t invalid_mac[6]={0};
    REQUIRE(owly_config_request(&cfg,true,122,&u,invalid_mac,149)==-EINVAL);
    invalid_mac[0]=3;REQUIRE(owly_config_request(&cfg,true,122,&u,invalid_mac,149)==-EINVAL);
    REQUIRE(owly_config_request(&cfg,true,122,&u,mac,11)==-ENETDOWN);
    REQUIRE(owly_config_request(NULL,true,122,&u,mac,149)==-EINVAL);
    REQUIRE(owly_config_request(&cfg,true,122,NULL,mac,149)==-EINVAL);
    REQUIRE(owly_config_request(&cfg,true,999,&u,mac,149)==-EINVAL);++groups;
    memset(&b,0xa5,sizeof(b));REQUIRE(owly_config_request(&cfg,false,125,&b,mac,149)==0 && b.version==1 && !memcmp(b.opaque_mac,saved.bssid_opaque,6));
    memset(&r,0xa5,sizeof(r));REQUIRE(owly_config_request(&cfg,false,135,&r,mac,149)==0 && r.version==1 && r.threshold==-73 && !r.reserved[2]);
    REQUIRE(!memcmp(&cfg,&saved,sizeof(cfg)));++groups;
    REQUIRE(owly_config_rx_allowed(-73,-73) && owly_config_rx_allowed(-73,-72) && !owly_config_rx_allowed(-73,-74));
    REQUIRE(owly_config_rx_allowed(-128,-128));++groups;

    owly_sync_reset(&engine);
    REQUIRE(owly_sync_template(&engine,service,sizeof(service),mac)==0 && engine.template_len==12 && !engine.requested && !engine.active);++groups;
    memcpy(bad,service,sizeof(bad));bad[1]=10;struct owly_sync prior=engine;
    REQUIRE(owly_sync_template(&engine,bad,sizeof(bad),mac)!=0 && !memcmp(&engine,&prior,sizeof(engine)));
    bad[1]=8;REQUIRE(owly_sync_template(&engine,bad,11,mac)!=0 && !memcmp(&engine,&prior,sizeof(engine)));
    REQUIRE(owly_sync_template(&engine,engine.template_bytes,12,mac)!=0 && !memcmp(&engine,&prior,sizeof(engine)));++groups;
    static const uint8_t old_template[5]={21,2,0,0x32,1};
    REQUIRE(owly_sync_template(&engine,old_template,5,mac)==0);
    REQUIRE(owly_sync_template(&engine,service,12,mac)==0);++groups;
    /* Actual deadlines and transmitted Sync TLV must follow the SET116 period
     * in milliseconds. AW window lengths remain 16 TU. */
    for (i=0;i<2;i++) {
        params.psf_period=i?440:110;
        REQUIRE(owly_config_request(&engine.native,true,116,&params,mac,149)==0);
        owly_sync_request(&engine,false);owly_sync_request(&engine,true);
        const uint64_t start=1000000,delta=(uint64_t)params.psf_period*1000;
        owly_sync_reconcile(&engine,true,149,start);
        REQUIRE(owly_sync_frame(&engine,start,frame,sizeof(frame),&length)==1 && length==172);
        REQUIRE(engine.due_us==start+delta && frame[43]==149);
        REQUIRE((frame[50] | (unsigned)frame[51]<<8)==params.psf_period);
        REQUIRE(owly_sync_frame(&engine,start+delta-1,frame,sizeof(frame),&length)==0 && !length);
        REQUIRE(owly_sync_frame(&engine,start+delta*4,frame,sizeof(frame),&length)==1 && engine.due_us==start+delta*5);
    }++groups;
    params.psf_period=109;struct owly_native_config before=engine.native;
    REQUIRE(owly_config_request(&engine.native,true,116,&params,mac,149)==-EOPNOTSUPP && !memcmp(&before,&engine.native,sizeof(before)));++groups;
    owly_sync_reconcile(&engine,false,149,9000000);REQUIRE(!engine.active && !engine.due_us);
    owly_sync_reset(&engine);REQUIRE(!engine.requested && !engine.template_len && !engine.native.committed);++groups;
    printf("native configuration production C: %u groups passed; no kernel/RF proof\n",groups);
    return 0;
}
