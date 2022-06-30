#ifndef _MAYAN_HEADERS_H
#define _MAYAN_HEADERS_H
#include "sol/cpi.h"
#include "sol/entrypoint.h"
#include "sol/pubkey.h"
#include "sol/types.h"
#include <solana_sdk.h>

#define _AS_STRING_EXPAND_MACRO(x) #x
#define AS_STRING(x) _AS_STRING_EXPAND_MACRO(x)

#define DEBUG_FLAG
#define OVERFLOW_FLAG

#define max(a, b)                                                              \
	({                                                                     \
		__typeof__(a) _a = (a);                                        \
		__typeof__(b) _b = (b);                                        \
		_a > _b ? _a : _b;                                             \
	})

#define min(a, b)                                                              \
	({                                                                     \
		__typeof__(a) _a = (a);                                        \
		__typeof__(b) _b = (b);                                        \
		_a < _b ? _a : _b;                                             \
	})


typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

#define mayan_log_base(level, file, line, msg) sol_log(level "" file ":" AS_STRING(line) ": " msg)
#define mayan_error(msg) mayan_log_base("[ERROR] ", __FILE__, __LINE__, msg)

#ifdef DEBUG_FLAG
#define mayan_debug(msg) sol_log(__FILE__ ":" AS_STRING(__LINE__) ": " msg)
#define mayan_debug_64(u64_1, u64_2, u64_3, u64_4, u64_5) sol_log_64(u64_1, u64_2, u64_3, u64_4, u64_5)
#else
#define mayan_debug(msg)
#define mayan_debug_64(u64_1, u64_2, u64_3, u64_4, u64_5) 
#endif // DEBUG_FLAG

#define mayan_assert(value, msg) if (!(value)) {\
	mayan_error("assertation failed");\
	return ERROR_CUSTOM_ZERO;\
}

#ifdef OVERFLOW_FLAG
#define check_buffer_done(data, data_ptr) mayan_assert(data_ptr - data == SOL_ARRAY_SIZE(data), "data overflow")
#define check_buffer_overflow(data, data_ptr) mayan_assert(data_ptr - data <= SOL_ARRAY_SIZE(data), "data overflow")
#else
#define check_buffer_done(data, data_ptr) ;
#define check_buffer_overflow(data, data_ptr) ;
#endif


static inline void write_u8(u8 *data, u8 **data_ptr, u8 value)
{
	*(u8*)(*data_ptr) = value;
	(*data_ptr)++;
}

static inline void write_u16(u8 *data, u8 **data_ptr, u16 value)
{
	*(u16*)(*data_ptr) = value;
	(*data_ptr) += sizeof(u16);
}

static inline void write_u32(u8 *data, u8 **data_ptr, u32 value)
{
	*(u32*)(*data_ptr) = value;
	(*data_ptr) += sizeof(u32);
}

static inline void write_u64(u8 *data, u8 **data_ptr, u64 value)
{
	*(u64*)(*data_ptr) = value;
	(*data_ptr) += sizeof(u64);
}

static inline void write_i64(u8 *data, u8 **data_ptr, i64 value)
{
	*(i64*)(*data_ptr) = value;
	(*data_ptr) += sizeof(i64);
}

static inline void write_buffer(u8 *data, u8 **data_ptr, const u8 *buf,
				int size)
{
	for (int i = 0; i < size; ++i) {
		write_u8(data, data_ptr, *buf);
		buf++;
	}
}

static inline bool check_buffer_is_done(const u8 *data, const u8 *data_ptr, int size)
{
	return (data_ptr - data) == size;
}

static inline bool check_buffer_is_ok(const u8 *data, const u8 *data_ptr, int size)
{
	return (data_ptr - data) <= size;
}

static inline u64 read_u64_be(const u8* data)
{
	u64 result = 0;
	for (int i = 0; i < 8; ++i) {
		result *= 256;
		result += *data;
		++data;
	}
	return result;
}

static inline u16 read_u16_be(const u8* data)
{
	u16 result = 0;
	for (int i = 0; i < 2; ++i) {
		result *= 256;
		result += *data;
		++data;
	}
	return result;
}

static inline void write_u16_be(u8 *buffer, u16 x)
{
	buffer += 1;

	for (int i = 0; i < 2; ++i) {
		*buffer = x % 256;
		x /= 256;
		buffer--;
	}
}

static inline void write_u64_be(u8 *buffer, u64 x)
{
	buffer += 7;

	for (int i = 0; i < 8; ++i) {
		*buffer = x % 256;
		x /= 256;
		buffer--;
	}
}

inline u8 spl_get_decimals(const SolAccountInfo *mint)
{
	if (mint->data_len < 45)
		return 0;
	return mint->data[44];
}

// mayan power function
u64 mpow(u64 base, u64 exp);

/*
   this is really bad.
   TODO: maybe not inline?
 */
inline u64 decimal_pow(u8 decimals)
{
	switch (decimals) {
	case 0:
		return (long)1e0;
	case 1:
		return (long)1e1;
	case 2:
		return (long)1e2;
	case 3:
		return (long)1e3;
	case 4:
		return (long)1e4;
	case 5:
		return (long)1e5;
	case 6:
		return (long)1e6;
	case 7:
		return (long)1e7;
	case 8:
		return (long)1e8;
	case 9:
		return (long)1e9;
	default:
		return mpow(10, decimals);
	}
}

static inline bool buf_pubkey_same(const u8 *data, const SolPubkey *key)
{
	int i;
	const u8 *ref;

	ref = key->x;

	for (i = 0; i < 32; ++i) {
		if (*ref != *data)
			return false;
		ref++;
		data++;
	}

	return true;
}

inline static u64 get_token_amount(const SolAccountInfo* info, u64* result)
{
	sol_log("get toekn amount");
	if (info->data_len < 64 + 8) {
		return ERROR_ACCOUNT_DATA_TOO_SMALL;
	}
	u64 token_amount = *(u64*)(info->data + 64);
	*result = token_amount;
	return SUCCESS;
}

void mayan_log_buf_32(const u8* buf);

#endif // _MAYAN_HEADERS_H
