/*
 * lttng-context-pid-ns.c
 *
 * LTTng UST pid namespace context.
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

/*
 * We cache the result to ensure we don't trigger a system call for
 * each event. The PID namespace is global to the process.
 */
static unsigned int cached_pid_ns;

static
unsigned int get_pid_ns(void)
{
	if (caa_unlikely(!cached_pid_ns)) {
		struct stat sb;

		if (stat("/proc/self/ns/pid", &sb) == 0) {
			cached_pid_ns = sb.st_ino;
		}
	}

	return cached_pid_ns;
}

/*
 * A process's PID namespace membership is determined when the process is
 * created and cannot be changed thereafter.
 *
 * The pid namespace can change only on clone(2) / fork(2) :
 *  - clone(2) with the CLONE_NEWPID flag
 *  - clone(2) / fork(2) after a call to unshare(2) with the CLONE_NEWPID flag
 *  - clone(2) / fork(2) after a call to setns(2) with a PID namespace fd
 */
void lttng_context_pid_ns_reset(void)
{
	cached_pid_ns = 0;
}

static
size_t pid_ns_get_size(struct lttng_ctx_field *field, size_t offset)
{
	size_t size = 0;

	size += lib_ring_buffer_align(offset, lttng_alignof(unsigned int));
	size += sizeof(unsigned int);
	return size;
}

static
void pid_ns_record(struct lttng_ctx_field *field,
		 struct lttng_ust_lib_ring_buffer_ctx *ctx,
		 struct lttng_channel *chan)
{
	unsigned int pid_ns;

	pid_ns = get_pid_ns();
	lib_ring_buffer_align_ctx(ctx, lttng_alignof(pid_ns));
	chan->ops->event_write(ctx, &pid_ns, sizeof(pid_ns));
}

static
void pid_ns_get_value(struct lttng_ctx_field *field,
		struct lttng_ctx_value *value)
{
	unsigned int pid_ns;

	pid_ns = get_pid_ns();
	value->u.s64 = pid_ns;
}

int lttng_add_pid_ns_to_ctx(struct lttng_ctx **ctx)
{
	struct lttng_ctx_field *field;

	field = lttng_append_context(ctx);
	if (!field)
		return -ENOMEM;
	if (lttng_find_context(*ctx, "pid_ns")) {
		lttng_remove_context_field(ctx, field);
		return -EEXIST;
	}
	field->event_field.name = "pid_ns";
	field->event_field.type.atype = atype_integer;
	field->event_field.type.u.basic.integer.size = sizeof(unsigned int) * CHAR_BIT;
	field->event_field.type.u.basic.integer.alignment = lttng_alignof(unsigned int) * CHAR_BIT;
	field->event_field.type.u.basic.integer.signedness = lttng_is_signed_type(unsigned int);
	field->event_field.type.u.basic.integer.reverse_byte_order = 0;
	field->event_field.type.u.basic.integer.base = 10;
	field->event_field.type.u.basic.integer.encoding = lttng_encode_none;
	field->get_size = pid_ns_get_size;
	field->record = pid_ns_record;
	field->get_value = pid_ns_get_value;
	lttng_context_update(*ctx);
	return 0;
}
