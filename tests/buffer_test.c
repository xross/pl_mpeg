/* Buffer compaction preserves unread bytes, bit offsets and signalled EOF.
 * Exercise both sides of the memcpy threshold, partial final chunks and empty
 * buffers, with guards around the backing storage. No media fixture is needed.
 */
#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

#include <stdio.h>

#define CHECK(condition) do { \
	if (!(condition)) { \
		fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition); \
		return 1; \
	} \
} while (0)

int main(void) {
	enum { LENGTH = 1024, GUARD = 32 };
	uint8_t expected[LENGTH];
	uint8_t storage[LENGTH + 2 * GUARD];
	const int modes[] = {
		PLM_BUFFER_MODE_RING, PLM_BUFFER_MODE_APPEND,
		PLM_BUFFER_MODE_FIXED_MEM, PLM_BUFFER_MODE_FILE
	};
	for (size_t i = 0; i < LENGTH; ++i) {
		expected[i] = (uint8_t)(i * 73 + 19);
	}

	for (size_t mode = 0; mode < sizeof(modes) / sizeof(modes[0]); ++mode) {
		for (int signalled = 0; signalled <= 1; ++signalled) {
			for (size_t consumed = 0; consumed <= LENGTH; ++consumed) {
				for (size_t bits = 0; bits < 8; ++bits) {
					if (consumed == LENGTH && bits != 0) {
						continue;
					}
					memset(storage, 0xa5, sizeof(storage));
					memcpy(storage + GUARD, expected, LENGTH);
					plm_buffer_t buffer = {0};
					buffer.bytes = storage + GUARD;
					buffer.capacity = LENGTH;
					buffer.length = LENGTH;
					buffer.bit_index = consumed * 8 + bits;
					buffer.mode = modes[mode];
					buffer.total_size = signalled ? LENGTH : 0;

					plm_buffer_discard_read_bytes(&buffer);
					size_t remaining = LENGTH - consumed;
					int ring_end = signalled && buffer.mode == PLM_BUFFER_MODE_RING;
					CHECK(buffer.length == remaining);
					CHECK(buffer.bit_index == bits);
					CHECK(memcmp(buffer.bytes, expected + consumed, remaining) == 0);
					CHECK(buffer.total_size == (ring_end ? remaining : (signalled ? LENGTH : 0)));
					CHECK(buffer.has_ended == (ring_end && remaining == 0));
					for (size_t i = 0; i < GUARD; ++i) {
						CHECK(storage[i] == 0xa5);
						CHECK(storage[GUARD + LENGTH + i] == 0xa5);
					}
					/* Repeated compaction must not lose an already-recorded EOF. */
					plm_buffer_discard_read_bytes(&buffer);
					CHECK(buffer.has_ended == (ring_end && remaining == 0));
				}
			}
		}
	}
	puts("PASS: buffer bytes, bit offsets, guards and signalled EOF");
	return 0;
}
