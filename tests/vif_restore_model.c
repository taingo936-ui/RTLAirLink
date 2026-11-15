/* SPDX-License-Identifier: GPL-2.0 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define OWLY_BACKEND_REAL_VIF 1
#define OWLY_AWDL_RADIO_SCC 1
#define RTW89_AWDL_VIF_STAGE 1
#define RTW89_AWDL_VIF_REGISTER 0
#define OWLY_RADIO_OK 0
#define NL80211_IFTYPE_STATION 2
#define NL80211_IFTYPE_P2P_CLIENT 8
#define ETH_ALEN 6
#define BIT(n) (1u << (n))
#define IOLog(...) ((void)0)
#define INIT_DELAYED_WORK(work, callback) do { *(work)=0; (void)(callback); } while (0)

typedef uint8_t u8;
struct mutex { bool held; };
struct ieee80211_vif { u8 addr[ETH_ALEN]; bool p2p, registered; };
struct wiphy { bool held; unsigned int interface_modes; u8 perm_addr[ETH_ALEN]; };
struct ieee80211_hw { struct wiphy *wiphy; };
struct fake_ops {
    int (*start)(struct ieee80211_hw *);
    void (*stop)(struct ieee80211_hw *, bool);
    int (*add_interface)(struct ieee80211_hw *, struct ieee80211_vif *);
    void (*remove_interface)(struct ieee80211_hw *, struct ieee80211_vif *);
    void (*configure_filter)(struct ieee80211_hw *, unsigned int, unsigned int *, uint64_t);
};
struct rtw89_m80211_local { const struct fake_ops *ops; };
static struct {
    bool active, probed, up, have_mac, ppdu_flush;
    struct mutex cmd_lock;
    struct { void (*irq_enable)(void *, bool); void *ctx; } plat;
    struct ieee80211_vif *vif;
    u8 mac[ETH_ALEN], awdl_vif_mac[ETH_ALEN];
    unsigned int ppdu_flush_work;
} glue = { .active=true, .probed=true };
static struct wiphy wiphy = { .interface_modes=BIT(NL80211_IFTYPE_P2P_CLIENT), .perm_addr={2,1,2,3,4,5} };
static struct ieee80211_hw hw = { &wiphy };
static struct ieee80211_vif *owly_core04c_vif;
static bool owly_core04e_r1_attach_attempted, owly_radio_state, radio_running;
static uint64_t owly_idle_move_us, owly_idle_policy_us;
static int aux_error;
static unsigned int aux_adds, live_vifs;
static const int glue_m80211_ops;
static void (*rtw88_napi_post_poll)(int);

static void mutex_lock(struct mutex *lock) { assert(!lock->held); lock->held=true; }
static void mutex_unlock(struct mutex *lock) { assert(lock->held); lock->held=false; }
static void wiphy_lock(struct wiphy *w) { assert(glue.cmd_lock.held && !w->held); w->held=true; }
static void wiphy_unlock(struct wiphy *w) { assert(w->held); w->held=false; }
static void lockdep_assert_wiphy(struct wiphy *w) { assert(w->held); }
static struct ieee80211_hw *glue_hw(void) { return &hw; }
static void rtw89_m80211_set_glue(struct ieee80211_hw *h, const void *ops, void *ctx)
{ assert(h==&hw && ops==&glue_m80211_ops && !ctx); }
static bool is_zero_ether_addr(const u8 *addr)
{ return !memcmp(addr, (const u8[ETH_ALEN]){0}, ETH_ALEN); }
static bool owly_core04c_make_mac(const u8 *primary, u8 *result)
{ memcpy(result,primary,ETH_ALEN); result[5]^=1; return true; }
static struct ieee80211_vif *rtw89_m80211_vif_alloc(struct ieee80211_hw *h, int type, const u8 *addr)
{
    assert(h==&hw && type==NL80211_IFTYPE_STATION);
    struct ieee80211_vif *vif=calloc(1,sizeof(*vif));
    assert(vif);
    ++live_vifs;
    memcpy(vif->addr,addr,ETH_ALEN);
    return vif;
}
static void rtw89_m80211_vif_free(struct ieee80211_vif *vif)
{ assert(vif && !vif->registered && live_vifs); --live_vifs; free(vif); }
static void rtw89_m80211_vif_set_in_driver(struct ieee80211_vif *vif, bool on)
{ vif->registered=on; }
static int start(struct ieee80211_hw *h)
{ assert(h->wiphy->held && !radio_running); radio_running=true; return 0; }
static void stop(struct ieee80211_hw *h, bool suspended)
{ assert(h->wiphy->held && !suspended && radio_running); radio_running=false; }
static int add(struct ieee80211_hw *h, struct ieee80211_vif *vif)
{
    assert(h->wiphy->held && radio_running);
    if (!vif->p2p) return 0;
    if (aux_error) return aux_error;
    ++aux_adds;
    return 0;
}
static void remove_vif(struct ieee80211_hw *h, struct ieee80211_vif *vif)
{ assert(h->wiphy->held && vif && !vif->registered); }
static void filter(struct ieee80211_hw *h, unsigned int changed, unsigned int *flags, uint64_t multicast)
{ assert(h->wiphy->held && !changed && !*flags && !multicast); }
static const struct fake_ops ops = {start,stop,add,remove_vif,filter};
static struct rtw89_m80211_local local = { &ops };
static struct rtw89_m80211_local *hw_to_local(struct ieee80211_hw *h)
{ assert(h==&hw); return &local; }
static void glue_link_notify(void) {}
static void glue_ppdu_flush_work(void) {}
static void glue_napi_post_poll(int done) { (void)done; }
static void rtw89_mlme_start(struct ieee80211_hw *h, struct ieee80211_vif *vif, void (*notify)(void))
{ assert(h->wiphy->held && vif==glue.vif && vif->registered && notify==glue_link_notify); }
static void owly_core04d_detach(void *ctx) { (void)ctx; }
static void rtw89_mlme_awdl_set_release_hook(void (*hook)(void *)) { assert(hook==owly_core04d_detach); }
static int owly_radio_reset(bool *state) { assert(!*state); return OWLY_RADIO_OK; }
static void owly_radio_maybe_attach(void)
{ assert(wiphy.held && glue.up && owly_core04c_vif && owly_core04c_vif->registered); }
static void owly_core04c_vif_unregister(struct ieee80211_hw *h)
{
    assert(h->wiphy->held);
    if (owly_core04c_vif) {
        rtw89_m80211_vif_set_in_driver(owly_core04c_vif,false);
        rtw89_m80211_vif_free(owly_core04c_vif);
        owly_core04c_vif=NULL;
    }
}

static int glue_up_locked(void);
#include "production_vif.inc"

static void power_down(void)
{
    mutex_lock(&glue.cmd_lock);
    wiphy_lock(&wiphy);
    owly_core04c_vif_unregister(&hw);
    rtw89_m80211_vif_set_in_driver(glue.vif,false);
    rtw89_m80211_vif_free(glue.vif);
    glue.vif=NULL;
    glue.up=false;
    stop(&hw,false);
    wiphy_unlock(&wiphy);
    mutex_unlock(&glue.cmd_lock);
}

int main(void)
{
    const u8 requested[ETH_ALEN]={2,6,7,8,9,10};
    const u8 different[ETH_ALEN]={2,6,7,8,9,11};
    assert(is_zero_ether_addr(glue.awdl_vif_mac));
    owly_idle_move_us=1000000; owly_idle_policy_us=999999;
    assert(!rtw89_glue_up() && !rtw89_glue_awdl_vif_present());
    assert(!owly_idle_move_us && !owly_idle_policy_us);
    assert(rtw89_glue_awdl_vif_create(wiphy.perm_addr)==-EINVAL);
    assert(!rtw89_glue_awdl_vif_create(requested) && aux_adds==1);
    assert(!rtw89_glue_awdl_vif_create(requested) && aux_adds==1);
    assert(rtw89_glue_awdl_vif_create(different)==-EBUSY);
    power_down();
    assert(!rtw89_glue_awdl_vif_present() && !live_vifs);
    assert(!rtw89_glue_up() && rtw89_glue_awdl_vif_present());
    assert(aux_adds==2 && !memcmp(owly_core04c_vif->addr,requested,ETH_ALEN));
    assert(!rtw89_glue_up() && aux_adds==2);
    power_down();
    aux_error=-EIO;
    assert(!rtw89_glue_up() && glue.up && !rtw89_glue_awdl_vif_present() && live_vifs==1);
    power_down();
    aux_error=0;
    assert(!rtw89_glue_up() && rtw89_glue_awdl_vif_present() && aux_adds==3);
    power_down();
    assert(!rtw89_glue_awdl_vif_destroy());
    assert(!rtw89_glue_up() && !rtw89_glue_awdl_vif_present() && aux_adds==3);
    power_down();
    assert(!live_vifs);
    puts("AWDL VIF restore: real add path, restart, duplicate, failure, and explicit delete passed");
    return 0;
}
