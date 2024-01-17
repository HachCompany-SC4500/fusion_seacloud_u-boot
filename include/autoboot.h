/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * Add to readline cmdline-editing by
 * (C) Copyright 2005
 * JinHua Luo, GuangDong Linux Center, <luo.jinhua@gd-linux.com>
 */

#ifndef __AUTOBOOT_H
#define __AUTOBOOT_H

#ifdef CONFIG_AUTOBOOT
/**
 * bootdelay_process() - process the bootd delay
 *
 * Process the boot delay, boot limit, then get the value of either
 * bootcmd, failbootcmd or altbootcmd depending on the current state.
 * Return this command so it can be executed.
 *
 * @return command to executed
 */
const char *bootdelay_process(void);

/**
 * autoboot_command() - run the autoboot command
 *
 * If enabled, run the autoboot command returned from bootdelay_process().
 * Also do the CONFIG_MENUKEY processing if enabled.
 *
 * @cmd: Command to run
 */
void autoboot_command(const char *cmd);

/**
 * autoboot_unmask_uboot_shell() - check unmasking the uboot shell post autoboot sequence
 *
 * If CONFIG_AUTOBOOT_UBOOT_SHELL_MASKING is eanbled unlocks uboot hush shell
 * once autoboot_command is interrupted
 *
 * @return wether uboot cli is unmasked
 */
int autoboot_unmask_uboot_shell(void);

/**
 * get_fuse_content() - wrapper function to read otp fuses
 *
 * @fuse_sha_passphrase: sha256 passphrase formed from fuse contents
 *
 * @return wether reading fuses and sha256 conversion was successfull
 */
int  get_fuse_content(uint8_t *fuse_sha_passphrase);
#else
static inline const char *bootdelay_process(void)
{
	return NULL;
}

static inline void autoboot_command(const char *s)
{
}
#endif

#endif
