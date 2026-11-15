/* SPDX-License-Identifier: GPL-2.0 */
/* Execute the production request builder; only allocation/driver I/O mocked. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "owly_native_scan_policy.h"
typedef uint8_t u8;
#define NUM_NL80211_BANDS 2
#define NL80211_BAND_2GHZ 0
#define NL80211_BAND_5GHZ 1
#define IEEE80211_CHAN_DISABLED 1u
#define GFP_KERNEL 0
#define GLUE_SCAN_SLICE 8
#define OWLY_NATIVE_PEER_DATA 1
struct ieee80211_channel {unsigned flags,center_freq;};
struct ieee80211_supported_band {unsigned n_channels;struct ieee80211_channel *channels;};
struct cfg80211_ssid {u8 ssid[32];unsigned ssid_len;};
struct wiphy {struct ieee80211_supported_band *bands[2];bool locked;};
struct cfg80211_scan_request {struct cfg80211_ssid *ssids;unsigned n_ssids,n_channels;struct wiphy *wiphy;unsigned long scan_start;u8 mac_addr[6],bssid[6];const u8 *ie;unsigned ie_len;struct ieee80211_channel *channels[];};
struct ieee80211_scan_request {struct {u8 *ies[2];unsigned len[2];} ies;struct cfg80211_scan_request req;};
struct ieee80211_hw {struct wiphy *wiphy;};
struct ieee80211_vif {u8 addr[6];struct {bool assoc;} cfg;};
struct rtw89_m80211_local {const struct {int (*hw_scan)(struct ieee80211_hw *,struct ieee80211_vif *,struct ieee80211_scan_request *);} *ops;};
static struct ieee80211_channel channels[39];
static struct ieee80211_supported_band band={39,channels};
static struct wiphy phy={.bands={&band,NULL}};static struct ieee80211_hw hw={&phy};
static struct ieee80211_vif vif={.addr={2,1,2,3,4,5}};
static struct {bool up,scanning,scan_connected;struct ieee80211_vif *vif;struct ieee80211_scan_request *scan_req;
 unsigned scan_slice_pos,last_scan_channels,scans_connected,scan_gen;unsigned long scan_started;bool bss_lock;} glue;
static u8 glue_ies_2ghz[2]={0},glue_ies_5ghz[2]={0};static unsigned long jiffies=1234;
static bool protect;static unsigned submitted;
static void IOLog(const char *format,...) {(void)format;}
static void *kzalloc(size_t n,int flags) {(void)flags;return calloc(1,n);}
static void kfree(void *p) {free(p);}
static void ether_addr_copy(u8 *d,const u8 *s) {memcpy(d,s,6);}
static void eth_broadcast_addr(u8 *d) {memset(d,255,6);}
static struct ieee80211_hw *glue_hw(void) {return &hw;}
static void wiphy_lock(struct wiphy *w) {assert(!w->locked);w->locked=true;}
static void wiphy_unlock(struct wiphy *w) {assert(w->locked);w->locked=false;}
static void spin_lock(bool *p) {assert(!*p);*p=true;}
static void spin_unlock(bool *p) {assert(*p);*p=false;}
static bool owly_scan_protect_locked(bool *busy) {assert(phy.locked);if(busy)*busy=false;return protect;}
static int scan(struct ieee80211_hw *h,struct ieee80211_vif *v,struct ieee80211_scan_request *s) {
 assert(h==&hw && v==&vif && phy.locked && glue.scanning);submitted++;
 assert(s->req.n_channels && s->req.wiphy==&phy && s->req.n_ssids==1);
 assert(!memcmp(s->req.mac_addr,vif.addr,6));return 0;
}
static const typeof(*((struct rtw89_m80211_local *)0)->ops) ops={scan};
static struct rtw89_m80211_local local={&ops};
static struct rtw89_m80211_local *hw_to_local(struct ieee80211_hw *h) {assert(h==&hw);return &local;}
#include "production_scan_builder.inc"
static void reset(void) {
 free(glue.scan_req);memset(&glue,0,sizeof(glue));glue.up=true;glue.vif=&vif;vif.cfg.assoc=false;
 protect=false;submitted=0;for(unsigned i=0;i<39;i++){channels[i].flags=0;channels[i].center_freq=2412+i*5;}
}
int main(void) {
 reset();protect=true;assert(!glue_scan_locked());assert(glue.last_scan_channels==4 && !glue.scan_connected && !vif.cfg.assoc);
 unsigned total=4;glue.scanning=false;
 for(unsigned i=0;i<9;i++){assert(!glue_scan_locked());total+=glue.last_scan_channels;glue.scanning=false;}
 assert(total==39 && glue.scan_slice_pos==0 && submitted==10);
 reset();vif.cfg.assoc=true;assert(!glue_scan_locked());assert(glue.last_scan_channels==8 && glue.scan_connected);
 reset();assert(!glue_scan_locked());assert(glue.last_scan_channels==39 && !glue.scan_connected);
 reset();protect=true;glue.scanning=true;assert(glue_scan_locked()==-EBUSY && !submitted);
 reset();for(unsigned i=0;i<39;i++)channels[i].flags=IEEE80211_CHAN_DISABLED;
 assert(glue_scan_locked()==-EINVAL && !submitted);
 assert(owly_native_scan_cached(true,true,100,0));
 assert(owly_native_scan_cached(true,false,200,100));
 assert(!owly_native_scan_cached(true,false,OWLY_NATIVE_SCAN_GAP_NS+100,100));
 assert(!owly_native_scan_cached(true,false,10,100));
 assert(!owly_native_scan_cached(false,true,100,0));
 free(glue.scan_req);puts("production scan builder: four-channel AWDL slices, full sweep, AP/idle scans and cache timing passed");
}
