/* SPDX-License-Identifier: GPL-2.0 */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "owly_awdl_sync.h"
static struct owly_sync owly_sync_state;
static int owly_cfg_rssi_floor=-128;
static unsigned owly_cfg_rssi_drops;
static struct { bool active,probed,up,scanning; int cmd_lock; } glue;
struct wiphy { int locked; };
struct ieee80211_hw { struct wiphy *wiphy; };
struct ieee80211_channel { unsigned hw_value; };
struct vif { uint8_t addr[6]; };
static struct wiphy phy;
static struct ieee80211_hw hw={&phy};
static struct ieee80211_channel chan={149};
static struct vif vif={{2,1,2,3,4,5}},*owly_core04c_vif=&vif;
static bool ready=true;
static unsigned paused,rechecked;
static void mutex_lock(int *p){assert(!*p);*p=1;}
static void mutex_unlock(int *p){assert(*p==1);*p=0;}
static void wiphy_lock(struct wiphy *p){assert(glue.cmd_lock==1 && !p->locked);p->locked=1;}
static void wiphy_unlock(struct wiphy *p){assert(p->locked==1);p->locked=0;}
static struct ieee80211_hw *glue_hw(void){return &hw;}
static uint64_t glue_now_us(void){return 1000000;}
static struct ieee80211_channel *owly_sync_bound_channel(void){assert(phy.locked==1);return ready?&chan:NULL;}
static void owly_sync_pause(bool clear){assert(!clear && phy.locked==1);++paused;owly_sync_state.active=false;owly_sync_state.due_us=0;}
static void owly_sync_recheck(void){assert(phy.locked==1);++rechecked;owly_sync_reconcile(&owly_sync_state,ready&&!glue.scanning,149,glue_now_us());}
#include "production_config_glue.inc"

int main(void)
{
    struct owly_cfg_u32 channel={0,149};
    struct owly_cfg_rssi rssi={0,{0,0,0},-70};
    struct owly_cfg_sync role={0};
    struct owly_cfg_params params={0,16,16,16,440,0};
    uint8_t service[12]={6,9,0};
#if OWLY_AWDL_SOFTWARE_SYNC
    assert(rtw89_glue_awdl_config(true,132,&channel)==-ENODEV && !glue.cmd_lock && !phy.locked);
    glue.active=glue.probed=glue.up=true;
    ready=false;
    assert(rtw89_glue_awdl_config(true,132,&channel)==-ENETDOWN && !glue.cmd_lock && !phy.locked && !rechecked);
    ready=true;
    assert(rtw89_glue_awdl_config(true,132,&channel)==0 && owly_sync_state.native.master_channel==149 && !owly_sync_state.requested);
    assert(rtw89_glue_awdl_config(true,135,&rssi)==0 && owly_cfg_rssi_floor==-70 && !owly_cfg_rssi_drops);
    rssi.threshold=10;
    assert(rtw89_glue_awdl_config(true,135,&rssi)==-EINVAL && owly_cfg_rssi_floor==-70);
    assert(owly_sync_template(&owly_sync_state,service,12,vif.addr)==0);
    owly_sync_request(&owly_sync_state,true);owly_sync_reconcile(&owly_sync_state,true,149,1000);
    assert(owly_sync_state.active);
    role.state=1;memcpy(role.master,vif.addr,6);role.flags=1;
    assert(rtw89_glue_awdl_config(true,134,&role)==0 && owly_sync_state.active && !paused);
    assert(rtw89_glue_awdl_config(true,116,&params)==0 && !paused && owly_sync_state.active && owly_sync_state.native.psf_period_ms==440);
    assert(owly_sync_state.due_us==glue_now_us());
    assert(rtw89_glue_awdl_config(true,116,&params)==0 && !paused);
    glue.scanning=true;params.psf_period=110;
    assert(rtw89_glue_awdl_config(true,116,&params)==0 && !paused && !owly_sync_state.active);
    assert(owly_sync_configured(&owly_sync_state) && owly_sync_state.native.psf_period_ms==110);
    assert(rtw89_glue_awdl_config(true,132,&channel)==0 && !owly_sync_state.active);
    unsigned before=rechecked;role.state=0;ready=false;
    assert(rtw89_glue_awdl_config(true,134,&role)==0 && paused==1 && rechecked==before && !owly_sync_state.active && !owly_sync_state.requested);
    assert(!glue.cmd_lock && !phy.locked);
    puts("production config glue: lifecycle, real clock state, rollback and balanced locks passed; mocked bus, no RF");
#else
    assert(rtw89_glue_awdl_config(true,132,&channel)==-EOPNOTSUPP && !glue.cmd_lock && !phy.locked);
    puts("production config glue feature off: unsupported, no state or locks changed");
#endif
    return 0;
}
