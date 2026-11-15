/* SPDX-License-Identifier: GPL-2.0 */
/* Replay against both old and patched production serializers. */
#include "owly_awdl_sync.h"
#include "rtw89_awdl_action_decode.h"
#include <stdio.h>
#include <string.h>
int main(void)
{
    static const unsigned char v[]={21,2,0,0x3e,1},own[]={2,1,2,3,4,5},root[]={2,6,7,8,9,10};
    struct owly_sync s;unsigned char f[OWLY_SYNC_FRAME_MAX];size_t n=0;
    owly_sync_reset(&s);owly_sync_template(&s,v,sizeof(v),own);
    owly_sync_request(&s,1);owly_sync_reconcile(&s,1,149,2000000);
    s.peer_clock.active=1;s.peer_clock.remote=1;s.peer_clock.channel=149;s.peer_clock.master_channel=149;
    s.peer_clock.sampled_us=2000000;s.peer_clock.sequence=100;s.peer_clock.until_next_tu=4;
    memcpy(s.peer_clock.master,root,6);s.have_master_election=1;memcpy(s.master_election,root,6);
    if(owly_sync_frame(&s,2006144,f,sizeof(f),&n)!=1)return 2;
    printf("sequence=%u slot_countdown=%u; expected sequence=101 slot_countdown=46\n",owly_le16(f+72),owly_le16(f+44));
    return !(owly_le16(f+72)==101 && owly_le16(f+44)==46);
}
