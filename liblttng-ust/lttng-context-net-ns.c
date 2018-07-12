/*
 * lttng-context-net-ns.c
 *
 * LTTng UST net namespace context.
 *
 * Copyright (C) 2009-2012 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *               2018 Michael Jeanson <mjeanson@efficios.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; only
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define _LGPL_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <lttng/ust-events.h>
#include <lttng/ust-tracer.h>
#include <lttng/ringbuffer-config.h>
#include <lttng/ust-tid.h>
#include <urcu/tls-compat.h>
#include "lttng-tracer-core.h"

/*
 * We cache the result to ensure we don't trigger a system call for
 * each event.
 */
static DEFINE_URCU_TLS(unsigned int, cached_net_ns);

static
unsigned int get_net_ns(void)
{
	if (caa_unlikely(!URCU_TLS(cached_net_ns))) {
		struct stat sb;

		/*
		 * /proc/thread-self was introduced in kernel v3.17
		 */
		if (stat("/proc/thread-self/ns/net", &sb) == 0) {
			URCU_TLS(cached_net_ns) = sb.st_ino;
		} else {
			char proc_ns_path[LTTNG_PROC_NS_PATH_MAX];

			if (snprintf(proc_ns_path, LTTNG_PROC_NS_PATH_MAX,
					"/proc/self/task/%d/ns/net",
					gettid()) < 0) {
				goto end;
			}
			if (stat(proc_ns_path, &sb) == 0) {
				URCU_TLS(cached_net_ns) = sb.st_ino;
			}
		}
	}
end:
	return URCU_TLS(cached_net_ns);
}

/*
 * The net namespace can change for 3 reasons
 *  * clone(2) called with CLONE_NEWNET
 *  * setns(2) called with the fd of a different net ns
 *  * unshare(2) called with CLONE_NEWNET
 */
void lttng_context_net_ns_reset(void)
{
	URCU_TLS(cached_net_ns) = 0;
}

static
size_t net_ns_get_size(struct lttng_ctx_field *field, size_t offset)
{
	size_t size = 0;

	size += lib_ring_buffer_align(offset, lttng_alignof(unsigned int));
	size += sizeof(unsigned int);
	return size;
}

static
void net_ns_record(struct lttng_ctx_field *field,
		 struct lttng_ust_lib_ring_buffer_ctx *ctx,
		 struct lttng_channel *chan)
{
	unsigned int net_ns;

	net_ns = get_net_ns();
	lib_ring_buffer_align_ctx(ctx, lttng_alignof(net_ns));
	chan->ops->event_write(ctx, &net_ns, sizeof(net_ns));
}

static
void net_ns_get_value(struct lttng_ctx_field *field,
		struct lttng_ctx_value *value)
{
	unsigned int net_ns;

	net_ns = get_net_ns();
	value->u.s64 = net_ns;
}

int lttng_add_net_ns_to_ctx(struct lttng_ctx **ctx)
{
	struct lttng_ctx_field *field;

	field = lttng_append_context(ctx);
	if (!field)
		return -ENOMEM;
	if (lttng_find_context(*ctx, "net_ns")) {
		lttng_remove_context_field(ctx, field);
		return -EEXIST;
	}
	field->event_field.name = "net_ns";
	field->event_field.type.atype = atype_integer;
	field->event_field.type.u.basic.integer.size = sizeof(unsigned int) * CHAR_BIT;
	field->event_field.type.u.basic.integer.alignment = lttng_alignof(unsigned int) * CHAR_BIT;
	field->event_field.type.u.basic.integer.signedness = lttng_is_signed_type(unsigned int);
	field->event_field.type.u.basic.integer.reverse_byte_order = 0;
	field->event_field.type.u.basic.integer.base = 10;
	field->event_field.type.u.basic.integer.encoding = lttng_encode_none;
	field->get_size = net_ns_get_size;
	field->record = net_ns_record;
	field->get_value = net_ns_get_value;
	lttng_context_update(*ctx);
	return 0;
}

/*
 *  * Force a read (imply TLS fixup for dlopen) of TLS variables.
 *   */
void lttng_fixup_net_ns_tls(void)
{
	asm volatile ("" : : "m" (URCU_TLS(cached_net_ns)));
}
