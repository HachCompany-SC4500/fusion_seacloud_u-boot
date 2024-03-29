/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2016-2018 Toradex AG
 *
 * Configuration settings for the Colibri iMX7 module.
 *
 * based on mx7dsabresd.h:
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 */

#ifndef __COLIBRI_IMX7_CONFIG_H
#define __COLIBRI_IMX7_CONFIG_H

#include "mx7_common.h"

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		(32 * SZ_1M)

/* Network */
#define CONFIG_FEC_MXC
#define CONFIG_FEC_XCV_TYPE             RMII
#define CONFIG_ETHPRIME                 "FEC"
#define CONFIG_FEC_MXC_PHYADDR          0

#define CONFIG_TFTP_TSIZE

/* ENET1 */
#define IMX_FEC_BASE			ENET_IPS_BASE_ADDR

/* MMC Config*/
#define CONFIG_SYS_FSL_ESDHC_ADDR	0
#ifdef CONFIG_TARGET_COLIBRI_IMX7_NAND
#define CONFIG_SYS_FSL_USDHC_NUM	1
#elif CONFIG_TARGET_COLIBRI_IMX7_EMMC
#define CONFIG_SYS_FSL_USDHC_NUM	2
#endif

#undef CONFIG_BOOTM_PLAN9
#undef CONFIG_BOOTM_RTEMS

/* I2C configs */
#define CONFIG_SYS_I2C_MXC
#define CONFIG_SYS_I2C_SPEED		100000

#define CONFIG_IPADDR			192.168.10.2
#define CONFIG_NETMASK			255.255.255.0
#define CONFIG_SERVERIP			192.168.10.1

#define DUAL_BANK_VARIABLES \
	"bank_boot_attempt_count=0\0" \
	"bank_selection=a\0" \
	"bank_stable=true\0"

#define EMMC_BOOTCMD_HEADER1 \
	"run setup; " \
	"setenv bootargs ${defargs} "

#define EMMC_BOOTCMD_HEADER2 \
	"${setupargs} ${vidargs}; " \
	"echo Booting from internal eMMC chip...; " \
	"run m4boot && "

#define EMMC_BOOTCMD_FOOTER \
	"run fdt_fixup && bootz ${kernel_addr_r} - ${fdt_addr_r}\0"

	/*
	 * switch_to_stable_bank is called after detecting an issue with
	 * the newly flashed image. It recovers the old bootloader and its environment,
	 * which effectively selects the previous bank.
	 */
#define EMMC_STABLE_BANK_FALLBACK \
	"bootloader_block_count=0x400\0" \
	"primary_bootloader_start_block=0x2\0" \
	"secondary_bootloader_start_block=0x402\0" \
	"environment_block_count=0x10\0" \
	"primary_environment_start_block=0xfef\0" \
	"secondary_environment_start_block=0xfdf\0" \
	"switch_to_stable_bank=mmc dev 0 1; " \
		"mmc read ${loadaddr} ${secondary_bootloader_start_block} ${bootloader_block_count}; " \
		"mmc write ${loadaddr} ${primary_bootloader_start_block} ${bootloader_block_count}; " \
		"mmc read ${loadaddr} ${secondary_environment_start_block} ${environment_block_count}; " \
		"mmc write ${loadaddr} ${primary_environment_start_block} ${environment_block_count}; " \
		"reset\0"

#define EMMC_BOOTCMD \
	"emmcargs_bank_a=ip=off root=/dev/mmcblk0p2 ro rootfstype=ext4 rootwait\0" \
	"emmcboot_bank_a=" \
		EMMC_BOOTCMD_HEADER1 \
		"${emmcargs_bank_a} " \
		EMMC_BOOTCMD_HEADER2 \
		"load mmc 0:1 ${fdt_addr_r} ${soc}-colibri-emmc-${fdt_board}.dtb && " \
		"load mmc 0:1 ${kernel_addr_r} ${boot_file} && " \
		EMMC_BOOTCMD_FOOTER \
	"emmcargs_bank_b=ip=off root=/dev/mmcblk0p4 ro rootfstype=ext4 rootwait\0" \
	"emmcboot_bank_b=" \
		EMMC_BOOTCMD_HEADER1 \
		"${emmcargs_bank_b} " \
		EMMC_BOOTCMD_HEADER2 \
		"load mmc 0:3 ${fdt_addr_r} ${soc}-colibri-emmc-${fdt_board}.dtb && " \
		"load mmc 0:3 ${kernel_addr_r} ${boot_file} && " \
		EMMC_BOOTCMD_FOOTER \
	"emmcboot=if test ${bank_stable} = false; then " \
		"run handle_bank_counters; fi; " \
		"run emmcboot_select_bank\0" \
	"emmcboot_select_bank=if test ${bank_selection} = a; then " \
		"run emmcboot_bank_a; else " \
		"run emmcboot_bank_b; fi\0" \
	"handle_bank_counters=run test_bank_counters; " \
		"run bank_boot_attempt_count_inc\0" \
	"test_bank_counters=" \
		"if test ${bank_boot_attempt_count} != 0; then " \
		"run switch_to_stable_bank; fi\0" \
	EMMC_STABLE_BANK_FALLBACK \
	"bank_boot_attempt_count_inc=" \
		"setexpr bank_boot_attempt_count ${bank_boot_attempt_count} + 1; " \
		"saveenv\0"

#ifdef CONFIG_TDX_EASY_INSTALLER
#define FDT_HIGH_SETTING \
	""
#else
#define FDT_HIGH_SETTING \
	"fdt_high=0xffffffff\0"
#endif

#define MEM_LAYOUT_ENV_SETTINGS \
	"bootm_size=0x10000000\0" \
	"fdt_addr_r=0x82000000\0" \
	FDT_HIGH_SETTING \
	"initrd_high=0xffffffff\0" \
	"kernel_addr_r=0x81000000\0" \
	"pxefile_addr_r=0x87100000\0" \
	"ramdisk_addr_r=0x82100000\0" \
	"scriptaddr=0x87000000\0"

#if defined(CONFIG_TARGET_COLIBRI_IMX7_NAND)
#define SD_BOOTDEV 0
#elif defined(CONFIG_TARGET_COLIBRI_IMX7_EMMC)
#define SD_BOOTDEV 1
#endif

#define SD_BOOTCMD \
	"set_sdargs=setenv sdargs root=/dev/mmcblk1p2 ro rootwait\0" \
	"sdboot=run setup; run sdfinduuid; run set_sdargs; " \
	"setenv bootargs ${defargs} ${sdargs} " \
	"${setupargs} ${vidargs}; echo Booting from MMC/SD card...; " \
	"run m4boot && " \
	"load mmc ${sddev}:${sdbootpart} ${kernel_addr_r} ${kernel_file} && " \
	"load mmc ${sddev}:${sdbootpart} ${fdt_addr_r} " \
	"${soc}-colibri-${fdt_board}.dtb && " \
	"run fdt_fixup && bootz ${kernel_addr_r} - ${fdt_addr_r}\0" \
	"sdbootpart=1\0" \
	"sddev=" __stringify(SD_BOOTDEV) "\0" \
	"sdfinduuid=part uuid mmc ${sddev}:${sdrootpart} uuid\0" \
	"sdrootpart=2\0"


#define NFS_BOOTCMD \
	"nfsargs=ip=:::::eth0: root=/dev/nfs\0" \
	"nfsboot=run setup; " \
		"setenv bootargs ${defargs} ${nfsargs} " \
		"${setupargs} ${vidargs}; echo Booting from NFS...;" \
		"dhcp ${kernel_addr_r} && " \
		"tftp ${fdt_addr_r} ${soc}-colibri${variant}-${fdt_board}.dtb && " \
		"run fdt_fixup && bootz ${kernel_addr_r} - ${fdt_addr_r}\0" \

#define UBI_BOOTCMD	\
	"ubiargs=ubi.mtd=ubi root=ubi0:rootfs rootfstype=ubifs " \
		"ubi.fm_autoconvert=1\0" \
	"ubiboot=run setup; " \
		"setenv bootargs ${defargs} ${ubiargs} " \
		"${setupargs} ${vidargs}; echo Booting from NAND...; " \
		"ubi part ubi && run m4boot && " \
		"ubi read ${kernel_addr_r} kernel && " \
		"ubi read ${fdt_addr_r} dtb && " \
		"run fdt_fixup && bootz ${kernel_addr_r} - ${fdt_addr_r}\0" \

#if defined(CONFIG_TARGET_COLIBRI_IMX7_NAND)
#define CONFIG_BOOTCOMMAND "run ubiboot ; echo ; echo ubiboot failed ; " \
	"setenv fdtfile ${soc}-colibri-${fdt_board}.dtb && run distro_bootcmd;"
#define MODULE_EXTRA_ENV_SETTINGS \
	"mtdparts=" CONFIG_MTDPARTS_DEFAULT "\0" \
	UBI_BOOTCMD
#elif defined(CONFIG_TARGET_COLIBRI_IMX7_EMMC)
#define CONFIG_BOOTCOMMAND "run emmcboot ; echo ; echo emmcboot failed ; " \
	"setenv fdtfile ${soc}-colibri-emmc-${fdt_board}.dtb && run distro_bootcmd;"
#define MODULE_EXTRA_ENV_SETTINGS \
	"variant=-emmc\0" \
	EMMC_BOOTCMD
#endif

#if defined(CONFIG_TARGET_COLIBRI_IMX7_NAND)
#define BOOT_TARGET_DEVICES(func) \
	func(MMC, mmc, 0) \
	func(USB, usb, 0) \
	func(DHCP, dhcp, na)
#elif defined(CONFIG_TARGET_COLIBRI_IMX7_EMMC)
#define BOOT_TARGET_DEVICES(func) \
	func(MMC, mmc, 0) \
	func(MMC, mmc, 1) \
	func(USB, usb, 0) \
	func(DHCP, dhcp, na)
#endif
#include <config_distro_bootcmd.h>

/*
	videomode variable description:
		depth : color depth in bit per pixel
		pclk : clock in pico seconds
		le : left margin from sync to picture in pixelclocks
		ri : right margin from picture to sync in pixelclocks
		up : upper margin from sync to picture
		lo : lower margin from picture to sync
		hs : hsync length
		vs : vsync length
		sync : 0 sync low active, 1 hsync high active, 2 vsync high active
		vmode : 0 non interlaced
*/

#define VIDEOMODE_JIYA \
	"videomode=video=ctfb:x:320,y:240,depth:18,pclk:153846,le:58,ri:20,up:16,lo:4,hs:10,vs:2,sync:0,vmode:0\0"

#define VIDEOMODE_IPS \
	"videomode=video=ctfb:x:320,y:240,depth:18,pclk:166667,le:39,ri:8,up:8,lo:8,hs:4,vs:4,sync:0,vmode:0\0"


#define CONFIG_EXTRA_ENV_SETTINGS \
	BOOTENV \
	DUAL_BANK_VARIABLES \
	MEM_LAYOUT_ENV_SETTINGS \
	NFS_BOOTCMD \
	SD_BOOTCMD \
	MODULE_EXTRA_ENV_SETTINGS \
	"boot_file=zImage\0" \
	"console=ttymxc0\0" \
	"defargs=\0" \
	"fdt_board=1370\0" \
	"fdt_fixup=;\0" \
	"m4boot=;\0" \
	"ip_dyn=yes\0" \
	"kernel_file=zImage\0" \
	"setethupdate=if env exists ethaddr; then; else setenv ethaddr " \
		"00:14:2d:00:00:00; fi; tftpboot ${loadaddr} " \
		"${board}/flash_eth.img && source ${loadaddr}\0" \
	"setsdupdate=mmc rescan && setenv interface mmc && " \
		"fatload ${interface} 1:1 ${loadaddr} " \
		"${board}/flash_blk.img && source ${loadaddr}\0" \
	"setup=setenv setupargs " \
		"vt.global_cursor_default=0 console=${console}" \
		",${baudrate}n8 ${memargs} consoleblank=0; " \
		"setenv bootm_boot_mode sec\0" \
	"setupdate=run setsdupdate || run setusbupdate || run setethupdate\0" \
	"setusbupdate=usb start && setenv interface usb && " \
		"fatload ${interface} 0:1 ${loadaddr} " \
		"${board}/flash_blk.img && source ${loadaddr}\0" \
	"splashpos=m,m\0" \
	VIDEOMODE_IPS \
	"vidargs=\0" \
	"updlevel=2"

/* Miscellaneous configurable options */

#define CONFIG_SYS_MEMTEST_START	0x80000000
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_MEMTEST_START + 0x3c000000)

#define CONFIG_SYS_LOAD_ADDR		CONFIG_LOADADDR
#define CONFIG_SYS_HZ			1000

/* Physical Memory Map */
#define PHYS_SDRAM			MMDC0_ARB_BASE_ADDR

#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM
#define CONFIG_SYS_INIT_RAM_ADDR	IRAM_BASE_ADDR
#define CONFIG_SYS_INIT_RAM_SIZE	IRAM_SIZE

#define CONFIG_SYS_INIT_SP_OFFSET \
	(CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

/* environment organization */

#if defined(CONFIG_ENV_IS_IN_MMC)
#define CONFIG_SYS_NO_FLASH
/* Environment in eMMC, before config block at the end of 1st "boot sector" */
#define CONFIG_ENV_SIZE			(8 * 1024)

#define CONFIG_ENV_OFFSET		(-CONFIG_ENV_SIZE + \
					 CONFIG_TDX_CFG_BLOCK_OFFSET)
#define CONFIG_SECONDARY_ENV_OFFSET	(-CONFIG_ENV_SIZE + \
					 -CONFIG_ENV_SIZE + \
					 CONFIG_TDX_CFG_BLOCK_OFFSET)
#define CONFIG_SYS_MMC_ENV_DEV		0
#define CONFIG_SYS_MMC_ENV_PART		1
#elif defined(CONFIG_ENV_IS_IN_NAND)
#define CONFIG_ENV_SECT_SIZE		(128 * 1024)
#define CONFIG_ENV_OFFSET		(28 * CONFIG_ENV_SECT_SIZE)
#define CONFIG_ENV_SIZE			CONFIG_ENV_SECT_SIZE
#endif

#ifdef CONFIG_TARGET_COLIBRI_IMX7_NAND
/* NAND stuff */
#define CONFIG_SYS_MAX_NAND_DEVICE	1
#define CONFIG_SYS_NAND_BASE		0x40000000
#define CONFIG_SYS_NAND_5_ADDR_CYCLE
#define CONFIG_SYS_NAND_ONFI_DETECTION
#define CONFIG_SYS_NAND_MX7_GPMI_62_ECC_BYTES
#endif

/* USB Configs */
#define CONFIG_EHCI_HCD_INIT_AFTER_RESET

#define CONFIG_MXC_USB_PORTSC		(PORT_PTS_UTMI | PORT_PTS_PTW)
#define CONFIG_MXC_USB_FLAGS		0
#define CONFIG_USB_MAX_CONTROLLER_COUNT 2

#define CONFIG_IMX_THERMAL

#define CONFIG_USBD_HS

/* USB Device Firmware Update support */
#define CONFIG_SYS_DFU_DATA_BUF_SIZE	SZ_16M
#define DFU_DEFAULT_POLL_TIMEOUT	300

#if defined(CONFIG_VIDEO) || defined(CONFIG_DM_VIDEO)
#define CONFIG_VIDEO_MXS
#define CONFIG_VIDEO_LOGO
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN
#define CONFIG_BMP_16BPP
#define CONFIG_VIDEO_BMP_RLE8
#define CONFIG_VIDEO_BMP_LOGO
#endif

#endif
