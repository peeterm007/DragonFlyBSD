/*-
 * Copyright (c) 2008, Pyun YongHyeon <yongari@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/jme/if_jme.c,v 1.2 2008/07/18 04:20:48 yongari Exp $
 * $DragonFly: src/sys/dev/netif/jme/if_jme.c,v 1.12 2008/11/26 11:55:18 sephe Exp $
 */

#include "opt_polling.h"
#include "opt_rss.h"
#include "opt_jme.h"

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/serialize.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/bpf.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/ifq_var.h>
#ifdef RSS
#include <net/toeplitz.h>
#endif
#include <net/vlan/if_vlan_var.h>
#include <net/vlan/if_vlan_ether.h>

#include <dev/netif/mii_layer/miivar.h>
#include <dev/netif/mii_layer/jmphyreg.h>

#include <bus/pci/pcireg.h>
#include <bus/pci/pcivar.h>
#include <bus/pci/pcidevs.h>

#include <dev/netif/jme/if_jmereg.h>
#include <dev/netif/jme/if_jmevar.h>

#include "miibus_if.h"

/* Define the following to disable printing Rx errors. */
#undef	JME_SHOW_ERRORS

#define	JME_CSUM_FEATURES	(CSUM_IP | CSUM_TCP | CSUM_UDP)

#ifdef JME_RSS_DEBUG
#define JME_RSS_DPRINTF(sc, lvl, fmt, ...) \
do { \
	if ((sc)->jme_rss_debug >= (lvl)) \
		if_printf(&(sc)->arpcom.ac_if, fmt, __VA_ARGS__); \
} while (0)
#else	/* !JME_RSS_DEBUG */
#define JME_RSS_DPRINTF(sc, lvl, fmt, ...)	((void)0)
#endif	/* JME_RSS_DEBUG */

static int	jme_probe(device_t);
static int	jme_attach(device_t);
static int	jme_detach(device_t);
static int	jme_shutdown(device_t);
static int	jme_suspend(device_t);
static int	jme_resume(device_t);

static int	jme_miibus_readreg(device_t, int, int);
static int	jme_miibus_writereg(device_t, int, int, int);
static void	jme_miibus_statchg(device_t);

static void	jme_init(void *);
static int	jme_ioctl(struct ifnet *, u_long, caddr_t, struct ucred *);
static void	jme_start(struct ifnet *);
static void	jme_watchdog(struct ifnet *);
static void	jme_mediastatus(struct ifnet *, struct ifmediareq *);
static int	jme_mediachange(struct ifnet *);
#ifdef DEVICE_POLLING
static void	jme_poll(struct ifnet *, enum poll_cmd, int);
#endif

static void	jme_intr(void *);
static void	jme_txeof(struct jme_softc *);
static void	jme_rxeof(struct jme_softc *, int);
static int	jme_rxeof_chain(struct jme_softc *, int,
				struct mbuf_chain *, int);
static void	jme_rx_intr(struct jme_softc *, uint32_t);

static int	jme_dma_alloc(struct jme_softc *);
static void	jme_dma_free(struct jme_softc *);
static int	jme_init_rx_ring(struct jme_softc *, int);
static void	jme_init_tx_ring(struct jme_softc *);
static void	jme_init_ssb(struct jme_softc *);
static int	jme_newbuf(struct jme_softc *, int, struct jme_rxdesc *, int);
static int	jme_encap(struct jme_softc *, struct mbuf **);
static void	jme_rxpkt(struct jme_softc *, int, struct mbuf_chain *);
static int	jme_rxring_dma_alloc(struct jme_softc *, int);
static int	jme_rxbuf_dma_alloc(struct jme_softc *, int);

static void	jme_tick(void *);
static void	jme_stop(struct jme_softc *);
static void	jme_reset(struct jme_softc *);
static void	jme_set_vlan(struct jme_softc *);
static void	jme_set_filter(struct jme_softc *);
static void	jme_stop_tx(struct jme_softc *);
static void	jme_stop_rx(struct jme_softc *);
static void	jme_mac_config(struct jme_softc *);
static void	jme_reg_macaddr(struct jme_softc *, uint8_t[]);
static int	jme_eeprom_macaddr(struct jme_softc *, uint8_t[]);
static int	jme_eeprom_read_byte(struct jme_softc *, uint8_t, uint8_t *);
#ifdef notyet
static void	jme_setwol(struct jme_softc *);
static void	jme_setlinkspeed(struct jme_softc *);
#endif
static void	jme_set_tx_coal(struct jme_softc *);
static void	jme_set_rx_coal(struct jme_softc *);
#ifdef RSS
static void	jme_enable_rss(struct jme_softc *);
#endif
static void	jme_disable_rss(struct jme_softc *);

static void	jme_sysctl_node(struct jme_softc *);
static int	jme_sysctl_tx_coal_to(SYSCTL_HANDLER_ARGS);
static int	jme_sysctl_tx_coal_pkt(SYSCTL_HANDLER_ARGS);
static int	jme_sysctl_rx_coal_to(SYSCTL_HANDLER_ARGS);
static int	jme_sysctl_rx_coal_pkt(SYSCTL_HANDLER_ARGS);

/*
 * Devices supported by this driver.
 */
static const struct jme_dev {
	uint16_t	jme_vendorid;
	uint16_t	jme_deviceid;
	uint32_t	jme_caps;
	const char	*jme_name;
} jme_devs[] = {
	{ PCI_VENDOR_JMICRON, PCI_PRODUCT_JMICRON_JMC250,
	    JME_CAP_JUMBO,
	    "JMicron Inc, JMC250 Gigabit Ethernet" },
	{ PCI_VENDOR_JMICRON, PCI_PRODUCT_JMICRON_JMC260,
	    JME_CAP_FASTETH,
	    "JMicron Inc, JMC260 Fast Ethernet" },
	{ 0, 0, 0, NULL }
};

static device_method_t jme_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,		jme_probe),
	DEVMETHOD(device_attach,	jme_attach),
	DEVMETHOD(device_detach,	jme_detach),
	DEVMETHOD(device_shutdown,	jme_shutdown),
	DEVMETHOD(device_suspend,	jme_suspend),
	DEVMETHOD(device_resume,	jme_resume),

	/* Bus interface. */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface. */
	DEVMETHOD(miibus_readreg,	jme_miibus_readreg),
	DEVMETHOD(miibus_writereg,	jme_miibus_writereg),
	DEVMETHOD(miibus_statchg,	jme_miibus_statchg),

	{ NULL, NULL }
};

static driver_t jme_driver = {
	"jme",
	jme_methods,
	sizeof(struct jme_softc)
};

static devclass_t jme_devclass;

DECLARE_DUMMY_MODULE(if_jme);
MODULE_DEPEND(if_jme, miibus, 1, 1, 1);
DRIVER_MODULE(if_jme, pci, jme_driver, jme_devclass, 0, 0);
DRIVER_MODULE(miibus, jme, miibus_driver, miibus_devclass, 0, 0);

static const struct {
	uint32_t	jme_coal;
	uint32_t	jme_comp;
} jme_rx_status[JME_NRXRING_MAX] = {
	{ INTR_RXQ0_COAL | INTR_RXQ0_COAL_TO, INTR_RXQ0_COMP },
	{ INTR_RXQ1_COAL | INTR_RXQ1_COAL_TO, INTR_RXQ1_COMP },
	{ INTR_RXQ2_COAL | INTR_RXQ2_COAL_TO, INTR_RXQ2_COMP },
	{ INTR_RXQ3_COAL | INTR_RXQ3_COAL_TO, INTR_RXQ3_COMP }
};

static int	jme_rx_desc_count = JME_RX_DESC_CNT_DEF;
static int	jme_tx_desc_count = JME_TX_DESC_CNT_DEF;
static int	jme_rx_ring_count = JME_NRXRING_DEF;

TUNABLE_INT("hw.jme.rx_desc_count", &jme_rx_desc_count);
TUNABLE_INT("hw.jme.tx_desc_count", &jme_tx_desc_count);
TUNABLE_INT("hw.jme.rx_ring_count", &jme_rx_ring_count);

/*
 *	Read a PHY register on the MII of the JMC250.
 */
static int
jme_miibus_readreg(device_t dev, int phy, int reg)
{
	struct jme_softc *sc = device_get_softc(dev);
	uint32_t val;
	int i;

	/* For FPGA version, PHY address 0 should be ignored. */
	if (sc->jme_caps & JME_CAP_FPGA) {
		if (phy == 0)
			return (0);
	} else {
		if (sc->jme_phyaddr != phy)
			return (0);
	}

	CSR_WRITE_4(sc, JME_SMI, SMI_OP_READ | SMI_OP_EXECUTE |
	    SMI_PHY_ADDR(phy) | SMI_REG_ADDR(reg));

	for (i = JME_PHY_TIMEOUT; i > 0; i--) {
		DELAY(1);
		if (((val = CSR_READ_4(sc, JME_SMI)) & SMI_OP_EXECUTE) == 0)
			break;
	}
	if (i == 0) {
		device_printf(sc->jme_dev, "phy read timeout: "
			      "phy %d, reg %d\n", phy, reg);
		return (0);
	}

	return ((val & SMI_DATA_MASK) >> SMI_DATA_SHIFT);
}

/*
 *	Write a PHY register on the MII of the JMC250.
 */
static int
jme_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct jme_softc *sc = device_get_softc(dev);
	int i;

	/* For FPGA version, PHY address 0 should be ignored. */
	if (sc->jme_caps & JME_CAP_FPGA) {
		if (phy == 0)
			return (0);
	} else {
		if (sc->jme_phyaddr != phy)
			return (0);
	}

	CSR_WRITE_4(sc, JME_SMI, SMI_OP_WRITE | SMI_OP_EXECUTE |
	    ((val << SMI_DATA_SHIFT) & SMI_DATA_MASK) |
	    SMI_PHY_ADDR(phy) | SMI_REG_ADDR(reg));

	for (i = JME_PHY_TIMEOUT; i > 0; i--) {
		DELAY(1);
		if (((val = CSR_READ_4(sc, JME_SMI)) & SMI_OP_EXECUTE) == 0)
			break;
	}
	if (i == 0) {
		device_printf(sc->jme_dev, "phy write timeout: "
			      "phy %d, reg %d\n", phy, reg);
	}

	return (0);
}

/*
 *	Callback from MII layer when media changes.
 */
static void
jme_miibus_statchg(device_t dev)
{
	struct jme_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct mii_data *mii;
	struct jme_txdesc *txd;
	bus_addr_t paddr;
	int i, r;

	ASSERT_SERIALIZED(ifp->if_serializer);

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	mii = device_get_softc(sc->jme_miibus);

	sc->jme_flags &= ~JME_FLAG_LINK;
	if ((mii->mii_media_status & IFM_AVALID) != 0) {
		switch (IFM_SUBTYPE(mii->mii_media_active)) {
		case IFM_10_T:
		case IFM_100_TX:
			sc->jme_flags |= JME_FLAG_LINK;
			break;
		case IFM_1000_T:
			if (sc->jme_caps & JME_CAP_FASTETH)
				break;
			sc->jme_flags |= JME_FLAG_LINK;
			break;
		default:
			break;
		}
	}

	/*
	 * Disabling Rx/Tx MACs have a side-effect of resetting
	 * JME_TXNDA/JME_RXNDA register to the first address of
	 * Tx/Rx descriptor address. So driver should reset its
	 * internal procucer/consumer pointer and reclaim any
	 * allocated resources.  Note, just saving the value of
	 * JME_TXNDA and JME_RXNDA registers before stopping MAC
	 * and restoring JME_TXNDA/JME_RXNDA register is not
	 * sufficient to make sure correct MAC state because
	 * stopping MAC operation can take a while and hardware
	 * might have updated JME_TXNDA/JME_RXNDA registers
	 * during the stop operation.
	 */

	/* Disable interrupts */
	CSR_WRITE_4(sc, JME_INTR_MASK_CLR, JME_INTRS);

	/* Stop driver */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;
	callout_stop(&sc->jme_tick_ch);

	/* Stop receiver/transmitter. */
	jme_stop_rx(sc);
	jme_stop_tx(sc);

	for (r = 0; r < sc->jme_rx_ring_inuse; ++r) {
		struct jme_rxdata *rdata = &sc->jme_cdata.jme_rx_data[r];

		jme_rxeof(sc, r);
		if (rdata->jme_rxhead != NULL)
			m_freem(rdata->jme_rxhead);
		JME_RXCHAIN_RESET(sc, r);

		/*
		 * Reuse configured Rx descriptors and reset
		 * procuder/consumer index.
		 */
		rdata->jme_rx_cons = 0;
	}

	jme_txeof(sc);
	if (sc->jme_cdata.jme_tx_cnt != 0) {
		/* Remove queued packets for transmit. */
		for (i = 0; i < sc->jme_tx_desc_cnt; i++) {
			txd = &sc->jme_cdata.jme_txdesc[i];
			if (txd->tx_m != NULL) {
				bus_dmamap_unload(
				    sc->jme_cdata.jme_tx_tag,
				    txd->tx_dmamap);
				m_freem(txd->tx_m);
				txd->tx_m = NULL;
				txd->tx_ndesc = 0;
				ifp->if_oerrors++;
			}
		}
	}
	jme_init_tx_ring(sc);

	/* Initialize shadow status block. */
	jme_init_ssb(sc);

	/* Program MAC with resolved speed/duplex/flow-control. */
	if (sc->jme_flags & JME_FLAG_LINK) {
		jme_mac_config(sc);

		CSR_WRITE_4(sc, JME_TXCSR, sc->jme_txcsr);

		/* Set Tx ring address to the hardware. */
		paddr = sc->jme_cdata.jme_tx_ring_paddr;
		CSR_WRITE_4(sc, JME_TXDBA_HI, JME_ADDR_HI(paddr));
		CSR_WRITE_4(sc, JME_TXDBA_LO, JME_ADDR_LO(paddr));

		for (r = 0; r < sc->jme_rx_ring_inuse; ++r) {
			CSR_WRITE_4(sc, JME_RXCSR,
			    sc->jme_rxcsr | RXCSR_RXQ_N_SEL(r));

			/* Set Rx ring address to the hardware. */
			paddr = sc->jme_cdata.jme_rx_data[r].jme_rx_ring_paddr;
			CSR_WRITE_4(sc, JME_RXDBA_HI, JME_ADDR_HI(paddr));
			CSR_WRITE_4(sc, JME_RXDBA_LO, JME_ADDR_LO(paddr));
		}

		/* Restart receiver/transmitter. */
		CSR_WRITE_4(sc, JME_RXCSR, sc->jme_rxcsr | RXCSR_RX_ENB |
		    RXCSR_RXQ_START);
		CSR_WRITE_4(sc, JME_TXCSR, sc->jme_txcsr | TXCSR_TX_ENB);
	}

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	callout_reset(&sc->jme_tick_ch, hz, jme_tick, sc);

#ifdef DEVICE_POLLING
	if (!(ifp->if_flags & IFF_POLLING))
#endif
	/* Reenable interrupts. */
	CSR_WRITE_4(sc, JME_INTR_MASK_SET, JME_INTRS);
}

/*
 *	Get the current interface media status.
 */
static void
jme_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct jme_softc *sc = ifp->if_softc;
	struct mii_data *mii = device_get_softc(sc->jme_miibus);

	ASSERT_SERIALIZED(ifp->if_serializer);

	mii_pollstat(mii);
	ifmr->ifm_status = mii->mii_media_status;
	ifmr->ifm_active = mii->mii_media_active;
}

/*
 *	Set hardware to newly-selected media.
 */
static int
jme_mediachange(struct ifnet *ifp)
{
	struct jme_softc *sc = ifp->if_softc;
	struct mii_data *mii = device_get_softc(sc->jme_miibus);
	int error;

	ASSERT_SERIALIZED(ifp->if_serializer);

	if (mii->mii_instance != 0) {
		struct mii_softc *miisc;

		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}
	error = mii_mediachg(mii);

	return (error);
}

static int
jme_probe(device_t dev)
{
	const struct jme_dev *sp;
	uint16_t vid, did;

	vid = pci_get_vendor(dev);
	did = pci_get_device(dev);
	for (sp = jme_devs; sp->jme_name != NULL; ++sp) {
		if (vid == sp->jme_vendorid && did == sp->jme_deviceid) {
			struct jme_softc *sc = device_get_softc(dev);

			sc->jme_caps = sp->jme_caps;
			device_set_desc(dev, sp->jme_name);
			return (0);
		}
	}
	return (ENXIO);
}

static int
jme_eeprom_read_byte(struct jme_softc *sc, uint8_t addr, uint8_t *val)
{
	uint32_t reg;
	int i;

	*val = 0;
	for (i = JME_TIMEOUT; i > 0; i--) {
		reg = CSR_READ_4(sc, JME_SMBCSR);
		if ((reg & SMBCSR_HW_BUSY_MASK) == SMBCSR_HW_IDLE)
			break;
		DELAY(1);
	}

	if (i == 0) {
		device_printf(sc->jme_dev, "EEPROM idle timeout!\n");
		return (ETIMEDOUT);
	}

	reg = ((uint32_t)addr << SMBINTF_ADDR_SHIFT) & SMBINTF_ADDR_MASK;
	CSR_WRITE_4(sc, JME_SMBINTF, reg | SMBINTF_RD | SMBINTF_CMD_TRIGGER);
	for (i = JME_TIMEOUT; i > 0; i--) {
		DELAY(1);
		reg = CSR_READ_4(sc, JME_SMBINTF);
		if ((reg & SMBINTF_CMD_TRIGGER) == 0)
			break;
	}

	if (i == 0) {
		device_printf(sc->jme_dev, "EEPROM read timeout!\n");
		return (ETIMEDOUT);
	}

	reg = CSR_READ_4(sc, JME_SMBINTF);
	*val = (reg & SMBINTF_RD_DATA_MASK) >> SMBINTF_RD_DATA_SHIFT;

	return (0);
}

static int
jme_eeprom_macaddr(struct jme_softc *sc, uint8_t eaddr[])
{
	uint8_t fup, reg, val;
	uint32_t offset;
	int match;

	offset = 0;
	if (jme_eeprom_read_byte(sc, offset++, &fup) != 0 ||
	    fup != JME_EEPROM_SIG0)
		return (ENOENT);
	if (jme_eeprom_read_byte(sc, offset++, &fup) != 0 ||
	    fup != JME_EEPROM_SIG1)
		return (ENOENT);
	match = 0;
	do {
		if (jme_eeprom_read_byte(sc, offset, &fup) != 0)
			break;
		if (JME_EEPROM_MKDESC(JME_EEPROM_FUNC0, JME_EEPROM_PAGE_BAR1) ==
		    (fup & (JME_EEPROM_FUNC_MASK | JME_EEPROM_PAGE_MASK))) {
			if (jme_eeprom_read_byte(sc, offset + 1, &reg) != 0)
				break;
			if (reg >= JME_PAR0 &&
			    reg < JME_PAR0 + ETHER_ADDR_LEN) {
				if (jme_eeprom_read_byte(sc, offset + 2,
				    &val) != 0)
					break;
				eaddr[reg - JME_PAR0] = val;
				match++;
			}
		}
		/* Check for the end of EEPROM descriptor. */
		if ((fup & JME_EEPROM_DESC_END) == JME_EEPROM_DESC_END)
			break;
		/* Try next eeprom descriptor. */
		offset += JME_EEPROM_DESC_BYTES;
	} while (match != ETHER_ADDR_LEN && offset < JME_EEPROM_END);

	if (match == ETHER_ADDR_LEN)
		return (0);

	return (ENOENT);
}

static void
jme_reg_macaddr(struct jme_softc *sc, uint8_t eaddr[])
{
	uint32_t par0, par1;

	/* Read station address. */
	par0 = CSR_READ_4(sc, JME_PAR0);
	par1 = CSR_READ_4(sc, JME_PAR1);
	par1 &= 0xFFFF;
	if ((par0 == 0 && par1 == 0) || (par0 & 0x1)) {
		device_printf(sc->jme_dev,
		    "generating fake ethernet address.\n");
		par0 = karc4random();
		/* Set OUI to JMicron. */
		eaddr[0] = 0x00;
		eaddr[1] = 0x1B;
		eaddr[2] = 0x8C;
		eaddr[3] = (par0 >> 16) & 0xff;
		eaddr[4] = (par0 >> 8) & 0xff;
		eaddr[5] = par0 & 0xff;
	} else {
		eaddr[0] = (par0 >> 0) & 0xFF;
		eaddr[1] = (par0 >> 8) & 0xFF;
		eaddr[2] = (par0 >> 16) & 0xFF;
		eaddr[3] = (par0 >> 24) & 0xFF;
		eaddr[4] = (par1 >> 0) & 0xFF;
		eaddr[5] = (par1 >> 8) & 0xFF;
	}
}

static int
jme_attach(device_t dev)
{
	struct jme_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = &sc->arpcom.ac_if;
	uint32_t reg;
	uint16_t did;
	uint8_t pcie_ptr, rev;
	int error = 0;
	uint8_t eaddr[ETHER_ADDR_LEN];

	sc->jme_rx_desc_cnt = roundup(jme_rx_desc_count, JME_NDESC_ALIGN);
	if (sc->jme_rx_desc_cnt > JME_NDESC_MAX)
		sc->jme_rx_desc_cnt = JME_NDESC_MAX;

	sc->jme_tx_desc_cnt = roundup(jme_tx_desc_count, JME_NDESC_ALIGN);
	if (sc->jme_tx_desc_cnt > JME_NDESC_MAX)
		sc->jme_tx_desc_cnt = JME_NDESC_MAX;

#ifdef RSS
	sc->jme_rx_ring_cnt = jme_rx_ring_count;
	if (sc->jme_rx_ring_cnt <= 0)
		sc->jme_rx_ring_cnt = JME_NRXRING_1;
	if (sc->jme_rx_ring_cnt > ncpus2)
		sc->jme_rx_ring_cnt = ncpus2;

	if (sc->jme_rx_ring_cnt >= JME_NRXRING_4)
		sc->jme_rx_ring_cnt = JME_NRXRING_4;
	else if (sc->jme_rx_ring_cnt >= JME_NRXRING_2)
		sc->jme_rx_ring_cnt = JME_NRXRING_2;
#else
	sc->jme_rx_ring_cnt = JME_NRXRING_MIN;
#endif
	sc->jme_rx_ring_inuse = sc->jme_rx_ring_cnt;

	sc->jme_dev = dev;
	sc->jme_lowaddr = BUS_SPACE_MAXADDR;

	if_initname(ifp, device_get_name(dev), device_get_unit(dev));

	callout_init(&sc->jme_tick_ch);

#ifndef BURN_BRIDGES
	if (pci_get_powerstate(dev) != PCI_POWERSTATE_D0) {
		uint32_t irq, mem;

		irq = pci_read_config(dev, PCIR_INTLINE, 4);
		mem = pci_read_config(dev, JME_PCIR_BAR, 4);

		device_printf(dev, "chip is in D%d power mode "
		    "-- setting to D0\n", pci_get_powerstate(dev));

		pci_set_powerstate(dev, PCI_POWERSTATE_D0);

		pci_write_config(dev, PCIR_INTLINE, irq, 4);
		pci_write_config(dev, JME_PCIR_BAR, mem, 4);
	}
#endif	/* !BURN_BRIDGE */

	/* Enable bus mastering */
	pci_enable_busmaster(dev);

	/*
	 * Allocate IO memory
	 *
	 * JMC250 supports both memory mapped and I/O register space
	 * access.  Because I/O register access should use different
	 * BARs to access registers it's waste of time to use I/O
	 * register spce access.  JMC250 uses 16K to map entire memory
	 * space.
	 */
	sc->jme_mem_rid = JME_PCIR_BAR;
	sc->jme_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
						 &sc->jme_mem_rid, RF_ACTIVE);
	if (sc->jme_mem_res == NULL) {
		device_printf(dev, "can't allocate IO memory\n");
		return ENXIO;
	}
	sc->jme_mem_bt = rman_get_bustag(sc->jme_mem_res);
	sc->jme_mem_bh = rman_get_bushandle(sc->jme_mem_res);

	/*
	 * Allocate IRQ
	 */
	sc->jme_irq_rid = 0;
	sc->jme_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
						 &sc->jme_irq_rid,
						 RF_SHAREABLE | RF_ACTIVE);
	if (sc->jme_irq_res == NULL) {
		device_printf(dev, "can't allocate irq\n");
		error = ENXIO;
		goto fail;
	}

	/*
	 * Extract revisions
	 */
	reg = CSR_READ_4(sc, JME_CHIPMODE);
	if (((reg & CHIPMODE_FPGA_REV_MASK) >> CHIPMODE_FPGA_REV_SHIFT) !=
	    CHIPMODE_NOT_FPGA) {
		sc->jme_caps |= JME_CAP_FPGA;
		if (bootverbose) {
			device_printf(dev, "FPGA revision: 0x%04x\n",
				      (reg & CHIPMODE_FPGA_REV_MASK) >>
				      CHIPMODE_FPGA_REV_SHIFT);
		}
	}

	/* NOTE: FM revision is put in the upper 4 bits */
	rev = ((reg & CHIPMODE_REVFM_MASK) >> CHIPMODE_REVFM_SHIFT) << 4;
	rev |= (reg & CHIPMODE_REVECO_MASK) >> CHIPMODE_REVECO_SHIFT;
	if (bootverbose)
		device_printf(dev, "Revision (FM/ECO): 0x%02x\n", rev);

	did = pci_get_device(dev);
	switch (did) {
	case PCI_PRODUCT_JMICRON_JMC250:
		if (rev == JME_REV1_A2)
			sc->jme_workaround |= JME_WA_EXTFIFO | JME_WA_HDX;
		break;

	case PCI_PRODUCT_JMICRON_JMC260:
		if (rev == JME_REV2)
			sc->jme_lowaddr = BUS_SPACE_MAXADDR_32BIT;
		break;

	default:
		panic("unknown device id 0x%04x\n", did);
	}
	if (rev >= JME_REV2) {
		sc->jme_clksrc = GHC_TXOFL_CLKSRC | GHC_TXMAC_CLKSRC;
		sc->jme_clksrc_1000 = GHC_TXOFL_CLKSRC_1000 |
				      GHC_TXMAC_CLKSRC_1000;
	}

	/* Reset the ethernet controller. */
	jme_reset(sc);

	/* Get station address. */
	reg = CSR_READ_4(sc, JME_SMBCSR);
	if (reg & SMBCSR_EEPROM_PRESENT)
		error = jme_eeprom_macaddr(sc, eaddr);
	if (error != 0 || (reg & SMBCSR_EEPROM_PRESENT) == 0) {
		if (error != 0 && (bootverbose)) {
			device_printf(dev, "ethernet hardware address "
				      "not found in EEPROM.\n");
		}
		jme_reg_macaddr(sc, eaddr);
	}

	/*
	 * Save PHY address.
	 * Integrated JR0211 has fixed PHY address whereas FPGA version
	 * requires PHY probing to get correct PHY address.
	 */
	if ((sc->jme_caps & JME_CAP_FPGA) == 0) {
		sc->jme_phyaddr = CSR_READ_4(sc, JME_GPREG0) &
		    GPREG0_PHY_ADDR_MASK;
		if (bootverbose) {
			device_printf(dev, "PHY is at address %d.\n",
			    sc->jme_phyaddr);
		}
	} else {
		sc->jme_phyaddr = 0;
	}

	/* Set max allowable DMA size. */
	pcie_ptr = pci_get_pciecap_ptr(dev);
	if (pcie_ptr != 0) {
		uint16_t ctrl;

		sc->jme_caps |= JME_CAP_PCIE;
		ctrl = pci_read_config(dev, pcie_ptr + PCIER_DEVCTRL, 2);
		if (bootverbose) {
			device_printf(dev, "Read request size : %d bytes.\n",
			    128 << ((ctrl >> 12) & 0x07));
			device_printf(dev, "TLP payload size : %d bytes.\n",
			    128 << ((ctrl >> 5) & 0x07));
		}
		switch (ctrl & PCIEM_DEVCTL_MAX_READRQ_MASK) {
		case PCIEM_DEVCTL_MAX_READRQ_128:
			sc->jme_tx_dma_size = TXCSR_DMA_SIZE_128;
			break;
		case PCIEM_DEVCTL_MAX_READRQ_256:
			sc->jme_tx_dma_size = TXCSR_DMA_SIZE_256;
			break;
		default:
			sc->jme_tx_dma_size = TXCSR_DMA_SIZE_512;
			break;
		}
		sc->jme_rx_dma_size = RXCSR_DMA_SIZE_128;
	} else {
		sc->jme_tx_dma_size = TXCSR_DMA_SIZE_512;
		sc->jme_rx_dma_size = RXCSR_DMA_SIZE_128;
	}

#ifdef notyet
	if (pci_find_extcap(dev, PCIY_PMG, &pmc) == 0)
		sc->jme_caps |= JME_CAP_PMCAP;
#endif

	/*
	 * Create sysctl tree
	 */
	jme_sysctl_node(sc);

	/* Allocate DMA stuffs */
	error = jme_dma_alloc(sc);
	if (error)
		goto fail;

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = jme_init;
	ifp->if_ioctl = jme_ioctl;
	ifp->if_start = jme_start;
#ifdef DEVICE_POLLING
	ifp->if_poll = jme_poll;
#endif
	ifp->if_watchdog = jme_watchdog;
	ifq_set_maxlen(&ifp->if_snd, sc->jme_tx_desc_cnt - JME_TXD_RSVD);
	ifq_set_ready(&ifp->if_snd);

	/* JMC250 supports Tx/Rx checksum offload and hardware vlan tagging. */
	ifp->if_capabilities = IFCAP_HWCSUM |
			       IFCAP_VLAN_MTU |
			       IFCAP_VLAN_HWTAGGING;
	if (sc->jme_rx_ring_cnt > JME_NRXRING_MIN)
		ifp->if_capabilities |= IFCAP_RSS;
	ifp->if_hwassist = JME_CSUM_FEATURES;
	ifp->if_capenable = ifp->if_capabilities;

	/* Set up MII bus. */
	error = mii_phy_probe(dev, &sc->jme_miibus,
			      jme_mediachange, jme_mediastatus);
	if (error) {
		device_printf(dev, "no PHY found!\n");
		goto fail;
	}

	/*
	 * Save PHYADDR for FPGA mode PHY.
	 */
	if (sc->jme_caps & JME_CAP_FPGA) {
		struct mii_data *mii = device_get_softc(sc->jme_miibus);

		if (mii->mii_instance != 0) {
			struct mii_softc *miisc;

			LIST_FOREACH(miisc, &mii->mii_phys, mii_list) {
				if (miisc->mii_phy != 0) {
					sc->jme_phyaddr = miisc->mii_phy;
					break;
				}
			}
			if (sc->jme_phyaddr != 0) {
				device_printf(sc->jme_dev,
				    "FPGA PHY is at %d\n", sc->jme_phyaddr);
				/* vendor magic. */
				jme_miibus_writereg(dev, sc->jme_phyaddr,
				    JMPHY_CONF, JMPHY_CONF_DEFFIFO);

				/* XXX should we clear JME_WA_EXTFIFO */
			}
		}
	}

	ether_ifattach(ifp, eaddr, NULL);

	/* Tell the upper layer(s) we support long frames. */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);

	error = bus_setup_intr(dev, sc->jme_irq_res, INTR_MPSAFE, jme_intr, sc,
			       &sc->jme_irq_handle, ifp->if_serializer);
	if (error) {
		device_printf(dev, "could not set up interrupt handler.\n");
		ether_ifdetach(ifp);
		goto fail;
	}

	ifp->if_cpuid = ithread_cpuid(rman_get_start(sc->jme_irq_res));
	KKASSERT(ifp->if_cpuid >= 0 && ifp->if_cpuid < ncpus);
	return 0;
fail:
	jme_detach(dev);
	return (error);
}

static int
jme_detach(device_t dev)
{
	struct jme_softc *sc = device_get_softc(dev);

	if (device_is_attached(dev)) {
		struct ifnet *ifp = &sc->arpcom.ac_if;

		lwkt_serialize_enter(ifp->if_serializer);
		jme_stop(sc);
		bus_teardown_intr(dev, sc->jme_irq_res, sc->jme_irq_handle);
		lwkt_serialize_exit(ifp->if_serializer);

		ether_ifdetach(ifp);
	}

	if (sc->jme_sysctl_tree != NULL)
		sysctl_ctx_free(&sc->jme_sysctl_ctx);

	if (sc->jme_miibus != NULL)
		device_delete_child(dev, sc->jme_miibus);
	bus_generic_detach(dev);

	if (sc->jme_irq_res != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->jme_irq_rid,
				     sc->jme_irq_res);
	}

	if (sc->jme_mem_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->jme_mem_rid,
				     sc->jme_mem_res);
	}

	jme_dma_free(sc);

	return (0);
}

static void
jme_sysctl_node(struct jme_softc *sc)
{
	int coal_max;
#ifdef JME_RSS_DEBUG
	char rx_ring_pkt[32];
	int r;
#endif

	sysctl_ctx_init(&sc->jme_sysctl_ctx);
	sc->jme_sysctl_tree = SYSCTL_ADD_NODE(&sc->jme_sysctl_ctx,
				SYSCTL_STATIC_CHILDREN(_hw), OID_AUTO,
				device_get_nameunit(sc->jme_dev),
				CTLFLAG_RD, 0, "");
	if (sc->jme_sysctl_tree == NULL) {
		device_printf(sc->jme_dev, "can't add sysctl node\n");
		return;
	}

	SYSCTL_ADD_PROC(&sc->jme_sysctl_ctx,
	    SYSCTL_CHILDREN(sc->jme_sysctl_tree), OID_AUTO,
	    "tx_coal_to", CTLTYPE_INT | CTLFLAG_RW,
	    sc, 0, jme_sysctl_tx_coal_to, "I", "jme tx coalescing timeout");

	SYSCTL_ADD_PROC(&sc->jme_sysctl_ctx,
	    SYSCTL_CHILDREN(sc->jme_sysctl_tree), OID_AUTO,
	    "tx_coal_pkt", CTLTYPE_INT | CTLFLAG_RW,
	    sc, 0, jme_sysctl_tx_coal_pkt, "I", "jme tx coalescing packet");

	SYSCTL_ADD_PROC(&sc->jme_sysctl_ctx,
	    SYSCTL_CHILDREN(sc->jme_sysctl_tree), OID_AUTO,
	    "rx_coal_to", CTLTYPE_INT | CTLFLAG_RW,
	    sc, 0, jme_sysctl_rx_coal_to, "I", "jme rx coalescing timeout");

	SYSCTL_ADD_PROC(&sc->jme_sysctl_ctx,
	    SYSCTL_CHILDREN(sc->jme_sysctl_tree), OID_AUTO,
	    "rx_coal_pkt", CTLTYPE_INT | CTLFLAG_RW,
	    sc, 0, jme_sysctl_rx_coal_pkt, "I", "jme rx coalescing packet");

	SYSCTL_ADD_INT(&sc->jme_sysctl_ctx,
		       SYSCTL_CHILDREN(sc->jme_sysctl_tree), OID_AUTO,
		       "rx_desc_count", CTLFLAG_RD, &sc->jme_rx_desc_cnt,
		       0, "RX desc count");
	SYSCTL_ADD_INT(&sc->jme_sysctl_ctx,
		       SYSCTL_CHILDREN(sc->jme_sysctl_tree), OID_AUTO,
		       "tx_desc_count", CTLFLAG_RD, &sc->jme_tx_desc_cnt,
		       0, "TX desc count");
	SYSCTL_ADD_INT(&sc->jme_sysctl_ctx,
		       SYSCTL_CHILDREN(sc->jme_sysctl_tree), OID_AUTO,
		       "rx_ring_count", CTLFLAG_RD, &sc->jme_rx_ring_cnt,
		       0, "RX ring count");
	SYSCTL_ADD_INT(&sc->jme_sysctl_ctx,
		       SYSCTL_CHILDREN(sc->jme_sysctl_tree), OID_AUTO,
		       "rx_ring_inuse", CTLFLAG_RD, &sc->jme_rx_ring_inuse,
		       0, "RX ring in use");
#ifdef JME_RSS_DEBUG
	SYSCTL_ADD_INT(&sc->jme_sysctl_ctx,
		       SYSCTL_CHILDREN(sc->jme_sysctl_tree), OID_AUTO,
		       "rss_debug", CTLFLAG_RW, &sc->jme_rss_debug,
		       0, "RSS debug level");
	for (r = 0; r < sc->jme_rx_ring_cnt; ++r) {
		ksnprintf(rx_ring_pkt, sizeof(rx_ring_pkt), "rx_ring%d_pkt", r);
		SYSCTL_ADD_UINT(&sc->jme_sysctl_ctx,
				SYSCTL_CHILDREN(sc->jme_sysctl_tree), OID_AUTO,
				rx_ring_pkt, CTLFLAG_RD,
				&sc->jme_rx_ring_pkt[r],
				0, "RXed packets");
	}
#endif

	/*
	 * Set default coalesce valves
	 */
	sc->jme_tx_coal_to = PCCTX_COAL_TO_DEFAULT;
	sc->jme_tx_coal_pkt = PCCTX_COAL_PKT_DEFAULT;
	sc->jme_rx_coal_to = PCCRX_COAL_TO_DEFAULT;
	sc->jme_rx_coal_pkt = PCCRX_COAL_PKT_DEFAULT;

	/*
	 * Adjust coalesce valves, in case that the number of TX/RX
	 * descs are set to small values by users.
	 *
	 * NOTE: coal_max will not be zero, since number of descs
	 * must aligned by JME_NDESC_ALIGN (16 currently)
	 */
	coal_max = sc->jme_tx_desc_cnt / 6;
	if (coal_max < sc->jme_tx_coal_pkt)
		sc->jme_tx_coal_pkt = coal_max;

	coal_max = sc->jme_rx_desc_cnt / 4;
	if (coal_max < sc->jme_rx_coal_pkt)
		sc->jme_rx_coal_pkt = coal_max;
}

static int
jme_dma_alloc(struct jme_softc *sc)
{
	struct jme_txdesc *txd;
	bus_dmamem_t dmem;
	int error, i;

	sc->jme_cdata.jme_txdesc =
	kmalloc(sc->jme_tx_desc_cnt * sizeof(struct jme_txdesc),
		M_DEVBUF, M_WAITOK | M_ZERO);
	for (i = 0; i < sc->jme_rx_ring_cnt; ++i) {
		sc->jme_cdata.jme_rx_data[i].jme_rxdesc =
		kmalloc(sc->jme_rx_desc_cnt * sizeof(struct jme_rxdesc),
			M_DEVBUF, M_WAITOK | M_ZERO);
	}

	/* Create parent ring tag. */
	error = bus_dma_tag_create(NULL,/* parent */
	    1, JME_RING_BOUNDARY,	/* algnmnt, boundary */
	    sc->jme_lowaddr,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    BUS_SPACE_MAXSIZE_32BIT,	/* maxsize */
	    0,				/* nsegments */
	    BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
	    0,				/* flags */
	    &sc->jme_cdata.jme_ring_tag);
	if (error) {
		device_printf(sc->jme_dev,
		    "could not create parent ring DMA tag.\n");
		return error;
	}

	/*
	 * Create DMA stuffs for TX ring
	 */
	error = bus_dmamem_coherent(sc->jme_cdata.jme_ring_tag,
			JME_TX_RING_ALIGN, 0,
			BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR,
			JME_TX_RING_SIZE(sc),
			BUS_DMA_WAITOK | BUS_DMA_ZERO, &dmem);
	if (error) {
		device_printf(sc->jme_dev, "could not allocate Tx ring.\n");
		return error;
	}
	sc->jme_cdata.jme_tx_ring_tag = dmem.dmem_tag;
	sc->jme_cdata.jme_tx_ring_map = dmem.dmem_map;
	sc->jme_cdata.jme_tx_ring = dmem.dmem_addr;
	sc->jme_cdata.jme_tx_ring_paddr = dmem.dmem_busaddr;

	/*
	 * Create DMA stuffs for RX rings
	 */
	for (i = 0; i < sc->jme_rx_ring_cnt; ++i) {
		error = jme_rxring_dma_alloc(sc, i);
		if (error)
			return error;
	}

	/* Create parent buffer tag. */
	error = bus_dma_tag_create(NULL,/* parent */
	    1, 0,			/* algnmnt, boundary */
	    sc->jme_lowaddr,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    BUS_SPACE_MAXSIZE_32BIT,	/* maxsize */
	    0,				/* nsegments */
	    BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
	    0,				/* flags */
	    &sc->jme_cdata.jme_buffer_tag);
	if (error) {
		device_printf(sc->jme_dev,
		    "could not create parent buffer DMA tag.\n");
		return error;
	}

	/*
	 * Create DMA stuffs for shadow status block
	 */
	error = bus_dmamem_coherent(sc->jme_cdata.jme_buffer_tag,
			JME_SSB_ALIGN, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR,
			JME_SSB_SIZE, BUS_DMA_WAITOK | BUS_DMA_ZERO, &dmem);
	if (error) {
		device_printf(sc->jme_dev,
		    "could not create shadow status block.\n");
		return error;
	}
	sc->jme_cdata.jme_ssb_tag = dmem.dmem_tag;
	sc->jme_cdata.jme_ssb_map = dmem.dmem_map;
	sc->jme_cdata.jme_ssb_block = dmem.dmem_addr;
	sc->jme_cdata.jme_ssb_block_paddr = dmem.dmem_busaddr;

	/*
	 * Create DMA stuffs for TX buffers
	 */

	/* Create tag for Tx buffers. */
	error = bus_dma_tag_create(sc->jme_cdata.jme_buffer_tag,/* parent */
	    1, 0,			/* algnmnt, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    JME_JUMBO_FRAMELEN,		/* maxsize */
	    JME_MAXTXSEGS,		/* nsegments */
	    JME_MAXSEGSIZE,		/* maxsegsize */
	    BUS_DMA_ALLOCNOW | BUS_DMA_WAITOK | BUS_DMA_ONEBPAGE,/* flags */
	    &sc->jme_cdata.jme_tx_tag);
	if (error != 0) {
		device_printf(sc->jme_dev, "could not create Tx DMA tag.\n");
		return error;
	}

	/* Create DMA maps for Tx buffers. */
	for (i = 0; i < sc->jme_tx_desc_cnt; i++) {
		txd = &sc->jme_cdata.jme_txdesc[i];
		error = bus_dmamap_create(sc->jme_cdata.jme_tx_tag,
				BUS_DMA_WAITOK | BUS_DMA_ONEBPAGE,
				&txd->tx_dmamap);
		if (error) {
			int j;

			device_printf(sc->jme_dev,
			    "could not create %dth Tx dmamap.\n", i);

			for (j = 0; j < i; ++j) {
				txd = &sc->jme_cdata.jme_txdesc[j];
				bus_dmamap_destroy(sc->jme_cdata.jme_tx_tag,
						   txd->tx_dmamap);
			}
			bus_dma_tag_destroy(sc->jme_cdata.jme_tx_tag);
			sc->jme_cdata.jme_tx_tag = NULL;
			return error;
		}
	}

	/*
	 * Create DMA stuffs for RX buffers
	 */
	for (i = 0; i < sc->jme_rx_ring_cnt; ++i) {
		error = jme_rxbuf_dma_alloc(sc, i);
		if (error)
			return error;
	}
	return 0;
}

static void
jme_dma_free(struct jme_softc *sc)
{
	struct jme_txdesc *txd;
	struct jme_rxdesc *rxd;
	struct jme_rxdata *rdata;
	int i, r;

	/* Tx ring */
	if (sc->jme_cdata.jme_tx_ring_tag != NULL) {
		bus_dmamap_unload(sc->jme_cdata.jme_tx_ring_tag,
		    sc->jme_cdata.jme_tx_ring_map);
		bus_dmamem_free(sc->jme_cdata.jme_tx_ring_tag,
		    sc->jme_cdata.jme_tx_ring,
		    sc->jme_cdata.jme_tx_ring_map);
		bus_dma_tag_destroy(sc->jme_cdata.jme_tx_ring_tag);
		sc->jme_cdata.jme_tx_ring_tag = NULL;
	}

	/* Rx ring */
	for (r = 0; r < sc->jme_rx_ring_cnt; ++r) {
		rdata = &sc->jme_cdata.jme_rx_data[r];
		if (rdata->jme_rx_ring_tag != NULL) {
			bus_dmamap_unload(rdata->jme_rx_ring_tag,
					  rdata->jme_rx_ring_map);
			bus_dmamem_free(rdata->jme_rx_ring_tag,
					rdata->jme_rx_ring,
					rdata->jme_rx_ring_map);
			bus_dma_tag_destroy(rdata->jme_rx_ring_tag);
			rdata->jme_rx_ring_tag = NULL;
		}
	}

	/* Tx buffers */
	if (sc->jme_cdata.jme_tx_tag != NULL) {
		for (i = 0; i < sc->jme_tx_desc_cnt; i++) {
			txd = &sc->jme_cdata.jme_txdesc[i];
			bus_dmamap_destroy(sc->jme_cdata.jme_tx_tag,
			    txd->tx_dmamap);
		}
		bus_dma_tag_destroy(sc->jme_cdata.jme_tx_tag);
		sc->jme_cdata.jme_tx_tag = NULL;
	}

	/* Rx buffers */
	for (r = 0; r < sc->jme_rx_ring_cnt; ++r) {
		rdata = &sc->jme_cdata.jme_rx_data[r];
		if (rdata->jme_rx_tag != NULL) {
			for (i = 0; i < sc->jme_rx_desc_cnt; i++) {
				rxd = &rdata->jme_rxdesc[i];
				bus_dmamap_destroy(rdata->jme_rx_tag,
						   rxd->rx_dmamap);
			}
			bus_dmamap_destroy(rdata->jme_rx_tag,
					   rdata->jme_rx_sparemap);
			bus_dma_tag_destroy(rdata->jme_rx_tag);
			rdata->jme_rx_tag = NULL;
		}
	}

	/* Shadow status block. */
	if (sc->jme_cdata.jme_ssb_tag != NULL) {
		bus_dmamap_unload(sc->jme_cdata.jme_ssb_tag,
		    sc->jme_cdata.jme_ssb_map);
		bus_dmamem_free(sc->jme_cdata.jme_ssb_tag,
		    sc->jme_cdata.jme_ssb_block,
		    sc->jme_cdata.jme_ssb_map);
		bus_dma_tag_destroy(sc->jme_cdata.jme_ssb_tag);
		sc->jme_cdata.jme_ssb_tag = NULL;
	}

	if (sc->jme_cdata.jme_buffer_tag != NULL) {
		bus_dma_tag_destroy(sc->jme_cdata.jme_buffer_tag);
		sc->jme_cdata.jme_buffer_tag = NULL;
	}
	if (sc->jme_cdata.jme_ring_tag != NULL) {
		bus_dma_tag_destroy(sc->jme_cdata.jme_ring_tag);
		sc->jme_cdata.jme_ring_tag = NULL;
	}

	if (sc->jme_cdata.jme_txdesc != NULL) {
		kfree(sc->jme_cdata.jme_txdesc, M_DEVBUF);
		sc->jme_cdata.jme_txdesc = NULL;
	}
	for (r = 0; r < sc->jme_rx_ring_cnt; ++r) {
		rdata = &sc->jme_cdata.jme_rx_data[r];
		if (rdata->jme_rxdesc != NULL) {
			kfree(rdata->jme_rxdesc, M_DEVBUF);
			rdata->jme_rxdesc = NULL;
		}
	}
}

/*
 *	Make sure the interface is stopped at reboot time.
 */
static int
jme_shutdown(device_t dev)
{
	return jme_suspend(dev);
}

#ifdef notyet
/*
 * Unlike other ethernet controllers, JMC250 requires
 * explicit resetting link speed to 10/100Mbps as gigabit
 * link will cunsume more power than 375mA.
 * Note, we reset the link speed to 10/100Mbps with
 * auto-negotiation but we don't know whether that operation
 * would succeed or not as we have no control after powering
 * off. If the renegotiation fail WOL may not work. Running
 * at 1Gbps draws more power than 375mA at 3.3V which is
 * specified in PCI specification and that would result in
 * complete shutdowning power to ethernet controller.
 *
 * TODO
 *  Save current negotiated media speed/duplex/flow-control
 *  to softc and restore the same link again after resuming.
 *  PHY handling such as power down/resetting to 100Mbps
 *  may be better handled in suspend method in phy driver.
 */
static void
jme_setlinkspeed(struct jme_softc *sc)
{
	struct mii_data *mii;
	int aneg, i;

	JME_LOCK_ASSERT(sc);

	mii = device_get_softc(sc->jme_miibus);
	mii_pollstat(mii);
	aneg = 0;
	if ((mii->mii_media_status & IFM_AVALID) != 0) {
		switch IFM_SUBTYPE(mii->mii_media_active) {
		case IFM_10_T:
		case IFM_100_TX:
			return;
		case IFM_1000_T:
			aneg++;
		default:
			break;
		}
	}
	jme_miibus_writereg(sc->jme_dev, sc->jme_phyaddr, MII_100T2CR, 0);
	jme_miibus_writereg(sc->jme_dev, sc->jme_phyaddr, MII_ANAR,
	    ANAR_TX_FD | ANAR_TX | ANAR_10_FD | ANAR_10 | ANAR_CSMA);
	jme_miibus_writereg(sc->jme_dev, sc->jme_phyaddr, MII_BMCR,
	    BMCR_AUTOEN | BMCR_STARTNEG);
	DELAY(1000);
	if (aneg != 0) {
		/* Poll link state until jme(4) get a 10/100 link. */
		for (i = 0; i < MII_ANEGTICKS_GIGE; i++) {
			mii_pollstat(mii);
			if ((mii->mii_media_status & IFM_AVALID) != 0) {
				switch (IFM_SUBTYPE(mii->mii_media_active)) {
				case IFM_10_T:
				case IFM_100_TX:
					jme_mac_config(sc);
					return;
				default:
					break;
				}
			}
			JME_UNLOCK(sc);
			pause("jmelnk", hz);
			JME_LOCK(sc);
		}
		if (i == MII_ANEGTICKS_GIGE)
			device_printf(sc->jme_dev, "establishing link failed, "
			    "WOL may not work!");
	}
	/*
	 * No link, force MAC to have 100Mbps, full-duplex link.
	 * This is the last resort and may/may not work.
	 */
	mii->mii_media_status = IFM_AVALID | IFM_ACTIVE;
	mii->mii_media_active = IFM_ETHER | IFM_100_TX | IFM_FDX;
	jme_mac_config(sc);
}

static void
jme_setwol(struct jme_softc *sc)
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	uint32_t gpr, pmcs;
	uint16_t pmstat;
	int pmc;

	if (pci_find_extcap(sc->jme_dev, PCIY_PMG, &pmc) != 0) {
		/* No PME capability, PHY power down. */
		jme_miibus_writereg(sc->jme_dev, sc->jme_phyaddr,
		    MII_BMCR, BMCR_PDOWN);
		return;
	}

	gpr = CSR_READ_4(sc, JME_GPREG0) & ~GPREG0_PME_ENB;
	pmcs = CSR_READ_4(sc, JME_PMCS);
	pmcs &= ~PMCS_WOL_ENB_MASK;
	if ((ifp->if_capenable & IFCAP_WOL_MAGIC) != 0) {
		pmcs |= PMCS_MAGIC_FRAME | PMCS_MAGIC_FRAME_ENB;
		/* Enable PME message. */
		gpr |= GPREG0_PME_ENB;
		/* For gigabit controllers, reset link speed to 10/100. */
		if ((sc->jme_caps & JME_CAP_FASTETH) == 0)
			jme_setlinkspeed(sc);
	}

	CSR_WRITE_4(sc, JME_PMCS, pmcs);
	CSR_WRITE_4(sc, JME_GPREG0, gpr);

	/* Request PME. */
	pmstat = pci_read_config(sc->jme_dev, pmc + PCIR_POWER_STATUS, 2);
	pmstat &= ~(PCIM_PSTAT_PME | PCIM_PSTAT_PMEENABLE);
	if ((ifp->if_capenable & IFCAP_WOL) != 0)
		pmstat |= PCIM_PSTAT_PME | PCIM_PSTAT_PMEENABLE;
	pci_write_config(sc->jme_dev, pmc + PCIR_POWER_STATUS, pmstat, 2);
	if ((ifp->if_capenable & IFCAP_WOL) == 0) {
		/* No WOL, PHY power down. */
		jme_miibus_writereg(sc->jme_dev, sc->jme_phyaddr,
		    MII_BMCR, BMCR_PDOWN);
	}
}
#endif

static int
jme_suspend(device_t dev)
{
	struct jme_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = &sc->arpcom.ac_if;

	lwkt_serialize_enter(ifp->if_serializer);
	jme_stop(sc);
#ifdef notyet
	jme_setwol(sc);
#endif
	lwkt_serialize_exit(ifp->if_serializer);

	return (0);
}

static int
jme_resume(device_t dev)
{
	struct jme_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = &sc->arpcom.ac_if;
#ifdef notyet
	int pmc;
#endif

	lwkt_serialize_enter(ifp->if_serializer);

#ifdef notyet
	if (pci_find_extcap(sc->jme_dev, PCIY_PMG, &pmc) != 0) {
		uint16_t pmstat;

		pmstat = pci_read_config(sc->jme_dev,
		    pmc + PCIR_POWER_STATUS, 2);
		/* Disable PME clear PME status. */
		pmstat &= ~PCIM_PSTAT_PMEENABLE;
		pci_write_config(sc->jme_dev,
		    pmc + PCIR_POWER_STATUS, pmstat, 2);
	}
#endif

	if (ifp->if_flags & IFF_UP)
		jme_init(sc);

	lwkt_serialize_exit(ifp->if_serializer);

	return (0);
}

static int
jme_encap(struct jme_softc *sc, struct mbuf **m_head)
{
	struct jme_txdesc *txd;
	struct jme_desc *desc;
	struct mbuf *m;
	bus_dma_segment_t txsegs[JME_MAXTXSEGS];
	int maxsegs, nsegs;
	int error, i, prod, symbol_desc;
	uint32_t cflags, flag64;

	M_ASSERTPKTHDR((*m_head));

	prod = sc->jme_cdata.jme_tx_prod;
	txd = &sc->jme_cdata.jme_txdesc[prod];

	if (sc->jme_lowaddr != BUS_SPACE_MAXADDR_32BIT)
		symbol_desc = 1;
	else
		symbol_desc = 0;

	maxsegs = (sc->jme_tx_desc_cnt - sc->jme_cdata.jme_tx_cnt) -
		  (JME_TXD_RSVD + symbol_desc);
	if (maxsegs > JME_MAXTXSEGS)
		maxsegs = JME_MAXTXSEGS;
	KASSERT(maxsegs >= (sc->jme_txd_spare - symbol_desc),
		("not enough segments %d\n", maxsegs));

	error = bus_dmamap_load_mbuf_defrag(sc->jme_cdata.jme_tx_tag,
			txd->tx_dmamap, m_head,
			txsegs, maxsegs, &nsegs, BUS_DMA_NOWAIT);
	if (error)
		goto fail;

	bus_dmamap_sync(sc->jme_cdata.jme_tx_tag, txd->tx_dmamap,
			BUS_DMASYNC_PREWRITE);

	m = *m_head;
	cflags = 0;

	/* Configure checksum offload. */
	if (m->m_pkthdr.csum_flags & CSUM_IP)
		cflags |= JME_TD_IPCSUM;
	if (m->m_pkthdr.csum_flags & CSUM_TCP)
		cflags |= JME_TD_TCPCSUM;
	if (m->m_pkthdr.csum_flags & CSUM_UDP)
		cflags |= JME_TD_UDPCSUM;

	/* Configure VLAN. */
	if (m->m_flags & M_VLANTAG) {
		cflags |= (m->m_pkthdr.ether_vlantag & JME_TD_VLAN_MASK);
		cflags |= JME_TD_VLAN_TAG;
	}

	desc = &sc->jme_cdata.jme_tx_ring[prod];
	desc->flags = htole32(cflags);
	desc->addr_hi = htole32(m->m_pkthdr.len);
	if (sc->jme_lowaddr != BUS_SPACE_MAXADDR_32BIT) {
		/*
		 * Use 64bits TX desc chain format.
		 *
		 * The first TX desc of the chain, which is setup here,
		 * is just a symbol TX desc carrying no payload.
		 */
		flag64 = JME_TD_64BIT;
		desc->buflen = 0;
		desc->addr_lo = 0;

		/* No effective TX desc is consumed */
		i = 0;
	} else {
		/*
		 * Use 32bits TX desc chain format.
		 *
		 * The first TX desc of the chain, which is setup here,
		 * is an effective TX desc carrying the first segment of
		 * the mbuf chain.
		 */
		flag64 = 0;
		desc->buflen = htole32(txsegs[0].ds_len);
		desc->addr_lo = htole32(JME_ADDR_LO(txsegs[0].ds_addr));

		/* One effective TX desc is consumed */
		i = 1;
	}
	sc->jme_cdata.jme_tx_cnt++;
	KKASSERT(sc->jme_cdata.jme_tx_cnt - i <
		 sc->jme_tx_desc_cnt - JME_TXD_RSVD);
	JME_DESC_INC(prod, sc->jme_tx_desc_cnt);

	txd->tx_ndesc = 1 - i;
	for (; i < nsegs; i++) {
		desc = &sc->jme_cdata.jme_tx_ring[prod];
		desc->flags = htole32(JME_TD_OWN | flag64);
		desc->buflen = htole32(txsegs[i].ds_len);
		desc->addr_hi = htole32(JME_ADDR_HI(txsegs[i].ds_addr));
		desc->addr_lo = htole32(JME_ADDR_LO(txsegs[i].ds_addr));

		sc->jme_cdata.jme_tx_cnt++;
		KKASSERT(sc->jme_cdata.jme_tx_cnt <=
			 sc->jme_tx_desc_cnt - JME_TXD_RSVD);
		JME_DESC_INC(prod, sc->jme_tx_desc_cnt);
	}

	/* Update producer index. */
	sc->jme_cdata.jme_tx_prod = prod;
	/*
	 * Finally request interrupt and give the first descriptor
	 * owenership to hardware.
	 */
	desc = txd->tx_desc;
	desc->flags |= htole32(JME_TD_OWN | JME_TD_INTR);

	txd->tx_m = m;
	txd->tx_ndesc += nsegs;

	return 0;
fail:
	m_freem(*m_head);
	*m_head = NULL;
	return error;
}

static void
jme_start(struct ifnet *ifp)
{
	struct jme_softc *sc = ifp->if_softc;
	struct mbuf *m_head;
	int enq = 0;

	ASSERT_SERIALIZED(ifp->if_serializer);

	if ((sc->jme_flags & JME_FLAG_LINK) == 0) {
		ifq_purge(&ifp->if_snd);
		return;
	}

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	if (sc->jme_cdata.jme_tx_cnt >= JME_TX_DESC_HIWAT(sc))
		jme_txeof(sc);

	while (!ifq_is_empty(&ifp->if_snd)) {
		/*
		 * Check number of available TX descs, always
		 * leave JME_TXD_RSVD free TX descs.
		 */
		if (sc->jme_cdata.jme_tx_cnt + sc->jme_txd_spare >
		    sc->jme_tx_desc_cnt - JME_TXD_RSVD) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		m_head = ifq_dequeue(&ifp->if_snd, NULL);
		if (m_head == NULL)
			break;

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag and wait
		 * for the NIC to drain the ring.
		 */
		if (jme_encap(sc, &m_head)) {
			KKASSERT(m_head == NULL);
			ifp->if_oerrors++;
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		enq++;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		ETHER_BPF_MTAP(ifp, m_head);
	}

	if (enq > 0) {
		/*
		 * Reading TXCSR takes very long time under heavy load
		 * so cache TXCSR value and writes the ORed value with
		 * the kick command to the TXCSR. This saves one register
		 * access cycle.
		 */
		CSR_WRITE_4(sc, JME_TXCSR, sc->jme_txcsr | TXCSR_TX_ENB |
		    TXCSR_TXQ_N_START(TXCSR_TXQ0));
		/* Set a timeout in case the chip goes out to lunch. */
		ifp->if_timer = JME_TX_TIMEOUT;
	}
}

static void
jme_watchdog(struct ifnet *ifp)
{
	struct jme_softc *sc = ifp->if_softc;

	ASSERT_SERIALIZED(ifp->if_serializer);

	if ((sc->jme_flags & JME_FLAG_LINK) == 0) {
		if_printf(ifp, "watchdog timeout (missed link)\n");
		ifp->if_oerrors++;
		jme_init(sc);
		return;
	}

	jme_txeof(sc);
	if (sc->jme_cdata.jme_tx_cnt == 0) {
		if_printf(ifp, "watchdog timeout (missed Tx interrupts) "
			  "-- recovering\n");
		if (!ifq_is_empty(&ifp->if_snd))
			if_devstart(ifp);
		return;
	}

	if_printf(ifp, "watchdog timeout\n");
	ifp->if_oerrors++;
	jme_init(sc);
	if (!ifq_is_empty(&ifp->if_snd))
		if_devstart(ifp);
}

static int
jme_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data, struct ucred *cr)
{
	struct jme_softc *sc = ifp->if_softc;
	struct mii_data *mii = device_get_softc(sc->jme_miibus);
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0, mask;

	ASSERT_SERIALIZED(ifp->if_serializer);

	switch (cmd) {
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < ETHERMIN || ifr->ifr_mtu > JME_JUMBO_MTU ||
		    (!(sc->jme_caps & JME_CAP_JUMBO) &&
		     ifr->ifr_mtu > JME_MAX_MTU)) {
			error = EINVAL;
			break;
		}

		if (ifp->if_mtu != ifr->ifr_mtu) {
			/*
			 * No special configuration is required when interface
			 * MTU is changed but availability of Tx checksum
			 * offload should be chcked against new MTU size as
			 * FIFO size is just 2K.
			 */
			if (ifr->ifr_mtu >= JME_TX_FIFO_SIZE) {
				ifp->if_capenable &= ~IFCAP_TXCSUM;
				ifp->if_hwassist &= ~JME_CSUM_FEATURES;
			}
			ifp->if_mtu = ifr->ifr_mtu;
			if (ifp->if_flags & IFF_RUNNING)
				jme_init(sc);
		}
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING) {
				if ((ifp->if_flags ^ sc->jme_if_flags) &
				    (IFF_PROMISC | IFF_ALLMULTI))
					jme_set_filter(sc);
			} else {
				jme_init(sc);
			}
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				jme_stop(sc);
		}
		sc->jme_if_flags = ifp->if_flags;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_flags & IFF_RUNNING)
			jme_set_filter(sc);
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
		break;

	case SIOCSIFCAP:
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;

		if ((mask & IFCAP_TXCSUM) && ifp->if_mtu < JME_TX_FIFO_SIZE) {
			ifp->if_capenable ^= IFCAP_TXCSUM;
			if (IFCAP_TXCSUM & ifp->if_capenable)
				ifp->if_hwassist |= JME_CSUM_FEATURES;
			else
				ifp->if_hwassist &= ~JME_CSUM_FEATURES;
		}
		if (mask & IFCAP_RXCSUM) {
			uint32_t reg;

			ifp->if_capenable ^= IFCAP_RXCSUM;
			reg = CSR_READ_4(sc, JME_RXMAC);
			reg &= ~RXMAC_CSUM_ENB;
			if (ifp->if_capenable & IFCAP_RXCSUM)
				reg |= RXMAC_CSUM_ENB;
			CSR_WRITE_4(sc, JME_RXMAC, reg);
		}

		if (mask & IFCAP_VLAN_HWTAGGING) {
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;
			jme_set_vlan(sc);
		}

		if (mask & IFCAP_RSS) {
			ifp->if_capenable ^= IFCAP_RSS;
			if (ifp->if_flags & IFF_RUNNING)
				jme_init(sc);
		}
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}
	return (error);
}

static void
jme_mac_config(struct jme_softc *sc)
{
	struct mii_data *mii;
	uint32_t ghc, rxmac, txmac, txpause, gp1;
	int phyconf = JMPHY_CONF_DEFFIFO, hdx = 0;

	mii = device_get_softc(sc->jme_miibus);

	CSR_WRITE_4(sc, JME_GHC, GHC_RESET);
	DELAY(10);
	CSR_WRITE_4(sc, JME_GHC, 0);
	ghc = 0;
	rxmac = CSR_READ_4(sc, JME_RXMAC);
	rxmac &= ~RXMAC_FC_ENB;
	txmac = CSR_READ_4(sc, JME_TXMAC);
	txmac &= ~(TXMAC_CARRIER_EXT | TXMAC_FRAME_BURST);
	txpause = CSR_READ_4(sc, JME_TXPFC);
	txpause &= ~TXPFC_PAUSE_ENB;
	if ((IFM_OPTIONS(mii->mii_media_active) & IFM_FDX) != 0) {
		ghc |= GHC_FULL_DUPLEX;
		rxmac &= ~RXMAC_COLL_DET_ENB;
		txmac &= ~(TXMAC_COLL_ENB | TXMAC_CARRIER_SENSE |
		    TXMAC_BACKOFF | TXMAC_CARRIER_EXT |
		    TXMAC_FRAME_BURST);
#ifdef notyet
		if ((IFM_OPTIONS(mii->mii_media_active) & IFM_ETH_TXPAUSE) != 0)
			txpause |= TXPFC_PAUSE_ENB;
		if ((IFM_OPTIONS(mii->mii_media_active) & IFM_ETH_RXPAUSE) != 0)
			rxmac |= RXMAC_FC_ENB;
#endif
		/* Disable retry transmit timer/retry limit. */
		CSR_WRITE_4(sc, JME_TXTRHD, CSR_READ_4(sc, JME_TXTRHD) &
		    ~(TXTRHD_RT_PERIOD_ENB | TXTRHD_RT_LIMIT_ENB));
	} else {
		rxmac |= RXMAC_COLL_DET_ENB;
		txmac |= TXMAC_COLL_ENB | TXMAC_CARRIER_SENSE | TXMAC_BACKOFF;
		/* Enable retry transmit timer/retry limit. */
		CSR_WRITE_4(sc, JME_TXTRHD, CSR_READ_4(sc, JME_TXTRHD) |
		    TXTRHD_RT_PERIOD_ENB | TXTRHD_RT_LIMIT_ENB);
	}

	/*
	 * Reprogram Tx/Rx MACs with resolved speed/duplex.
	 */
	gp1 = CSR_READ_4(sc, JME_GPREG1);
	gp1 &= ~GPREG1_WA_HDX;

	if ((IFM_OPTIONS(mii->mii_media_active) & IFM_FDX) == 0)
		hdx = 1;

	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_10_T:
		ghc |= GHC_SPEED_10 | sc->jme_clksrc;
		if (hdx)
			gp1 |= GPREG1_WA_HDX;
		break;

	case IFM_100_TX:
		ghc |= GHC_SPEED_100 | sc->jme_clksrc;
		if (hdx)
			gp1 |= GPREG1_WA_HDX;

		/*
		 * Use extended FIFO depth to workaround CRC errors
		 * emitted by chips before JMC250B
		 */
		phyconf = JMPHY_CONF_EXTFIFO;
		break;

	case IFM_1000_T:
		if (sc->jme_caps & JME_CAP_FASTETH)
			break;

		ghc |= GHC_SPEED_1000 | sc->jme_clksrc_1000;
		if (hdx)
			txmac |= TXMAC_CARRIER_EXT | TXMAC_FRAME_BURST;
		break;

	default:
		break;
	}
	CSR_WRITE_4(sc, JME_GHC, ghc);
	CSR_WRITE_4(sc, JME_RXMAC, rxmac);
	CSR_WRITE_4(sc, JME_TXMAC, txmac);
	CSR_WRITE_4(sc, JME_TXPFC, txpause);

	if (sc->jme_workaround & JME_WA_EXTFIFO) {
		jme_miibus_writereg(sc->jme_dev, sc->jme_phyaddr,
				    JMPHY_CONF, phyconf);
	}
	if (sc->jme_workaround & JME_WA_HDX)
		CSR_WRITE_4(sc, JME_GPREG1, gp1);
}

static void
jme_intr(void *xsc)
{
	struct jme_softc *sc = xsc;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	uint32_t status;
	int r;

	ASSERT_SERIALIZED(ifp->if_serializer);

	status = CSR_READ_4(sc, JME_INTR_REQ_STATUS);
	if (status == 0 || status == 0xFFFFFFFF)
		return;

	/* Disable interrupts. */
	CSR_WRITE_4(sc, JME_INTR_MASK_CLR, JME_INTRS);

	status = CSR_READ_4(sc, JME_INTR_STATUS);
	if ((status & JME_INTRS) == 0 || status == 0xFFFFFFFF)
		goto back;

	/* Reset PCC counter/timer and Ack interrupts. */
	status &= ~(INTR_TXQ_COMP | INTR_RXQ_COMP);

	if (status & (INTR_TXQ_COAL | INTR_TXQ_COAL_TO))
		status |= INTR_TXQ_COAL | INTR_TXQ_COAL_TO | INTR_TXQ_COMP;

	for (r = 0; r < sc->jme_rx_ring_inuse; ++r) {
		if (status & jme_rx_status[r].jme_coal) {
			status |= jme_rx_status[r].jme_coal |
				  jme_rx_status[r].jme_comp;
		}
	}

	CSR_WRITE_4(sc, JME_INTR_STATUS, status);

	if (ifp->if_flags & IFF_RUNNING) {
		if (status & (INTR_RXQ_COAL | INTR_RXQ_COAL_TO))
			jme_rx_intr(sc, status);

		if (status & INTR_RXQ_DESC_EMPTY) {
			/*
			 * Notify hardware availability of new Rx buffers.
			 * Reading RXCSR takes very long time under heavy
			 * load so cache RXCSR value and writes the ORed
			 * value with the kick command to the RXCSR. This
			 * saves one register access cycle.
			 */
			CSR_WRITE_4(sc, JME_RXCSR, sc->jme_rxcsr |
			    RXCSR_RX_ENB | RXCSR_RXQ_START);
		}

		if (status & (INTR_TXQ_COAL | INTR_TXQ_COAL_TO)) {
			jme_txeof(sc);
			if (!ifq_is_empty(&ifp->if_snd))
				if_devstart(ifp);
		}
	}
back:
	/* Reenable interrupts. */
	CSR_WRITE_4(sc, JME_INTR_MASK_SET, JME_INTRS);
}

static void
jme_txeof(struct jme_softc *sc)
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct jme_txdesc *txd;
	uint32_t status;
	int cons, nsegs;

	cons = sc->jme_cdata.jme_tx_cons;
	if (cons == sc->jme_cdata.jme_tx_prod)
		return;

	/*
	 * Go through our Tx list and free mbufs for those
	 * frames which have been transmitted.
	 */
	while (cons != sc->jme_cdata.jme_tx_prod) {
		txd = &sc->jme_cdata.jme_txdesc[cons];
		KASSERT(txd->tx_m != NULL,
			("%s: freeing NULL mbuf!\n", __func__));

		status = le32toh(txd->tx_desc->flags);
		if ((status & JME_TD_OWN) == JME_TD_OWN)
			break;

		if (status & (JME_TD_TMOUT | JME_TD_RETRY_EXP)) {
			ifp->if_oerrors++;
		} else {
			ifp->if_opackets++;
			if (status & JME_TD_COLLISION) {
				ifp->if_collisions +=
				    le32toh(txd->tx_desc->buflen) &
				    JME_TD_BUF_LEN_MASK;
			}
		}

		/*
		 * Only the first descriptor of multi-descriptor
		 * transmission is updated so driver have to skip entire
		 * chained buffers for the transmiited frame. In other
		 * words, JME_TD_OWN bit is valid only at the first
		 * descriptor of a multi-descriptor transmission.
		 */
		for (nsegs = 0; nsegs < txd->tx_ndesc; nsegs++) {
			sc->jme_cdata.jme_tx_ring[cons].flags = 0;
			JME_DESC_INC(cons, sc->jme_tx_desc_cnt);
		}

		/* Reclaim transferred mbufs. */
		bus_dmamap_unload(sc->jme_cdata.jme_tx_tag, txd->tx_dmamap);
		m_freem(txd->tx_m);
		txd->tx_m = NULL;
		sc->jme_cdata.jme_tx_cnt -= txd->tx_ndesc;
		KASSERT(sc->jme_cdata.jme_tx_cnt >= 0,
			("%s: Active Tx desc counter was garbled\n", __func__));
		txd->tx_ndesc = 0;
	}
	sc->jme_cdata.jme_tx_cons = cons;

	if (sc->jme_cdata.jme_tx_cnt == 0)
		ifp->if_timer = 0;

	if (sc->jme_cdata.jme_tx_cnt + sc->jme_txd_spare <=
	    sc->jme_tx_desc_cnt - JME_TXD_RSVD)
		ifp->if_flags &= ~IFF_OACTIVE;
}

static __inline void
jme_discard_rxbufs(struct jme_softc *sc, int ring, int cons, int count)
{
	struct jme_rxdata *rdata = &sc->jme_cdata.jme_rx_data[ring];
	int i;

	for (i = 0; i < count; ++i) {
		struct jme_desc *desc = &rdata->jme_rx_ring[cons];

		desc->flags = htole32(JME_RD_OWN | JME_RD_INTR | JME_RD_64BIT);
		desc->buflen = htole32(MCLBYTES);
		JME_DESC_INC(cons, sc->jme_rx_desc_cnt);
	}
}

/* Receive a frame. */
static void
jme_rxpkt(struct jme_softc *sc, int ring, struct mbuf_chain *chain)
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct jme_rxdata *rdata = &sc->jme_cdata.jme_rx_data[ring];
	struct jme_desc *desc;
	struct jme_rxdesc *rxd;
	struct mbuf *mp, *m;
	uint32_t flags, status;
	int cons, count, nsegs;

	cons = rdata->jme_rx_cons;
	desc = &rdata->jme_rx_ring[cons];
	flags = le32toh(desc->flags);
	status = le32toh(desc->buflen);
	nsegs = JME_RX_NSEGS(status);

	JME_RSS_DPRINTF(sc, 10, "ring%d, flags 0x%08x, "
			"hash 0x%08x, hash type 0x%08x\n",
			ring, flags, desc->addr_hi, desc->addr_lo);

	if (status & JME_RX_ERR_STAT) {
		ifp->if_ierrors++;
		jme_discard_rxbufs(sc, ring, cons, nsegs);
#ifdef JME_SHOW_ERRORS
		device_printf(sc->jme_dev, "%s : receive error = 0x%b\n",
		    __func__, JME_RX_ERR(status), JME_RX_ERR_BITS);
#endif
		rdata->jme_rx_cons += nsegs;
		rdata->jme_rx_cons %= sc->jme_rx_desc_cnt;
		return;
	}

	rdata->jme_rxlen = JME_RX_BYTES(status) - JME_RX_PAD_BYTES;
	for (count = 0; count < nsegs; count++,
	     JME_DESC_INC(cons, sc->jme_rx_desc_cnt)) {
		rxd = &rdata->jme_rxdesc[cons];
		mp = rxd->rx_m;

		/* Add a new receive buffer to the ring. */
		if (jme_newbuf(sc, ring, rxd, 0) != 0) {
			ifp->if_iqdrops++;
			/* Reuse buffer. */
			jme_discard_rxbufs(sc, ring, cons, nsegs - count);
			if (rdata->jme_rxhead != NULL) {
				m_freem(rdata->jme_rxhead);
				JME_RXCHAIN_RESET(sc, ring);
			}
			break;
		}

		/*
		 * Assume we've received a full sized frame.
		 * Actual size is fixed when we encounter the end of
		 * multi-segmented frame.
		 */
		mp->m_len = MCLBYTES;

		/* Chain received mbufs. */
		if (rdata->jme_rxhead == NULL) {
			rdata->jme_rxhead = mp;
			rdata->jme_rxtail = mp;
		} else {
			/*
			 * Receive processor can receive a maximum frame
			 * size of 65535 bytes.
			 */
			mp->m_flags &= ~M_PKTHDR;
			rdata->jme_rxtail->m_next = mp;
			rdata->jme_rxtail = mp;
		}

		if (count == nsegs - 1) {
			/* Last desc. for this frame. */
			m = rdata->jme_rxhead;
			/* XXX assert PKTHDR? */
			m->m_flags |= M_PKTHDR;
			m->m_pkthdr.len = rdata->jme_rxlen;
			if (nsegs > 1) {
				/* Set first mbuf size. */
				m->m_len = MCLBYTES - JME_RX_PAD_BYTES;
				/* Set last mbuf size. */
				mp->m_len = rdata->jme_rxlen -
				    ((MCLBYTES - JME_RX_PAD_BYTES) +
				    (MCLBYTES * (nsegs - 2)));
			} else {
				m->m_len = rdata->jme_rxlen;
			}
			m->m_pkthdr.rcvif = ifp;

			/*
			 * Account for 10bytes auto padding which is used
			 * to align IP header on 32bit boundary. Also note,
			 * CRC bytes is automatically removed by the
			 * hardware.
			 */
			m->m_data += JME_RX_PAD_BYTES;

			/* Set checksum information. */
			if ((ifp->if_capenable & IFCAP_RXCSUM) &&
			    (flags & JME_RD_IPV4)) {
				m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
				if (flags & JME_RD_IPCSUM)
					m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
				if ((flags & JME_RD_MORE_FRAG) == 0 &&
				    ((flags & (JME_RD_TCP | JME_RD_TCPCSUM)) ==
				     (JME_RD_TCP | JME_RD_TCPCSUM) ||
				     (flags & (JME_RD_UDP | JME_RD_UDPCSUM)) ==
				     (JME_RD_UDP | JME_RD_UDPCSUM))) {
					m->m_pkthdr.csum_flags |=
					    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
					m->m_pkthdr.csum_data = 0xffff;
				}
			}

			/* Check for VLAN tagged packets. */
			if ((ifp->if_capenable & IFCAP_VLAN_HWTAGGING) &&
			    (flags & JME_RD_VLAN_TAG)) {
				m->m_pkthdr.ether_vlantag =
				    flags & JME_RD_VLAN_MASK;
				m->m_flags |= M_VLANTAG;
			}

			ifp->if_ipackets++;
			/* Pass it on. */
			ether_input_chain(ifp, m, NULL, chain);

			/* Reset mbuf chains. */
			JME_RXCHAIN_RESET(sc, ring);
#ifdef JME_RSS_DEBUG
			sc->jme_rx_ring_pkt[ring]++;
#endif
		}
	}

	rdata->jme_rx_cons += nsegs;
	rdata->jme_rx_cons %= sc->jme_rx_desc_cnt;
}

static int
jme_rxeof_chain(struct jme_softc *sc, int ring, struct mbuf_chain *chain,
		int count)
{
	struct jme_rxdata *rdata = &sc->jme_cdata.jme_rx_data[ring];
	struct jme_desc *desc;
	int nsegs, prog, pktlen;

	prog = 0;
	for (;;) {
#ifdef DEVICE_POLLING
		if (count >= 0 && count-- == 0)
			break;
#endif
		desc = &rdata->jme_rx_ring[rdata->jme_rx_cons];
		if ((le32toh(desc->flags) & JME_RD_OWN) == JME_RD_OWN)
			break;
		if ((le32toh(desc->buflen) & JME_RD_VALID) == 0)
			break;

		/*
		 * Check number of segments against received bytes.
		 * Non-matching value would indicate that hardware
		 * is still trying to update Rx descriptors. I'm not
		 * sure whether this check is needed.
		 */
		nsegs = JME_RX_NSEGS(le32toh(desc->buflen));
		pktlen = JME_RX_BYTES(le32toh(desc->buflen));
		if (nsegs != howmany(pktlen, MCLBYTES)) {
			if_printf(&sc->arpcom.ac_if, "RX fragment count(%d) "
				  "and packet size(%d) mismach\n",
				  nsegs, pktlen);
			break;
		}

		/* Received a frame. */
		jme_rxpkt(sc, ring, chain);
		prog++;
	}
	return prog;
}

static void
jme_rxeof(struct jme_softc *sc, int ring)
{
	struct mbuf_chain chain[MAXCPU];

	ether_input_chain_init(chain);
	if (jme_rxeof_chain(sc, ring, chain, -1))
		ether_input_dispatch(chain);
}

static void
jme_tick(void *xsc)
{
	struct jme_softc *sc = xsc;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct mii_data *mii = device_get_softc(sc->jme_miibus);

	lwkt_serialize_enter(ifp->if_serializer);

	mii_tick(mii);
	callout_reset(&sc->jme_tick_ch, hz, jme_tick, sc);

	lwkt_serialize_exit(ifp->if_serializer);
}

static void
jme_reset(struct jme_softc *sc)
{
#ifdef foo
	/* Stop receiver, transmitter. */
	jme_stop_rx(sc);
	jme_stop_tx(sc);
#endif
	CSR_WRITE_4(sc, JME_GHC, GHC_RESET);
	DELAY(10);
	CSR_WRITE_4(sc, JME_GHC, 0);
}

static void
jme_init(void *xsc)
{
	struct jme_softc *sc = xsc;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct mii_data *mii;
	uint8_t eaddr[ETHER_ADDR_LEN];
	bus_addr_t paddr;
	uint32_t reg;
	int error, r;

	ASSERT_SERIALIZED(ifp->if_serializer);

	/*
	 * Cancel any pending I/O.
	 */
	jme_stop(sc);

	/*
	 * Reset the chip to a known state.
	 */
	jme_reset(sc);

	sc->jme_txd_spare =
	howmany(ifp->if_mtu + sizeof(struct ether_vlan_header), MCLBYTES);
	KKASSERT(sc->jme_txd_spare >= 1);

	/*
	 * If we use 64bit address mode for transmitting, each Tx request
	 * needs one more symbol descriptor.
	 */
	if (sc->jme_lowaddr != BUS_SPACE_MAXADDR_32BIT)
		sc->jme_txd_spare += 1;

#ifdef RSS
	if (ifp->if_capenable & IFCAP_RSS)
		jme_enable_rss(sc);
	else
#endif
		jme_disable_rss(sc);

	/* Init RX descriptors */
	for (r = 0; r < sc->jme_rx_ring_inuse; ++r) {
		error = jme_init_rx_ring(sc, r);
		if (error) {
			if_printf(ifp, "initialization failed: "
				  "no memory for %dth RX ring.\n", r);
			jme_stop(sc);
			return;
		}
	}

	/* Init TX descriptors */
	jme_init_tx_ring(sc);

	/* Initialize shadow status block. */
	jme_init_ssb(sc);

	/* Reprogram the station address. */
	bcopy(IF_LLADDR(ifp), eaddr, ETHER_ADDR_LEN);
	CSR_WRITE_4(sc, JME_PAR0,
	    eaddr[3] << 24 | eaddr[2] << 16 | eaddr[1] << 8 | eaddr[0]);
	CSR_WRITE_4(sc, JME_PAR1, eaddr[5] << 8 | eaddr[4]);

	/*
	 * Configure Tx queue.
	 *  Tx priority queue weight value : 0
	 *  Tx FIFO threshold for processing next packet : 16QW
	 *  Maximum Tx DMA length : 512
	 *  Allow Tx DMA burst.
	 */
	sc->jme_txcsr = TXCSR_TXQ_N_SEL(TXCSR_TXQ0);
	sc->jme_txcsr |= TXCSR_TXQ_WEIGHT(TXCSR_TXQ_WEIGHT_MIN);
	sc->jme_txcsr |= TXCSR_FIFO_THRESH_16QW;
	sc->jme_txcsr |= sc->jme_tx_dma_size;
	sc->jme_txcsr |= TXCSR_DMA_BURST;
	CSR_WRITE_4(sc, JME_TXCSR, sc->jme_txcsr);

	/* Set Tx descriptor counter. */
	CSR_WRITE_4(sc, JME_TXQDC, sc->jme_tx_desc_cnt);

	/* Set Tx ring address to the hardware. */
	paddr = sc->jme_cdata.jme_tx_ring_paddr;
	CSR_WRITE_4(sc, JME_TXDBA_HI, JME_ADDR_HI(paddr));
	CSR_WRITE_4(sc, JME_TXDBA_LO, JME_ADDR_LO(paddr));

	/* Configure TxMAC parameters. */
	reg = TXMAC_IFG1_DEFAULT | TXMAC_IFG2_DEFAULT | TXMAC_IFG_ENB;
	reg |= TXMAC_THRESH_1_PKT;
	reg |= TXMAC_CRC_ENB | TXMAC_PAD_ENB;
	CSR_WRITE_4(sc, JME_TXMAC, reg);

	/*
	 * Configure Rx queue.
	 *  FIFO full threshold for transmitting Tx pause packet : 128T
	 *  FIFO threshold for processing next packet : 128QW
	 *  Rx queue 0 select
	 *  Max Rx DMA length : 128
	 *  Rx descriptor retry : 32
	 *  Rx descriptor retry time gap : 256ns
	 *  Don't receive runt/bad frame.
	 */
	sc->jme_rxcsr = RXCSR_FIFO_FTHRESH_128T;
#if 0
	/*
	 * Since Rx FIFO size is 4K bytes, receiving frames larger
	 * than 4K bytes will suffer from Rx FIFO overruns. So
	 * decrease FIFO threshold to reduce the FIFO overruns for
	 * frames larger than 4000 bytes.
	 * For best performance of standard MTU sized frames use
	 * maximum allowable FIFO threshold, 128QW.
	 */
	if ((ifp->if_mtu + ETHER_HDR_LEN + EVL_ENCAPLEN + ETHER_CRC_LEN) >
	    JME_RX_FIFO_SIZE)
		sc->jme_rxcsr |= RXCSR_FIFO_THRESH_16QW;
	else
		sc->jme_rxcsr |= RXCSR_FIFO_THRESH_128QW;
#else
	/* Improve PCI Express compatibility */
	sc->jme_rxcsr |= RXCSR_FIFO_THRESH_16QW;
#endif
	sc->jme_rxcsr |= sc->jme_rx_dma_size;
	sc->jme_rxcsr |= RXCSR_DESC_RT_CNT(RXCSR_DESC_RT_CNT_DEFAULT);
	sc->jme_rxcsr |= RXCSR_DESC_RT_GAP_256 & RXCSR_DESC_RT_GAP_MASK;
	/* XXX TODO DROP_BAD */

	for (r = 0; r < sc->jme_rx_ring_inuse; ++r) {
		CSR_WRITE_4(sc, JME_RXCSR, sc->jme_rxcsr | RXCSR_RXQ_N_SEL(r));

		/* Set Rx descriptor counter. */
		CSR_WRITE_4(sc, JME_RXQDC, sc->jme_rx_desc_cnt);

		/* Set Rx ring address to the hardware. */
		paddr = sc->jme_cdata.jme_rx_data[r].jme_rx_ring_paddr;
		CSR_WRITE_4(sc, JME_RXDBA_HI, JME_ADDR_HI(paddr));
		CSR_WRITE_4(sc, JME_RXDBA_LO, JME_ADDR_LO(paddr));
	}

	/* Clear receive filter. */
	CSR_WRITE_4(sc, JME_RXMAC, 0);

	/* Set up the receive filter. */
	jme_set_filter(sc);
	jme_set_vlan(sc);

	/*
	 * Disable all WOL bits as WOL can interfere normal Rx
	 * operation. Also clear WOL detection status bits.
	 */
	reg = CSR_READ_4(sc, JME_PMCS);
	reg &= ~PMCS_WOL_ENB_MASK;
	CSR_WRITE_4(sc, JME_PMCS, reg);

	/*
	 * Pad 10bytes right before received frame. This will greatly
	 * help Rx performance on strict-alignment architectures as
	 * it does not need to copy the frame to align the payload.
	 */
	reg = CSR_READ_4(sc, JME_RXMAC);
	reg |= RXMAC_PAD_10BYTES;

	if (ifp->if_capenable & IFCAP_RXCSUM)
		reg |= RXMAC_CSUM_ENB;
	CSR_WRITE_4(sc, JME_RXMAC, reg);

	/* Configure general purpose reg0 */
	reg = CSR_READ_4(sc, JME_GPREG0);
	reg &= ~GPREG0_PCC_UNIT_MASK;
	/* Set PCC timer resolution to micro-seconds unit. */
	reg |= GPREG0_PCC_UNIT_US;
	/*
	 * Disable all shadow register posting as we have to read
	 * JME_INTR_STATUS register in jme_intr. Also it seems
	 * that it's hard to synchronize interrupt status between
	 * hardware and software with shadow posting due to
	 * requirements of bus_dmamap_sync(9).
	 */
	reg |= GPREG0_SH_POST_DW7_DIS | GPREG0_SH_POST_DW6_DIS |
	    GPREG0_SH_POST_DW5_DIS | GPREG0_SH_POST_DW4_DIS |
	    GPREG0_SH_POST_DW3_DIS | GPREG0_SH_POST_DW2_DIS |
	    GPREG0_SH_POST_DW1_DIS | GPREG0_SH_POST_DW0_DIS;
	/* Disable posting of DW0. */
	reg &= ~GPREG0_POST_DW0_ENB;
	/* Clear PME message. */
	reg &= ~GPREG0_PME_ENB;
	/* Set PHY address. */
	reg &= ~GPREG0_PHY_ADDR_MASK;
	reg |= sc->jme_phyaddr;
	CSR_WRITE_4(sc, JME_GPREG0, reg);

	/* Configure Tx queue 0 packet completion coalescing. */
	jme_set_tx_coal(sc);

	/* Configure Rx queue 0 packet completion coalescing. */
	jme_set_rx_coal(sc);

	/* Configure shadow status block but don't enable posting. */
	paddr = sc->jme_cdata.jme_ssb_block_paddr;
	CSR_WRITE_4(sc, JME_SHBASE_ADDR_HI, JME_ADDR_HI(paddr));
	CSR_WRITE_4(sc, JME_SHBASE_ADDR_LO, JME_ADDR_LO(paddr));

	/* Disable Timer 1 and Timer 2. */
	CSR_WRITE_4(sc, JME_TIMER1, 0);
	CSR_WRITE_4(sc, JME_TIMER2, 0);

	/* Configure retry transmit period, retry limit value. */
	CSR_WRITE_4(sc, JME_TXTRHD,
	    ((TXTRHD_RT_PERIOD_DEFAULT << TXTRHD_RT_PERIOD_SHIFT) &
	    TXTRHD_RT_PERIOD_MASK) |
	    ((TXTRHD_RT_LIMIT_DEFAULT << TXTRHD_RT_LIMIT_SHIFT) &
	    TXTRHD_RT_LIMIT_SHIFT));

#ifdef DEVICE_POLLING
	if (!(ifp->if_flags & IFF_POLLING))
#endif
	/* Initialize the interrupt mask. */
	CSR_WRITE_4(sc, JME_INTR_MASK_SET, JME_INTRS);
	CSR_WRITE_4(sc, JME_INTR_STATUS, 0xFFFFFFFF);

	/*
	 * Enabling Tx/Rx DMA engines and Rx queue processing is
	 * done after detection of valid link in jme_miibus_statchg.
	 */
	sc->jme_flags &= ~JME_FLAG_LINK;

	/* Set the current media. */
	mii = device_get_softc(sc->jme_miibus);
	mii_mediachg(mii);

	callout_reset(&sc->jme_tick_ch, hz, jme_tick, sc);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
}

static void
jme_stop(struct jme_softc *sc)
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct jme_txdesc *txd;
	struct jme_rxdesc *rxd;
	struct jme_rxdata *rdata;
	int i, r;

	ASSERT_SERIALIZED(ifp->if_serializer);

	/*
	 * Mark the interface down and cancel the watchdog timer.
	 */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	callout_stop(&sc->jme_tick_ch);
	sc->jme_flags &= ~JME_FLAG_LINK;

	/*
	 * Disable interrupts.
	 */
	CSR_WRITE_4(sc, JME_INTR_MASK_CLR, JME_INTRS);
	CSR_WRITE_4(sc, JME_INTR_STATUS, 0xFFFFFFFF);

	/* Disable updating shadow status block. */
	CSR_WRITE_4(sc, JME_SHBASE_ADDR_LO,
	    CSR_READ_4(sc, JME_SHBASE_ADDR_LO) & ~SHBASE_POST_ENB);

	/* Stop receiver, transmitter. */
	jme_stop_rx(sc);
	jme_stop_tx(sc);

	/*
	 * Free partial finished RX segments
	 */
	for (r = 0; r < sc->jme_rx_ring_inuse; ++r) {
		rdata = &sc->jme_cdata.jme_rx_data[r];
		if (rdata->jme_rxhead != NULL)
			m_freem(rdata->jme_rxhead);
		JME_RXCHAIN_RESET(sc, r);
	}

	/*
	 * Free RX and TX mbufs still in the queues.
	 */
	for (r = 0; r < sc->jme_rx_ring_inuse; ++r) {
		rdata = &sc->jme_cdata.jme_rx_data[r];
		for (i = 0; i < sc->jme_rx_desc_cnt; i++) {
			rxd = &rdata->jme_rxdesc[i];
			if (rxd->rx_m != NULL) {
				bus_dmamap_unload(rdata->jme_rx_tag,
						  rxd->rx_dmamap);
				m_freem(rxd->rx_m);
				rxd->rx_m = NULL;
			}
		}
	}
	for (i = 0; i < sc->jme_tx_desc_cnt; i++) {
		txd = &sc->jme_cdata.jme_txdesc[i];
		if (txd->tx_m != NULL) {
			bus_dmamap_unload(sc->jme_cdata.jme_tx_tag,
			    txd->tx_dmamap);
			m_freem(txd->tx_m);
			txd->tx_m = NULL;
			txd->tx_ndesc = 0;
		}
        }
}

static void
jme_stop_tx(struct jme_softc *sc)
{
	uint32_t reg;
	int i;

	reg = CSR_READ_4(sc, JME_TXCSR);
	if ((reg & TXCSR_TX_ENB) == 0)
		return;
	reg &= ~TXCSR_TX_ENB;
	CSR_WRITE_4(sc, JME_TXCSR, reg);
	for (i = JME_TIMEOUT; i > 0; i--) {
		DELAY(1);
		if ((CSR_READ_4(sc, JME_TXCSR) & TXCSR_TX_ENB) == 0)
			break;
	}
	if (i == 0)
		device_printf(sc->jme_dev, "stopping transmitter timeout!\n");
}

static void
jme_stop_rx(struct jme_softc *sc)
{
	uint32_t reg;
	int i;

	reg = CSR_READ_4(sc, JME_RXCSR);
	if ((reg & RXCSR_RX_ENB) == 0)
		return;
	reg &= ~RXCSR_RX_ENB;
	CSR_WRITE_4(sc, JME_RXCSR, reg);
	for (i = JME_TIMEOUT; i > 0; i--) {
		DELAY(1);
		if ((CSR_READ_4(sc, JME_RXCSR) & RXCSR_RX_ENB) == 0)
			break;
	}
	if (i == 0)
		device_printf(sc->jme_dev, "stopping recevier timeout!\n");
}

static void
jme_init_tx_ring(struct jme_softc *sc)
{
	struct jme_chain_data *cd;
	struct jme_txdesc *txd;
	int i;

	sc->jme_cdata.jme_tx_prod = 0;
	sc->jme_cdata.jme_tx_cons = 0;
	sc->jme_cdata.jme_tx_cnt = 0;

	cd = &sc->jme_cdata;
	bzero(cd->jme_tx_ring, JME_TX_RING_SIZE(sc));
	for (i = 0; i < sc->jme_tx_desc_cnt; i++) {
		txd = &sc->jme_cdata.jme_txdesc[i];
		txd->tx_m = NULL;
		txd->tx_desc = &cd->jme_tx_ring[i];
		txd->tx_ndesc = 0;
	}
}

static void
jme_init_ssb(struct jme_softc *sc)
{
	struct jme_chain_data *cd;

	cd = &sc->jme_cdata;
	bzero(cd->jme_ssb_block, JME_SSB_SIZE);
}

static int
jme_init_rx_ring(struct jme_softc *sc, int ring)
{
	struct jme_rxdata *rdata = &sc->jme_cdata.jme_rx_data[ring];
	struct jme_rxdesc *rxd;
	int i;

	KKASSERT(rdata->jme_rxhead == NULL &&
		 rdata->jme_rxtail == NULL &&
		 rdata->jme_rxlen == 0);
	rdata->jme_rx_cons = 0;

	bzero(rdata->jme_rx_ring, JME_RX_RING_SIZE(sc));
	for (i = 0; i < sc->jme_rx_desc_cnt; i++) {
		int error;

		rxd = &rdata->jme_rxdesc[i];
		rxd->rx_m = NULL;
		rxd->rx_desc = &rdata->jme_rx_ring[i];
		error = jme_newbuf(sc, ring, rxd, 1);
		if (error)
			return error;
	}
	return 0;
}

static int
jme_newbuf(struct jme_softc *sc, int ring, struct jme_rxdesc *rxd, int init)
{
	struct jme_rxdata *rdata = &sc->jme_cdata.jme_rx_data[ring];
	struct jme_desc *desc;
	struct mbuf *m;
	bus_dma_segment_t segs;
	bus_dmamap_t map;
	int error, nsegs;

	m = m_getcl(init ? MB_WAIT : MB_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL)
		return ENOBUFS;
	/*
	 * JMC250 has 64bit boundary alignment limitation so jme(4)
	 * takes advantage of 10 bytes padding feature of hardware
	 * in order not to copy entire frame to align IP header on
	 * 32bit boundary.
	 */
	m->m_len = m->m_pkthdr.len = MCLBYTES;

	error = bus_dmamap_load_mbuf_segment(rdata->jme_rx_tag,
			rdata->jme_rx_sparemap, m, &segs, 1, &nsegs,
			BUS_DMA_NOWAIT);
	if (error) {
		m_freem(m);
		if (init)
			if_printf(&sc->arpcom.ac_if, "can't load RX mbuf\n");
		return error;
	}

	if (rxd->rx_m != NULL) {
		bus_dmamap_sync(rdata->jme_rx_tag, rxd->rx_dmamap,
				BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(rdata->jme_rx_tag, rxd->rx_dmamap);
	}
	map = rxd->rx_dmamap;
	rxd->rx_dmamap = rdata->jme_rx_sparemap;
	rdata->jme_rx_sparemap = map;
	rxd->rx_m = m;

	desc = rxd->rx_desc;
	desc->buflen = htole32(segs.ds_len);
	desc->addr_lo = htole32(JME_ADDR_LO(segs.ds_addr));
	desc->addr_hi = htole32(JME_ADDR_HI(segs.ds_addr));
	desc->flags = htole32(JME_RD_OWN | JME_RD_INTR | JME_RD_64BIT);

	return 0;
}

static void
jme_set_vlan(struct jme_softc *sc)
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	uint32_t reg;

	ASSERT_SERIALIZED(ifp->if_serializer);

	reg = CSR_READ_4(sc, JME_RXMAC);
	reg &= ~RXMAC_VLAN_ENB;
	if (ifp->if_capenable & IFCAP_VLAN_HWTAGGING)
		reg |= RXMAC_VLAN_ENB;
	CSR_WRITE_4(sc, JME_RXMAC, reg);
}

static void
jme_set_filter(struct jme_softc *sc)
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct ifmultiaddr *ifma;
	uint32_t crc;
	uint32_t mchash[2];
	uint32_t rxcfg;

	ASSERT_SERIALIZED(ifp->if_serializer);

	rxcfg = CSR_READ_4(sc, JME_RXMAC);
	rxcfg &= ~(RXMAC_BROADCAST | RXMAC_PROMISC | RXMAC_MULTICAST |
	    RXMAC_ALLMULTI);

	/*
	 * Always accept frames destined to our station address.
	 * Always accept broadcast frames.
	 */
	rxcfg |= RXMAC_UNICAST | RXMAC_BROADCAST;

	if (ifp->if_flags & (IFF_PROMISC | IFF_ALLMULTI)) {
		if (ifp->if_flags & IFF_PROMISC)
			rxcfg |= RXMAC_PROMISC;
		if (ifp->if_flags & IFF_ALLMULTI)
			rxcfg |= RXMAC_ALLMULTI;
		CSR_WRITE_4(sc, JME_MAR0, 0xFFFFFFFF);
		CSR_WRITE_4(sc, JME_MAR1, 0xFFFFFFFF);
		CSR_WRITE_4(sc, JME_RXMAC, rxcfg);
		return;
	}

	/*
	 * Set up the multicast address filter by passing all multicast
	 * addresses through a CRC generator, and then using the low-order
	 * 6 bits as an index into the 64 bit multicast hash table.  The
	 * high order bits select the register, while the rest of the bits
	 * select the bit within the register.
	 */
	rxcfg |= RXMAC_MULTICAST;
	bzero(mchash, sizeof(mchash));

	LIST_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		crc = ether_crc32_be(LLADDR((struct sockaddr_dl *)
		    ifma->ifma_addr), ETHER_ADDR_LEN);

		/* Just want the 6 least significant bits. */
		crc &= 0x3f;

		/* Set the corresponding bit in the hash table. */
		mchash[crc >> 5] |= 1 << (crc & 0x1f);
	}

	CSR_WRITE_4(sc, JME_MAR0, mchash[0]);
	CSR_WRITE_4(sc, JME_MAR1, mchash[1]);
	CSR_WRITE_4(sc, JME_RXMAC, rxcfg);
}

static int
jme_sysctl_tx_coal_to(SYSCTL_HANDLER_ARGS)
{
	struct jme_softc *sc = arg1;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	int error, v;

	lwkt_serialize_enter(ifp->if_serializer);

	v = sc->jme_tx_coal_to;
	error = sysctl_handle_int(oidp, &v, 0, req);
	if (error || req->newptr == NULL)
		goto back;

	if (v < PCCTX_COAL_TO_MIN || v > PCCTX_COAL_TO_MAX) {
		error = EINVAL;
		goto back;
	}

	if (v != sc->jme_tx_coal_to) {
		sc->jme_tx_coal_to = v;
		if (ifp->if_flags & IFF_RUNNING)
			jme_set_tx_coal(sc);
	}
back:
	lwkt_serialize_exit(ifp->if_serializer);
	return error;
}

static int
jme_sysctl_tx_coal_pkt(SYSCTL_HANDLER_ARGS)
{
	struct jme_softc *sc = arg1;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	int error, v;

	lwkt_serialize_enter(ifp->if_serializer);

	v = sc->jme_tx_coal_pkt;
	error = sysctl_handle_int(oidp, &v, 0, req);
	if (error || req->newptr == NULL)
		goto back;

	if (v < PCCTX_COAL_PKT_MIN || v > PCCTX_COAL_PKT_MAX) {
		error = EINVAL;
		goto back;
	}

	if (v != sc->jme_tx_coal_pkt) {
		sc->jme_tx_coal_pkt = v;
		if (ifp->if_flags & IFF_RUNNING)
			jme_set_tx_coal(sc);
	}
back:
	lwkt_serialize_exit(ifp->if_serializer);
	return error;
}

static int
jme_sysctl_rx_coal_to(SYSCTL_HANDLER_ARGS)
{
	struct jme_softc *sc = arg1;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	int error, v;

	lwkt_serialize_enter(ifp->if_serializer);

	v = sc->jme_rx_coal_to;
	error = sysctl_handle_int(oidp, &v, 0, req);
	if (error || req->newptr == NULL)
		goto back;

	if (v < PCCRX_COAL_TO_MIN || v > PCCRX_COAL_TO_MAX) {
		error = EINVAL;
		goto back;
	}

	if (v != sc->jme_rx_coal_to) {
		sc->jme_rx_coal_to = v;
		if (ifp->if_flags & IFF_RUNNING)
			jme_set_rx_coal(sc);
	}
back:
	lwkt_serialize_exit(ifp->if_serializer);
	return error;
}

static int
jme_sysctl_rx_coal_pkt(SYSCTL_HANDLER_ARGS)
{
	struct jme_softc *sc = arg1;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	int error, v;

	lwkt_serialize_enter(ifp->if_serializer);

	v = sc->jme_rx_coal_pkt;
	error = sysctl_handle_int(oidp, &v, 0, req);
	if (error || req->newptr == NULL)
		goto back;

	if (v < PCCRX_COAL_PKT_MIN || v > PCCRX_COAL_PKT_MAX) {
		error = EINVAL;
		goto back;
	}

	if (v != sc->jme_rx_coal_pkt) {
		sc->jme_rx_coal_pkt = v;
		if (ifp->if_flags & IFF_RUNNING)
			jme_set_rx_coal(sc);
	}
back:
	lwkt_serialize_exit(ifp->if_serializer);
	return error;
}

static void
jme_set_tx_coal(struct jme_softc *sc)
{
	uint32_t reg;

	reg = (sc->jme_tx_coal_to << PCCTX_COAL_TO_SHIFT) &
	    PCCTX_COAL_TO_MASK;
	reg |= (sc->jme_tx_coal_pkt << PCCTX_COAL_PKT_SHIFT) &
	    PCCTX_COAL_PKT_MASK;
	reg |= PCCTX_COAL_TXQ0;
	CSR_WRITE_4(sc, JME_PCCTX, reg);
}

static void
jme_set_rx_coal(struct jme_softc *sc)
{
	uint32_t reg;
	int r;

	reg = (sc->jme_rx_coal_to << PCCRX_COAL_TO_SHIFT) &
	    PCCRX_COAL_TO_MASK;
	reg |= (sc->jme_rx_coal_pkt << PCCRX_COAL_PKT_SHIFT) &
	    PCCRX_COAL_PKT_MASK;
	for (r = 0; r < sc->jme_rx_ring_cnt; ++r) {
		if (r < sc->jme_rx_ring_inuse)
			CSR_WRITE_4(sc, JME_PCCRX(r), reg);
		else
			CSR_WRITE_4(sc, JME_PCCRX(r), 0);
	}
}

#ifdef DEVICE_POLLING

static void
jme_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct jme_softc *sc = ifp->if_softc;
	struct mbuf_chain chain[MAXCPU];
	uint32_t status;
	int r, prog = 0;

	ASSERT_SERIALIZED(ifp->if_serializer);

	switch (cmd) {
	case POLL_REGISTER:
		CSR_WRITE_4(sc, JME_INTR_MASK_CLR, JME_INTRS);
		break;

	case POLL_DEREGISTER:
		CSR_WRITE_4(sc, JME_INTR_MASK_SET, JME_INTRS);
		break;

	case POLL_AND_CHECK_STATUS:
	case POLL_ONLY:
		status = CSR_READ_4(sc, JME_INTR_STATUS);

		ether_input_chain_init(chain);
		for (r = 0; r < sc->jme_rx_ring_inuse; ++r)
			prog += jme_rxeof_chain(sc, r, chain, count);
		if (prog)
			ether_input_dispatch(chain);

		if (status & INTR_RXQ_DESC_EMPTY) {
			CSR_WRITE_4(sc, JME_INTR_STATUS, status);
			CSR_WRITE_4(sc, JME_RXCSR, sc->jme_rxcsr |
			    RXCSR_RX_ENB | RXCSR_RXQ_START);
		}

		jme_txeof(sc);
		if (!ifq_is_empty(&ifp->if_snd))
			if_devstart(ifp);
		break;
	}
}

#endif	/* DEVICE_POLLING */

static int
jme_rxring_dma_alloc(struct jme_softc *sc, int ring)
{
	struct jme_rxdata *rdata = &sc->jme_cdata.jme_rx_data[ring];
	bus_dmamem_t dmem;
	int error;

	error = bus_dmamem_coherent(sc->jme_cdata.jme_ring_tag,
			JME_RX_RING_ALIGN, 0,
			BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR,
			JME_RX_RING_SIZE(sc),
			BUS_DMA_WAITOK | BUS_DMA_ZERO, &dmem);
	if (error) {
		device_printf(sc->jme_dev,
		    "could not allocate %dth Rx ring.\n", ring);
		return error;
	}
	rdata->jme_rx_ring_tag = dmem.dmem_tag;
	rdata->jme_rx_ring_map = dmem.dmem_map;
	rdata->jme_rx_ring = dmem.dmem_addr;
	rdata->jme_rx_ring_paddr = dmem.dmem_busaddr;

	return 0;
}

static int
jme_rxbuf_dma_alloc(struct jme_softc *sc, int ring)
{
	struct jme_rxdata *rdata = &sc->jme_cdata.jme_rx_data[ring];
	int i, error;

	/* Create tag for Rx buffers. */
	error = bus_dma_tag_create(sc->jme_cdata.jme_buffer_tag,/* parent */
	    JME_RX_BUF_ALIGN, 0,	/* algnmnt, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MCLBYTES,			/* maxsize */
	    1,				/* nsegments */
	    MCLBYTES,			/* maxsegsize */
	    BUS_DMA_ALLOCNOW | BUS_DMA_WAITOK | BUS_DMA_ALIGNED,/* flags */
	    &rdata->jme_rx_tag);
	if (error) {
		device_printf(sc->jme_dev,
		    "could not create %dth Rx DMA tag.\n", ring);
		return error;
	}

	/* Create DMA maps for Rx buffers. */
	error = bus_dmamap_create(rdata->jme_rx_tag, BUS_DMA_WAITOK,
				  &rdata->jme_rx_sparemap);
	if (error) {
		device_printf(sc->jme_dev,
		    "could not create %dth spare Rx dmamap.\n", ring);
		bus_dma_tag_destroy(rdata->jme_rx_tag);
		rdata->jme_rx_tag = NULL;
		return error;
	}
	for (i = 0; i < sc->jme_rx_desc_cnt; i++) {
		struct jme_rxdesc *rxd = &rdata->jme_rxdesc[i];

		error = bus_dmamap_create(rdata->jme_rx_tag, BUS_DMA_WAITOK,
					  &rxd->rx_dmamap);
		if (error) {
			int j;

			device_printf(sc->jme_dev,
			    "could not create %dth Rx dmamap "
			    "for %dth RX ring.\n", i, ring);

			for (j = 0; j < i; ++j) {
				rxd = &rdata->jme_rxdesc[j];
				bus_dmamap_destroy(rdata->jme_rx_tag,
						   rxd->rx_dmamap);
			}
			bus_dmamap_destroy(rdata->jme_rx_tag,
					   rdata->jme_rx_sparemap);
			bus_dma_tag_destroy(rdata->jme_rx_tag);
			rdata->jme_rx_tag = NULL;
			return error;
		}
	}
	return 0;
}

static void
jme_rx_intr(struct jme_softc *sc, uint32_t status)
{
	struct mbuf_chain chain[MAXCPU];
	int r, prog = 0;

	ether_input_chain_init(chain);
	for (r = 0; r < sc->jme_rx_ring_inuse; ++r) {
		if (status & jme_rx_status[r].jme_coal)
			prog += jme_rxeof_chain(sc, r, chain, -1);
	}
	if (prog)
		ether_input_dispatch(chain);
}

#ifdef RSS

static void
jme_enable_rss(struct jme_softc *sc)
{
	uint32_t rssc, ind;
	uint8_t key[RSSKEY_NREGS * RSSKEY_REGSIZE];
	int i;

	sc->jme_rx_ring_inuse = sc->jme_rx_ring_cnt;

	KASSERT(sc->jme_rx_ring_inuse == JME_NRXRING_2 ||
		sc->jme_rx_ring_inuse == JME_NRXRING_4,
		("%s: invalid # of RX rings (%d)\n",
		 sc->arpcom.ac_if.if_xname, sc->jme_rx_ring_inuse));

	rssc = RSSC_HASH_64_ENTRY;
	rssc |= RSSC_HASH_IPV4 | RSSC_HASH_IPV4_TCP;
	rssc |= sc->jme_rx_ring_inuse >> 1;
	JME_RSS_DPRINTF(sc, 1, "rssc 0x%08x\n", rssc);
	CSR_WRITE_4(sc, JME_RSSC, rssc);

	toeplitz_get_key(key, sizeof(key));
	for (i = 0; i < RSSKEY_NREGS; ++i) {
		uint32_t keyreg;

		keyreg = RSSKEY_REGVAL(key, i);
		JME_RSS_DPRINTF(sc, 5, "keyreg%d 0x%08x\n", i, keyreg);

		CSR_WRITE_4(sc, RSSKEY_REG(i), keyreg);
	}

	/*
	 * Create redirect table in following fashion:
	 * (hash & ring_cnt_mask) == rdr_table[(hash & rdr_table_mask)]
	 */
	ind = 0;
	for (i = 0; i < RSSTBL_REGSIZE; ++i) {
		int q;

		q = i % sc->jme_rx_ring_inuse;
		ind |= q << (i * 8);
	}
	JME_RSS_DPRINTF(sc, 1, "ind 0x%08x\n", ind);

	for (i = 0; i < RSSTBL_NREGS; ++i)
		CSR_WRITE_4(sc, RSSTBL_REG(i), ind);
}

#endif	/* RSS */

static void
jme_disable_rss(struct jme_softc *sc)
{
	sc->jme_rx_ring_inuse = JME_NRXRING_1;
	CSR_WRITE_4(sc, JME_RSSC, RSSC_DIS_RSS);
}
