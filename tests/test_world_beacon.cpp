#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
using u8=uint8_t;using u16=uint16_t;
constexpr unsigned IEEE80211_CHAN_DISABLED=1,IEEE80211_CHAN_NO_IR=2,IEEE80211_CHAN_RADAR=4;
constexpr unsigned REGULATORY_CUSTOM_REG=1,REGULATORY_STRICT_REG=2,REGULATORY_DISABLE_BEACON_HINTS=4,REGULATORY_WIPHY_SELF_MANAGED=128;
constexpr unsigned NL80211_BAND_2GHZ=0,NL80211_BAND_5GHZ=1,NL80211_BAND_6GHZ=2;
constexpr unsigned WLAN_CAPABILITY_ESS=1,WLAN_CAPABILITY_IBSS=2;
struct ieee80211_channel{unsigned flags=2,orig_flags=0,band=1,center_freq=5745;int max_power=20,max_reg_power=20;};
struct ieee80211_supported_band{ieee80211_channel*channels=nullptr;int n_channels=0;};
struct wiphy{void*regd=nullptr;unsigned regulatory_flags=0;ieee80211_supported_band*bands[3]={};};
struct rtw89_cfg80211_rdev{ieee80211_channel*owly_beacon_channel=nullptr;struct wiphy wiphy;};
int locks=1;
void lockdep_assert_wiphy(struct wiphy*){assert(locks==1);}
rtw89_cfg80211_rdev*wiphy_to_rdev(struct wiphy*w){return reinterpret_cast<rtw89_cfg80211_rdev*>(reinterpret_cast<char*>(w)-offsetof(rtw89_cfg80211_rdev,wiphy));}
bool ether_addr_equal(const u8*a,const u8*b){return !std::memcmp(a,b,6);}
bool is_valid_ether_addr(const u8*a){u8 bits=0;for(int i=0;i<6;i++)bits|=a[i];return bits && !(a[0]&1);}
#include "production.inc"
constexpr int RTW89_MLME_ASSOCIATED=2,RTW89_MLME_CONNECTED=3,IEEE80211_STA_AUTHORIZED=4;
struct Mgmt{u8 bssid[6],sa[6];struct{struct{u16 capab_info=1;}beacon;}u;};
struct Context{struct{ieee80211_channel*chan=nullptr;}def;};
struct Vif{struct{bool assoc=true;}cfg;struct{Context*chanctx_conf=nullptr;}bss_conf;};
struct ModelHW{struct wiphy*wiphy=nullptr;};
struct {bool running=true;int state=3;Vif*vif=nullptr;void*sta=nullptr;int sta_state=4;
        bool chanctx_assigned=true;Context*chanctx=nullptr;ieee80211_channel*chan=nullptr;
        ModelHW*hw=nullptr;void(*notify)()=nullptr;}mlme;
int notifications=0;
void notify(){++notifications;}
u16 le16_to_cpu(u16 x){return x;}
void mlme_info(const char*,unsigned){}
void production_beacon_prefix(const Mgmt*mgmt,size_t len,u16 freq){
    const size_t fixed=36;auto*conf=&mlme.vif->bss_conf;
#include "beacon_gate.inc"
}
int main(){
    rtw89_cfg80211_rdev dev;ieee80211_channel channels[2],foreign;
    ieee80211_supported_band band;u8 ap[6]={2,1,2,3,4,5},source[6];
    auto reset=[&]{dev={};channels[0]={};channels[1]={};channels[1].center_freq=5220;
        foreign={};band.channels=channels;band.n_channels=2;dev.wiphy.bands[1]=&band;std::memcpy(source,ap,6);};
    int groups=0;reset();
#if OWLY_WORLD_BEACON_HINT
    for(unsigned f:{5220u,5745u}){
        reset();channels[0].center_freq=f;
        assert(rtw89_cfg80211_link_beacon(&dev.wiphy,&channels[0],ap,source,1));
        assert(channels[0].flags==0 && dev.owly_beacon_channel==&channels[0]);
        assert(channels[0].max_power==20 && channels[0].max_reg_power==20);++groups;
    }
    for(int fault=0;fault<20;fault++){
        reset();auto*w=&dev.wiphy;auto*c=&channels[0];auto*bssid=ap;auto*sa=source;u16 cap=1;
        if(fault==0)w=nullptr;if(fault==1)c=nullptr;if(fault==2)bssid=nullptr;if(fault==3)sa=nullptr;
        if(fault==4)dev.wiphy.regd=&foreign;if(fault==5)dev.wiphy.regulatory_flags=REGULATORY_DISABLE_BEACON_HINTS;
        if(fault==6)dev.wiphy.regulatory_flags=REGULATORY_WIPHY_SELF_MANAGED;if(fault==7)dev.wiphy.regulatory_flags=REGULATORY_CUSTOM_REG;
        if(fault==8)cap=0;if(fault==9)cap=3;if(fault==10)source[5]^=1;
        if(fault==11)std::memset(source,0,6),bssid=source;if(fault==12)source[0]=3,bssid=source;
        if(fault==13)c=&foreign;if(fault==14)dev.wiphy.bands[1]=nullptr;
        if(fault==15)channels[0].band=2;if(fault==16)channels[0].center_freq=5260;
        if(fault==17)channels[0].flags|=1;if(fault==18)channels[0].flags|=4;if(fault==19)channels[0].orig_flags|=2;
        assert(!rtw89_cfg80211_link_beacon(w,c,bssid,sa,cap));
        assert(channels[0].flags&2);assert(!dev.owly_beacon_channel);++groups;
    }
    for(unsigned hard:{1u,4u}){reset();channels[0].orig_flags=hard;
        assert(!rtw89_cfg80211_link_beacon(&dev.wiphy,&channels[0],ap,source,1));++groups;}
    reset();channels[0].flags=0;assert(!rtw89_cfg80211_link_beacon(&dev.wiphy,&channels[0],ap,source,1));++groups;
    reset();channels[0].flags|=0x4000;
    assert(rtw89_cfg80211_link_beacon(&dev.wiphy,&channels[0],ap,source,1));
    assert(channels[0].flags==0x4000 && (channels[1].flags&2));
    assert(!rtw89_cfg80211_link_beacon(&dev.wiphy,&channels[0],ap,source,1));
    assert(!rtw89_cfg80211_link_beacon(&dev.wiphy,&channels[1],ap,source,1));++groups;
    channels[0].flags|=0x8000;
    assert(rtw89_cfg80211_link_beacon_reset(&dev.wiphy));
    assert(channels[0].flags==(0xc000|2) && !dev.owly_beacon_channel);++groups;
    assert(!rtw89_cfg80211_link_beacon_reset(&dev.wiphy));assert(!rtw89_cfg80211_link_beacon_reset(nullptr));++groups;
    assert(rtw89_cfg80211_link_beacon(&dev.wiphy,&channels[1],ap,source,1));
    assert((channels[0].flags&2) && !(channels[1].flags&2));++groups;
    assert(rtw89_cfg80211_link_beacon_reset(&dev.wiphy));++groups;
    for(int i=0;i<10000;i++){assert(rtw89_cfg80211_link_beacon(&dev.wiphy,&channels[0],ap,source,1));assert(rtw89_cfg80211_link_beacon_reset(&dev.wiphy));}++groups;
    reset();dev.wiphy.regulatory_flags=REGULATORY_STRICT_REG;
    assert(rtw89_cfg80211_link_beacon(&dev.wiphy,&channels[0],ap,source,1));++groups;
    for(int fault=-1;fault<12;fault++){
        reset();Context ctx;ctx.def.chan=&channels[0];Vif vif;vif.bss_conf.chanctx_conf=&ctx;
        ModelHW hw{&dev.wiphy};Mgmt mgmt;std::memcpy(mgmt.bssid,ap,6);std::memcpy(mgmt.sa,source,6);
        mlme={};mlme.hw=&hw;mlme.chan=&channels[0];mlme.chanctx=&ctx;mlme.vif=&vif;mlme.sta=&vif;mlme.notify=notify;notifications=0;
        size_t length=36;u16 freq=5745;
        if(fault==0)mlme.state=2;if(fault==1)length=35;if(fault==2)freq=5775;if(fault==3)mlme.running=false;
        if(fault==4)vif.cfg.assoc=false;if(fault==5)mlme.sta=nullptr;if(fault==6)mlme.sta_state=3;
        if(fault==7)mlme.chanctx_assigned=false;if(fault==8)mlme.chanctx=nullptr;
        if(fault==9)ctx.def.chan=&foreign;if(fault==10)vif.bss_conf.chanctx_conf=nullptr;if(fault==11)channels[0].orig_flags=2;
        production_beacon_prefix(&mgmt,length,freq);
        assert(notifications==(fault==-1?1:0));assert(channels[0].flags==(fault==-1?0u:2u));
        if(fault==-1){production_beacon_prefix(&mgmt,length,freq);assert(notifications==1);}++groups;
    }
    std::printf("%d world-beacon production groups passed; no native/RF proof\n",groups);
#else
    assert(!rtw89_cfg80211_link_beacon(&dev.wiphy,&channels[0],ap,source,1));
    assert(!rtw89_cfg80211_link_beacon_reset(&dev.wiphy));assert(channels[0].flags==2 && !dev.owly_beacon_channel);
    std::puts("world-beacon feature off preserves every channel flag");
#endif
}
