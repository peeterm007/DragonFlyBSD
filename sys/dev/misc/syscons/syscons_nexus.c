/*-
 * Copyright (c) 1999 Kazutaka YOKOTA <yokota@zodiac.mech.utsunomiya-u.ac.jp>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/isa/syscons_isa.c,v 1.11.2.2 2001/08/01 10:42:28 yokota Exp $
 */

#include "opt_syscons.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systimer.h>
#include <sys/bus.h>
#include <sys/cons.h>

#include <machine/console.h>
#include <machine/framebuffer.h>

#include <dev/misc/syscons/syscons.h>

static devclass_t	sc_devclass;

static void	scidentify(driver_t *driver, device_t parent);
static int	scprobe(device_t dev);
static int	scattach(device_t dev);

static device_method_t sc_methods[] = {
	DEVMETHOD(device_identify,      scidentify),
	DEVMETHOD(device_probe,         scprobe),
	DEVMETHOD(device_attach,        scattach),
	DEVMETHOD_END
};

static driver_t sc_driver = {
	SC_DRIVER_NAME,
	sc_methods,
	sizeof(sc_softc_t),
};

static sc_softc_t main_softc;

static void
scidentify(driver_t *driver, device_t parent)
{
	device_t child;
	int i, u;
	int f;

	for (i = -1; (i = resource_locate(i, SC_DRIVER_NAME)) >= 0;) {
		u = resource_query_unit(i);
		if (u < 0)
			continue;
		if (resource_disabled(SC_DRIVER_NAME, u))
			continue;
		if (resource_int_value(SC_DRIVER_NAME, u, "flags", &f) != 0)
			f = 0;
	        child = BUS_ADD_CHILD(parent, parent, 0, "sc", u);
	        if (child == NULL)
			panic("%s", __func__);
		device_set_flags(child, f);
	}
}

static int
scprobe(device_t dev)
{
	device_set_desc(dev, "System console");
	return sc_probe_unit(device_get_unit(dev), device_get_flags(dev));
}

static int
scattach(device_t dev)
{
	return sc_attach_unit(device_get_unit(dev), device_get_flags(dev));
}

int
sc_max_unit(void)
{
	return devclass_get_maxunit(sc_devclass);
}

sc_softc_t *
sc_get_softc(int unit, int flags)
{
	sc_softc_t *sc;

	if (unit < 0)
		return NULL;
	if (flags & SC_KERNEL_CONSOLE) {
		/* FIXME: clear if it is wired to another unit! */
		sc = &main_softc;
	} else {
	        sc = (sc_softc_t *)device_get_softc(devclass_get_device(sc_devclass, unit));
		if (sc == NULL)
			return NULL;
	}
	sc->unit = unit;
	if (!(sc->flags & SC_INIT_DONE)) {
		sc->keyboard = -1;
		sc->adapter = -1;
		sc->cursor_char = SC_CURSOR_CHAR;
		sc->mouse_char = SC_MOUSE_CHAR;
	}
	return sc;
}

sc_softc_t *
sc_find_softc(struct video_adapter *adp, struct keyboard *kbd)
{
	sc_softc_t *sc;
	int units;
	int i;

	sc = &main_softc;
	if (((adp == NULL) || (adp == sc->adp))
	    && ((kbd == NULL) || (kbd == sc->kbd)))
		return sc;
	units = devclass_get_maxunit(sc_devclass);
	for (i = 0; i < units; ++i) {
	        sc = (sc_softc_t *)device_get_softc(devclass_get_device(sc_devclass, i));
		if (sc == NULL)
			continue;
		if (((adp == NULL) || (adp == sc->adp))
		    && ((kbd == NULL) || (kbd == sc->kbd)))
			return sc;
	}
	return NULL;
}

int
sc_get_cons_priority(int *unit, int *flags)
{
	int u, f;
	int i;
	int have_efi_fb = (probe_efi_fb(1) == 0);

	*unit = -1;
	for (i = -1; (i = resource_locate(i, SC_DRIVER_NAME)) >= 0;) {
		u = resource_query_unit(i);
		if (resource_disabled(SC_DRIVER_NAME, u))
			continue;
		if (resource_int_value(SC_DRIVER_NAME, u, "flags", &f) != 0)
			f = 0;
		/* We prefer the EFI Framebuffer over other video devices */
		if (have_efi_fb && !(f & SC_EFI_FB))
			continue;
		if (f & SC_KERNEL_CONSOLE) {
			/* the user designates this unit to be the console */
			*unit = u;
			*flags = f;
			break;
		}
		if (*unit < 0) {
			/* ...otherwise remember the first found unit */
			*unit = u;
			*flags = f;
		}
	}
	if ((i < 0) && (*unit < 0))
		return CN_DEAD;
	if (!have_efi_fb)
		*flags &= ~SC_EFI_FB;
#if 0
	return ((*flags & SC_KERNEL_CONSOLE) ? CN_INTERNAL : CN_NORMAL);
#endif
	return CN_INTERNAL;
}

void
sc_get_bios_values(bios_values_t *values)
{
	values->cursor_start = 0;
	values->cursor_end = 32;
	values->shift_state = 0;
	values->bell_pitch = BELL_PITCH;
}

int
sc_tone(int hertz)
{
	return EBUSY;
#if 0
	/* XXX use sound device if available */
	return 0;
#endif
}

DRIVER_MODULE(sc, nexus, sc_driver, sc_devclass, NULL, NULL);
