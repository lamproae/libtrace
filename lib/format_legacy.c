/*
 * This file is part of libtrace
 *
 * Copyright (c) 2004 The University of Waikato, Hamilton, New Zealand.
 * Authors: Daniel Lawson 
 *          Perry Lorier 
 *          
 * All rights reserved.
 *
 * This code has been developed by the University of Waikato WAND 
 * research group. For further information please see http://www.wand.net.nz/
 *
 * libtrace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libtrace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libtrace; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id$
 *
 */
#define _GNU_SOURCE

#include "config.h"
#include "common.h"
#include "libtrace.h"
#include "libtrace_int.h"
#include "format_helper.h"
#include "parse_cmd.h"

#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef WIN32
#  include <io.h>
#  include <share.h>
#  define snprintf sprintf_s
#endif


/* Catch undefined O_LARGEFILE on *BSD etc */
#ifndef O_LARGEFILE
#  define O_LARGEFILE 0
#endif 

#define DATA(x) ((struct legacy_format_data_t *)x->format_data)

#define INPUT DATA(libtrace)->input

struct legacy_format_data_t {
	union {
                int fd;
		libtrace_io_t *file;
        } input;
};

static int legacyeth_get_framing_length(const libtrace_packet_t *packet UNUSED) 
{
	return sizeof(legacy_ether_t);
}

static int legacypos_get_framing_length(const libtrace_packet_t *packet UNUSED) 
{
	return sizeof(legacy_pos_t);
}

static int legacyatm_get_framing_length(const libtrace_packet_t *packet UNUSED) 
{
	return sizeof(legacy_cell_t);
}

static int erf_init_input(libtrace_t *libtrace) 
{
	libtrace->format_data = malloc(sizeof(struct legacy_format_data_t));

	return 0;
}

static int erf_start_input(libtrace_t *libtrace)
{
	if (DATA(libtrace)->input.file)
		return 0;

	DATA(libtrace)->input.file = trace_open_file(libtrace);

	if (DATA(libtrace)->input.file)
		return 0;

	return -1;
}

static int erf_fin_input(libtrace_t *libtrace) {
	libtrace_io_close(INPUT.file);
	free(libtrace->format_data);
	return 0;
}

static int legacy_read_packet(libtrace_t *libtrace, libtrace_packet_t *packet) {
	int numbytes;
	void *buffer;

	if (!packet->buffer || packet->buf_control == TRACE_CTRL_EXTERNAL) {
		packet->buf_control = TRACE_CTRL_PACKET;
		packet->buffer=malloc(LIBTRACE_PACKET_BUFSIZE);
	}
	buffer = packet->buffer;

	switch(libtrace->format->type) {
		case TRACE_FORMAT_LEGACY_ATM:
			packet->type = RT_DATA_LEGACY_ATM;
			break;
		case TRACE_FORMAT_LEGACY_POS:
			packet->type = RT_DATA_LEGACY_POS;
			break;
		case TRACE_FORMAT_LEGACY_ETH:
			packet->type = RT_DATA_LEGACY_ETH;
			break;
		default:
			assert(0);
	}
	
	if ((numbytes=libtrace_io_read(INPUT.file,
					buffer,
					64)) != 64) {
		if (numbytes!=0) {
			trace_set_err(libtrace,errno,"read(%s)",libtrace->uridata);
		}
		return numbytes;
	}
	
	packet->header = packet->buffer;
	packet->payload = (void*)((char*)packet->buffer + 
		libtrace->format->get_framing_length(packet));
	
	return 64;
	
}

static libtrace_linktype_t legacypos_get_link_type(const libtrace_packet_t *packet UNUSED) {
	return TRACE_TYPE_POS;
}

static libtrace_linktype_t legacyatm_get_link_type(const libtrace_packet_t *packet UNUSED) {
	return TRACE_TYPE_ATM;
}

static libtrace_linktype_t legacyeth_get_link_type(const libtrace_packet_t *packet UNUSED) {
	return TRACE_TYPE_ETH;
}

static int legacy_get_capture_length(const libtrace_packet_t *packet UNUSED) {
	return 64;
}

static int legacypos_get_wire_length(const libtrace_packet_t *packet) {
	legacy_pos_t *lpos = (legacy_pos_t *)packet->header;
	assert(ntohl(lpos->wlen)>0);
	return ntohl(lpos->wlen);
}

static int legacyatm_get_wire_length(const libtrace_packet_t *packet UNUSED) {
	return 53;
}

static int legacyeth_get_wire_length(const libtrace_packet_t *packet) {
	legacy_ether_t *leth = (legacy_ether_t *)packet->header;
	return ntohs(leth->wlen);
}

static uint64_t legacy_get_erf_timestamp(const libtrace_packet_t *packet)
{
	legacy_ether_t *legacy = (legacy_ether_t*)packet->header;
	return legacy->ts;
}  

static void legacypos_help() {
	printf("legacypos format module: $Revision$\n");
	printf("Supported input URIs:\n");
	printf("\tlegacypos:/path/to/file\t(uncompressed)\n");
	printf("\tlegacypos:/path/to/file.gz\t(gzip-compressed)\n");
	printf("\tlegacypos:-\t(stdin, either compressed or not)\n");
	printf("\n");
	printf("\te.g.: legacypos:/tmp/trace.gz\n");
	printf("\n");
}

static void legacyatm_help() {
	printf("legacyatm format module: $Revision$\n");
	printf("Supported input URIs:\n");
	printf("\tlegacyatm:/path/to/file\t(uncompressed)\n");
	printf("\tlegacyatm:/path/to/file.gz\t(gzip-compressed)\n");
	printf("\tlegacyatm:-\t(stdin, either compressed or not)\n");
	printf("\n");
	printf("\te.g.: legacyatm:/tmp/trace.gz\n");
	printf("\n");
}

static void legacyeth_help() {
	printf("legacyeth format module: $Revision$\n");
	printf("Supported input URIs:\n");
	printf("\tlegacyeth:/path/to/file\t(uncompressed)\n");
	printf("\tlegacyeth:/path/to/file.gz\t(gzip-compressed)\n");
	printf("\tlegacyeth:-\t(stdin, either compressed or not)\n");
	printf("\n");
	printf("\te.g.: legacyeth:/tmp/trace.gz\n");
	printf("\n");
}

static struct libtrace_format_t legacyatm = {
	"legacyatm",
	"$Id$",
	TRACE_FORMAT_LEGACY_ATM,
	erf_init_input,			/* init_input */	
	NULL,				/* config_input */
	erf_start_input,		/* start_input */
	NULL,				/* pause_input */
	NULL,				/* init_output */
	NULL,				/* config_output */
	NULL,				/* start_output */
	erf_fin_input,			/* fin_input */
	NULL,				/* fin_output */
	legacy_read_packet,		/* read_packet */
	NULL,				/* fin_packet */
	NULL,				/* write_packet */
	legacyatm_get_link_type,	/* get_link_type */
	NULL,				/* get_direction */
	NULL,				/* set_direction */
	legacy_get_erf_timestamp,	/* get_erf_timestamp */
	NULL,				/* get_timeval */
	NULL,				/* get_seconds */
	NULL,				/* seek_erf */
	NULL,				/* seek_timeval */
	NULL,				/* seek_seconds */
	legacy_get_capture_length,	/* get_capture_length */
	legacyatm_get_wire_length,	/* get_wire_length */
	legacyatm_get_framing_length,	/* get_framing_length */
	NULL,				/* set_capture_length */
	NULL,				/* get_fd */
	trace_event_trace,		/* trace_event */
	legacyatm_help,			/* help */
	NULL				/* next pointer */
};

static struct libtrace_format_t legacyeth = {
	"legacyeth",
	"$Id$",
	TRACE_FORMAT_LEGACY_ETH,
	erf_init_input,			/* init_input */	
	NULL,				/* config_input */
	erf_start_input,		/* start_input */
	NULL,				/* pause_input */
	NULL,				/* init_output */
	NULL,				/* config_output */
	NULL,				/* start_output */
	erf_fin_input,			/* fin_input */
	NULL,				/* fin_output */
	legacy_read_packet,		/* read_packet */
	NULL,				/* fin_packet */
	NULL,				/* write_packet */
	legacyeth_get_link_type,	/* get_link_type */
	NULL,				/* get_direction */
	NULL,				/* set_direction */
	legacy_get_erf_timestamp,	/* get_erf_timestamp */
	NULL,				/* get_timeval */
	NULL,				/* get_seconds */
	NULL,				/* seek_erf */
	NULL,				/* seek_timeval */
	NULL,				/* seek_seconds */
	legacy_get_capture_length,	/* get_capture_length */
	legacyeth_get_wire_length,	/* get_wire_length */
	legacyeth_get_framing_length,	/* get_framing_length */
	NULL,				/* set_capture_length */
	NULL,				/* get_fd */
	trace_event_trace,		/* trace_event */
	legacyeth_help,			/* help */
	NULL				/* next pointer */
};

static struct libtrace_format_t legacypos = {
	"legacypos",
	"$Id$",
	TRACE_FORMAT_LEGACY_POS,
	erf_init_input,			/* init_input */	
	NULL,				/* config_input */
	erf_start_input,		/* start_input */
	NULL,				/* pause_input */
	NULL,				/* init_output */
	NULL,				/* config_output */
	NULL,				/* start_output */
	erf_fin_input,			/* fin_input */
	NULL,				/* fin_output */
	legacy_read_packet,		/* read_packet */
	NULL,				/* fin_packet */
	NULL,				/* write_packet */
	legacypos_get_link_type,	/* get_link_type */
	NULL,				/* get_direction */
	NULL,				/* set_direction */
	legacy_get_erf_timestamp,	/* get_erf_timestamp */
	NULL,				/* get_timeval */
	NULL,				/* get_seconds */
	NULL,				/* seek_erf */
	NULL,				/* seek_timeval */
	NULL,				/* seek_seconds */
	legacy_get_capture_length,	/* get_capture_length */
	legacypos_get_wire_length,	/* get_wire_length */
	legacypos_get_framing_length,	/* get_framing_length */
	NULL,				/* set_capture_length */
	NULL,				/* get_fd */
	trace_event_trace,		/* trace_event */
	legacypos_help,			/* help */
	NULL,				/* next pointer */
};

	
void legacy_constructor() {
	register_format(&legacypos);
	register_format(&legacyeth);
	register_format(&legacyatm);
}
