/* SPDX-License-Identifier: GPL-2.0 */
/* The real CAM transaction, with only firmware and driver lookup replaced. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
typedef uint8_t u8;
#define ETH_ALEN 6
#define RTL8852B 1
#define NL80211_IFTYPE_STATION 2
#define RTW89_NET_TYPE_NO_LINK 0
#define RTW89_ROLE_INFO_CHANGE 3
struct ieee80211_vif {bool p2p;int type;struct {bool assoc;} cfg;};
struct ieee80211_hw {bool wiphy;};
struct rtw89_dev {struct ieee80211_hw *hw;struct {unsigned chip_id;} *chip;bool scanning;};
struct rtw89_vif_link {u8 mac_addr[6];struct {bool valid;} addr_cam;int net_type;};
struct rtw89_vif {struct rtw89_dev *rtwdev;struct {void *next,*prev;} dlink_pool;};
static struct ieee80211_vif vif;static struct ieee80211_hw hw={true};
static typeof(*((struct rtw89_dev *)0)->chip) chip={RTL8852B};
static struct rtw89_dev dev={&hw,&chip,false};static struct rtw89_vif role;
static struct rtw89_vif_link link;static bool no_link,empty;
static unsigned writes,errors;static int fail_first,fail_second;
static u8 sent[2][6];
static const u8 station[6]={2,1,2,3,4,5},awdl[6]={2,6,7,8,9,10};
static bool is_zero_ether_addr(const u8 *a){return !memcmp(a,(u8[6]){0},6);}
static bool is_multicast_ether_addr(const u8 *a){return a[0]&1;}
static bool ether_addr_equal(const u8 *a,const u8 *b){return !memcmp(a,b,6);}
static void ether_addr_copy(u8 *d,const u8 *s){memcpy(d,s,6);}
static struct rtw89_vif *vif_to_rtwvif(struct ieee80211_vif *v){assert(v==&vif);return &role;}
#define lockdep_assert_wiphy(w) assert(w)
#define list_empty(p) (empty)
static struct rtw89_vif_link *rtw89_get_designated_link(struct rtw89_vif *v){assert(v==&role);return no_link?NULL:&link;}
#define rtw89_err(...) (++errors)
static int rtw89_fw_h2c_cam(struct rtw89_dev *d,struct rtw89_vif_link *l,void *unused,const u8 *a,int change) {
 assert(d==&dev && l==&link && !unused && change==RTW89_ROLE_INFO_CHANGE);
 assert(!memcmp(l->mac_addr,a,6) && writes<2);memcpy(sent[writes],a,6);writes++;
 return writes==1 ? fail_first : fail_second;
}
#include "owly_awdl_self_addr.inc"
static void reset(void) {
 memset(&vif,0,sizeof(vif));vif.type=NL80211_IFTYPE_STATION;
 role=(struct rtw89_vif){&dev,{&role,&role}};link.net_type=RTW89_NET_TYPE_NO_LINK;
 link.addr_cam.valid=true;memcpy(link.mac_addr,station,6);dev.scanning=false;chip.chip_id=RTL8852B;
 no_link=empty=false;writes=errors=0;fail_first=fail_second=0;memset(sent,0,sizeof(sent));
}
int main(void) {
 reset();assert(rtw89_compat_awdl_self_addr(&vif,awdl,false)==1 && writes==1);
 assert(!memcmp(link.mac_addr,awdl,6) && !memcmp(sent[0],awdl,6));
 assert(!rtw89_compat_awdl_self_addr(&vif,awdl,false) && writes==1);
 writes=0;assert(rtw89_compat_awdl_self_addr(&vif,station,true)==1 && writes==1);
 assert(!memcmp(link.mac_addr,station,6));
 reset();assert(rtw89_compat_awdl_self_addr(&vif,station,true)==1 && writes==1);
 reset();fail_first=-EIO;assert(rtw89_compat_awdl_self_addr(&vif,awdl,false)==-EIO && writes==2);
 assert(!memcmp(sent[0],awdl,6) && !memcmp(sent[1],station,6) && !memcmp(link.mac_addr,station,6));
 reset();fail_first=fail_second=-EIO;assert(rtw89_compat_awdl_self_addr(&vif,awdl,false)==-EIO && errors==1);
 assert(!memcmp(link.mac_addr,station,6));
 reset();role.dlink_pool.next=NULL;assert(rtw89_compat_awdl_self_addr(&vif,awdl,false)==-ENETDOWN && !writes);
 reset();empty=true;assert(rtw89_compat_awdl_self_addr(&vif,awdl,false)==-ENETDOWN && !writes);
 reset();no_link=true;assert(rtw89_compat_awdl_self_addr(&vif,awdl,false)==-ENETDOWN && !writes);
 reset();link.addr_cam.valid=false;assert(rtw89_compat_awdl_self_addr(&vif,awdl,false)==-ENETDOWN && !writes);
 for(unsigned i=0;i<5;i++) {
  reset();if(i==0)vif.cfg.assoc=true;if(i==1)dev.scanning=true;if(i==2)vif.p2p=true;if(i==3)link.net_type=2;if(i==4)vif.type=3;
  assert(rtw89_compat_awdl_self_addr(&vif,awdl,false)==-EBUSY && !writes);
 }
 reset();chip.chip_id=2;assert(rtw89_compat_awdl_self_addr(&vif,awdl,false)==-EOPNOTSUPP && !writes);
 reset();assert(rtw89_compat_awdl_self_addr(&vif,(u8[6]){1},false)==-EINVAL && !writes);
 assert(rtw89_compat_awdl_self_addr(&vif,(u8[6]){0},false)==-EINVAL && !writes);
 assert(rtw89_compat_awdl_self_addr(NULL,awdl,false)==-EINVAL);
 puts("production self-address CAM: write/no-op/forced restore, firmware rollback, radio lifetime and AP/scan exclusion passed");
}
