/*
 * Copyright (c) 2008-2011 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/dma-mapping.h>
#include "ath9k.h"

#define FUDGE 2

static void ath9k_reset_beacon_status(struct ath_softc *sc)
{
	sc->beacon.tx_processed = false;
	sc->beacon.tx_last = false;
}

/*
 *  This function will modify certain transmit queue properties depending on
 *  the operating mode of the station (AP or AdHoc).  Parameters are AIFS
 *  settings and channel width min/max
*/
static void ath9k_beaconq_config(struct ath_softc *sc)
{
	struct ath_hw *ah = sc->sc_ah;
	struct ath_common *common = ath9k_hw_common(ah);
	struct ath9k_tx_queue_info qi, qi_be;
	struct ath_txq *txq;

	ath9k_hw_get_txq_props(ah, sc->beacon.beaconq, &qi);

	if (sc->sc_ah->opmode == NL80211_IFTYPE_AP ||
	    sc->sc_ah->opmode == NL80211_IFTYPE_MESH_POINT) {
		/* Always burst out beacon and CAB traffic. */
		qi.tqi_aifs = 1;
		qi.tqi_cwmin = 0;
		qi.tqi_cwmax = 0;
	} else {
		/* Adhoc mode; important thing is to use 2x cwmin. */
		txq = sc->tx.txq_map[IEEE80211_AC_BE];
		ath9k_hw_get_txq_props(ah, txq->axq_qnum, &qi_be);
		qi.tqi_aifs = qi_be.tqi_aifs;
		if (ah->slottime == ATH9K_SLOT_TIME_20)
			qi.tqi_cwmin = 2*qi_be.tqi_cwmin;
		else
			qi.tqi_cwmin = 4*qi_be.tqi_cwmin;
		qi.tqi_cwmax = qi_be.tqi_cwmax;
	}

	if (!ath9k_hw_set_txq_props(ah, sc->beacon.beaconq, &qi)) {
		ath_err(common, "Unable to update h/w beacon queue parameters\n");
	} else {
		ath9k_hw_resettxqueue(ah, sc->beacon.beaconq);
	}
}

/*
 *  Associates the beacon frame buffer with a transmit descriptor.  Will set
 *  up rate codes, and channel flags. Beacons are always sent out at the
 *  lowest rate, and are not retried.
*/
static void ath9k_beacon_setup(struct ath_softc *sc, struct ieee80211_vif *vif,
			     struct ath_buf *bf, int rateidx)
{
	struct sk_buff *skb = bf->bf_mpdu;
	struct ath_hw *ah = sc->sc_ah;
	struct ath_common *common = ath9k_hw_common(ah);
	struct ath_tx_info info;
	struct ieee80211_supported_band *sband;
	u8 chainmask = ah->txchainmask;
	u8 rate = 0;

	sband = &common->sbands[sc->cur_chandef.chan->band];
	rate = sband->bitrates[rateidx].hw_value;
	if (vif->bss_conf.use_short_preamble)
		rate |= sband->bitrates[rateidx].hw_value_short;

	memset(&info, 0, sizeof(info));
	info.pkt_len = skb->len + FCS_LEN;
	info.type = ATH9K_PKT_TYPE_BEACON;
	info.txpower = MAX_RATE_POWER;
	info.keyix = ATH9K_TXKEYIX_INVALID;
	info.keytype = ATH9K_KEY_TYPE_CLEAR;
	info.flags = ATH9K_TXDESC_NOACK | ATH9K_TXDESC_CLRDMASK;

	info.buf_addr[0] = bf->bf_buf_addr;
	info.buf_len[0] = roundup(skb->len, 4);

	info.is_first = true;
	info.is_last = true;

	info.qcu = sc->beacon.beaconq;

	info.rates[0].Tries = 1;
	info.rates[0].Rate = rate;
	info.rates[0].ChSel = ath_txchainmask_reduction(sc, chainmask, rate);

	ath9k_hw_set_txdesc(ah, bf->bf_desc, &info);
}

static void ath9k_beacon_add_noa(struct ath_softc *sc, struct ath_vif *avp,
				 struct sk_buff *skb)
{
	static const u8 noa_ie_hdr[] = {
		WLAN_EID_VENDOR_SPECIFIC,	/* type */
		0,				/* length */
		0x50, 0x6f, 0x9a,		/* WFA OUI */
		0x09,				/* P2P subtype */
		0x0c,				/* Notice of Absence */
		0x00,				/* LSB of little-endian len */
		0x00,				/* MSB of little-endian len */
	};

	struct ieee80211_p2p_noa_attr *noa;
	int noa_len, noa_desc, i = 0;
	u8 *hdr;

	if (!avp->offchannel_duration && !avp->periodic_noa_duration)
		return;

	noa_desc = !!avp->offchannel_duration + !!avp->periodic_noa_duration;
	noa_len = 2 + sizeof(struct ieee80211_p2p_noa_desc) * noa_desc;

	hdr = skb_put(skb, sizeof(noa_ie_hdr));
	memcpy(hdr, noa_ie_hdr, sizeof(noa_ie_hdr));
	hdr[1] = sizeof(noa_ie_hdr) + noa_len - 2;
	hdr[7] = noa_len;

	noa = (void *) skb_put(skb, noa_len);
	memset(noa, 0, noa_len);

	noa->index = avp->noa_index;
	if (avp->periodic_noa_duration) {
		u32 interval = TU_TO_USEC(sc->cur_chan->beacon.beacon_interval);

		noa->desc[i].count = 255;
		noa->desc[i].start_time = cpu_to_le32(avp->periodic_noa_start);
		noa->desc[i].duration = cpu_to_le32(avp->periodic_noa_duration);
		noa->desc[i].interval = cpu_to_le32(interval);
		i++;
	}

	if (avp->offchannel_duration) {
		noa->desc[i].count = 1;
		noa->desc[i].start_time = cpu_to_le32(avp->offchannel_start);
		noa->desc[i].duration = cpu_to_le32(avp->offchannel_duration);
	}
}

static struct ath_buf *ath9k_beacon_generate(struct ieee80211_hw *hw,
					     struct ieee80211_vif *vif)
{
	struct ath_softc *sc = hw->priv;
	struct ath_common *common = ath9k_hw_common(sc->sc_ah);
	struct ath_buf *bf;
	struct ath_vif *avp = (void *)vif->drv_priv;
	struct sk_buff *skb;
	struct ath_txq *cabq = sc->beacon.cabq;
	struct ieee80211_tx_info *info;
	struct ieee80211_mgmt *mgmt_hdr;
	int cabq_depth;

	if (avp->av_bcbuf == NULL)
		return NULL;

	bf = avp->av_bcbuf;
	skb = bf->bf_mpdu;
	if (skb) {
		dma_unmap_single(sc->dev, bf->bf_buf_addr,
				 skb->len, DMA_TO_DEVICE);
		dev_kfree_skb_any(skb);
		bf->bf_buf_addr = 0;
		bf->bf_mpdu = NULL;
	}

	skb = ieee80211_beacon_get(hw, vif);
	if (skb == NULL)
		return NULL;

	bf->bf_mpdu = skb;

	mgmt_hdr = (struct ieee80211_mgmt *)skb->data;
	mgmt_hdr->u.beacon.timestamp = avp->tsf_adjust;

	info = IEEE80211_SKB_CB(skb);
	if (info->flags & IEEE80211_TX_CTL_ASSIGN_SEQ) {
		/*
		 * TODO: make sure the seq# gets assigned properly (vs. other
		 * TX frames)
		 */
		struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
		sc->tx.seq_no += 0x10;
		hdr->seq_ctrl &= cpu_to_le16(IEEE80211_SCTL_FRAG);
		hdr->seq_ctrl |= cpu_to_le16(sc->tx.seq_no);
	}

	if (vif->p2p)
		ath9k_beacon_add_noa(sc, avp, skb);

	bf->bf_buf_addr = dma_map_single(sc->dev, skb->data,
					 skb->len, DMA_TO_DEVICE);
	if (unlikely(dma_mapping_error(sc->dev, bf->bf_buf_addr))) {
		dev_kfree_skb_any(skb);
		bf->bf_mpdu = NULL;
		bf->bf_buf_addr = 0;
		ath_err(common, "dma_mapping_error on beaconing\n");
		return NULL;
	}

	skb = ieee80211_get_buffered_bc(hw, vif);

	/*
	 * if the CABQ traffic from previous DTIM is pending and the current
	 *  beacon is also a DTIM.
	 *  1) if there is only one vif let the cab traffic continue.
	 *  2) if there are more than one vif and we are using staggered
	 *     beacons, then drain the cabq by dropping all the frames in
	 *     the cabq so that the current vifs cab traffic can be scheduled.
	 */
	spin_lock_bh(&cabq->axq_lock);
	cabq_depth = cabq->axq_depth;
	spin_unlock_bh(&cabq->axq_lock);

	if (skb && cabq_depth) {
		if (sc->nvifs > 1) {
			ath_dbg(common, BEACON,
				"Flushing previous cabq traffic\n");
			ath_draintxq(sc, cabq);
		}
	}

	ath9k_beacon_setup(sc, vif, bf, info->control.rates[0].idx);

	if (skb)
		ath_tx_cabq(hw, vif, skb);

	return bf;
}

void ath9k_beacon_assign_slot(struct ath_softc *sc, struct ieee80211_vif *vif)
{
	struct ath_common *common = ath9k_hw_common(sc->sc_ah);
	struct ath_vif *avp = (void *)vif->drv_priv;
	int slot;

	avp->av_bcbuf = list_first_entry(&sc->beacon.bbuf, struct ath_buf, list);
	list_del(&avp->av_bcbuf->list);

	for (slot = 0; slot < ATH_BCBUF; slot++) {
		if (sc->beacon.bslot[slot] == NULL) {
			avp->av_bslot = slot;
			break;
		}
	}

	sc->beacon.bslot[avp->av_bslot] = vif;
	sc->nbcnvifs++;

	ath_dbg(common, CONFIG, "Added interface at beacon slot: %d\n",
		avp->av_bslot);
}

void ath9k_beacon_remove_slot(struct ath_softc *sc, struct ieee80211_vif *vif)
{
	struct ath_common *common = ath9k_hw_common(sc->sc_ah);
	struct ath_vif *avp = (void *)vif->drv_priv;
	struct ath_buf *bf = avp->av_bcbuf;

	ath_dbg(common, CONFIG, "Removing interface at beacon slot: %d\n",
		avp->av_bslot);

	tasklet_disable(&sc->bcon_tasklet);

	if (bf && bf->bf_mpdu) {
		struct sk_buff *skb = bf->bf_mpdu;
		dma_unmap_single(sc->dev, bf->bf_buf_addr,
				 skb->len, DMA_TO_DEVICE);
		dev_kfree_skb_any(skb);
		bf->bf_mpdu = NULL;
		bf->bf_buf_addr = 0;
	}

	avp->av_bcbuf = NULL;
	sc->beacon.bslot[avp->av_bslot] = NULL;
	sc->nbcnvifs--;
	list_add_tail(&bf->list, &sc->beacon.bbuf);

	tasklet_enable(&sc->bcon_tasklet);
}

static int ath9k_beacon_choose_slot(struct ath_softc *sc)
{
	struct ath_common *common = ath9k_hw_common(sc->sc_ah);
	struct ath_beacon_config *cur_conf = &sc->cur_chan->beacon;
	u16 intval;
	u32 tsftu;
	u64 tsf;
	int slot;

	if (sc->sc_ah->opmode != NL80211_IFTYPE_AP &&
	    sc->sc_ah->opmode != NL80211_IFTYPE_MESH_POINT) {
		ath_dbg(common, BEACON, "slot 0, tsf: %llu\n",
			ath9k_hw_gettsf64(sc->sc_ah));
		return 0;
	}

	intval = cur_conf->beacon_interval ? : ATH_DEFAULT_BINTVAL;
	tsf = ath9k_hw_gettsf64(sc->sc_ah);
	tsf += TU_TO_USEC(sc->sc_ah->config.sw_beacon_response_time);
	tsftu = TSF_TO_TU((tsf * ATH_BCBUF) >>32, tsf * ATH_BCBUF);
	slot = (tsftu % (intval * ATH_BCBUF)) / intval;

	ath_dbg(common, BEACON, "slot: %d tsf: %llu tsftu: %u\n",
		slot, tsf, tsftu / ATH_BCBUF);

	return slot;
}

static void ath9k_set_tsfadjust(struct ath_softc *sc, struct ieee80211_vif *vif)
{
	struct ath_common *common = ath9k_hw_common(sc->sc_ah);
	struct ath_vif *avp = (void *)vif->drv_priv;
	struct ath_beacon_config *cur_conf = &avp->chanctx->beacon;
	u32 tsfadjust;

	if (avp->av_bslot == 0)
		return;

	tsfadjust = cur_conf->beacon_interval * avp->av_bslot;
	tsfadjust = TU_TO_USEC(tsfadjust) / ATH_BCBUF;
	avp->tsf_adjust = cpu_to_le64(tsfadjust);

	ath_dbg(common, CONFIG, "tsfadjust is: %llu for bslot: %d\n",
		(unsigned long long)tsfadjust, avp->av_bslot);
}

bool ath9k_csa_is_finished(struct ath_softc *sc, struct ieee80211_vif *vif)
{
	if (!vif || !vif->csa_active)
		return false;

	if (!ieee80211_csa_is_complete(vif))
		return false;

	ieee80211_csa_finish(vif);
	return true;
}

static void ath9k_csa_update_vif(void *data, u8 *mac, struct ieee80211_vif *vif)
{
	struct ath_softc *sc = data;
	ath9k_csa_is_finished(sc, vif);
}

void ath9k_csa_update(struct ath_softc *sc)
{
	ieee80211_iterate_active_interfaces_atomic(sc->hw,
						   IEEE80211_IFACE_ITER_NORMAL,
						   ath9k_csa_update_vif, sc);
}

void ath9k_beacon_tasklet(unsigned long data)
{
	struct ath_softc *sc = (struct ath_softc *)data;
	struct ath_hw *ah = sc->sc_ah;
	struct ath_common *common = ath9k_hw_common(ah);
	struct ath_buf *bf = NULL;
	struct ieee80211_vif *vif;
	bool edma = !!(ah->caps.hw_caps & ATH9K_HW_CAP_EDMA);
	int slot;

	if (test_bit(ATH_OP_HW_RESET, &common->op_flags)) {
		ath_dbg(common, RESET,
			"reset work is pending, skip beaconing now\n");
		return;
	}

	/*
	 * Check if the previous beacon has gone out.  If
	 * not don't try to post another, skip this period
	 * and wait for the next.  Missed beacons indicate
	 * a problem and should not occur.  If we miss too
	 * many consecutive beacons reset the device.
	 */
	if (ath9k_hw_numtxpending(ah, sc->beacon.beaconq) != 0) {
		sc->beacon.bmisscnt++;

		ath9k_hw_check_nav(ah);

		/*
		 * If the previous beacon has not been transmitted
		 * and a MAC/BB hang has been identified, return
		 * here because a chip reset would have been
		 * initiated.
		 */
		if (!ath_hw_check(sc))
			return;

		if (sc->beacon.bmisscnt < BSTUCK_THRESH * sc->nbcnvifs) {
			ath_dbg(common, BSTUCK,
				"missed %u consecutive beacons\n",
				sc->beacon.bmisscnt);
			ath9k_hw_stop_dma_queue(ah, sc->beacon.beaconq);
			if (sc->beacon.bmisscnt > 3)
				ath9k_hw_bstuck_nfcal(ah);
		} else if (sc->beacon.bmisscnt >= BSTUCK_THRESH) {
			ath_dbg(common, BSTUCK, "beacon is officially stuck\n");
			sc->beacon.bmisscnt = 0;
			ath9k_queue_reset(sc, RESET_TYPE_BEACON_STUCK);
		}

		return;
	}

	slot = ath9k_beacon_choose_slot(sc);
	vif = sc->beacon.bslot[slot];

	/* EDMA devices check that in the tx completion function. */
	if (!edma) {
		if (sc->sched.beacon_pending)
			ath_chanctx_event(sc, NULL,
					  ATH_CHANCTX_EVENT_BEACON_SENT);

		if (ath9k_csa_is_finished(sc, vif))
			return;
	}

	if (!vif || !vif->bss_conf.enable_beacon)
		return;

	ath_chanctx_event(sc, vif, ATH_CHANCTX_EVENT_BEACON_PREPARE);
	bf = ath9k_beacon_generate(sc->hw, vif);

	if (sc->beacon.bmisscnt != 0) {
		ath_dbg(common, BSTUCK, "resume beacon xmit after %u misses\n",
			sc->beacon.bmisscnt);
		sc->beacon.bmisscnt = 0;
	}

	/*
	 * Handle slot time change when a non-ERP station joins/leaves
	 * an 11g network.  The 802.11 layer notifies us via callback,
	 * we mark updateslot, then wait one beacon before effecting
	 * the change.  This gives associated stations at least one
	 * beacon interval to note the state change.
	 *
	 * NB: The slot time change state machine is clocked according
	 *     to whether we are bursting or staggering beacons.  We
	 *     recognize the request to update and record the current
	 *     slot then don't transition until that slot is reached
	 *     again.  If we miss a beacon for that slot then we'll be
	 *     slow to transition but we'll be sure at least one beacon
	 *     interval has passed.  When bursting slot is always left
	 *     set to ATH_BCBUF so this check is a noop.
	 */
	if (sc->beacon.updateslot == UPDATE) {
		sc->beacon.updateslot = COMMIT;
		sc->beacon.slotupdate = slot;
	} else if (sc->beacon.updateslot == COMMIT &&
		   sc->beacon.slotupdate == slot) {
		ah->slottime = sc->beacon.slottime;
		ath9k_hw_init_global_settings(ah);
		sc->beacon.updateslot = OK;
	}

	if (bf) {
		ath9k_reset_beacon_status(sc);

		ath_dbg(common, BEACON,
			"Transmitting beacon for slot: %d\n", slot);

		/* NB: cabq traffic should already be queued and primed */
		ath9k_hw_puttxbuf(ah, sc->beacon.beaconq, bf->bf_daddr);

		if (!edma)
			ath9k_hw_txstart(ah, sc->beacon.beaconq);
	}
}

/*
 * Both nexttbtt and intval have to be in usecs.
 */
static void ath9k_beacon_init(struct ath_softc *sc, u32 nexttbtt,
			      u32 intval, bool reset_tsf)
{
	struct ath_hw *ah = sc->sc_ah;

	ath9k_hw_disable_interrupts(ah);
	if (reset_tsf)
		ath9k_hw_reset_tsf(ah);
	ath9k_beaconq_config(sc);
	ath9k_hw_beaconinit(ah, nexttbtt, intval);
	sc->beacon.bmisscnt = 0;
	ath9k_hw_set_interrupts(ah);
	ath9k_hw_enable_interrupts(ah);
}

/*
 * For multi-bss ap support beacons are either staggered evenly over N slots or
 * burst together.  For the former arrange for the SWBA to be delivered for each
 * slot. Slots that are not occupied will generate nothing.
 */
static void ath9k_beacon_config_ap(struct ath_softc *sc,
				   struct ath_beacon_config *conf)
{
	struct ath_hw *ah = sc->sc_ah;

	ath9k_cmn_beacon_config_ap(ah, conf, ATH_BCBUF);
	ath9k_beacon_init(sc, conf->nexttbtt, conf->intval, false);
}

static void ath9k_beacon_config_sta(struct ath_hw *ah,
				    struct ath_beacon_config *conf)
{
	struct ath9k_beacon_state bs;

	if (ath9k_cmn_beacon_config_sta(ah, conf, &bs) == -EPERM)
		return;

	ath9k_hw_disable_interrupts(ah);
	ath9k_hw_set_sta_beacon_timers(ah, &bs);
	ah->imask |= ATH9K_INT_BMISS;

	ath9k_hw_set_interrupts(ah);
	ath9k_hw_enable_interrupts(ah);
}

static void ath9k_beacon_config_adhoc(struct ath_softc *sc,
				      struct ath_beacon_config *conf)
{
	struct ath_hw *ah = sc->sc_ah;
	struct ath_common *common = ath9k_hw_common(ah);

	ath9k_reset_beacon_status(sc);

	ath9k_cmn_beacon_config_adhoc(ah, conf);

	ath9k_beacon_init(sc, conf->nexttbtt, conf->intval, conf->ibss_creator);

	/*
	 * Set the global 'beacon has been configured' flag for the
	 * joiner case in IBSS mode.
	 */
	if (!conf->ibss_creator && conf->enable_beacon)
		set_bit(ATH_OP_BEACONS, &common->op_flags);
}

static bool ath9k_allow_beacon_config(struct ath_softc *sc,
				      struct ieee80211_vif *vif)
{
	struct ath_common *common = ath9k_hw_common(sc->sc_ah);

	if (sc->sc_ah->opmode == NL80211_IFTYPE_AP) {
		if ((vif->type != NL80211_IFTYPE_AP) ||
		    (sc->nbcnvifs > 1)) {
			ath_dbg(common, CONFIG,
				"An AP interface is already present !\n");
			return false;
		}
	}

	if (sc->sc_ah->opmode == NL80211_IFTYPE_STATION) {
		if ((vif->type == NL80211_IFTYPE_STATION) &&
		    test_bit(ATH_OP_BEACONS, &common->op_flags) &&
		    vif != sc->cur_chan->primary_sta) {
			ath_dbg(common, CONFIG,
				"Beacon already configured for a station interface\n");
			return false;
		}
	}

	return true;
}

static void ath9k_cache_beacon_config(struct ath_softc *sc,
				      struct ath_chanctx *ctx,
				      struct ieee80211_bss_conf *bss_conf)
{
	struct ath_common *common = ath9k_hw_common(sc->sc_ah);
	struct ath_beacon_config *cur_conf = &ctx->beacon;

	ath_dbg(common, BEACON,
		"Caching beacon data for BSS: %pM\n", bss_conf->bssid);

	cur_conf->beacon_interval = bss_conf->beacon_int;
	cur_conf->dtim_period = bss_conf->dtim_period;
	cur_conf->dtim_count = 1;
	cur_conf->ibss_creator = bss_conf->ibss_creator;

	/*
	 * It looks like mac80211 may end up using beacon interval of zero in
	 * some cases (at least for mesh point). Avoid getting into an
	 * infinite loop by using a bit safer value instead. To be safe,
	 * do sanity check on beacon interval for all operating modes.
	 */
	if (cur_conf->beacon_interval == 0)
		cur_conf->beacon_interval = 100;

	cur_conf->bmiss_timeout =
		ATH_DEFAULT_BMISS_LIMIT * cur_conf->beacon_interval;

	/*
	 * We don't parse dtim period from mac80211 during the driver
	 * initialization as it breaks association with hidden-ssid
	 * AP and it causes latency in roaming
	 */
	if (cur_conf->dtim_period == 0)
		cur_conf->dtim_period = 1;

}

void ath9k_beacon_config(struct ath_softc *sc, struct ieee80211_vif *vif,
			 u32 changed)
{
	struct ieee80211_bss_conf *bss_conf = &vif->bss_conf;
        struct ath_hw *ah = sc->sc_ah;
        struct ath_common *common = ath9k_hw_common(ah);
	struct ath_vif *avp = (void *)vif->drv_priv;
	struct ath_chanctx *ctx = avp->chanctx;
	struct ath_beacon_config *cur_conf;
	unsigned long flags;
	bool skip_beacon = false;

	if (!ctx)
		return;

	cur_conf = &avp->chanctx->beacon;
	if (vif->type == NL80211_IFTYPE_AP)
		ath9k_set_tsfadjust(sc, vif);

	if (!ath9k_allow_beacon_config(sc, vif))
		return;

	if (vif->type == NL80211_IFTYPE_STATION) {
		ath9k_cache_beacon_config(sc, ctx, bss_conf);
		if (ctx != sc->cur_chan)
			return;

		ath9k_set_beacon(sc);
		set_bit(ATH_OP_BEACONS, &common->op_flags);
		return;
	}

	/*
	 * Take care of multiple interfaces when
	 * enabling/disabling SWBA.
	 */
	if (changed & BSS_CHANGED_BEACON_ENABLED) {
		if (!bss_conf->enable_beacon &&
		    (sc->nbcnvifs <= 1)) {
			cur_conf->enable_beacon = false;
		} else if (bss_conf->enable_beacon) {
			cur_conf->enable_beacon = true;
			ath9k_cache_beacon_config(sc, ctx, bss_conf);
		}
	}

	if (ctx != sc->cur_chan)
		return;

	/*
	 * Configure the HW beacon registers only when we have a valid
	 * beacon interval.
	 */
	if (cur_conf->beacon_interval) {
		/*
		 * If we are joining an existing IBSS network, start beaconing
		 * only after a TSF-sync has taken place. Ensure that this
		 * happens by setting the appropriate flags.
		 */
		if ((changed & BSS_CHANGED_IBSS) && !bss_conf->ibss_creator &&
		    bss_conf->enable_beacon) {
			spin_lock_irqsave(&sc->sc_pm_lock, flags);
			sc->ps_flags |= PS_BEACON_SYNC | PS_WAIT_FOR_BEACON;
			spin_unlock_irqrestore(&sc->sc_pm_lock, flags);
			skip_beacon = true;
		} else {
			ath9k_set_beacon(sc);
		}

		/*
		 * Do not set the ATH_OP_BEACONS flag for IBSS joiner mode
		 * here, it is done in ath9k_beacon_config_adhoc().
		 */
		if (cur_conf->enable_beacon && !skip_beacon)
			set_bit(ATH_OP_BEACONS, &common->op_flags);
		else
			clear_bit(ATH_OP_BEACONS, &common->op_flags);
	}
}

void ath9k_set_beacon(struct ath_softc *sc)
{
	struct ath_common *common = ath9k_hw_common(sc->sc_ah);
	struct ath_beacon_config *cur_conf = &sc->cur_chan->beacon;

	switch (sc->sc_ah->opmode) {
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_MESH_POINT:
		ath9k_beacon_config_ap(sc, cur_conf);
		break;
	case NL80211_IFTYPE_ADHOC:
		ath9k_beacon_config_adhoc(sc, cur_conf);
		break;
	case NL80211_IFTYPE_STATION:
		ath9k_beacon_config_sta(sc->sc_ah, cur_conf);
		break;
	default:
		ath_dbg(common, CONFIG, "Unsupported beaconing mode\n");
		return;
	}
}
