#include "utils.h"

// https://stackoverflow.com/a/101613
u64 mpow(u64 base, u64 exp)
{
	u64 result = 1;
	for(;;) {
		if (exp & 1)
			result *= base;
		exp >>= 1;
		if (!exp)
			break;
		base *= base;
	}
	return result;
}

void mayan_log_buf_32(const u8* buf)
{
	u64 x1, x2, x3, x4;
	const u64 *buf_64 = (u64 *)buf;

	x1 = *buf_64;
	buf_64++;
	x2 = *buf_64;
	buf_64++;
	x3 = *buf_64;
	buf_64++;
	x4 = *buf_64;
	buf_64++;

	sol_log_64(x1, x2, x3, x4, 0);
}
