/* SPDX-License-Identifier: GPL-2.0 */
/* Independent wire fixtures: OWL frame.h/tx.c and Legacy4C build/parser. */
#include "owly_native_link.h"
#include "owly_awdl_sync.h"
#include <errno.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
static const uint8_t local[6]={2,1,2,3,4,5},remote[6]={2,6,7,8,9,10};
static unsigned checks;
#define CHECK(x) do{++checks;assert(x);}while(0)
static void p16(uint8_t *p,unsigned v){p[0]=v;p[1]=v>>8;}
static void p32(uint8_t *p,unsigned v){for(unsigned i=0;i<4;i++)p[i]=v>>(8*i);}
static unsigned g16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static unsigned g32(const uint8_t *p){return g16(p)|(unsigned)g16(p+2)<<16;}
static uint8_t *find(uint8_t *p,size_t n,unsigned type){size_t a=40;while(a+3<=n){unsigned z=g16(p+a+1);CHECK(z<=n-a-3);if(p[a]==type)return p+a+3;a+=3+z;}CHECK(a==n);return NULL;}
static size_t peer(uint8_t *f,unsigned seq){
    memset(f,0,256);f[0]=0xd0;memset(f+4,255,6);memcpy(f+10,remote,6);memcpy(f+16,(uint8_t[]){0,0x25,0,0xff,0x94,0x73},6);
    f[24]=127;f[26]=0x17;f[27]=0xf2;f[28]=8;f[29]=0x10;f[30]=3;
    f[40]=4;p16(f+41,31);uint8_t *v=f+43;
    v[0]=v[3]=149;p16(v+1,9+(3-(seq&3))*16);p16(v+5,16);p16(v+15,9);memcpy(v+21,remote,6);v[27]=4;p16(v+29,seq);
    size_t at=74;f[at]=18;p16(f+at+1,22);v=f+at+3;v[0]=15;v[3]=3;v[4]=v[5]=255;memset(v+6,149,16);at+=25;
    f[at]=24;p16(f+at+1,40);v=f+at+3;memcpy(v,remote,6);memcpy(v+6,remote,6);p32(v+12,11);p32(v+16,0);p32(v+20,777);p32(v+24,777);p32(v+36,11);at+=43;
    f[at]=21;p16(f+at+1,2);f[at+3]=0x3e;f[at+4]=2;return at+5;
}
static size_t template(uint8_t *t){
    memset(t,0,256);size_t at=0;t[at]=5;p16(t+at+1,21);uint8_t *v=t+at+3;memcpy(v+5,local,6);p32(v+11,65);p32(v+15,65);at+=24;
    t[at]=24;p16(t+at+1,40);v=t+at+3;memcpy(v,local,6);memcpy(v+6,local,6);p32(v+12,9);p32(v+20,65);p32(v+24,65);p32(v+36,9);at+=43;
    t[at]=12;p16(t+at+1,15);v=t+at+3;p16(v,0x8f24);memcpy(v+2,"ZZ\0",3);p16(v+5,1);memcpy(v+7,local,6);at+=18;
    t[at]=6;p16(t+at+1,9);v=t+at+3;v[3]=0x7a;v[5]=0x55;at+=12;
    t[at]=21;p16(t+at+1,2);t[at+3]=0x3e;t[at+4]=1;return at+5;
}
static void election_and_channel(void){
    struct owly_native_link link;struct owly_sync sync;uint8_t raw[256],t[256],snapshot[256],frame[2048],root[24];size_t n;
    owly_link_reset(&link);owly_link_reconcile(&link,true,local,149,1000000);link.sync_enabled=true;
    const uint16_t shared = (1u << 0) | (1u << 2) | (1u << 10);
    uint64_t now = 0;
    for (unsigned i = 0; i < 3; ++i) {
        now = 2000000 + i * 7 * OWLY_LINK_AW_US;
        size_t count = peer(raw, 100 + i * 7);
        uint8_t *channels = find(raw, count, 18);
        CHECK(channels);
        for (unsigned j = 0; j < 16; ++j)
            channels[6+j] = shared & (1u << j) ? 149 : 44;
        CHECK(owly_link_observe(&link, raw, count, 149, -35, now));
    }
    size_t tn=template(t);memcpy(snapshot,t,tn);owly_sync_reset(&sync);CHECK(!owly_sync_template(&sync,t,tn,local));owly_sync_request(&sync,true);owly_sync_reconcile(&sync,true,149,now);
    CHECK(owly_link_window(&link,now,&sync.peer_clock)&&sync.peer_clock.remote);
    sync.have_master_election=owly_link_root_election(&link,now,sync.master_election);CHECK(sync.have_master_election);CHECK(g32(sync.master_election+20)==777);
    sync.shared_slots = owly_link_shared_slots(&link, now);
    CHECK(sync.shared_slots == shared);
    CHECK(owly_sync_frame(&sync,now,frame,sizeof(frame),&n)==1);
    uint8_t *v2=find(frame,n,24),*v1=find(frame,n,5),*path=find(frame,n,12),*service=find(frame,n,6),*timing=find(frame,n,4);
    CHECK(v2&&v1&&path&&service&&timing);CHECK(!memcmp(v2,remote,6)&&!memcmp(v2+6,remote,6));
    CHECK(g32(v2+12)==11&&g32(v2+16)==1&&g32(v2+20)==777);CHECK(g32(v2+24)==65&&g32(v2+36)==9);
    CHECK(v1[3]==1&&!memcmp(v1+5,remote,6)&&g32(v1+11)==777&&g32(v1+15)==65);
    CHECK(!memcmp(timing+21,remote,6));CHECK(g16(timing+29)==114&&g16(timing+1)==25&&g16(timing+15)==0);
    uint8_t *channels = find(frame, n, 18);
    CHECK(channels);
    for (unsigned i = 0; i < 16; ++i) {
        const bool available = (shared & (1u << i)) != 0;
        CHECK(timing[39+2*i] == (available ? 0x1d : 0));
        CHECK(timing[40+2*i] == (available ? 151 : 0));
        CHECK(channels[6+2*i] == (available ? 149 : 0));
        CHECK(channels[7+2*i] == (available ? 0x80 : 0));
    }
    CHECK(g16(path)==0x8f24&&g16(path+5)==4&&!memcmp(path+2,"ZZ\0",3)&&!memcmp(path+7,local,6));
    CHECK(service[3]==0x7a&&service[5]==0x55);CHECK(!memcmp(snapshot,t,tn)&&!memcmp(sync.template_bytes,t,tn));
    /* An old remote window must not be paired with a new local clock. */
    sync.due_us=now+OWLY_LINK_AW_US+1;CHECK(owly_sync_frame(&sync,sync.due_us,frame,sizeof(frame),&n)==1);
    v2=find(frame,n,24);v1=find(frame,n,5);timing=find(frame,n,4);
    CHECK(!memcmp(v2,local,6)&&!memcmp(v2+6,local,6)&&g32(v2+16)==0&&g32(v2+20)==65&&g32(v2+12)==9);
    CHECK(v1[3]==0&&!memcmp(v1+5,local,6)&&g32(v1+11)==65);CHECK(!memcmp(timing+21,local,6));
    channels = find(frame, n, 18);
    CHECK(channels && channels[8] == 149 && timing[42] == 151);
    /* Guard the backend-private root snapshot independently of exported ABI. */
    link.tx_paused=true;memset(root,0xaa,sizeof(root));CHECK(!owly_link_root_election(&link,now,root));CHECK(!memcmp(root,(uint8_t[24]){0},24));link.tx_paused=false;
    CHECK(!owly_link_root_election(&link,now+OWLY_LINK_PEER_TTL_US+1,root));CHECK(!owly_link_root_election(NULL,now,root));CHECK(!owly_link_root_election(&link,now,NULL));
    CHECK(!owly_link_shared_slots(&link, now+OWLY_LINK_PEER_TTL_US+1));
    size_t count=peer(raw,121);uint8_t *rx=find(raw,count,24);rx[20]^=1;rx[16]=1;
    CHECK(owly_link_observe(&link,raw,count,149,-35,now+7*OWLY_LINK_AW_US));CHECK(!link.peers[0].has_election);
}
static void path_variants(void){
    for(unsigned ch=0;ch<3;ch++)for(unsigned country=0;country<2;country++)for(unsigned short_value=0;short_value<2;short_value++){
        const unsigned channel=(unsigned[]){6,44,149}[ch],map=2+3*country;
        uint8_t t[32]={12,0,0},f[2048];size_t size=map+2-short_value;t[1]=size;p16(t+3,0x200|(country?0x100:0));
        if(country)memcpy(t+5,"VN\0",3);
        if(!short_value)p16(t+3+map,7);
        size_t at=3+size;t[at]=21;t[at+1]=2;t[at+3]=0x3e;t[at+4]=1;at+=5;
        struct owly_sync s;owly_sync_reset(&s);CHECK(!owly_sync_template(&s,t,at,local));owly_sync_request(&s,true);owly_sync_reconcile(&s,true,channel,1000000);
        size_t n;CHECK(owly_sync_frame(&s,1000000,f,sizeof(f),&n)==1);uint8_t *p=find(f,n,12);CHECK(p);
        if(short_value)CHECK(!memcmp(p,t+3,size));else CHECK(g16(p+map)==(1u<<ch));
        if(country)CHECK(!memcmp(p+2,"VN\0",3));
    }
}
static void native_mi(void)
{
    static const uint8_t broadcast[6]={255,255,255,255,255,255};
    static const uint8_t request[]={1,4,0,0x11,0x22,0x33,0x44};
    static const uint8_t response[]={2,5,0,0,0xc0,0x0c,0,0x21};
    struct owly_sync s;
    uint8_t psf[256], mi[256], frame[2048];
    size_t n, pos=0;
    uint64_t now=1000000;
    const size_t psf_len=template(psf);
    memcpy(mi,psf+24,43); /* Legacy MI begins with election v2. */
    pos+=43;
    memcpy(mi+pos,request,sizeof(request));pos+=sizeof(request);
    memcpy(mi+pos,response,sizeof(response));pos+=sizeof(response);
    memcpy(mi+pos,psf+67,psf_len-67);pos+=psf_len-67;
    mi[pos++]=4;mi[pos++]=1;mi[pos++]=0;mi[pos++]=6;
    mi[pos++]=18;mi[pos++]=1;mi[pos++]=0;mi[pos++]=6;

    owly_sync_reset(&s);
    CHECK(!owly_sync_mi(&s,true,broadcast,mi,pos,local,149,now));
    CHECK(!owly_sync_template(&s,psf,psf_len,local));
    CHECK(s.auto_mi.length==pos-8);
    CHECK(owly_sync_mi(&s,true,remote,mi,pos,local,149,now)==OWLY_MI_INVALID);
    CHECK(owly_sync_mi(&s,true,broadcast,mi,pos-1,local,149,now)==OWLY_MI_INVALID);
    CHECK(owly_sync_mi(&s,true,broadcast,mi,OWLY_MI_TEMPLATE_MAX+1,local,149,now)==OWLY_MI_INVALID);
    CHECK(s.auto_mi.length==pos-8);
    owly_sync_request(&s,true);
    owly_sync_reconcile(&s,true,149,now);
    CHECK(owly_sync_frame(&s,now,frame,sizeof(frame),&n)==1 && frame[30]==0);
    CHECK(!find(frame,n,1) && !find(frame,n,2));
    CHECK(owly_sync_frame(&s,++now,frame,sizeof(frame),&n)==1 && frame[30]==3);
    CHECK(!memcmp(frame+4,broadcast,6) && !memcmp(frame+10,local,6));
    CHECK(g16(frame+41)==73 && g16(frame+117)==41 && n==160+pos-8);
    CHECK(find(frame,n,4)[0]==149 && find(frame,n,18)[6]==149);
    CHECK(!memcmp(find(frame,n,1),request+3,sizeof(request)-3));
    CHECK(!memcmp(find(frame,n,2),response+3,sizeof(response)-3));
    owly_sync_result(&s,0);
    CHECK(s.native_mi_handed_off==1 && s.handed_off==1);
    const uint64_t next_broadcast=s.mif_due_us;
    CHECK(!owly_sync_mi(&s,true,broadcast,mi,pos,local,149,now));
    CHECK(s.mif_due_us==next_broadcast);
    CHECK(!owly_sync_mi(&s,false,remote,mi,pos,local,149,now));
    CHECK(owly_sync_mi(&s,false,broadcast,mi,pos,local,149,now)==OWLY_MI_BUSY);
    CHECK(owly_sync_frame(&s,++now,frame,sizeof(frame),&n)==0 && s.pending_mi.length);
    s.pending_mi_ready=true;
    CHECK(owly_sync_frame(&s,++now,frame,sizeof(frame),&n)==1 && frame[30]==3);
    CHECK(!memcmp(frame+4,remote,6) && s.pending_mi.length && s.mif_due_us==next_broadcast);
    owly_sync_result(&s,-ENOSPC);
    CHECK(s.pending_mi.length && s.active && !s.blocked);
    CHECK(owly_sync_frame(&s,++now,frame,sizeof(frame),&n)==1 && !memcmp(frame+4,remote,6));
    owly_sync_result(&s,-1);
    CHECK(!s.pending_mi.length && s.active && !s.blocked && s.failed==2 && s.native_mi_handed_off==1);
    CHECK(!owly_sync_mi(&s,false,broadcast,mi,pos,local,149,now));
    owly_sync_reconcile(&s,true,44,++now);
    CHECK(owly_sync_frame(&s,now,frame,sizeof(frame),&n)==1 && !s.pending_mi.length);
    CHECK(!owly_sync_mi(&s,false,remote,mi,pos,local,44,now));
    now+=OWLY_MI_PENDING_US;
    CHECK(owly_sync_frame(&s,now,frame,sizeof(frame),&n)==1 && !s.pending_mi.length);
    CHECK(!owly_sync_mi(&s,true,broadcast,NULL,0,local,44,now) && !s.auto_mi.length);
    CHECK(!owly_sync_mi(&s,true,broadcast,mi,pos,local,44,now));
    owly_sync_request(&s,false);
    CHECK(s.auto_mi.length==pos-8 && !s.pending_mi.length && s.native_mi_handed_off==1);
    CHECK(!owly_sync_template(&s,NULL,0,local) && !s.auto_mi.length);
}

int main(void)
{
    election_and_channel();
    path_variants();
    native_mi();
    printf("transport wire and native MI: %u checks passed\n",checks);
    return 0;
}
