/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#ifndef _COBALT_H_
#define _COBALT_H_

/* #define PCI_SERIAL				1 */

#define BAUD_RATE						115200

#define LED_QUBE_LEFT				(1 << 0)
#define LED_QUBE_RIGHT				(1 << 1)
#define LED_RAQ_WEB					(1 << 2)
#define LED_RAQ_POWER_OFF			(1 << 3)

#define BUTTON_CLEAR					(1 << 1)
#define BUTTON_LEFT					(1 << 2)
#define BUTTON_UP						(1 << 3)
#define BUTTON_DOWN					(1 << 4)
#define BUTTON_RIGHT					(1 << 5)
#define BUTTON_ENTER					(1 << 6)
#define BUTTON_SELECT				(1 << 7)

#define BUTTON_MASK					(BUTTON_CLEAR | \
											BUTTON_LEFT | \
											BUTTON_UP | \
											BUTTON_DOWN | \
											BUTTON_RIGHT | \
											BUTTON_ENTER | \
											BUTTON_SELECT)
#endif

/* vi:set ts=3 sw=3 cin path=include,../include: */
