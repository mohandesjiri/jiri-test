#ifndef _WORMHOLE_H_
#define _WORMHOLE_H_

#include "sol/types.h"
#include "utils.h"
#include "sol/entrypoint.h"
#include "sol/pubkey.h"
#include "ctx.h"

static inline u16 vaa_chain_id(const u8 *data) { return *(u16 *)(&data[57]); }
static inline u64 vaa_seq_id(const u8 *data) { return *(u64 *)(&data[49]); }
static inline const u8* vaa_emitter_addr_buf(const u8 *data)
{
	return data + 59;
}

// first u8 after vaa header (valid for non empty payloads)
static inline u8 vaa_payload_id(const u8 *data) { return *(u8 *)(&data[95]); }

// transfer specific vaa
static inline u8* vaa_transfer_tkn_addr(u8 *data) { return data + 128; }
static inline u16 vaa_transfer_chain_id(const u8 *data)
{
	return read_u16_be(data + 160);
}

// mayan specific vaa
static inline u8 *vaa_mayan_tkn_addr(u8 *data) { return data + 128; }
static inline u16 vaa_mayan_tkn_chain_id(const u8 *data)
{
	return read_u16_be(data + 160);
}
static inline const u8 *vaa_mayan_market1(const u8 *data)
{
	return data + 260;
}

static inline const u8 *vaa_mayan_market2(const u8 *data)
{
	return data + 292;
}

static inline u64 vaa_mayan_amount(u8 *data)
{
	return read_u64_be(data + 96 + 32 - 8);
}
static inline u64 vaa_mayan_amount_min(u8 *data)
{
	return read_u64_be(data + 324 + 32 - 8);
}
static inline const u8 *vaa_mayan_to_addr_buf(const u8 *data)
{
	return data + 162;
}
static inline u16 vaa_mayan_to_chain(const u8 *data)
{
	return read_u16_be(data + 194);
}

static inline u64 vaa_mayan_ref_seq_id(u8 *data)
{
	return read_u64_be(data + 356);
}

static inline u64 vaa_mayan_fee_swap(const u8 *data)
{
	return read_u64_be(data + 196 + 32 - 8);
}

static inline u64 vaa_mayan_fee_return(const u8 *data)
{
	return read_u64_be(data + 228 + 32 - 8);
}

static inline u64 vaa_mayan_deadline(const u8 *data)
{
	return read_u64_be(data + 364 + 32 - 8);
}

u64 check_vaa_pair(const SolAccountInfo *msg1, const SolAccountInfo *msg2);
u64 wh_check_msg_addr(const struct prog_ctx *ctx, const SolPubkey *msg,
		      u8 nonce, const u8 *hash);
u64 wh_check_claimed(const struct prog_ctx *ctx, const SolAccountInfo *msg,
                     u8 nonce, const SolAccountInfo *claim);

bool is_emitter_token_bridge(const u8 *vaa);
bool is_emitter_mayan_bridge(const u8 *vaa);

u64 wh_get_mint(const struct prog_ctx *ctx, const u8 *buf, u16 chain_id,
		u8 nonce, SolPubkey *result);


struct wh_transfer_acc {
	// wormhole static
	SolAccountInfo *config;
	SolAccountInfo *auth_signer;
	SolAccountInfo *custody_signer; // native
	SolAccountInfo *emitter;
	SolAccountInfo *bridge_conf;
	SolAccountInfo *seq_key;
	SolAccountInfo *fee_acc;

	// mint dependent
	SolAccountInfo *mint;
	SolAccountInfo *meta; // wrapped
	SolAccountInfo *custody; // native

	// transfer dependent
	SolAccountInfo *acc;
	SolAccountInfo *new_msg;

	SolPubkey *owner; // wrapped
	SolPubkey *payer;

	const u8 *address;
	u16 chain;
	u32 nonce;
	u64 fee;
	u64 relayer_fee;
	u64 amount;
};

static inline u64 parse_wh_trn_accounts(struct prog_ctx *ctx,
					struct wh_transfer_acc *transfer)
{
	transfer->config = ctx->cursor++;
	transfer->auth_signer = ctx->cursor++;
	transfer->custody_signer = ctx->cursor++;
	transfer->emitter = ctx->cursor++;
	transfer->bridge_conf = ctx->cursor++;
	transfer->seq_key = ctx->cursor++;
	transfer->fee_acc = ctx->cursor++;
	// mint dependent
	transfer->mint = ctx->cursor++;
	transfer->custody = ctx->cursor++;
	// transfer dependent
	transfer->acc = ctx->cursor++;
	transfer->new_msg = ctx->cursor++;

	transfer->nonce = *(u32 *)ctx->data_ptr;
	ctx->data_ptr += 4;
	transfer->fee = *(u64 *)ctx->data_ptr;
	ctx->data_ptr += 8;

	return SUCCESS;
}

u64 wh_transfer_native(struct prog_ctx *ctx, struct wh_transfer_acc *transfer);

static inline u64 parse_wh_trw_accounts(struct prog_ctx *ctx,
					struct wh_transfer_acc *transfer)
{
	// wormhole static
	transfer->config = ctx->cursor++;
	transfer->auth_signer = ctx->cursor++;
	transfer->emitter = ctx->cursor++;
	transfer->bridge_conf = ctx->cursor++;
	transfer->seq_key = ctx->cursor++;
	transfer->fee_acc = ctx->cursor++;
	// mint dependent
	transfer->mint = ctx->cursor++;
	transfer->meta = ctx->cursor++;
	// transfer dependent
	transfer->acc = ctx->cursor++;
	transfer->new_msg = ctx->cursor++;

	transfer->nonce = *(u32 *)ctx->data_ptr;
	ctx->data_ptr += 4;
	transfer->fee = *(u64 *)ctx->data_ptr;
	ctx->data_ptr += 8;

	return SUCCESS;
}

u64 wh_transfer_wrapped(struct prog_ctx *ctx, struct wh_transfer_acc *transfer);

static inline u64 wh_seq_id(const SolAccountInfo *acc, u64 *seq_id)
{
	if (acc->data_len != 8) {
		mayan_error("cannot read seq id");
		return ERROR_ACCOUNT_DATA_TOO_SMALL;
	}

	*seq_id = *(u64 *)acc->data;
	return SUCCESS;
}

#endif // _WORMHOLE_H_
