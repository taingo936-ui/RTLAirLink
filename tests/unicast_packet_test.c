/* SPDX-License-Identifier: GPL-2.0 */
/* Synthetic TCP SYN-ACK tests parser/admission only; no network is simulated. */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "owly_unicast_observe.h"
#include "owly_awdl_poll_policy.h"
#include "owly_awdl_frame.h"
#include "owly_native_link.h"
#include "owly_awdl_sync.h"
int main(void)
{
 assert(owly_awdl_poll_delay_us(1000000,1110000,1064000,false,false,false,0)==16384);
 assert(owly_awdl_poll_delay_us(1000000,1008000,1064000,false,false,false,0)==8000);
 assert(owly_awdl_poll_delay_us(1000000,1000500,1064000,false,false,false,0)==1000);
 assert(owly_awdl_poll_delay_us(1000000,999999,1064000,false,false,false,0)==1000);
 assert(owly_awdl_poll_delay_us(1000000,1110000,1064000,true,false,false,0)==1000);
 assert(owly_awdl_poll_delay_us(1000000,1110000,1064000,false,true,false,0)==1000);
 assert(owly_awdl_poll_delay_us(1000000,999000,999000,false,false,true,12)==12288);
 assert(owly_awdl_poll_delay_us(1000000,999000,999000,true,false,true,12)==1000);
 const uint8_t local[6]={2,1,2,3,4,5},peer[6]={2,6,7,8,9,10},tlv[]={21,2,0,0x3e,1};
 struct owly_sync sync={0};struct owly_native_link link={0};
 uint8_t wire[2048],action[2048],ip[60]={0},before[60];size_t length=0,action_len=0;
 assert(!owly_sync_template(&sync,tlv,sizeof(tlv),peer));owly_sync_request(&sync,true);
 owly_sync_reconcile(&sync,true,149,1000000);assert(owly_sync_frame(&sync,1000000,action,sizeof(action),&action_len)==1);
 owly_link_reconcile(&link,true,local,149,2000000);link.sync_enabled=true;
 assert(owly_link_observe(&link,action,action_len,149,-40,2000000));
 ip[0]=0x60;ip[5]=20;ip[6]=6;ip[7]=64;ip[8]=ip[24]=0xfe;ip[9]=ip[25]=0x80;
 ip[40]=0x22;ip[41]=0x42;ip[42]=0xc0;ip[43]=0x01;ip[47]=1;ip[51]=2;ip[52]=0x50;ip[53]=0x12;
 memcpy(before,ip,sizeof(ip));
 assert(!owly_awdl_pack_ipv6(ip,sizeof(ip),local,peer,17,3,wire,sizeof(wire),&length));
 struct owly_awdl_batch batch;
 assert(owly_awdl_parse_80211(wire,length,&batch)==OWLY_AWDL_OK && batch.count==1);
 assert(owly_link_admit_ipv6(&link,batch.parts[0].src_mac,batch.parts[0].dst_mac,
                           batch.parts[0].packet,batch.parts[0].packet_len,2000100)==OWLY_LINK_IPV6_ACCEPTED);
 assert(!memcmp(before,batch.parts[0].packet,sizeof(ip)) && link.data_rx_admitted==1);
 struct owly_native_link continuous=link;
 struct owly_tcp_observation o=owly_observe_ip(batch.parts[0].packet,batch.parts[0].packet_len);
 assert(o.ipv6 && o.tcp && o.flags==0x12 && o.source==8770 && o.destination==49153 && o.sequence==1 && o.acknowledgment==2);
 ip[53]=0x52;assert(owly_observe_ip(ip,60).tcp && owly_observe_ip(ip,60).flags==0x52);ip[53]=0x12;
 size_t n;const uint8_t *observed=owly_observe_frame_ip(wire,length,&n);
 assert(observed && n==60 && !memcmp(observed,ip,60));
 for(size_t i=0;i<length;i++) {size_t size=123;assert(!owly_observe_frame_ip(wire,i,&size) && !size);}
 for(size_t i=0;i<60;i++)assert(!owly_observe_ip(ip,i).ipv6);
 ip[52]=0x40;assert(owly_observe_ip(ip,60).ipv6 && !owly_observe_ip(ip,60).tcp);ip[52]=0x50;
 ip[6]=17;assert(owly_observe_ip(ip,60).ipv6 && !owly_observe_ip(ip,60).tcp);ip[6]=6;
 wire[4]|=1;assert(!owly_observe_unicast_frame(wire,length));wire[4]&=~1;
 link.sync_enabled=false;
 assert(owly_link_admit_ipv6(&link,peer,local,ip,60,2000200)==OWLY_LINK_IPV6_INACTIVE);
 link.sync_enabled=true;
 assert(owly_link_admit_ipv6(&link,peer,peer,ip,60,2000300)==OWLY_LINK_IPV6_BAD_DEST);
 assert(owly_link_admit_ipv6(&link,peer,local,ip,60,5000001)==OWLY_LINK_IPV6_NO_PEER);
 const uint64_t clock_sample=continuous.peers[0].clock_rx_us;
 for(uint64_t now=3000100;now<=6000100;now+=1000000)
     assert(owly_link_admit_ipv6(&continuous,peer,local,ip,60,now)==OWLY_LINK_IPV6_ACCEPTED);
 assert(continuous.peers[0].clock_rx_us==clock_sample);
 struct owly_link_window live={0};(void)owly_link_window(&continuous,6000100,&live);
 assert(live.peers==1 && !live.remote);
 assert(owly_link_admit_ipv6(&continuous,peer,local,ip,60,8000101)==OWLY_LINK_IPV6_NO_PEER);
 puts("synthetic SYN-ACK: production QoS decode, IPv6 admission and byte preservation passed; malformed, stopped and stale peer rejected; no RF proof");
}
