#ifndef _MAYAN_H_
#define  _MAYAN_H_

#include "sol/entrypoint.h"
#include "sol/pubkey.h"
#include "sol/types.h"
#include "utils.h"
#include "ctx.h"
#include "wormhole.h"
#include "dex.h"

enum mayan_state {
	STATE_NOT_INITIALIZED,
	STATE_CLAIMED,
	STATE_SWAP_DONE,
	STATE_SWAP_CANCELED,
	STATE_DONE_SWAPPED,
	STATE_DONE_NOT_SWAPPED,
};

#define MAYAN_STATE_DATA_SIZE 284

static inline void mayan_data_set_state(u8 *data, u8 state)
{
	data[0] = state;
}

static inline void mayan_data_set_amount(u8 *data, u64 amount)
{
	*(u64 *)(data + 65) = amount;
}

static inline void mayan_data_set_seq(u8 *data, u64 seq)
{
	*(u64 *)(data + 74) = seq;
}

static inline u8 mayan_data_state(const u8 *data)
{
	return data[0];
}

static inline const u8 *mayan_data_msg1(const u8 *data)
{
	return data + 1;
}

static inline const u8 *mayan_data_msg2(const u8 *data)
{
	return data + 33;
}

static inline const u8 *mayan_data_mint_from(const u8 *data)
{
	return data + 82;
}

static inline const u8 *mayan_data_mint_to(const u8 *data)
{
	return data + 114;
}

static inline const u8 *mayan_data_to_addr(const u8 *data)
{
	return data + 146;
}

static inline const u16 mayan_data_to_chain(const u8 *data)
{
	return *(u16 *)(data + 178);
}

static inline const u8 *mayan_data_market1(const u8 *data)
{
	return data + 180;
}

static inline const u8 *mayan_data_market2(const u8 *data)
{
	return data + 212;
}

static inline u64 mayan_data_amount(const u8 *data)
{
	return *(u64 *)(data + 65);
}

static inline u64 mayan_data_rate(const u8 *data)
{
	return *(u64 *)(data + 74);
}

static inline u8 mayan_data_decimal(const u8 *data)
{
	return *(data + 73);
}

static inline u64 mayan_data_fee_swap(const u8 *data)
{
	return *(u64 *)(data + 244);
}

static inline u64 mayan_data_fee_cancel(const u8 *data)
{
	return *(u64 *)(data + 252);
}

static inline u64 mayan_data_fee_return(const u8 *data)
{
	return *(u64 *)(data + 260);
}

static inline u64 mayan_data_deadline(const u8 *data)
{
	return *(u64 *)(data + 268);
}

static inline u64 mayan_data_amount_min(const u8 *data)
{
	return *(u64 *)(data + 276);
}

struct claim_acc {
	SolAccountInfo *owner;
	SolAccountInfo *msg1;
	SolAccountInfo *msg2;
	SolAccountInfo *final;
	SolAccountInfo *state;
	SolAccountInfo *main;

	SolAccountInfo *mint_from;
	SolAccountInfo *mint_to;

	u8 final_nonce;
	u8 state_nonce;
	u8 main_nonce;
	u8 mint_from_nonce;
	u8 mint_to_nonce;

	u8 state_val;
};

bool mayan_init_state(struct prog_ctx *ctx, struct claim_acc *mayan);

static const u8 final_seed[] = {'V', '3', 'S', 'T', 'A', 'T', 'E', 'f'};

bool validate_mint_accounts(struct prog_ctx *ctx, struct claim_acc *mayan);

static inline u64 parse_claim_accounts(struct prog_ctx *ctx,
				       struct claim_acc *mayan)
{
	u64 result;
	u8* msg1_buf;
	u8* msg2_buf;


	mayan_debug("parse mayan account");
	mayan->owner = ctx->cursor++;
	mayan->msg1 = ctx->cursor++;
	mayan->msg2 = ctx->cursor++;
	mayan->final = ctx->cursor++;
	mayan->state = ctx->cursor++;
	mayan->main = ctx->cursor++;

	mayan->mint_from = ctx->cursor++;
	mayan->mint_to = ctx->cursor++;

	mayan->final_nonce = *ctx->data_ptr;
	ctx->data_ptr++;
	mayan->state_nonce = *ctx->data_ptr;
	ctx->data_ptr++;
	mayan->main_nonce = *ctx->data_ptr;
	ctx->data_ptr++;

	mayan->mint_from_nonce = *ctx->data_ptr;
	ctx->data_ptr++;
	mayan->mint_to_nonce = *ctx->data_ptr;
	ctx->data_ptr++;

	if (!mayan->owner->is_signer) {
		mayan_error("owner is not signer");
		return ERROR_MISSING_REQUIRED_SIGNATURES;
	}

	// set global payer in context
	ctx->payer = mayan->owner->key;

	if (mayan->state->data_len != 0) {
		mayan_error("account already initialized");
		return ERROR_ACCOUNT_ALREADY_INITIALIZED;
	}

	if (mayan->final->data_len != 0) {
		mayan_error("final account already initialized");
		return ERROR_ACCOUNT_ALREADY_INITIALIZED;
	}

	msg1_buf = (u8 *)mayan->msg1->key;
	msg2_buf = (u8 *)mayan->msg2->key;

	// set seed
	set_ctx_seed(ctx, msg1_buf, msg2_buf, &mayan->state_nonce,
		     &mayan->main_nonce);

	// validate seed account
	result = ctx_check_seed_addr(ctx, mayan->state->key);
	if (result != SUCCESS) {
		mayan_error("cannot validate seed addr");
		return result;
	}

	// validate main account
	result = ctx_check_main_addr(ctx, mayan->main->key);
	if (result != SUCCESS) {
		mayan_error("cannot validate main addr");
		return result;
	}

	// validate mint accounts
	if (!validate_mint_accounts(ctx, mayan)) {
		mayan_error("mints are bad");
		return ERROR_CUSTOM_ZERO;
	}

	// TODO clean this section up
	// this is really bad. but whatever
	const SolSignerSeed fseeds[] = {
		{.addr=final_seed, .len=SOL_ARRAY_SIZE(final_seed)},
		{.addr=msg1_buf, .len=32},
		{.addr=msg2_buf, .len=32},
		{.addr=&mayan->final_nonce, .len=1},
	};
	SolPubkey tmp;
	result = sol_create_program_address(fseeds, SOL_ARRAY_SIZE(fseeds), ctx->prog_id,
				   &tmp);
	if (result != SUCCESS) {
		mayan_error("cannot create final addr");
		return result;
	}

	if (!SolPubkey_same(&tmp, mayan->final->key)) {
		mayan_error("final addr is wrong");
		return ERROR_CUSTOM_ZERO;
	}

	return SUCCESS;
}

struct close_acc {
	SolAccountInfo *msg1;
	SolAccountInfo *msg2;
	SolAccountInfo *state;

	u8 state_nonce;
	
	u8 main_nonce; // fake
};

static inline u64 parse_close_accounts(struct prog_ctx *ctx,
				       struct close_acc *close)
{
	u64 result;
	u8* msg1_buf;
	u8* msg2_buf;

	close->msg1 = ctx->cursor++;
	close->msg2 = ctx->cursor++;
	close->state = ctx->cursor++;
		
	close->state_nonce = *ctx->data_ptr;
	ctx->data_ptr++;

	close->main_nonce = 0;

	msg1_buf = (u8 *)close->msg1->key;
	msg2_buf = (u8 *)close->msg2->key;

	// set seed
	set_ctx_seed(ctx, msg1_buf, msg2_buf, &close->state_nonce,
		     &close->main_nonce);

	// validate seed account
	result = ctx_check_seed_addr(ctx, close->state->key);
	if (result != SUCCESS) {
		mayan_error("cannot validate seed addr");
		return result;
	}

	return SUCCESS;
}

struct swap_transitive_acc {
	SolAccountInfo *state;
	SolAccountInfo *main;

	struct serum_market m1;
	struct serum_market m2;
	struct serum_accs s_acc;

	u8 state_nonce;
	u8 main_nonce;

	u8 side; // only in simple
};

u64 parse_swap_x_accounts(struct prog_ctx *ctx,
                          struct swap_transitive_acc *swap, bool transitive);

struct transfer_acc {
	SolAccountInfo *owner;
	SolAccountInfo *state;
	SolAccountInfo *main;

	struct wh_transfer_acc transfer;

	u8 state_nonce;
	u8 main_nonce;

	bool try_cancel;
	u8 success_state;
};

u64 parse_transfer_accounts(struct prog_ctx *ctx, struct transfer_acc *trn,
			    bool is_wrapped);

#endif // _MAYAN_H_
