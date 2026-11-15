/* SPDX-License-Identifier: GPL-2.0 */
/* Execute the pinned upstream allocator and H2C implementation. Hardware,
 * allocation and transport are mocked; no claim of firmware/RF execution. */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
typedef uint8_t u8;
typedef uint32_t u32;
struct list_head {struct list_head *next,*prev;};
#define INIT_LIST_HEAD(h) ((h)->next=(h)->prev=(h))
static void list_add_tail(struct list_head *n,struct list_head *h)
{n->prev=h->prev;n->next=h;h->prev->next=n;h->prev=n;}
static void list_del(struct list_head *n)
{n->prev->next=n->next;n->next->prev=n->prev;n->next=n->prev=n;}
#define container_of(p,t,m) ((t *)((char *)(p)-offsetof(t,m)))
#define list_for_each_entry_safe(e,t,h,m) \
 for(struct list_head *_n=(h)->next,*_next; \
     _n!=(h) && ((_next=_n->next),(e)=container_of(_n,__typeof__(*(e)),m), \
                (t)=_next==(h)?NULL:container_of(_next,__typeof__(*(t)),m),1); _n=_next)
#define for_each_set_bit(i,addr,size) for((i)=0;(i)<(int)(size);++(i))if(*(addr)&(1ul<<(i)))
struct rtw89_ba_cam_entry {struct list_head list;u8 tid;};
struct rtw89_cam_info {unsigned long ba_cam_map[1];struct rtw89_ba_cam_entry ba_cam_entry[2];};
struct rtw89_chip_info {unsigned bacam_num,bacam_dynamic_num,bacam_ver;};
struct wiphy {bool held;};
struct ieee80211_hw {struct wiphy *wiphy;};
struct rtw89_dev {const struct rtw89_chip_info *chip;struct rtw89_cam_info cam_info;struct ieee80211_hw *hw;};
struct rtw89_sta_link {struct list_head ba_cam_list;u8 mac_id;};
struct rtw89_vif_link {u8 mac_idx;};
struct ieee80211_ampdu_params {u8 tid;unsigned ssn,buf_size;};
struct rtw89_h2c_ba_cam {u32 w0,w1;};
struct sk_buff {struct rtw89_h2c_ba_cam payload;};
#define lockdep_assert_wiphy(p) assert((p)->held)
#define RTW89_DBG_TXRX 0
#define RTW89_BACAM_V0_EXT 1
#define RTW89_H2C_BA_CAM_W0_MACID 0xffu
#define RTW89_H2C_BA_CAM_W0_ENTRY_IDX 0x300u
#define RTW89_H2C_BA_CAM_W0_VALID 0x400u
#define RTW89_H2C_BA_CAM_W0_TID 0x7800u
#define RTW89_H2C_BA_CAM_W0_BMAP_SIZE 0x38000u
#define RTW89_H2C_BA_CAM_W0_INIT_REQ 0x40000u
#define RTW89_H2C_BA_CAM_W0_SSN 0xfff00000u
#define RTW89_H2C_BA_CAM_W1_ENTRY_IDX_V1 0xffu
#define RTW89_H2C_BA_CAM_W1_STD_EN 0x100u
#define RTW89_H2C_BA_CAM_W1_BAND 0x600u
#define FWCMD_TYPE_H2C 0
#define H2C_CAT_MAC 0
#define H2C_CL_BA_CAM 0
#define H2C_FUNC_MAC_BA_CAM 0
static u32 le32_encode_bits(u32 v,u32 mask){return (v<<__builtin_ctz(mask))&mask;}
static u8 rtw89_core_acquire_bit_map(unsigned long *map,unsigned count)
{for(unsigned i=0;i<count;i++)if(!(*map&(1ul<<i))){*map|=1ul<<i;return (u8)i;}return (u8)count;}
static void rtw89_core_release_bit_map(unsigned long *map,u8 i){*map&=~(1ul<<i);}
#define rtw89_debug(...) ((void)0)
#define rtw89_err(...) ((void)0)
static bool fail_alloc;
static int tx_result;
static unsigned allocations,frees,h2cs;
static struct sk_buff *rtw89_fw_h2c_alloc_skb_with_hdr(struct rtw89_dev *d,u32 n)
{(void)d;assert(n==sizeof(struct rtw89_h2c_ba_cam));if(fail_alloc)return NULL;++allocations;return calloc(1,sizeof(struct sk_buff));}
static void skb_put(struct sk_buff *s,u32 n){assert(s && n==sizeof(s->payload));}
static void *skb_data(struct sk_buff *s){return &s->payload;}
static void rtw89_h2c_pkt_set_hdr(struct rtw89_dev *d,struct sk_buff *s,int a,int b,int c,int e,int f,int g,u32 n)
{(void)d;(void)a;(void)b;(void)c;(void)e;(void)f;(void)g;assert(s && n==sizeof(s->payload));}
static int rtw89_h2c_tx(struct rtw89_dev *d,struct sk_buff *s,bool wait)
{(void)d;assert(s && !wait);++h2cs;if(tx_result)return tx_result;free(s);++frees;return 0;}
static void dev_kfree_skb_any(struct sk_buff *s){free(s);++frees;}
/* Generated from actual core.c/fw.c by check_block_ack.py. The only lexical
 * adaptation maps skb->data to this harness's payload storage. */
#include "production_ba_firmware.inc"
static unsigned entries(struct rtw89_sta_link *s)
{unsigned n=0;for(struct list_head *p=s->ba_cam_list.next;p!=&s->ba_cam_list;p=p->next)++n;return n;}
int main(void)
{
 struct rtw89_chip_info chip={2,4,0};struct wiphy phy={true};struct ieee80211_hw hw={&phy};
 struct rtw89_dev d={.chip=&chip,.hw=&hw};struct rtw89_vif_link vif={0};
 struct rtw89_sta_link ap={.mac_id=1},peer={.mac_id=2};
 INIT_LIST_HEAD(&ap.ba_cam_list);INIT_LIST_HEAD(&peer.ba_cam_list);
 struct ieee80211_ampdu_params a={.tid=0,.ssn=100,.buf_size=64};
 assert(!rtw89_fw_h2c_ba_cam(&d,&vif,&ap,true,&a));
 a.tid=5;assert(!rtw89_fw_h2c_ba_cam(&d,&vif,&ap,true,&a));
 assert(entries(&ap)==2 && h2cs==2 && d.cam_info.ba_cam_map[0]==3);
 /* Both static entries occupied by an AP: TID1 intentionally succeeds
  * without a static reservation or an H2C. The chip handles it dynamically. */
 a.tid=1;unsigned before=h2cs;
 assert(!rtw89_fw_h2c_ba_cam(&d,&vif,&peer,true,&a));
 assert(!entries(&peer) && entries(&ap)==2 && h2cs==before);
 assert(!rtw89_fw_h2c_ba_cam(&d,&vif,&peer,false,&a) && h2cs==before);
 /* A free static slot still uses H2C and propagates allocation/TX errors. */
 a.tid=5;assert(!rtw89_fw_h2c_ba_cam(&d,&vif,&ap,false,&a));
 a.tid=1;fail_alloc=true;
 assert(rtw89_fw_h2c_ba_cam(&d,&vif,&peer,true,&a)==-ENOMEM);
 fail_alloc=false;assert(!rtw89_fw_h2c_ba_cam(&d,&vif,&peer,false,&a));
 tx_result=-EIO;assert(rtw89_fw_h2c_ba_cam(&d,&vif,&peer,true,&a)==-EIO);
 tx_result=0;assert(!rtw89_fw_h2c_ba_cam(&d,&vif,&peer,false,&a));
 /* Upstream may move a non-priority static entry to dynamic service when
  * TID0 needs it. Lists and reservation bits must stay consistent. */
 assert(!rtw89_fw_h2c_ba_cam(&d,&vif,&peer,true,&a));
 a.tid=0;assert(!rtw89_fw_h2c_ba_cam(&d,&vif,&ap,true,&a));
 assert(entries(&peer)==0 && entries(&ap)==2 && d.cam_info.ba_cam_map[0]==3);
 assert(allocations==frees);
 puts("pinned upstream BA allocator/H2C: static exhaustion fallback, priority replacement, allocation/TX errors and ownership passed (hardware mocked)");
 return 0;
}
