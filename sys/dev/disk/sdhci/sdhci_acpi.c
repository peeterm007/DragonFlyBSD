/*-
 * Copyright (c) 2008 Alexander Motin <mav@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/taskqueue.h>

#include "acpi.h"
#include "opt_acpi.h"
#include <dev/acpica/acpivar.h>

#include <bus/pci/pcivar.h>

#include <bus/mmc/bridge.h>

#include <dev/disk/sdhci/sdhci.h>

#include "mmcbr_if.h"
#include "sdhci_if.h"

ACPI_MODULE_NAME("sdhci_acpi");

struct sdhci_acpi_softc {
	device_t	dev;		/* Controller device */
	ACPI_HANDLE	handle;
	struct resource *irq_res;	/* IRQ resource */
	void 		*intrhand;	/* Interrupt handle */

	struct sdhci_slot slot;
	struct resource	*mem_res;	/* Memory resource */
};

static uint8_t
sdhci_acpi_read_1(device_t dev, struct sdhci_slot *slot __unused,
    bus_size_t off)
{
	struct sdhci_acpi_softc *sc = device_get_softc(dev);

	bus_barrier(sc->mem_res, 0, 0xFF,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
	return bus_read_1(sc->mem_res, off);
}

static void
sdhci_acpi_write_1(device_t dev, struct sdhci_slot *slot __unused,
    bus_size_t off, uint8_t val)
{
	struct sdhci_acpi_softc *sc = device_get_softc(dev);

	bus_barrier(sc->mem_res, 0, 0xFF,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
	bus_write_1(sc->mem_res, off, val);
}

static uint16_t
sdhci_acpi_read_2(device_t dev, struct sdhci_slot *slot __unused,
    bus_size_t off)
{
	struct sdhci_acpi_softc *sc = device_get_softc(dev);

	bus_barrier(sc->mem_res, 0, 0xFF,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
	return bus_read_2(sc->mem_res, off);
}

static void
sdhci_acpi_write_2(device_t dev, struct sdhci_slot *slot __unused,
    bus_size_t off, uint16_t val)
{
	struct sdhci_acpi_softc *sc = device_get_softc(dev);

	bus_barrier(sc->mem_res, 0, 0xFF,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
	bus_write_2(sc->mem_res, off, val);
}

static uint32_t
sdhci_acpi_read_4(device_t dev, struct sdhci_slot *slot __unused,
    bus_size_t off)
{
	struct sdhci_acpi_softc *sc = device_get_softc(dev);

	bus_barrier(sc->mem_res, 0, 0xFF,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
	return bus_read_4(sc->mem_res, off);
}

static void
sdhci_acpi_write_4(device_t dev, struct sdhci_slot *slot __unused,
    bus_size_t off, uint32_t val)
{
	struct sdhci_acpi_softc *sc = device_get_softc(dev);

	bus_barrier(sc->mem_res, 0, 0xFF,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
	bus_write_4(sc->mem_res, off, val);
}

static void
sdhci_acpi_read_multi_4(device_t dev, struct sdhci_slot *slot __unused,
    bus_size_t off, uint32_t *data, bus_size_t count)
{
	struct sdhci_acpi_softc *sc = device_get_softc(dev);

	bus_read_multi_stream_4(sc->mem_res, off, data, count);
}

static void
sdhci_acpi_write_multi_4(device_t dev, struct sdhci_slot *slot __unused,
    bus_size_t off, uint32_t *data, bus_size_t count)
{
	struct sdhci_acpi_softc *sc = device_get_softc(dev);

	bus_write_multi_stream_4(sc->mem_res, off, data, count);
}

static void sdhci_acpi_intr(void *arg);

#define INTEL_ATOM_QUIRKS_SDCARD					\
	SDHCI_QUIRK_WHITELIST_ADMA2 | SDHCI_QUIRK_WAIT_WHILE_BUSY
#define INTEL_ATOM_QUIRKS_EMMC						\
	INTEL_ATOM_QUIRKS_SDCARD | SDHCI_QUIRK_MMC_DDR52 |		\
	SDHCI_QUIRK_CAPS_BIT63_FOR_MMC_HS400 | SDHCI_QUIRK_PRESET_VALUE_BROKEN

static struct {
	char *hid;
	char *uid;
	u_int quirks;
} sdhci_devices[] = {
	/* The Intel Atom integrated controllers work fine with ADMA2. */
	/* Bay Trail / Braswell */
	{"80860F14", "1", INTEL_ATOM_QUIRKS_EMMC},
	{"80860F14", "3", INTEL_ATOM_QUIRKS_SDCARD},
	{"80860F16", NULL, INTEL_ATOM_QUIRKS_SDCARD},
	/* Apollo Lake */
	{"80865ACA", NULL,
	    SDHCI_QUIRK_BROKEN_DMA |	/* APL18 erratum */
	    INTEL_ATOM_QUIRKS_SDCARD},
	{"80865ACC", NULL,
	    SDHCI_QUIRK_BROKEN_DMA |	/* APL18 erratum */
	    INTEL_ATOM_QUIRKS_EMMC},
};

static char *sdhci_ids[] = {
	"80860F14",
	"80860F16",
	"80865ACA",
	"80865ACC",
	NULL
};

static int
sdhci_acpi_probe(device_t dev)
{
	if (acpi_disabled("sdhci"))
		return (ENXIO);

	if (ACPI_ID_PROBE(device_get_parent(dev), dev, sdhci_ids) == NULL)
		return (ENXIO);

	device_set_desc(dev, "SDHCI controller");
	return (0);
}

static int
sdhci_acpi_attach(device_t dev)
{
	struct sdhci_acpi_softc *sc = device_get_softc(dev);
	char *id;
	int err, i, rid, quirks;

	id = ACPI_ID_PROBE(device_get_parent(dev), dev, sdhci_ids);
	if (id == NULL)
		return (ENXIO);

	sc->dev = dev;
	sc->handle = acpi_get_handle(dev);

	quirks = 0;
	for (i = 0; i < NELEM(sdhci_devices); i++) {
		if (strcmp(sdhci_devices[i].hid, id) == 0 &&
		    (sdhci_devices[i].uid == NULL ||
		     acpi_MatchUid(sc->handle, sdhci_devices[i].uid))) {
			quirks = sdhci_devices[i].quirks;
			break;
		}
	}
	quirks &= ~sdhci_quirk_clear;
	quirks |= sdhci_quirk_set;

	/* Allocate IRQ. */
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
		RF_SHAREABLE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "Can't allocate IRQ\n");
		err = ENOMEM;
		goto error;
	}

	/* Allocate memory. */
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "Can't allocate memory for slot %d\n", 0);
		err = ENOMEM;
		goto error;
	}

	pci_set_powerstate(dev, PCI_POWERSTATE_D0);

	sc->slot.quirks = quirks;
	if (sdhci_init_slot(dev, &sc->slot, 0) != 0) {
		device_printf(dev, "sdhci initialization failed\n");
		pci_set_powerstate(dev, PCI_POWERSTATE_D3);
		err = ENXIO;
		goto error;
	}

	device_printf(dev, "%d slot(s) allocated\n", 1);
	/* Activate the interrupt */
	err = bus_setup_intr(dev, sc->irq_res, INTR_MPSAFE,
	    sdhci_acpi_intr, sc, &sc->intrhand, NULL);
	if (err)
		device_printf(dev, "Can't setup IRQ\n");

	/* Process cards detection. */
	sdhci_start_slot(&sc->slot);

	return (0);

error:
	if (sc->irq_res != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->irq_res), sc->irq_res);
	}
	if (sc->mem_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->mem_res), sc->mem_res);
	}
	return (err);
}

static int
sdhci_acpi_detach(device_t dev)
{
	struct sdhci_acpi_softc *sc = device_get_softc(dev);

	bus_teardown_intr(dev, sc->irq_res, sc->intrhand);
	bus_release_resource(dev, SYS_RES_IRQ,
	    rman_get_rid(sc->irq_res), sc->irq_res);

	sdhci_cleanup_slot(&sc->slot);
	bus_release_resource(dev, SYS_RES_MEMORY,
	    rman_get_rid(sc->mem_res), sc->mem_res);
	pci_set_powerstate(dev, PCI_POWERSTATE_D3);
	return (0);
}

static int
sdhci_acpi_suspend(device_t dev)
{
	struct sdhci_acpi_softc *sc = device_get_softc(dev);
	int err;

	err = bus_generic_suspend(dev);
	if (err)
		return (err);
	sdhci_generic_suspend(&sc->slot);
	return (0);
}

static int
sdhci_acpi_resume(device_t dev)
{
	struct sdhci_acpi_softc *sc = device_get_softc(dev);

	sdhci_generic_resume(&sc->slot);
	return (bus_generic_resume(dev));
}

static void
sdhci_acpi_intr(void *arg)
{
	struct sdhci_acpi_softc *sc = (struct sdhci_acpi_softc *)arg;

	sdhci_generic_intr(&sc->slot);
}

static device_method_t sdhci_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe, sdhci_acpi_probe),
	DEVMETHOD(device_attach, sdhci_acpi_attach),
	DEVMETHOD(device_detach, sdhci_acpi_detach),
	DEVMETHOD(device_suspend, sdhci_acpi_suspend),
	DEVMETHOD(device_resume, sdhci_acpi_resume),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	sdhci_generic_read_ivar),
	DEVMETHOD(bus_write_ivar,	sdhci_generic_write_ivar),

	/* mmcbr_if */
	DEVMETHOD(mmcbr_update_ios,	sdhci_generic_update_ios),
	DEVMETHOD(mmcbr_switch_vccq,	sdhci_generic_switch_vccq),
	DEVMETHOD(mmcbr_request,	sdhci_generic_request),
	DEVMETHOD(mmcbr_get_ro,		sdhci_generic_get_ro),
	DEVMETHOD(mmcbr_acquire_host,	sdhci_generic_acquire_host),
	DEVMETHOD(mmcbr_release_host,	sdhci_generic_release_host),

	/* SDHCI accessors */
	DEVMETHOD(sdhci_read_1,		sdhci_acpi_read_1),
	DEVMETHOD(sdhci_read_2,		sdhci_acpi_read_2),
	DEVMETHOD(sdhci_read_4,		sdhci_acpi_read_4),
	DEVMETHOD(sdhci_read_multi_4,	sdhci_acpi_read_multi_4),
	DEVMETHOD(sdhci_write_1,	sdhci_acpi_write_1),
	DEVMETHOD(sdhci_write_2,	sdhci_acpi_write_2),
	DEVMETHOD(sdhci_write_4,	sdhci_acpi_write_4),
	DEVMETHOD(sdhci_write_multi_4,	sdhci_acpi_write_multi_4),
	DEVMETHOD(sdhci_set_uhs_timing,	sdhci_generic_set_uhs_timing),

	DEVMETHOD_END
};

static driver_t sdhci_acpi_driver = {
	"sdhci_acpi",
	sdhci_methods,
	sizeof(struct sdhci_acpi_softc),
	.gpri = KOBJ_GPRI_ACPI
};
static devclass_t sdhci_acpi_devclass;

DRIVER_MODULE(sdhci_acpi, acpi, sdhci_acpi_driver, sdhci_acpi_devclass, NULL,
    NULL);
MODULE_DEPEND(sdhci_acpi, sdhci, 1, 1, 1);
