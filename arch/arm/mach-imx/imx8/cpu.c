// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 NXP
 */

#include <common.h>
#include <clk.h>
#include <cpu.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/uclass.h>
#include <errno.h>
#include <fdt_support.h>
#include <fdtdec.h>
#include <thermal.h>
#include <asm/arch/sci/sci.h>
#include <asm/arch/sid.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch-imx/cpu.h>
#include <asm/armv8/cpu.h>
#include <asm/armv8/mmu.h>
#include <asm/mach-imx/boot_mode.h>

DECLARE_GLOBAL_DATA_PTR;

#define BT_PASSOVER_TAG	0x504F
struct pass_over_info_t *get_pass_over_info(void)
{
	struct pass_over_info_t *p =
		(struct pass_over_info_t *)PASS_OVER_INFO_ADDR;

	if (p->barker != BT_PASSOVER_TAG ||
	    p->len != sizeof(struct pass_over_info_t))
		return NULL;

	return p;
}

int arch_cpu_init(void)
{
#ifdef CONFIG_SPL_BUILD
	struct pass_over_info_t *pass_over;

	if (is_soc_rev(CHIP_REV_A)) {
		pass_over = get_pass_over_info();
		if (pass_over && pass_over->g_ap_mu == 0) {
			/*
			 * When ap_mu is 0, means the U-Boot booted
			 * from first container
			 */
			sc_misc_boot_status(-1, SC_MISC_BOOT_STATUS_SUCCESS);
		}
	}
#endif

	return 0;
}

int arch_cpu_init_dm(void)
{
	struct udevice *devp;
	int node, ret;

	node = fdt_node_offset_by_compatible(gd->fdt_blob, -1, "fsl,imx8-mu");
	ret = device_bind_driver_to_node(gd->dm_root, "imx8_scu", "imx8_scu",
					 offset_to_ofnode(node), &devp);

	if (ret) {
		printf("could not find scu %d\n", ret);
		return ret;
	}

	ret = device_probe(devp);
	if (ret) {
		printf("scu probe failed %d\n", ret);
		return ret;
	}

	return 0;
}

int print_bootinfo(void)
{
	enum boot_device bt_dev = get_boot_device();

	puts("Boot:  ");
	switch (bt_dev) {
	case SD1_BOOT:
		puts("SD0\n");
		break;
	case SD2_BOOT:
		puts("SD1\n");
		break;
	case SD3_BOOT:
		puts("SD2\n");
		break;
	case MMC1_BOOT:
		puts("MMC0\n");
		break;
	case MMC2_BOOT:
		puts("MMC1\n");
		break;
	case MMC3_BOOT:
		puts("MMC2\n");
		break;
	case FLEXSPI_BOOT:
		puts("FLEXSPI\n");
		break;
	case SATA_BOOT:
		puts("SATA\n");
		break;
	case NAND_BOOT:
		puts("NAND\n");
		break;
	case USB_BOOT:
		puts("USB\n");
		break;
	default:
		printf("Unknown device %u\n", bt_dev);
		break;
	}

	return 0;
}

enum boot_device get_boot_device(void)
{
	enum boot_device boot_dev = SD1_BOOT;

	sc_rsrc_t dev_rsrc;

	sc_misc_get_boot_dev(-1, &dev_rsrc);

	switch (dev_rsrc) {
	case SC_R_SDHC_0:
		boot_dev = MMC1_BOOT;
		break;
	case SC_R_SDHC_1:
		boot_dev = SD2_BOOT;
		break;
	case SC_R_SDHC_2:
		boot_dev = SD3_BOOT;
		break;
	case SC_R_NAND:
		boot_dev = NAND_BOOT;
		break;
	case SC_R_FSPI_0:
		boot_dev = FLEXSPI_BOOT;
		break;
	case SC_R_SATA_0:
		boot_dev = SATA_BOOT;
		break;
	case SC_R_USB_0:
	case SC_R_USB_1:
	case SC_R_USB_2:
		boot_dev = USB_BOOT;
		break;
	default:
		break;
	}

	return boot_dev;
}

#ifdef CONFIG_ENV_IS_IN_MMC
__weak int board_mmc_get_env_dev(int devno)
{
	return CONFIG_SYS_MMC_ENV_DEV;
}

int mmc_get_env_dev(void)
{
	sc_rsrc_t dev_rsrc;
	int devno;

	sc_misc_get_boot_dev(-1, &dev_rsrc);

	switch (dev_rsrc) {
	case SC_R_SDHC_0:
		devno = 0;
		break;
	case SC_R_SDHC_1:
		devno = 1;
		break;
	case SC_R_SDHC_2:
		devno = 2;
		break;
	default:
		/* If not boot from sd/mmc, use default value */
		return CONFIG_SYS_MMC_ENV_DEV;
	}

	return board_mmc_get_env_dev(devno);
}
#endif

#ifdef CONFIG_OF_SYSTEM_SETUP
static bool check_owned_resource(sc_rsrc_t rsrc_id)
{
	sc_ipc_t ipcHndl = 0;
	bool owned;

	ipcHndl = -1;

	owned = sc_rm_is_resource_owned(ipcHndl, rsrc_id);

	return owned;
}

static int disable_fdt_node(void *blob, int nodeoffset)
{
	int rc, ret;
	const char *status = "disabled";

	do {
		rc = fdt_setprop(blob, nodeoffset, "status", status, strlen(status) + 1);
		if (rc) {
			if (rc == -FDT_ERR_NOSPACE) {
				ret = fdt_increase_size(blob, 512);
				if (ret)
					return ret;
			}
		}
	} while (rc == -FDT_ERR_NOSPACE);

	return rc;
}

static void update_fdt_with_owned_resources(void *blob)
{
	/* Traverses the fdt nodes,
	  * check its power domain and use the resource id in the power domain
	  * for checking whether it is owned by current partition
	  */

	int offset = 0, next_off, addr;
	int depth, next_depth;
	unsigned int rsrc_id;
	const fdt32_t *php;
	const char *name;
	int rc;

	for (offset = fdt_next_node(blob, offset, &depth); offset > 0;
		 offset = fdt_next_node(blob, offset, &depth)) {

		debug("Node name: %s, depth %d\n", fdt_get_name(blob, offset, NULL), depth);

		if (!fdtdec_get_is_enabled(blob, offset)) {
			debug("   - ignoring disabled device\n");
			continue;
		}

		if (!fdt_node_check_compatible(blob, offset, "nxp,imx8-pd")) {
			/* Skip to next depth=1 node*/
			next_off = offset;
			next_depth = depth;
			do {
				offset = next_off;
				depth = next_depth;
				next_off = fdt_next_node(blob, offset, &next_depth);
				if (next_off < 0 || next_depth < 1)
					break;

				debug("PD name: %s, offset %d, depth %d\n",
					fdt_get_name(blob, next_off, NULL), next_off, next_depth);
			} while (next_depth > 1);

			continue;
		}

		php = fdt_getprop(blob, offset, "power-domains", NULL);
		if (!php) {
			debug("   - ignoring no power-domains\n");
		} else {
			addr = fdt_node_offset_by_phandle(blob, fdt32_to_cpu(*php));
			rsrc_id = fdtdec_get_uint(blob, addr, "reg", 0);

			if (rsrc_id == SC_R_LAST) {
				name = fdt_get_name(blob, offset, NULL);
				printf("%s's power domain use SC_R_LAST\n", name);
				continue;
			}

			debug("power-domains phandle 0x%x, addr 0x%x, resource id %u\n",
				fdt32_to_cpu(*php), addr, rsrc_id);

			if (!check_owned_resource(rsrc_id)) {

				/* If the resource is not owned, disable it in FDT */
				rc = disable_fdt_node(blob, offset);
				if (!rc)
					printf("Disable %s, resource id %u, pd phandle 0x%x\n",
						fdt_get_name(blob, offset, NULL), rsrc_id, fdt32_to_cpu(*php));
				else
					printf("Unable to disable %s, err=%s\n",
						fdt_get_name(blob, offset, NULL), fdt_strerror(rc));
			}
		}
	}
}
#endif

#ifdef CONFIG_IMX_SMMU
static int get_srsc_from_fdt_node_power_domain(void *blob, int device_offset)
{
	const fdt32_t *prop;
	int pdnode_offset;

	prop = fdt_getprop(blob, device_offset, "power-domains", NULL);
	if (!prop) {
		debug("node %s has no power-domains\n",
				fdt_get_name(blob, device_offset, NULL));
		return -ENOENT;
	}

	pdnode_offset = fdt_node_offset_by_phandle(blob, fdt32_to_cpu(*prop));
	if (pdnode_offset < 0) {
		pr_err("failed to fetch node %s power-domain",
				fdt_get_name(blob, device_offset, NULL));
		return pdnode_offset;
	}

	return fdtdec_get_uint(blob, pdnode_offset, "reg", -ENOENT);
}

static int config_smmu_resource_sid(int rsrc, int sid)
{
	sc_err_t err;

	err = sc_rm_set_master_sid(-1, rsrc, sid);
	debug("set_master_sid rsrc=%d sid=0x%x err=%d\n", rsrc, sid, err);
	if (err != SC_ERR_NONE) {
		pr_err("fail set_master_sid rsrc=%d sid=0x%x err=%d", rsrc, sid, err);
		return -EINVAL;
	}

	return 0;
}

static int config_smmu_fdt_device_sid(void *blob, int device_offset, int sid)
{
	int rsrc;
	const char *name = fdt_get_name(blob, device_offset, NULL);

	rsrc = get_srsc_from_fdt_node_power_domain(blob, device_offset);
	debug("configure node %s sid 0x%x rsrc=%d\n", name, sid, rsrc);
	if (rsrc < 0) {
		debug("failed to determine SC_R_* for node %s\n", name);
		return rsrc;
	}

	return config_smmu_resource_sid(rsrc, sid);
}

/* assign master sid based on iommu properties in fdt */
static int config_smmu_fdt(void *blob)
{
	int offset, proplen;
	const fdt32_t *prop;
	const char *name;

	offset = 0;
	while ((offset = fdt_next_node(blob, offset, NULL)) > 0)
	{
		name = fdt_get_name(blob, offset, NULL);
		prop = fdt_getprop(blob, offset, "iommus", &proplen);
		if (!prop)
			continue;
		debug("node %s iommus proplen %d\n", name, proplen);

		if (proplen == 12) {
			int sid = fdt32_to_cpu(prop[1]);
			config_smmu_fdt_device_sid(blob, offset, sid);
		} else if (proplen != 4) {
			debug("node %s ignore unexpected iommus proplen=%d\n", name, proplen);
		}
	}

	return 0;
}
#endif

#ifdef CONFIG_OF_SYSTEM_SETUP
int ft_system_setup(void *blob, bd_t *bd)
{
	update_fdt_with_owned_resources(blob);
#ifdef CONFIG_IMX_SMMU
	config_smmu_fdt(blob);
#endif

	return 0;
}
#endif

#define MEMSTART_ALIGNMENT  SZ_2M /* Align the memory start with 2MB */

static int get_owned_memreg(sc_rm_mr_t mr, sc_faddr_t *addr_start,
			    sc_faddr_t *addr_end)
{
	sc_faddr_t start, end;
	int ret;
	bool owned;

	owned = sc_rm_is_memreg_owned(-1, mr);
	if (owned) {
		ret = sc_rm_get_memreg_info(-1, mr, &start, &end);
		if (ret) {
			printf("Memreg get info failed, %d\n", ret);
			return -EINVAL;
		}
		debug("0x%llx -- 0x%llx\n", start, end);
		*addr_start = start;
		*addr_end = end;

		return 0;
	}

	return -EINVAL;
}

phys_size_t get_effective_memsize(void)
{
	sc_rm_mr_t mr;
	sc_faddr_t start, end, end1;
	int err;

	end1 = (sc_faddr_t)PHYS_SDRAM_1 + PHYS_SDRAM_1_SIZE;

	for (mr = 0; mr < 64; mr++) {
		err = get_owned_memreg(mr, &start, &end);
		if (!err) {
			start = roundup(start, MEMSTART_ALIGNMENT);
			/* Too small memory region, not use it */
			if (start > end)
				continue;

			/* Find the memory region runs the U-Boot */
			if (start >= PHYS_SDRAM_1 && start <= end1 &&
			    (start <= CONFIG_SYS_TEXT_BASE &&
			    end >= CONFIG_SYS_TEXT_BASE)) {
				if ((end + 1) <= ((sc_faddr_t)PHYS_SDRAM_1 +
				    PHYS_SDRAM_1_SIZE))
					return (end - PHYS_SDRAM_1 + 1);
				else
					return PHYS_SDRAM_1_SIZE;
			}
		}
	}

	return PHYS_SDRAM_1_SIZE;
}

int dram_init(void)
{
	sc_rm_mr_t mr;
	sc_faddr_t start, end, end1, end2;
	int err;

	end1 = (sc_faddr_t)PHYS_SDRAM_1 + PHYS_SDRAM_1_SIZE;
	end2 = (sc_faddr_t)PHYS_SDRAM_2 + PHYS_SDRAM_2_SIZE;
	for (mr = 0; mr < 64; mr++) {
		err = get_owned_memreg(mr, &start, &end);
		if (!err) {
			start = roundup(start, MEMSTART_ALIGNMENT);
			/* Too small memory region, not use it */
			if (start > end)
				continue;

			if (start >= PHYS_SDRAM_1 && start <= end1) {
				if ((end + 1) <= end1)
					gd->ram_size += end - start + 1;
				else
					gd->ram_size += end1 - start;
			} else if (start >= PHYS_SDRAM_2 && start <= end2) {
				if ((end + 1) <= end2)
					gd->ram_size += end - start + 1;
				else
					gd->ram_size += end2 - start;
			}
		}
	}

	/* If error, set to the default value */
	if (!gd->ram_size) {
		gd->ram_size = PHYS_SDRAM_1_SIZE;
		gd->ram_size += PHYS_SDRAM_2_SIZE;
	}
	return 0;
}

static void dram_bank_sort(int current_bank)
{
	phys_addr_t start;
	phys_size_t size;

	while (current_bank > 0) {
		if (gd->bd->bi_dram[current_bank - 1].start >
		    gd->bd->bi_dram[current_bank].start) {
			start = gd->bd->bi_dram[current_bank - 1].start;
			size = gd->bd->bi_dram[current_bank - 1].size;

			gd->bd->bi_dram[current_bank - 1].start =
				gd->bd->bi_dram[current_bank].start;
			gd->bd->bi_dram[current_bank - 1].size =
				gd->bd->bi_dram[current_bank].size;

			gd->bd->bi_dram[current_bank].start = start;
			gd->bd->bi_dram[current_bank].size = size;
		}
		current_bank--;
	}
}

int dram_init_banksize(void)
{
	sc_rm_mr_t mr;
	sc_faddr_t start, end, end1, end2;
	int i = 0;
	int err;

	end1 = (sc_faddr_t)PHYS_SDRAM_1 + PHYS_SDRAM_1_SIZE;
	end2 = (sc_faddr_t)PHYS_SDRAM_2 + PHYS_SDRAM_2_SIZE;

	for (mr = 0; mr < 64 && i < CONFIG_NR_DRAM_BANKS; mr++) {
		err = get_owned_memreg(mr, &start, &end);
		if (!err) {
			start = roundup(start, MEMSTART_ALIGNMENT);
			if (start > end) /* Small memory region, no use it */
				continue;

			if (start >= PHYS_SDRAM_1 && start <= end1) {
				gd->bd->bi_dram[i].start = start;

				if ((end + 1) <= end1)
					gd->bd->bi_dram[i].size =
						end - start + 1;
				else
					gd->bd->bi_dram[i].size = end1 - start;

				dram_bank_sort(i);
				i++;
			} else if (start >= PHYS_SDRAM_2 && start <= end2) {
				gd->bd->bi_dram[i].start = start;

				if ((end + 1) <= end2)
					gd->bd->bi_dram[i].size =
						end - start + 1;
				else
					gd->bd->bi_dram[i].size = end2 - start;

				dram_bank_sort(i);
				i++;
			}
		}
	}

	/* If error, set to the default value */
	if (!i) {
		gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
		gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;
		gd->bd->bi_dram[1].start = PHYS_SDRAM_2;
		gd->bd->bi_dram[1].size = PHYS_SDRAM_2_SIZE;
	}

	return 0;
}

static u64 get_block_attrs(sc_faddr_t addr_start)
{
	u64 attr = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) | PTE_BLOCK_NON_SHARE |
		PTE_BLOCK_PXN | PTE_BLOCK_UXN;

	if ((addr_start >= PHYS_SDRAM_1 &&
	     addr_start <= ((sc_faddr_t)PHYS_SDRAM_1 + PHYS_SDRAM_1_SIZE)) ||
	    (addr_start >= PHYS_SDRAM_2 &&
	     addr_start <= ((sc_faddr_t)PHYS_SDRAM_2 + PHYS_SDRAM_2_SIZE)))
		return (PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_OUTER_SHARE);

	return attr;
}

static u64 get_block_size(sc_faddr_t addr_start, sc_faddr_t addr_end)
{
	sc_faddr_t end1, end2;

	end1 = (sc_faddr_t)PHYS_SDRAM_1 + PHYS_SDRAM_1_SIZE;
	end2 = (sc_faddr_t)PHYS_SDRAM_2 + PHYS_SDRAM_2_SIZE;

	if (addr_start >= PHYS_SDRAM_1 && addr_start <= end1) {
		if ((addr_end + 1) > end1)
			return end1 - addr_start;
	} else if (addr_start >= PHYS_SDRAM_2 && addr_start <= end2) {
		if ((addr_end + 1) > end2)
			return end2 - addr_start;
	}

	return (addr_end - addr_start + 1);
}

#define MAX_PTE_ENTRIES 512
#define MAX_MEM_MAP_REGIONS 16

static struct mm_region imx8_mem_map[MAX_MEM_MAP_REGIONS];
struct mm_region *mem_map = imx8_mem_map;

void enable_caches(void)
{
	sc_rm_mr_t mr;
	sc_faddr_t start, end;
	int err, i;

	/* Create map for registers access from 0x1c000000 to 0x80000000*/
	imx8_mem_map[0].virt = 0x1c000000UL;
	imx8_mem_map[0].phys = 0x1c000000UL;
	imx8_mem_map[0].size = 0x64000000UL;
	imx8_mem_map[0].attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN | PTE_BLOCK_UXN;

	i = 1;
	for (mr = 0; mr < 64 && i < MAX_MEM_MAP_REGIONS; mr++) {
		err = get_owned_memreg(mr, &start, &end);
		if (!err) {
			imx8_mem_map[i].virt = start;
			imx8_mem_map[i].phys = start;
			imx8_mem_map[i].size = get_block_size(start, end);
			imx8_mem_map[i].attrs = get_block_attrs(start);
			i++;
		}
	}

	if (i < MAX_MEM_MAP_REGIONS) {
		imx8_mem_map[i].size = 0;
		imx8_mem_map[i].attrs = 0;
	} else {
		puts("Error, need more MEM MAP REGIONS reserved\n");
		icache_enable();
		return;
	}

	for (i = 0; i < MAX_MEM_MAP_REGIONS; i++) {
		debug("[%d] vir = 0x%llx phys = 0x%llx size = 0x%llx attrs = 0x%llx\n",
		      i, imx8_mem_map[i].virt, imx8_mem_map[i].phys,
		      imx8_mem_map[i].size, imx8_mem_map[i].attrs);
	}

	icache_enable();
	dcache_enable();
}

#if !CONFIG_IS_ENABLED(SYS_DCACHE_OFF)
u64 get_page_table_size(void)
{
	u64 one_pt = MAX_PTE_ENTRIES * sizeof(u64);
	u64 size = 0;

	/*
	 * For each memory region, the max table size:
	 * 2 level 3 tables + 2 level 2 tables + 1 level 1 table
	 */
	size = (2 + 2 + 1) * one_pt * MAX_MEM_MAP_REGIONS + one_pt;

	/*
	 * We need to duplicate our page table once to have an emergency pt to
	 * resort to when splitting page tables later on
	 */
	size *= 2;

	/*
	 * We may need to split page tables later on if dcache settings change,
	 * so reserve up to 4 (random pick) page tables for that.
	 */
	size += one_pt * 4;

	return size;
}
#endif

#define FUSE_MAC0_WORD0 708
#define FUSE_MAC0_WORD1 709
#define FUSE_MAC1_WORD0 710
#define FUSE_MAC1_WORD1 711

void imx_get_mac_from_fuse(int dev_id, unsigned char *mac)
{
	u32 word[2], val[2] = {};
	int i, ret;

	if (dev_id == 0) {
		word[0] = FUSE_MAC0_WORD0;
		word[1] = FUSE_MAC0_WORD1;
	} else {
		word[0] = FUSE_MAC1_WORD0;
		word[1] = FUSE_MAC1_WORD1;
	}

	for (i = 0; i < 2; i++) {
		ret = sc_misc_otp_fuse_read(-1, word[i], &val[i]);
		if (ret < 0)
			goto err;
	}

	mac[0] = val[0];
	mac[1] = val[0] >> 8;
	mac[2] = val[0] >> 16;
	mac[3] = val[0] >> 24;
	mac[4] = val[1];
	mac[5] = val[1] >> 8;

	debug("%s: MAC%d: %02x.%02x.%02x.%02x.%02x.%02x\n",
	      __func__, dev_id, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return;
err:
	printf("%s: fuse %d, err: %d\n", __func__, word[i], ret);
}

#ifdef CONFIG_IMX_SMMU
struct smmu_sid dev_sids[] = {
	{ SC_R_SDHC_0, 0x11, "SDHC0" },
	{ SC_R_SDHC_1, 0x11, "SDHC1" },
	{ SC_R_SDHC_2, 0x11, "SDHC2" },
	{ SC_R_ENET_0, 0x12, "FEC0" },
	{ SC_R_ENET_1, 0x12, "FEC1" },
};

sc_err_t imx8_config_smmu_sid(struct smmu_sid *dev_sids, int size)
{
	int i;
	sc_err_t sciErr = SC_ERR_NONE;

	if ((dev_sids == NULL) || (size <= 0))
		return SC_ERR_NONE;

	for (i = 0; i < size; i++) {
		sciErr = sc_rm_set_master_sid(-1,
					      dev_sids[i].rsrc,
					      dev_sids[i].sid);
		if (sciErr != SC_ERR_NONE) {
			printf("set master sid error\n");
			return sciErr;
		}
	}

	return SC_ERR_NONE;
}
#endif

void arch_preboot_os(void)
{
#ifdef CONFIG_IMX_SMMU
	sc_pm_set_resource_power_mode(-1, SC_R_SMMU, SC_PM_PW_MODE_ON);

	imx8_config_smmu_sid(dev_sids, ARRAY_SIZE(dev_sids));
#endif
}

u32 get_cpu_rev(void)
{
	u32 id = 0, rev = 0;
	int ret;

	ret = sc_misc_get_control(-1, SC_R_SYSTEM, SC_C_ID, &id);
	if (ret)
		return 0;

	rev = (id >> 5)  & 0xf;
	id = (id & 0x1f) + MXC_SOC_IMX8;  /* Dummy ID for chip */

	return (id << 12) | rev;
}

#if CONFIG_IS_ENABLED(CPU)
struct cpu_imx_platdata {
	const char *name;
	const char *rev;
	const char *type;
	u32 cpurev;
	u32 freq_mhz;
};

const char *get_imx8_type(u32 imxtype)
{
	switch (imxtype) {
	case MXC_CPU_IMX8QXP:
	case MXC_CPU_IMX8QXP_A0:
		return "QXP";
	case MXC_CPU_IMX8QM:
		return "QM";
	default:
		return "??";
	}
}

const char *get_imx8_rev(u32 rev)
{
	switch (rev) {
	case CHIP_REV_A:
		return "A";
	case CHIP_REV_B:
		return "B";
	default:
		return "?";
	}
}

const char *get_core_name(void)
{
	if (is_cortex_a35())
		return "A35";
	else if (is_cortex_a53())
		return "A53";
	else if (is_cortex_a72())
		return "A72";
	else
		return "?";
}

static int cpu_imx_get_temp(void)
{
	struct udevice *thermal_dev;
	int cpu_tmp, ret;

	ret = uclass_get_device_by_name(UCLASS_THERMAL, "cpu-thermal0",
					&thermal_dev);

	if (!ret) {
		ret = thermal_get_temp(thermal_dev, &cpu_tmp);
		if (ret)
			return 0xdeadbeef;
	} else {
		return 0xdeadbeef;
	}

	return cpu_tmp;
}

int cpu_imx_get_desc(struct udevice *dev, char *buf, int size)
{
	struct cpu_imx_platdata *plat = dev_get_platdata(dev);
	int ret;

	if (size < 100)
		return -ENOSPC;

	ret = snprintf(buf, size, "NXP i.MX8%s Rev%s %s at %u MHz",
		       plat->type, plat->rev, plat->name, plat->freq_mhz);

	if (IS_ENABLED(CONFIG_IMX_SCU_THERMAL)) {
		buf = buf + ret;
		size = size - ret;
		ret = snprintf(buf, size, " at %dC", cpu_imx_get_temp());
	}

	snprintf(buf + ret, size - ret, "\n");

	return 0;
}

static int cpu_imx_get_info(struct udevice *dev, struct cpu_info *info)
{
	struct cpu_imx_platdata *plat = dev_get_platdata(dev);

	info->cpu_freq = plat->freq_mhz * 1000;
	info->features = BIT(CPU_FEAT_L1_CACHE) | BIT(CPU_FEAT_MMU);
	return 0;
}

static int cpu_imx_get_count(struct udevice *dev)
{
	return 4;
}

static int cpu_imx_get_vendor(struct udevice *dev,  char *buf, int size)
{
	snprintf(buf, size, "NXP");
	return 0;
}

static const struct cpu_ops cpu_imx8_ops = {
	.get_desc	= cpu_imx_get_desc,
	.get_info	= cpu_imx_get_info,
	.get_count	= cpu_imx_get_count,
	.get_vendor	= cpu_imx_get_vendor,
};

static const struct udevice_id cpu_imx8_ids[] = {
	{ .compatible = "arm,cortex-a35" },
	{ .compatible = "arm,cortex-a53" },
	{ }
};

static ulong imx8_get_cpu_rate(void)
{
	ulong rate;
	int ret;
	int type = is_cortex_a35() ? SC_R_A35 : is_cortex_a53() ?
		   SC_R_A53 : SC_R_A72;

	ret = sc_pm_get_clock_rate(-1, type, SC_PM_CLK_CPU,
				   (sc_pm_clock_rate_t *)&rate);
	if (ret) {
		printf("Could not read CPU frequency: %d\n", ret);
		return 0;
	}

	return rate;
}

static int imx8_cpu_probe(struct udevice *dev)
{
	struct cpu_imx_platdata *plat = dev_get_platdata(dev);
	u32 cpurev;

	cpurev = get_cpu_rev();
	plat->cpurev = cpurev;
	plat->name = get_core_name();
	plat->rev = get_imx8_rev(cpurev & 0xFFF);
	plat->type = get_imx8_type((cpurev & 0xFF000) >> 12);
	plat->freq_mhz = imx8_get_cpu_rate() / 1000000;
	return 0;
}

U_BOOT_DRIVER(cpu_imx8_drv) = {
	.name		= "imx8x_cpu",
	.id		= UCLASS_CPU,
	.of_match	= cpu_imx8_ids,
	.ops		= &cpu_imx8_ops,
	.probe		= imx8_cpu_probe,
	.platdata_auto_alloc_size = sizeof(struct cpu_imx_platdata),
	.flags		= DM_FLAG_PRE_RELOC,
};
#endif
