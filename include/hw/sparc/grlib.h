/*
 * QEMU GRLIB Components
 *
 * Copyright (c) 2010-2011 AdaCore
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef _GRLIB_H_
#define _GRLIB_H_

#include "hw/qdev.h"
#include "hw/sysbus.h"

/* Emulation of GrLib device is base on the GRLIB IP Core User's Manual:
 * http://www.gaisler.com/products/grlib/grip.pdf
 */

/* Definitions for AMBA PNP */                                                 
                                                                               
/* Vendors */                                                                  
#define VENDOR_GAISLER    1 
#define VENDOR_PENDER     2 
#define VENDOR_ESA        4 
#define VENDOR_DLR       10 

/* Devices */ 
#define GAISLER_LEON3    0x003 
#define GAISLER_APBMST   0x006 
#define GAISLER_APBUART  0x00C 
#define GAISLER_IRQMP    0x00D 
#define GAISLER_GPTIMER  0x011 
#define ESA_MCTRL        0x00F 

/* How to build entries in the plug&play area */ 
#define GRLIB_PP_ID(v, d, x, i) ((v & 0xff) << 24) | ((d & 0x3ff) << 12) |\
                                ((x & 0x1f) << 5) | (i & 0x1f) 
#define GRLIB_PP_AHBADDR(a, m, p, c, t) (a & 0xfff00000) | ((m & 0xfff) << 4) |\
                        ((p & 1) << 17) | ((c & 1) << 16) | (t & 0x3) 
#define GRLIB_PP_APBADDR(a, m) ((a & 0xfff00)<< 12) | ((m & 0xfff) << 4) | 1 

int grlib_apbpp_add(uint32_t id, uint32_t addr);                               
int grlib_ahbmpp_add(uint32_t id);                                             
int grlib_ahbspp_add(uint32_t id, uint32_t addr1, uint32_t addr2,              
                     uint32_t addr3, uint32_t addr4);
/* IRQMP */

typedef void (*set_pil_in_fn) (void *opaque, int cpu, uint32_t pil_in);
typedef void (*start_cpu_fn) (void *opaque, int cpu);

void grlib_irqmp_set_irq(void *opaque, int irq, int level);

void grlib_irqmp_ack(DeviceState *dev, int cpu, int intno);

static inline
DeviceState *grlib_irqmp_create(hwaddr   base,
				unsigned             nr_cpus,
                                qemu_irq           **cpu_irqs,
                                uint32_t             nr_irqs,
                                set_pil_in_fn        set_pil_in,
				start_cpu_fn         start_cpu,
				void                *opaque)
{
    DeviceState *dev;

    assert(cpu_irqs != NULL);

    dev = qdev_create(NULL, "grlib,irqmp");
    qdev_prop_set_uint32(dev, "nr-cpus", nr_cpus);
    qdev_prop_set_ptr(dev, "set_pil_in", set_pil_in);
    qdev_prop_set_ptr(dev, "start_cpu", start_cpu);
    qdev_prop_set_ptr(dev, "opaque", opaque);

    qdev_init_nofail(dev);

    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, base);

    *cpu_irqs = qemu_allocate_irqs(grlib_irqmp_set_irq, dev, nr_irqs);

    grlib_apbpp_add(GRLIB_PP_ID(VENDOR_GAISLER, GAISLER_IRQMP, 2, 0),
                    GRLIB_PP_APBADDR(base, 0xFFF));

    return dev;
}

/* GPTimer */

static inline
DeviceState *grlib_gptimer_create(hwaddr  base,
                                  uint32_t            nr_timers,
                                  uint32_t            freq,
                                  qemu_irq           *cpu_irqs,
                                  int                 base_irq)
{
    DeviceState *dev;
    int i;

    dev = qdev_create(NULL, "grlib,gptimer");
    qdev_prop_set_uint32(dev, "nr-timers", nr_timers);
    qdev_prop_set_uint32(dev, "frequency", freq);
    qdev_prop_set_uint32(dev, "irq-line", base_irq);

    qdev_init_nofail(dev);

    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, base);

    for (i = 0; i < nr_timers; i++) {
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), i, cpu_irqs[base_irq + i]);
    }

    grlib_apbpp_add(GRLIB_PP_ID(VENDOR_GAISLER, GAISLER_GPTIMER, 0, base_irq),
                    GRLIB_PP_APBADDR(base, 0xFFF));

    return dev;
}

/* APB UART */

static inline
DeviceState *grlib_apbuart_create(hwaddr  base,
                                  CharDriverState    *serial,
                                  qemu_irq           *cpu_irqs,
                                  int                 base_irq)
{
    DeviceState *dev;

    dev = qdev_create(NULL, "grlib,apbuart");
    qdev_prop_set_chr(dev, "chrdev", serial);

    qdev_init_nofail(dev);

    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, base);

    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, cpu_irqs[base_irq]);

    grlib_apbpp_add(GRLIB_PP_ID(VENDOR_GAISLER, GAISLER_APBUART, 1, base_irq),
                    GRLIB_PP_APBADDR(base, 0xFFF));

    return dev;
}

/* APB PNP */

static inline
DeviceState *grlib_apbpnp_create(hwaddr  base)
{
    DeviceState *dev;

    dev = qdev_create(NULL, "grlib,apbpnp");

    qdev_init_nofail(dev);

    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, base);

    return dev;
}

/* AHB PNP */

static inline
DeviceState *grlib_ahbpnp_create(hwaddr  base)
{
    DeviceState *dev;

    dev = qdev_create(NULL, "grlib,ahbpnp");

    qdev_init_nofail(dev);

    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, base);

    return dev;
}


/* AMBA PNP */

#define TYPE_GRLIB_AMBA_PNP "grlib,ambapnp"

static inline
DeviceState *grlib_ambapnp_create(hwaddr ahbpnp_base, hwaddr apbpnp_base)
{
    DeviceState *dev;

    dev = qdev_create(NULL, TYPE_GRLIB_AMBA_PNP);

    qdev_init_nofail(dev);

    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, ahbpnp_base);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, apbpnp_base);

    /* Add PP records for Leon3, APB bridge and memory controller
       as this is not done elsewhere */

    grlib_ahbspp_add(GRLIB_PP_ID(VENDOR_GAISLER, GAISLER_APBMST, 0, 0),
                     GRLIB_PP_AHBADDR(0x80000000, 0xFFF, 0, 0, 2),
                     0, 0, 0);
    grlib_apbpp_add(GRLIB_PP_ID(VENDOR_ESA, ESA_MCTRL, 1, 0),
                    GRLIB_PP_APBADDR(0x80000000, 0xFFF));
    grlib_ahbmpp_add(GRLIB_PP_ID(VENDOR_GAISLER, GAISLER_LEON3, 0, 0));
    grlib_ahbspp_add(GRLIB_PP_ID(VENDOR_ESA, ESA_MCTRL, 1, 0),
                     GRLIB_PP_AHBADDR(0x00000000, 0xE00, 1, 1, 2),
                     GRLIB_PP_AHBADDR(0x20000000, 0xE00, 0, 0, 2),
                     GRLIB_PP_AHBADDR(0x40000000, 0xC00, 1, 1, 2),
                     0);
    return dev;
}

#endif /* ! _GRLIB_H_ */
