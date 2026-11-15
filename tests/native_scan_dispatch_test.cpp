/* SPDX-License-Identifier: GPL-2.0 */
/* Actual nativeRequest scan cases, with driver and IOKit I/O replaced. */
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include "owly_native_scan_policy.h"
using UInt32=uint32_t;
enum {APPLE80211_IOC_SCAN_REQ=1,APPLE80211_IOC_SCAN_REQ_MULTIPLE,
      APPLE80211_IOC_SCANCACHE_CLEAR,APPLE80211_IOC_SCAN_RESULT,
      APPLE80211_M_SCAN_DONE,RTW89_GLUE_LINK_DOWN=0};
enum {kIOReturnSuccess=0,kIOReturnUnsupported=100,kIOReturnBusy=101,kIOReturnError=102};
struct apple80211_scan_result {};
struct rtw89_glue_bss {unsigned ssid_len;};
struct Scratch {uint8_t data[1024];explicit Scratch(unsigned n){assert(n==1024);}uint8_t *bytes(){return data;}};
static uint64_t now_ns=1000000000;
static bool protected_link,data_busy,command_lock,radio_up=true;
static unsigned scans,done_posts,protect_calls;
static int scan_error;
static uint64_t mach_absolute_time(){return now_ns;}
static void absolutetime_to_nanoseconds(uint64_t n,uint64_t *p){*p=n;}
static bool rtw89_glue_awdl_scan_protect(bool *busy){++protect_calls;*busy=data_busy;return protected_link;}
static bool rtw89_glue_is_up(){return radio_up;}
static int rtw89_glue_scan(){assert(command_lock);++scans;return scan_error;}
static bool rtw89_glue_scan_entry(unsigned,rtw89_glue_bss *,uint8_t *,unsigned,size_t *,uint16_t *){return false;}
static void IOLockLock(bool *p){assert(!*p);*p=true;}
static void IOLockUnlock(bool *p){assert(*p);*p=false;}
struct Link {unsigned state=0;};
static bool isConnected(const Link &l){return l.state==2;}
struct Fixture {
 struct State {unsigned scanCursor=0;bool scanInProgress=false,scanBootstrapTried=false;uint64_t lastConnectedScan=0,awdlScansDeferred=0;apple80211_scan_result scanResult;} state;
 State *_ns=&state;Link link;bool *_commandLock=&command_lock;
 void setProperty(const char *,bool){}
 void setProperty(const char *,uint64_t,unsigned){}
 void nativePost(unsigned code,void *,size_t n){assert(code==APPLE80211_M_SCAN_DONE && n==4);++done_posts;}
 void fillScanResult(const rtw89_glue_bss &,uint8_t *,size_t,uint16_t,apple80211_scan_result *,bool){}
 int request(bool isSet,unsigned type,void *data=nullptr) {
  switch(type) {
#include "production_native_scan_cases.inc"
  default:return kIOReturnUnsupported;
  }
 }
 Fixture(){protected_link=data_busy=command_lock=false;radio_up=true;scans=done_posts=protect_calls=0;scan_error=0;now_ns=1000000000;}
 int get(){apple80211_scan_result *result=reinterpret_cast<apple80211_scan_result *>(1);int rc=request(false,APPLE80211_IOC_SCAN_RESULT,&result);assert(!result);return rc;}
};
int main() {
 {Fixture f;assert(!f.request(true,APPLE80211_IOC_SCAN_REQ));assert(scans==1 && f.state.scanInProgress && f.link.state==0);
  assert(!f.request(true,APPLE80211_IOC_SCAN_REQ_MULTIPLE) && scans==1);assert(f.get()==kIOReturnBusy);}
 {Fixture f;protected_link=data_busy=true;
  assert(!f.request(true,APPLE80211_IOC_SCAN_REQ));assert(!scans && done_posts==1 && f.state.scanBootstrapTried);
  assert(!f.request(true,APPLE80211_IOC_SCANCACHE_CLEAR));assert(f.get()==5);
  assert(!scans && f.state.awdlScansDeferred==2 && !f.state.scanInProgress);}
 {Fixture f;protected_link=true;assert(!f.request(true,APPLE80211_IOC_SCAN_REQ));assert(scans==1);
  f.state.scanInProgress=false;now_ns+=1000000000;
  assert(!f.request(true,APPLE80211_IOC_SCAN_REQ) && scans==1 && done_posts==1);
  assert(!f.request(true,APPLE80211_IOC_SCANCACHE_CLEAR));assert(f.get()==5 && scans==1);
  now_ns+=OWLY_NATIVE_SCAN_GAP_NS;assert(!f.request(true,APPLE80211_IOC_SCANCACHE_CLEAR));
  assert(f.get()==kIOReturnBusy && scans==2 && f.state.lastConnectedScan==now_ns);}
 {Fixture f;f.link.state=1;assert(!f.request(true,APPLE80211_IOC_SCAN_REQ) && !scans && done_posts==1);}
 {Fixture f;f.link.state=2;assert(!f.request(true,APPLE80211_IOC_SCAN_REQ) && scans==1);
  f.state.scanInProgress=false;now_ns++;assert(!f.request(true,APPLE80211_IOC_SCAN_REQ) && scans==1);}
 {Fixture f;scan_error=-16;assert(f.request(true,APPLE80211_IOC_SCAN_REQ)==kIOReturnBusy && !f.state.scanInProgress);}
 {Fixture f;radio_up=false;assert(f.get()==5 && !scans);assert(f.request(false,APPLE80211_IOC_SCAN_REQ)==kIOReturnUnsupported);}
 puts("production native scan dispatch: SET/cache, GET bootstrap, cache clear, busy, AP/join and error cleanup passed");
}
