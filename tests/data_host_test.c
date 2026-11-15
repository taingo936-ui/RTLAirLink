/* SPDX-License-Identifier: GPL-2.0 */
#include "owly_awdl_sync.h"
#include "owly_native_link.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint8_t local[6]={2,0x11,0x22,0x33,0x44,0x55};
static const char host[]="12345678-1234-4abc-8def-1234567890ab";
static const uint8_t ip_local[16]={0xfe,0x80,0,0,0,0,0,0,0,0x11,0x22,0xff,0xfe,0x33,0x44,0x55};
static unsigned checks;
#define CHECK(x) do {checks++;assert(x);} while(0)
static void be16(uint8_t *p,unsigned v){p[0]=v>>8;p[1]=v;}
static size_t name(uint8_t *p,const char *s){size_t n=0;while(*s){const char *end=strchr(s,'.');size_t len=end?(size_t)(end-s):strlen(s);p[n++]=len;memcpy(p+n,s,len);n+=len;if(!end)break;s=end+1;}p[n++]=0;return n;}
static size_t packet(uint8_t *eth,bool compressed){
    memset(eth,0,1024);memcpy(eth,(uint8_t[]){0x33,0x33,0,0,0,0xfb},6);memcpy(eth+6,local,6);eth[12]=0x86;eth[13]=0xdd;
    uint8_t *ip=eth+14,*dns=ip+48;ip[0]=0x60;ip[6]=17;ip[7]=255;memcpy(ip+8,ip_local,16);ip[24]=0xff;ip[25]=2;ip[39]=0xfb;
    be16(ip+40,5353);be16(ip+42,5353);dns[2]=0x84;be16(dns+6,2);
    size_t at=12;at+=name(dns+at,"service._airdrop._tcp.local");
    be16(dns+at,33);be16(dns+at+2,0x8001);dns[at+7]=120;be16(dns+at+8,50);at+=10;be16(dns+at+4,8770);at+=6;
    const size_t target=at;char full[44];snprintf(full,sizeof(full),"%s.local",host);at+=name(dns+at,full);
    if(compressed){dns[at++]=0xc0|(target>>8);dns[at++]=target;}else at+=name(dns+at,full);
    be16(dns+at,28);be16(dns+at+2,0x8001);dns[at+7]=120;be16(dns+at+8,16);at+=10;memcpy(dns+at,ip_local,16);at+=16;
    be16(ip+4,at+8);be16(ip+44,at+8);return 14+48+at;
}
static void start(struct owly_sync *s){
    static const uint8_t tlvs[]={6,9,0,0,0,0,0,0,0,0,0,0,21,2,0,0x3e,1};
    owly_sync_reset(s);CHECK(!owly_sync_template(s,tlvs,sizeof(tlvs),local));owly_sync_request(s,true);owly_sync_reconcile(s,true,149,1000000);
}
static unsigned arpa(const uint8_t *frame,size_t n,const uint8_t **value){unsigned count=0;size_t at=40;while(at+3<=n){size_t len=frame[at+1]|(size_t)frame[at+2]<<8;CHECK(at+3+len<=n);if(frame[at]==16){count++;if(value)*value=frame+at+3;}at+=3+len;}CHECK(at==n);return count;}
static void host_identity(void){
    uint8_t eth[1024],frame[2048],wrapped[1040];struct owly_sync s;size_t n=packet(eth,false),size=0;const uint8_t *value=NULL;
    start(&s);CHECK(owly_sync_note_tx(&s,eth,n));CHECK(s.have_host && !memcmp(s.host,host,36));CHECK(!owly_sync_note_tx(&s,eth,n));
    CHECK(owly_sync_frame(&s,1000000,frame,sizeof(frame),&size)==1);CHECK(frame[30]==0 && !arpa(frame,size,NULL));
    CHECK(owly_sync_frame(&s,1001000,frame,sizeof(frame),&size)==1);CHECK(frame[30]==3 && arpa(frame,size,&value)==1);
    CHECK(value[0]==3 && value[1]==36 && !memcmp(value+2,host,36) && value[38]==0xc0 && value[39]==0x0c);
    owly_sync_reconcile(&s,false,149,1120000);CHECK(s.have_host);owly_sync_reconcile(&s,true,149,1130000);CHECK(s.have_host);
    start(&s);n=packet(eth,true);CHECK(owly_sync_note_tx(&s,eth,n));
    start(&s);memcpy(wrapped,eth,12);size_t payload=n-14+16;be16(wrapped+12,payload);memcpy(wrapped+14,(uint8_t[]){0xaa,0xaa,3,0,0x17,0xf2,8,0,3,0,0,0,0,0,0x86,0xdd},16);memcpy(wrapped+30,eth+14,n-14);
    CHECK(owly_sync_note_tx(&s,wrapped,n+16));CHECK(s.have_host && !memcmp(s.host,host,36));
    /* A native template's own ARPA remains authoritative and is not doubled. */
    static const uint8_t native[]={21,2,0,0x3e,1,16,5,0,3,1,'x',0xc0,0x0c};
    CHECK(!owly_sync_template(&s,native,sizeof(native),local));s.mif_due_us=0;
    CHECK(owly_sync_frame(&s,1200000,frame,sizeof(frame),&size)==1);
    CHECK(arpa(frame,size,&value)==1 && value[1]==1 && value[2]=='x');
    uint8_t mac[6];memcpy(mac,local,6);mac[5]^=2;static const uint8_t t[]={21,2,0,0x3e,1};CHECK(!owly_sync_template(&s,t,sizeof(t),mac));CHECK(!s.have_host);
}
static void reject_foreign_and_malformed(void){
    uint8_t eth[1024];struct owly_sync s;
    start(&s);size_t n=packet(eth,true);eth[6]^=2;CHECK(!owly_sync_note_tx(&s,eth,n) && !s.have_host);
    n=packet(eth,true);eth[14+8+15]^=2;CHECK(!owly_sync_note_tx(&s,eth,n) && !s.have_host);
    n=packet(eth,true);eth[n-1]^=2;CHECK(!owly_sync_note_tx(&s,eth,n) && !s.have_host);
    n=packet(eth,true);eth[14+48+2]=0;CHECK(!owly_sync_note_tx(&s,eth,n) && !s.have_host);
    n=packet(eth,true);eth[n-28]=0xc0;eth[n-27]=(uint8_t)(n-28-62);CHECK(!owly_sync_note_tx(&s,eth,n) && !s.have_host);
    n=packet(eth,true);eth[n-16-3]=0;/* zero the AAAA TTL */memset(eth+n-16-6,0,4);CHECK(!owly_sync_note_tx(&s,eth,n) && !s.have_host);
    for(size_t cut=1;cut<100;cut++){
        n=packet(eth,true);size_t smaller=n-cut;if(smaller>=62){be16(eth+14+4,smaller-54);be16(eth+14+44,smaller-54);}CHECK(!owly_sync_note_tx(&s,eth,smaller));CHECK(!s.have_host);
    }
    /* Mutation inputs exercise bounded DNS lengths, counts and pointers under
     * sanitizers; successful parses still require the local SRV/AAAA binding. */
    for(unsigned i=0;i<2000;i++) {
        n=packet(eth,true);unsigned at=62+(i*7919u)%(n-62);eth[at]^=(uint8_t)(1u+(i%255u));
        s.have_host=false;(void)owly_sync_note_tx(&s,eth,n);
    }
}
static void tx_sequence(void){
    uint8_t eth[1024],wrapped[1040],out[2048];size_t n=packet(eth,false),length=0;
    memcpy(wrapped,eth,12);size_t payload=n-14+16;be16(wrapped+12,payload);memcpy(wrapped+14,(uint8_t[]){0xaa,0xaa,3,0,0x17,0xf2,8,0,3,0,0,0,0,0,0x86,0xdd},16);memcpy(wrapped+30,eth+14,n-14);
    for(unsigned seq=65534;seq<=65538;seq++){
        CHECK(!owly_link_build_tx(wrapped,n+16,local,seq,seq,out,sizeof(out),&length));
        CHECK(out[32]==3 && out[33]==4 && out[34]==(seq&255) && out[35]==((seq>>8)&255));
        CHECK(!memcmp(out+40,eth+14,n-14));CHECK(wrapped[24]==0 && wrapped[25]==0);
    }
}
int main(void){host_identity();reject_foreign_and_malformed();tx_sequence();printf("data/host actual production: %u checks passed; no RF proof\n",checks);return 0;}
