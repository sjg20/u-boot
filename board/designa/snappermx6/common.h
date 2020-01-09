/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2017 DENX Software Engineering
 * Lukasz Majewski, DENX Software Engineering, lukma@denx.de
 */

#ifndef __SNAPPERMX6_COMMON_H_
#define __SNAPPERMX6_COMMON_H_

#define UART_PAD_CTRL  (PAD_CTL_PKE | PAD_CTL_PUE |	       \
	PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED |	       \
	PAD_CTL_DSE_40ohm   | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define USDHC_PAD_CTRL (PAD_CTL_PKE | PAD_CTL_PUE |	       \
	PAD_CTL_PUS_47K_UP  | PAD_CTL_SPEED_LOW |	       \
	PAD_CTL_DSE_80ohm   | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define ENET_PAD_CTRL  (PAD_CTL_PKE | PAD_CTL_PUE |		\
	PAD_CTL_PUS_22K_UP | PAD_CTL_SPEED_MED	  |		\
	PAD_CTL_DSE_40ohm   | PAD_CTL_HYS)

#endif /* __SNAPPERMX6_COMMON_H_ */
