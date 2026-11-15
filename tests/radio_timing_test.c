/* SPDX-License-Identifier: GPL-2.0 */
#include "owly_radio_timing.h"
#include <assert.h>
#include <stdio.h>
static void frame(uint8_t f[40],unsigned type)
{
    static const uint8_t own[6]={2,1,2,3,4,5},b[6]={0,0x25,0,0xff,0x94,0x73};
    memset(f,0,40);f[0]=(uint8_t)type;memcpy(f+10,own,6);memcpy(f+16,b,6);
}
static unsigned sequence(const uint8_t *f){return ((unsigned)f[22]|(unsigned)f[23]<<8)>>4;}
int main(void)
{
    struct owly_radio_sequence tx={0};uint8_t action[40],data[40];
    frame(action,0xd0);frame(data,8);
    assert(owly_radio_assign_sequence(&tx,action,sizeof(action)));
    assert(owly_radio_assign_sequence(&tx,data,sizeof(data)));
    assert(sequence(action)==0 && sequence(data)==1);
    for(unsigned i=2;i<8200;i++) {
        uint8_t f[40];frame(f,i&1 ? 8:0xd0);
        assert(owly_radio_assign_sequence(&tx,f,sizeof(f)));
        assert(sequence(f)==(i&4095u));
    }
    uint8_t invalid[40];frame(invalid,8);invalid[1]=0x40;
    uint16_t saved=tx.next;
    assert(!owly_radio_assign_sequence(&tx,invalid,sizeof(invalid)) && tx.next==saved);
    frame(invalid,8);invalid[16]=1;
    assert(!owly_radio_assign_sequence(&tx,invalid,sizeof(invalid)) && tx.next==saved);
    frame(invalid,8);invalid[15]++;
    assert(owly_radio_assign_sequence(&tx,invalid,sizeof(invalid)) && sequence(invalid)==0);
    uint16_t seq,left;
    assert(owly_radio_project_window(2000000,100,4,2006144,&seq,&left));
    assert(seq==101 && left==14);
    assert(owly_radio_project_window(2000000,65535,1,2001024,&seq,&left));
    assert(seq==0 && left==16);
    assert(!owly_radio_project_window(2000000,100,4,1999999,&seq,&left));
    assert(!owly_radio_project_window(2000000,100,4,2016384,&seq,&left));
    assert(!owly_radio_project_window(2000000,100,0,2000000,&seq,&left));
    struct owly_radio_rx_clock clock={0};
    assert(owly_radio_rx_time(&clock,1000100,1000,true)==1000100);
    assert(owly_radio_rx_time(&clock,1030000,11000,true)==1010100);
    assert(owly_radio_rx_time(&clock,1030100,10000,true)==1009100);
    assert(owly_radio_rx_time(&clock,1030200,0,false)==1030200);
    memset(&clock,0,sizeof(clock));
    assert(owly_radio_rx_time(&clock,0x100000000ull+100,0xfffffff0u,true)==0x100000000ull+100);
    assert(owly_radio_rx_time(&clock,0x100000000ull+1132,0x10u,true)==0x100000000ull+132);
    assert(owly_radio_rx_time(&clock,42,0,true)==42);
    assert(owly_radio_rx_time(&clock,6000100,10,true)==6000100);
    assert(owly_radio_rx_time(&clock,6000200,0xf0000000u,true)==6000200);
    puts("radio timing: shared sequence, wrap, peer projection and delayed RX passed");
}
