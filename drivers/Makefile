# SPDX-License-Identifier: GPL-2.0+

obj-$(CONFIG_BLK) += block/
obj-$(CONFIG_BOOTCOUNT_LIMIT) += bootcount/
obj-$(CONFIG_BUTTON) += button/
obj-$(CONFIG_CACHE) += cache/
obj-$(CONFIG_CLK) += clk/
obj-$(CONFIG_DM) += core/
obj-$(CONFIG_DMA) += dma/
obj-$(CONFIG_DMA_LEGACY) += dma/
obj-$(CONFIG_DFU) += dfu/
obj-$(CONFIG_GPIO) += gpio/
obj-$(CONFIG_DRIVERS_MISC) += misc/
obj-$(CONFIG_SYSRESET) += sysreset/
obj-$(CONFIG_FIRMWARE) +=firmware/
obj-$(CONFIG_I2C) += i2c/
obj-$(CONFIG_INPUT) += input/
obj-$(CONFIG_LED) += led/
obj-$(CONFIG_MMC) += mmc/
obj-y += mtd/
obj-$(CONFIG_MULTIPLEXER) += mux/
obj-$(CONFIG_ETH) += net/
obj-$(CONFIG_PCH) += pch/
obj-$(CONFIG_PCI) += pci/
obj-$(CONFIG_PHY) += phy/
obj-$(CONFIG_PINCTRL) += pinctrl/
obj-$(CONFIG_POWER) += power/
obj-$(CONFIG_RAM) += ram/
obj-$(CONFIG_RTC) += rtc/
obj-$(CONFIG_SERIAL) += serial/
obj-$(CONFIG_SPI) += spi/
obj-$(CONFIG_TIMER) += timer/
obj-$(CONFIG_VIRTIO) += virtio/
obj-$(CONFIG_DM_MAILBOX) += mailbox/
obj-$(CONFIG_REMOTEPROC) += remoteproc/
obj-$(CONFIG_SYSINFO) += sysinfo/
obj-$(CONFIG_TPM) += tpm/
obj-$(CONFIG_XEN) += xen/
obj-$(CONFIG_FPGA) += fpga/
obj-y += bus/

ifndef CONFIG_TPL_BUILD
ifndef CONFIG_VPL_BUILD
ifdef CONFIG_SPL_BUILD

obj-$(CONFIG_SPL_CPU) += cpu/
obj-$(CONFIG_SPL_CRYPTO) += crypto/
obj-$(CONFIG_SPL_MPC8XXX_INIT_DDR) += ddr/fsl/
obj-$(CONFIG_ARMADA_38X) += ddr/marvell/a38x/
obj-$(CONFIG_ARMADA_XP) += ddr/marvell/axp/
obj-$(CONFIG_ALTERA_SDRAM) += ddr/altera/
obj-$(CONFIG_ARCH_IMX8M) += ddr/imx/imx8m/
obj-$(CONFIG_IMX8ULP_DRAM) += ddr/imx/imx8ulp/
obj-$(CONFIG_ARCH_IMX9) += ddr/imx/imx9/
obj-$(CONFIG_SPL_DM_RESET) += reset/
obj-$(CONFIG_SPL_MUSB_NEW) += usb/musb-new/
obj-$(CONFIG_SPL_USB_GADGET) += usb/gadget/
obj-$(CONFIG_SPL_USB_GADGET) += usb/common/
obj-$(CONFIG_SPL_USB_GADGET) += usb/gadget/udc/
obj-$(CONFIG_SPL_WATCHDOG) += watchdog/
obj-$(CONFIG_SPL_USB_HOST) += usb/host/
obj-$(CONFIG_SPL_SATA) += ata/ scsi/
obj-$(CONFIG_SPL_LEGACY_BLOCK) += block/
obj-$(CONFIG_SPL_THERMAL) += thermal/

endif
endif
endif

ifdef CONFIG_TPL_BUILD

obj-$(CONFIG_TPL_MPC8XXX_INIT_DDR) += ddr/fsl/

endif

ifeq ($(CONFIG_SPL_BUILD)$(CONFIG_TPL_BUILD),)

obj-y += adc/
obj-y += ata/
obj-$(CONFIG_DM_DEMO) += demo/
obj-$(CONFIG_BIOSEMU) += bios_emulator/
obj-y += block/
obj-y += cache/
obj-$(CONFIG_CPU) += cpu/
obj-y += crypto/
obj-$(CONFIG_FASTBOOT) += fastboot/
obj-$(CONFIG_FWU_MDATA) += fwu-mdata/
obj-y += misc/
obj-$(CONFIG_MMC) += mmc/
obj-$(CONFIG_NVME) += nvme/
obj-$(CONFIG_PCI_ENDPOINT) += pci_endpoint/
obj-y += dfu/
obj-$(CONFIG_PCH) += pch/
obj-$(CONFIG_DM_REBOOT_MODE) += reboot-mode/
obj-y += rtc/
obj-y += scsi/
obj-y += sound/
obj-y += spmi/
obj-y += video/
obj-y += watchdog/
obj-$(CONFIG_QE) += qe/
obj-$(CONFIG_U_QE) += qe/
obj-y += mailbox/
obj-y += memory/
obj-y += mfd/
obj-y += mtd/
obj-y += pwm/
obj-y += reset/
obj-y += input/
obj-y += iommu/
# SOC specific infrastructure drivers.
obj-y += smem/
obj-y += thermal/
obj-$(CONFIG_TEE) += tee/
obj-y += axi/
obj-y += ufs/
obj-$(CONFIG_W1) += w1/
obj-$(CONFIG_W1_EEPROM) += w1-eeprom/

obj-$(CONFIG_MACH_PIC32) += ddr/microchip/
obj-$(CONFIG_FUZZ) += fuzz/
obj-$(CONFIG_DM_HWSPINLOCK) += hwspinlock/
obj-$(CONFIG_DM_RNG) += rng/
endif

obj-y += soc/
