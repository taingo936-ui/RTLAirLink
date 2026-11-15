#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint16_t __le16;
typedef uint32_t __le32;
#define BIT(x) (1u << (x))
#define GENMASK(h,l) ((UINT32_MAX << (l)) & (UINT32_MAX >> (31-(h))))
#define FIELD_PREP(m,v) (((u32)(v) << __builtin_ctz(m)) & (m))
#define FIELD_GET(m,v) (((u32)(v) & (m)) >> __builtin_ctz(m))
#define cpu_to_le32(x) (x)
#define le16_to_cpu(x) (x)
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define umin(a,b) ((unsigned)(a)<(unsigned)(b)?(unsigned)(a):(unsigned)(b))
#define likely(x) (x)
#define unlikely(x) (x)
#define lockdep_assert_wiphy(x) ((void)(x))
#define IEEE80211_SCTL_SEQ 0xfff0
#include "production_raw_flags.inc"
enum mac80211_rate_control_flags { TEST_RATE_FLAGS_DUMMY=0 };
#define IEEE80211_AC_VO 0
#define IEEE80211_AC_BE 2
#define RTW89_MGMT_HW_SSN_SEL 1
#define RTW89_MGMT_HW_SEQ_MODE 1
#define RTW89_TX_DATA_CNT_LMT_MAX 0x3f
#define RTW89_BAND_2G 0
#define RTW89_HW_RATE_CCK1 0
#define RTW89_HW_RATE_OFDM6 4
#define RTW89_CHIP_BE 1
#define OWLY_SYNC_FRAME_MAX 2048
enum rtw89_core_tx_type { RTW89_CORE_TX_TYPE_MGMT, RTW89_CORE_TX_TYPE_DATA, RTW89_CORE_TX_TYPE_FWCMD };
enum btc_pkt_type { PACKET_MAX };
struct ieee80211_vif { unsigned hw_queue[4]; };
struct ieee80211_sta { bool mlo; };
struct ieee80211_hdr { __le16 frame_control, duration; u8 addr1[6],addr2[6],addr3[6]; __le16 seq_ctrl; };
struct ieee80211_tx_info {
 u32 flags; unsigned band,hw_queue;
 struct { struct { s8 idx; unsigned count; u32 flags; } rates[4]; struct ieee80211_vif *vif; } control;
};
struct sk_buff { u8 bytes[4096],*data; size_t len; unsigned priority,mapping; struct ieee80211_tx_info info; };
#define IEEE80211_SKB_CB(skb) (&(skb)->info)
struct rtw89_addr_cam_entry { bool valid; };
struct rtw89_vif_link { struct rtw89_addr_cam_entry addr_cam; unsigned chanctx_idx; };
struct rtw89_vif { struct { void *next,*prev; } dlink_pool; };
struct rtw89_chan { unsigned band_type; };
struct rtw89_hw_rate_def { unsigned ht,vht[4]; };
static const struct rtw89_hw_rate_def rtw89_hw_rate[2] = {{128,{256,266,276,286}},{128,{256,268,280,292}}};
struct rtw89_dev { const struct { unsigned chip_gen; } *chip; struct rtw89_chan channel; };
struct ieee80211_channel { unsigned band,hw_value; };
struct ieee80211_supported_band { unsigned n_bitrates; struct { unsigned bitrate; } bitrates[12]; };
struct wiphy { struct ieee80211_supported_band *bands[2]; };
struct ieee80211_hw { struct wiphy *wiphy; struct rtw89_dev *priv; unsigned extra_tx_headroom; };
struct rtw89_tx_desc_info {
 bool use_rate,dis_data_fb,hw_rts_en,rts_en,no_ack,upd_wlan_hdr,mlo,sw_mld;
 bool is_bmc,wd_page,hiq,report,tx_cnt_lmt_en,en_wd_info,stbc,ldpc,fw_dl,agg_en,bk,a_ctrl_bsr;
 unsigned hw_ssn_sel,hw_seq_mode,tx_cnt_lmt,data_rate,data_retry_lowest_rate,data_bw,gi_ltf;
 unsigned seq,pkt_size,hdr_llc_len,wp_offset,ch_dma,mac_id,tid_indicate,qsel,port,ampdu_num,sn;
};
struct rtw89_core_tx_request {
 struct sk_buff *skb; struct ieee80211_vif *vif; struct ieee80211_sta *sta;
 struct rtw89_vif_link *rtwvif_link; void *rtwsta_link;
 struct rtw89_tx_desc_info desc_info; enum rtw89_core_tx_type tx_type; bool with_wait;
};
static struct ieee80211_supported_band band;
static struct wiphy wiphy;
static struct ieee80211_hw hw;
static struct rtw89_dev dev;
static struct ieee80211_vif vif;
static struct ieee80211_sta peer;
static struct rtw89_vif rvif;
static struct rtw89_vif_link link;
static struct ieee80211_vif *owly_core04c_vif=&vif;
static struct { bool scanning; } glue;
static unsigned checks,failures,hooks,submissions,kicks,frees;
static unsigned last_priority,last_mapping,last_hw_queue;
static struct rtw89_tx_desc_info last_desc;
static u8 sent[2048]; static size_t sent_len;
static bool alloc_fail; static int submit_result;
#define CHECK(c) do { ++checks; if(!(c)) { ++failures; fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); } } while(0)
static struct ieee80211_hw *glue_hw(void) { return &hw; }
static struct rtw89_vif *vif_to_rtwvif(struct ieee80211_vif *p) { (void)p; return &rvif; }
static bool list_empty(const void *p) { (void)p; return false; }
static struct sk_buff *dev_alloc_skb(size_t n) { (void)n; if(alloc_fail)return NULL;struct sk_buff *s=calloc(1,sizeof(*s));if(s)s->data=s->bytes;return s; }
static void skb_reserve(struct sk_buff *s,unsigned n) { s->data+=n; }
static u8 *skb_put(struct sk_buff *s,unsigned n) { u8 *p=s->data+s->len;s->len+=n;return p; }
static void skb_set_queue_mapping(struct sk_buff *s,unsigned q) { s->mapping=q; }
static void kfree_skb(struct sk_buff *s) { ++frees;free(s); }
static void ieee80211_free_txskb(struct ieee80211_hw *h,struct sk_buff *s) { (void)h;kfree_skb(s); }
static struct ieee80211_sta *owly_peer_tx_station(const u8 *mac) { (void)mac;return &peer; }
static const struct rtw89_chan *rtw89_chan_get(struct rtw89_dev *d,unsigned idx) { (void)idx;return &d->channel; }
static inline bool ieee80211_is_ctl(__le16 fc) { return (fc&12)==4; }
static bool ieee80211_vif_is_mld(struct ieee80211_vif *v) { (void)v;return false; }
static bool is_broadcast_ether_addr(const u8 *a) { return !memcmp(a,"\xff\xff\xff\xff\xff\xff",6); }
static bool is_multicast_ether_addr(const u8 *a) { return (a[0]&1)!=0; }
static struct rtw89_addr_cam_entry *rtw89_get_addr_cam_of(struct rtw89_vif_link *l,void *s) { (void)s;return &l->addr_cam; }
static enum rtw89_core_tx_type rtw89_core_get_tx_type(struct rtw89_dev *d,struct sk_buff *s) { (void)d;return (s->data[0]&12)==8?RTW89_CORE_TX_TYPE_DATA:RTW89_CORE_TX_TYPE_MGMT; }
static void rtw89_core_tx_update_h2c_info(struct rtw89_dev *d,struct rtw89_core_tx_request *r) { (void)d;(void)r; }
static void rtw89_tx_rpt_init(struct rtw89_dev *d,struct rtw89_core_tx_request *r) { (void)d;r->desc_info.report=true;r->desc_info.tx_cnt_lmt_en=true;r->desc_info.tx_cnt_lmt=8; }
static void rtw89_core_tx_update_mgmt_info(struct rtw89_dev *d,struct rtw89_core_tx_request *r) { (void)d;r->desc_info.hw_ssn_sel=1;r->desc_info.hw_seq_mode=1;r->desc_info.en_wd_info=true; }
static void rtw89_core_tx_update_data_info(struct rtw89_dev *d,struct rtw89_core_tx_request *r) { (void)d;r->desc_info.en_wd_info=true;r->desc_info.data_retry_lowest_rate=4; }
static enum btc_pkt_type rtw89_core_tx_btc_spec_pkt_notify(struct rtw89_dev *d,struct rtw89_core_tx_request *r) { (void)d;(void)r;++hooks;return PACKET_MAX; }
static void rtw89_core_tx_update_he_qos_htc(struct rtw89_dev *d,struct rtw89_core_tx_request *r,enum btc_pkt_type p) { (void)d;(void)p;++hooks;if(r->skb->data[0]==0x88)r->skb->data[1]|=0x80; }
static void rtw89_core_tx_update_ampdu_info(struct rtw89_dev *d,struct rtw89_core_tx_request *r,enum btc_pkt_type p) { (void)d;(void)p;++hooks;r->desc_info.agg_en=true; }
static void rtw89_core_tx_update_llc_hdr(struct rtw89_dev *d,struct rtw89_tx_desc_info *desc,struct sk_buff *s) { (void)d;desc->hdr_llc_len=s->data[0]==0x88?13:12; }

#include "production_raw_tx.inc"

static int rtw89_core_tx_write(struct rtw89_dev *d,struct ieee80211_vif *v,struct ieee80211_sta *s,struct sk_buff *skb,int *qsel)
{
 ++submissions;last_priority=skb->priority;last_mapping=skb->mapping;last_hw_queue=skb->info.hw_queue;
 if(submit_result)return submit_result;
 struct rtw89_core_tx_request r={.skb=skb,.vif=v,.sta=s,.rtwvif_link=&link};
 rtw89_core_tx_update_desc_info(d,&r);last_desc=r.desc_info;
 memcpy(sent,skb->data,skb->len);sent_len=skb->len;*qsel=(int)skb->mapping;kfree_skb(skb);return 0;
}
static void rtw89_core_tx_kick_off(struct rtw89_dev *d,int q) { (void)d;(void)q;++kicks; }
#include "production_submit.inc"

static struct sk_buff frame(u8 fc,u32 flags)
{
 struct sk_buff s={0};s.bytes[0]=fc;s.bytes[4]=2;s.bytes[10]=6;s.bytes[22]=0x30;s.bytes[23]=0x12;
 memcpy(s.bytes+16,"\0\x25\0\xff\x94\x73",6);s.len=108;s.info.flags=flags;
 s.info.control.rates[0].idx=4;s.info.control.rates[0].count=3;return s;
}
static struct rtw89_tx_desc_info describe(struct sk_buff *s)
{
 s->data=s->bytes;struct rtw89_core_tx_request r={.skb=s,.vif=&vif,.sta=&peer,.rtwvif_link=&link};
 hooks=0;rtw89_core_tx_update_desc_info(&dev,&r);return r.desc_info;
}
int main(void)
{
 static const struct { unsigned chip_gen; } chip={0};dev.chip=(const void *)&chip;
 hw.wiphy=&wiphy;hw.priv=&dev;hw.extra_tx_headroom=128;wiphy.bands[0]=&band;
 band.n_bitrates=12;band.bitrates[4].bitrate=60;rvif.dlink_pool.next=rvif.dlink_pool.prev=&rvif;
 vif.hw_queue[IEEE80211_AC_BE]=42;vif.hw_queue[IEEE80211_AC_VO]=40;
 struct ieee80211_channel ch={.band=0,.hw_value=6};
 const u32 injected=IEEE80211_TX_CTL_INJECTED|IEEE80211_TX_INTFL_DONT_ENCRYPT;
 struct sk_buff s=frame(8,injected|IEEE80211_TX_CTL_REQ_TX_STATUS);u8 original[108];memcpy(original,s.bytes,108);
 CHECK(owly_sync_submit(&ch,s.bytes,108,true)==0);
 CHECK(last_priority==0);CHECK(last_mapping==IEEE80211_AC_BE);CHECK(last_hw_queue==42);
 CHECK(last_desc.report);CHECK(last_desc.tx_cnt_lmt_en && last_desc.tx_cnt_lmt==3);
 CHECK(sent_len==108 && !memcmp(sent,original,108));CHECK(hooks==0);CHECK(!last_desc.agg_en);
 s=frame(0xd0,injected);memcpy(original,s.bytes,108);
 CHECK(owly_sync_submit(&ch,s.bytes,108,false)==0);
 CHECK(last_priority==7 && last_mapping==IEEE80211_AC_VO && last_hw_queue==40);
 CHECK(last_desc.hw_seq_mode==0 && last_desc.hw_ssn_sel==0);
 CHECK(sent_len==108 && !memcmp(sent,original,108));
 /* The extracted builder must preserve raw QoS bytes even with a live HE peer. */
 s=frame(0x88,injected|IEEE80211_TX_CTL_REQ_TX_STATUS);struct rtw89_tx_desc_info desc=describe(&s);
 CHECK(s.bytes[1]==0 && hooks==0 && !desc.agg_en);
 CHECK(desc.seq==0x123 && !desc.hw_seq_mode && !desc.hw_ssn_sel);
 CHECK(desc.data_rate==RTW89_HW_RATE_OFDM6 && !desc.hw_rts_en && !desc.rts_en);
 u32 i0=rtw89_build_txwd_info0(&desc),i1=rtw89_build_txwd_info1(&desc),i4=rtw89_build_txwd_info4(&desc);
 CHECK(FIELD_GET(RTW89_TXWD_INFO0_DATA_RATE,i0)==RTW89_HW_RATE_OFDM6);
 CHECK(FIELD_GET(RTW89_TXWD_INFO1_DATA_TXCNT_LMT_SEL,i1)==1);
 CHECK(FIELD_GET(RTW89_TXWD_INFO1_DATA_TXCNT_LMT,i1)==3);
 CHECK(!FIELD_GET(RTW89_TXWD_INFO4_RTS_EN,i4) && !FIELD_GET(RTW89_TXWD_INFO4_HW_RTS_EN,i4));
 CHECK(FIELD_GET(RTW89_TXWD_BODY0_HW_SSN_MODE,rtw89_build_txwd_body0(&desc))==0);
 CHECK(FIELD_GET(RTW89_TXWD_BODY3_SW_SEQ,rtw89_build_txwd_body3(&desc))==0x123);
 CHECK(FIELD_GET(RTW89_TXWD_BODY2_TXPKT_SIZE,rtw89_build_txwd_body2(&desc))==108);
 s=frame(8,injected|IEEE80211_TX_CTL_NO_ACK|IEEE80211_TX_CTL_REQ_TX_STATUS);desc=describe(&s);
 CHECK(desc.tx_cnt_lmt_en && desc.tx_cnt_lmt==1 && desc.no_ack);
 s=frame(8,injected|IEEE80211_TX_CTL_REQ_TX_STATUS);s.info.control.rates[0].count=63;desc=describe(&s);CHECK(desc.tx_cnt_lmt==63);
 s.info.control.rates[0].flags=IEEE80211_TX_RC_USE_RTS_CTS;desc=describe(&s);CHECK(desc.rts_en && !desc.hw_rts_en);
 s.info.flags|=IEEE80211_TX_CTL_NO_ACK;desc=describe(&s);CHECK(!desc.rts_en && desc.tx_cnt_lmt==1);
 s=frame(0xd0,injected|IEEE80211_TX_CTL_ASSIGN_SEQ);desc=describe(&s);CHECK(desc.hw_seq_mode==1);
 s=frame(8,injected|IEEE80211_TX_CTL_REQ_TX_STATUS);s.info.control.rates[0].idx=-1;desc=describe(&s);CHECK(desc.data_rate==RTW89_HW_RATE_OFDM6);
 s.info.control.rates[0].idx=127;desc=describe(&s);CHECK(desc.data_rate==11);
 dev.channel.band_type=1;s.info.control.rates[0].idx=0;desc=describe(&s);CHECK(desc.data_rate==RTW89_HW_RATE_OFDM6);dev.channel.band_type=0;
 /* Ordinary associated traffic keeps its original hooks and RTS policy. */
 s=frame(0x88,IEEE80211_TX_CTL_REQ_TX_STATUS);desc=describe(&s);
 CHECK(hooks==3 && (s.bytes[1]&0x80) && desc.agg_en);
 i4=rtw89_build_txwd_info4(&desc);
 CHECK(FIELD_GET(RTW89_TXWD_INFO4_RTS_EN,i4)==1 && FIELD_GET(RTW89_TXWD_INFO4_HW_RTS_EN,i4)==1);
 CHECK(desc.tx_cnt_lmt==8);
 const unsigned before=submissions,kick_before=kicks;
 glue.scanning=true;CHECK(owly_sync_submit(&ch,original,108,true)==-EBUSY);glue.scanning=false;
 alloc_fail=true;CHECK(owly_sync_submit(&ch,original,108,true)==-ENOMEM);alloc_fail=false;
 CHECK(before==submissions && kick_before==kicks);
 submit_result=-ENOSPC;const unsigned free_before=frees;CHECK(owly_sync_submit(&ch,original,108,true)==-ENOSPC);
 CHECK(frees==free_before+1 && kicks==kick_before);
 printf("raw TX production: %u checks, %u failures; radio I/O mocked, no transfer proof\n",checks,failures);
 return failures?1:0;
}
