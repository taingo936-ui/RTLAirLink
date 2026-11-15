/* SPDX-License-Identifier: GPL-2.0 */
#include "owly_awdl_sync.h"
#include "rtw89_glue.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define IEEE80211_CHAN_DISABLED 1u
#define IEEE80211_CHAN_NO_IR 2u
#define IEEE80211_CHAN_RADAR 4u
#define ETH_ALEN 6u
#define IOLog(...) ((void)0)
#define BIT(n) (1u << (n))

struct ieee80211_channel { unsigned flags, center_freq, hw_value; };
struct ieee80211_chanctx_conf { struct { struct ieee80211_channel *chan; } def; };
struct radio_lease { struct ieee80211_chanctx_conf *channel; bool bound, blocked; };
struct wiphy_work { int unused; };
struct wiphy_delayed_work { bool queued; };
struct wiphy { bool locked; };
struct ieee80211_hw { struct wiphy *wiphy; unsigned queues; };
struct rtw89_m80211_local {
    const struct { void (*flush)(struct ieee80211_hw *,void *,unsigned,bool); } *ops;
};

static struct radio_lease owly_radio_state;
static struct { bool active, probed, up, scanning, cmd_lock; } glue;
static struct ieee80211_channel ap = {0,5745,149};
static struct ieee80211_chanctx_conf primary = {{&ap}};
static struct ieee80211_chanctx_conf *primary_link = &primary;
static struct {
    uint8_t addr[6];
    struct { struct ieee80211_chanctx_conf *chanctx_conf; } bss_conf;
} aux = {{2,1,2,3,4,5},{&primary}}, *owly_core04c_vif = &aux;
static struct wiphy phy;
static struct ieee80211_hw hw = {&phy,4};
static struct owly_native_link owly_link_state;
static struct owly_sync owly_sync_state;
static struct wiphy_delayed_work owly_sync_work;
static uint8_t owly_sync_txbuf[OWLY_SYNC_FRAME_MAX], owly_link_txbuf[OWLY_LINK_FRAME_MAX];
static unsigned long last_work_delay;
static uint64_t now_us = 1000000;
static bool advancing_clock;
static bool owly_link_lock;
static int owly_cfg_rssi_floor;
static unsigned int owly_cfg_rssi_drops, action_tx, data_tx;
static uint64_t owly_idle_policy_us;
static struct wiphy_work owly_radio_work;
static int action_submit_error, data_submit_error;
static void owly_radio_maybe_attach(void) { assert(phy.locked); }
static void rtw89_mlme_awdl_release_channel(void) { assert(phy.locked); }
static void wiphy_work_queue(struct wiphy *wiphy,struct wiphy_work *work) {
    assert(wiphy->locked && work==&owly_radio_work);
}

static struct ieee80211_chanctx_conf *rtw89_mlme_awdl_shared_chanctx(void) { return primary_link; }
static bool owly_radio_is_bound(const struct radio_lease *lease) { return lease->bound; }
static uint64_t glue_now_us(void) { return advancing_clock ? now_us++ : now_us; }
static struct ieee80211_hw *glue_hw(void) { return &hw; }
static struct rtw89_m80211_local *hw_to_local(struct ieee80211_hw *device)
{
    static struct rtw89_m80211_local local = {0};
    static const typeof(*local.ops) ops = {0};
    assert(device==&hw);
    local.ops=&ops;
    return &local;
}
static void spin_lock(bool *lock) { assert(!*lock); *lock=true; }
static void spin_unlock(bool *lock) { assert(*lock); *lock=false; }
static void mutex_lock(bool *lock) { spin_lock(lock); }
static void mutex_unlock(bool *lock) { spin_unlock(lock); }
static void wiphy_lock(struct wiphy *wiphy) { spin_lock(&wiphy->locked); }
static void wiphy_unlock(struct wiphy *wiphy) { spin_unlock(&wiphy->locked); }
static void lockdep_assert_wiphy(const struct wiphy *wiphy) { assert(wiphy->locked); }
static unsigned long usecs_to_jiffies(unsigned long us) { return us; }
static bool wiphy_delayed_work_pending(const struct wiphy *wiphy,const struct wiphy_delayed_work *work)
{
    lockdep_assert_wiphy(wiphy);
    return work->queued;
}
static void wiphy_delayed_work_queue(struct wiphy *wiphy,struct wiphy_delayed_work *work,
                                    unsigned long delay)
{
    last_work_delay=delay;
    lockdep_assert_wiphy(wiphy);
    work->queued=true;
}
static void wiphy_delayed_work_cancel(struct wiphy *wiphy,struct wiphy_delayed_work *work)
{
    lockdep_assert_wiphy(wiphy);
    work->queued=false;
}
static void owly_sync_recheck(void);
static int owly_sync_submit(struct ieee80211_channel *ch,const uint8_t *bytes,size_t n,bool data)
{
    assert(!glue.scanning && phy.locked && ch==&ap && n);
    assert(bytes[0]==(data ? 8 : 0xd0));
    const int error=data ? data_submit_error : action_submit_error;
    if(error)return error;
    if(data)++data_tx;else ++action_tx;
    return 0;
}
static void owly_peer_tick(struct ieee80211_channel *ch,uint64_t now) {(void)ch;(void)now;}
#include "production_scan.inc"

static void recheck(void)
{
    wiphy_lock(&phy);
    owly_sync_recheck();
    wiphy_unlock(&phy);
}

static void run_worker(void)
{
    wiphy_lock(&phy);
    owly_sync_work.queued=false;
    owly_sync_work_run(&phy,NULL);
    wiphy_unlock(&phy);
}

static void cold_start(const uint8_t *version,size_t length)
{
    static const uint8_t broadcast[ETH_ALEN]={255,255,255,255,255,255};
    uint8_t mi[48]={24,40,0};
    static const uint8_t query[]={
        0,0,0,0,0,1,0,0,0,0,0,0,
        8,'_','a','i','r','d','r','o','p',
        4,'_','t','c','p',5,'l','o','c','a','l',0,0,12,0,1,
    };
    const uint8_t udp_length=8u+sizeof(query);
    uint8_t bonjour[62+sizeof(query)]={0x33,0x33,0,0,0,0xfb};
    struct owly_link_window window;
    memcpy(mi+43,version,length);
    memcpy(bonjour+6,aux.addr,ETH_ALEN);
    bonjour[12]=0x86;bonjour[13]=0xdd;bonjour[14]=0x60;bonjour[19]=udp_length;
    bonjour[20]=17;bonjour[21]=255;
    bonjour[38]=0xff;bonjour[39]=2;bonjour[53]=0xfb;
    bonjour[54]=bonjour[56]=0x14;bonjour[55]=bonjour[57]=0xe9;bonjour[59]=udp_length;
    memcpy(bonjour+62,query,sizeof(query));

    owly_radio_state.bound=false;
    assert(!rtw89_glue_awdl_mi(true,broadcast,mi,sizeof(mi)));
    assert(rtw89_glue_awdl_mi(false,broadcast,mi,sizeof(mi))==-ENETDOWN);
    assert(owly_sync_state.auto_mi.length==sizeof(mi) && !owly_sync_work.queued);
    assert(rtw89_glue_native_output(bonjour,sizeof(bonjour))==-ENETDOWN);
    owly_radio_state.bound=true;
    assert(!rtw89_glue_native_output(bonjour,sizeof(bonjour)));
    assert(owly_link_state.count==1 && !owly_sync_work.queued);
    run_worker();
    assert(!action_tx && !data_tx && owly_link_state.count==1);

    assert(!owly_sync_template(&owly_sync_state,version,length,aux.addr));
    owly_sync_request(&owly_sync_state,true);
    recheck();
    assert(owly_sync_work.queued && !rtw89_glue_native_window(&window) && window.active);
    run_worker();
    assert(action_tx==1 && !data_tx && owly_link_state.count==1);
    assert(owly_sync_state.auto_mi.length==sizeof(mi));
    action_submit_error=-ENOSPC;
    run_worker();
    assert(action_tx==1 && !data_tx && owly_link_state.count==1);
    action_submit_error=0;
    run_worker();
    assert(action_tx==2 && owly_sync_state.native_mi_handed_off==1);
    assert(data_tx==1 && !owly_link_state.count);

    /* A stop invalidates the old publication and blocks pending data. */
    assert(!rtw89_glue_native_output(bonjour,sizeof(bonjour)));
    assert(!rtw89_glue_awdl_vif_disable());
    run_worker();
    assert(!owly_link_state.count && !owly_sync_state.auto_mi.length && data_tx==1);
    assert(rtw89_glue_native_output(bonjour,sizeof(bonjour))==-ENETDOWN);
    owly_sync_reset(&owly_sync_state);
    owly_link_reset(&owly_link_state);
    action_tx=data_tx=0;
}

static void repeat_native_session(void)
{
    static const uint8_t broadcast[ETH_ALEN]={255,255,255,255,255,255};
    static const uint8_t version[]={21,2,0,0x3e,1};
    static const uint8_t peer[ETH_ALEN]={2,1,2,3,4,9};
    uint8_t mi[60]={24,40,0};
    struct owly_link_window window;
    const uint64_t saved_now=now_us;

    memcpy(mi+43,version,sizeof(version));
    mi[48]=6;mi[49]=9;
    for (uint8_t session=1;session<=3;++session) {
        mi[54]=session;
        /* Legacy rebuilds the current advertisement before pausing sync. */
        assert(!rtw89_glue_awdl_mi(true,broadcast,mi,sizeof(mi)));
        assert(!rtw89_glue_awdl_mi(false,peer,mi,sizeof(mi)));
        assert(!owly_sync_template(&owly_sync_state,version,sizeof(version),aux.addr));
        assert(!rtw89_glue_awdl_sync_enable(false));
        assert(!owly_sync_work.queued && !owly_sync_state.pending_mi.length);
        assert(owly_sync_state.auto_mi.length==sizeof(mi));
        assert(!memcmp(owly_sync_state.auto_mi.bytes,mi,sizeof(mi)));
        assert(rtw89_glue_native_window(&window)==-ECANCELED && !window.active);
        const unsigned before=action_tx;
        run_worker();
        assert(action_tx==before && !data_tx);

        assert(!rtw89_glue_awdl_sync_enable(true));
        assert(!rtw89_glue_native_window(&window) && window.active);
        run_worker();
        run_worker();
        assert(owly_sync_state.broadcast_mif_submitted);
        assert(owly_sync_state.native_mi_handed_off==session);
        assert(action_tx==before+2);
        now_us+=120000u;
    }
    assert(!rtw89_glue_awdl_vif_disable());
    assert(!owly_sync_state.auto_mi.length && !owly_sync_work.queued);
    assert(rtw89_glue_native_window(&window)==-ECANCELED && !window.active);
    owly_sync_reset(&owly_sync_state);
    owly_link_reset(&owly_link_state);
    action_tx=data_tx=0;
    now_us=saved_now;
}

static void unicast_before_broadcast_mif(void)
{
    static const uint8_t version[]={21,2,0,0x3e,1};
    static const uint8_t master[6]={2,1,2,3,4,6},iphone[6]={2,1,2,3,4,7};
    uint8_t answer[74]={0};
    struct owly_link_window window;

    assert(!owly_sync_template(&owly_sync_state,version,sizeof(version),aux.addr));
    owly_sync_request(&owly_sync_state,true);
    recheck();
    assert(!rtw89_glue_native_window(&window) && window.active);

    struct owly_link_peer *root=&owly_link_state.peers[0];
    *root=(struct owly_link_peer){
        .used=true, .clock_valid=true, .channel=149, .master_channel=149,
        .local_slots=1, .rssi=-40, .last_rx_us=now_us-8000,
        .clock_rx_us=now_us-8000, .anchor_us=now_us-2048,
    };
    memcpy(root->mac,master,6);
    memcpy(root->master,master,6);
    memset(root->channels,44,16);
    root->channels[1]=149;

    struct owly_link_peer *receiver=&owly_link_state.peers[1];
    *receiver=(struct owly_link_peer){
        .used=true, .clock_valid=true, .channel=149, .master_channel=149,
        .local_slots=16, .rssi=-45, .last_rx_us=now_us-1000,
        .clock_rx_us=now_us-1000, .anchor_us=now_us-8192,
    };
    memcpy(receiver->mac,iphone,6);
    memcpy(receiver->master,iphone,6);
    memset(receiver->channels,149,16);

    /* A native TCP reply to the second peer is due before the master
     * comes back to receive the first broadcast PSF/MIF. */
    memcpy(answer,iphone,6);
    memcpy(answer+6,aux.addr,6);
    answer[12]=0x86;answer[13]=0xdd;answer[14]=0x60;answer[19]=20;
    answer[20]=6;answer[21]=64;
    answer[22]=answer[38]=0xfe;answer[23]=answer[39]=0x80;
    answer[37]=aux.addr[5];answer[53]=iphone[5];
    answer[54]=0x22;answer[55]=0x42;answer[56]=0xc0;answer[57]=0x01;
    answer[66]=0x50;answer[67]=0x12;
    assert(!rtw89_glue_native_output(answer,sizeof(answer)));
    assert(!owly_link_action_tx_ready(&owly_link_state,now_us));
    run_worker();
    assert(!action_tx && !owly_sync_state.broadcast_mif_submitted);
    assert(data_tx==1 && !owly_link_state.count);

    const uint64_t saved_now=now_us;
    now_us+=4u*OWLY_LINK_AW_US;
    assert(owly_link_action_tx_ready(&owly_link_state,now_us));
    run_worker();
    run_worker();
    assert(action_tx==2 && owly_sync_state.broadcast_mif_submitted);
    owly_sync_reset(&owly_sync_state);
    owly_link_reset(&owly_link_state);
    owly_sync_work.queued=false;
    action_tx=data_tx=0;
    now_us=saved_now;
}

int main(void)
{
    static const uint8_t version[]={21,2,0,0x3e,1};
    static const uint8_t peer[6]={2,10,20,30,40,50};
    uint8_t multicast[55]={0x33,0x33,0,0,0,0xfb};
    uint8_t reply[sizeof(multicast)];
    struct owly_link_window window;
    struct rtw89_glue_awdl_sync_status status;
    memcpy(multicast+6,aux.addr,6);
    multicast[12]=0x86;multicast[13]=0xdd;multicast[14]=0x60;multicast[19]=1;
    multicast[38]=0xff;multicast[39]=2;multicast[53]=0xfb;
    memcpy(reply,multicast,sizeof(reply));
    memcpy(reply,peer,sizeof(peer));
    memset(reply+38,0,16);
    reply[38]=0xfe;reply[39]=0x80;reply[53]=2;

    glue.active=glue.probed=glue.up=true;
    owly_radio_state=(struct radio_lease){&primary,true,false};
    repeat_native_session();
    cold_start(version,sizeof(version));
    unicast_before_broadcast_mif();
    assert(!rtw89_glue_native_window(&window) && !window.active &&
           window.channel==149 && !memcmp(window.local,aux.addr,6));
    assert(rtw89_glue_native_output(multicast,sizeof(multicast))==-ENETDOWN);
    owly_sync_request(&owly_sync_state,false);
    assert(rtw89_glue_native_window(&window)==-ECANCELED && !window.active && !window.channel);
    glue.up=false;
    assert(rtw89_glue_awdl_vif_enable()==-ENETDOWN && owly_sync_state.explicitly_disabled);
    glue.up=true;
    assert(!rtw89_glue_awdl_vif_enable());
    assert(!rtw89_glue_native_window(&window) && !window.active && window.channel==149);
    assert(rtw89_glue_native_output(multicast,sizeof(multicast))==-ENETDOWN);
    assert(!owly_sync_template(&owly_sync_state,version,sizeof(version),aux.addr));
    owly_sync_request(&owly_sync_state,true);
    recheck();
    assert(!rtw89_glue_native_window(&window) && window.active);
    run_worker();
    run_worker();
    assert(owly_sync_state.broadcast_mif_submitted);
    action_tx=0;
    owly_link_state.peers[0].used=true;
    memcpy(owly_link_state.peers[0].mac,peer,sizeof(peer));
    owly_link_state.peers[0].last_rx_us=now_us;
    owly_link_state.peers[0].channel=149;
    assert(!rtw89_glue_native_output(reply,sizeof(reply)) && last_work_delay==0);
    bool data_busy=false;
    assert(rtw89_glue_awdl_scan_protect(&data_busy) && data_busy);
    const uint64_t generation=owly_link_state.generation;

    /* The LOQ scanned for 1.062 s and SET120 arrived 154 ms into it. */
    glue.scanning=true;
    now_us+=100000;
    recheck();
    now_us+=154000;
    assert(!rtw89_glue_native_window(&window) && !window.active);
    assert(!rtw89_glue_awdl_sync_status(&status));
    assert(status.enabled && status.scan_paused && !status.active && status.channel==149);
    assert(owly_link_state.peers[0].used && owly_link_state.generation==generation);
    assert(!rtw89_glue_native_output(multicast,sizeof(multicast)));
    assert(owly_link_state.count==2 && owly_link_state.sync_enabled && owly_link_state.tx_paused);
    run_worker();
    assert(!action_tx && !data_tx && !owly_sync_work.queued);

    /* Release the real radio, then wait for the next observed peer window.
     * Both frames must outlive their old wall-clock expiry. */
    now_us+=908000;
    glue.scanning=false;
    recheck();
    assert(!rtw89_glue_awdl_sync_status(&status) && status.enabled && !status.scan_paused && status.active);
    assert(owly_link_state.queue[0].pause_credit_us==1062000);
    assert(owly_link_state.queue[1].pause_credit_us==908000);
    now_us+=410000;
    /* A queued unicast reply and a multicast discovery packet both
     * depend on the worker preserving the queue and observed peer. */
    owly_link_state.peers[0].last_rx_us=now_us;
    action_submit_error=-ENOMEM;
    data_submit_error=-ENOSPC;
    run_worker();
    assert(owly_sync_state.active && owly_sync_state.failed==1 && !owly_sync_state.blocked);
    assert(!action_tx && !data_tx && owly_link_state.count==2 && !owly_link_state.tx_handoffs);
    action_submit_error=data_submit_error=0;
    advancing_clock=true;
    run_worker();
    advancing_clock=false;
    assert(owly_link_state.generation==generation && owly_link_state.peers[0].used);
    assert(data_tx==2 && !owly_link_state.count && !owly_link_state.tx_expired);
    /* A later periodic advertisement still goes out after the ring frees. */
    const unsigned action_before=action_tx;
    now_us+=OWLY_SYNC_AF_US;
    run_worker();
    assert(action_tx==action_before+1);
    const unsigned action_after=action_tx;

    /* A real binding loss still tears down queued and advertised state. */
    assert(!rtw89_glue_native_output(multicast,sizeof(multicast)));
    glue.scanning=true;
    ap.flags=IEEE80211_CHAN_NO_IR;
    recheck();
    assert(rtw89_glue_native_window(&window)==-ENETDOWN && !window.active);
    assert(rtw89_glue_native_output(multicast,sizeof(multicast))==-ENETDOWN);
    assert(!rtw89_glue_awdl_sync_status(&status) && !status.enabled && !status.active);
    assert(!owly_link_state.active && !owly_link_state.peers[0].used && !owly_link_state.count);
    run_worker();
    assert(action_tx==action_after && data_tx==2);
    puts("AWDL repeat native service sessions, explicit stop, scan resume and PCIe backpressure passed");
    return 0;
}
