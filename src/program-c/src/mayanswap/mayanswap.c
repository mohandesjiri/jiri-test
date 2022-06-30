#include "dex.h"
#include "sol/entrypoint.h"
#include "sol/pubkey.h"
#include "sol/types.h"
#include "spl.h"
#include "wormhole.h"
#include <solana_sdk.h>

#define DEBUG_FLAG
#define OVERFLOW_FLAG

#include "ctx.h"
#include "utils.h"
#include "mayan.h"
#include "build-info.h"

static inline u8 read_u8(struct prog_ctx *ctx)
{
	u8 res;
	res = *ctx->data_ptr;
	++ctx->data_ptr;
	return res;
}

static inline const u8* read_buffer(struct prog_ctx *ctx, int len)
{
	const u8* ptr = ctx->data_ptr;
	ctx->data_ptr += len;
	return ptr;
}

static inline u64 check_cursors(struct prog_ctx *ctx)
{
	mayan_debug("check cursors");
	mayan_debug_64((u64)ctx->data_ptr, (u64)ctx->params->data,
		       ctx->data_ptr - ctx->params->data, 0,
		       ctx->params->data_len);
	mayan_debug_64((u64)(ctx->cursor), (u64)(ctx->params->ka),
		       ctx->cursor - ctx->params->ka, 0, ctx->params->ka_num);

	// TODO check cursor
	if ((ctx->data_ptr - ctx->params->data) > ctx->params->data_len) {
		mayan_error("not enough arg");
		return ERROR_INVALID_ARGUMENT;
	}

	if ((ctx->cursor - ctx->params->ka) > ctx->params->ka_num) {
		mayan_error("not enough accounts");
		return ERROR_NOT_ENOUGH_ACCOUNT_KEYS;
	}

	return SUCCESS;
}

static inline u64 validate_vaas(const SolAccountInfo *msg1,
				const SolAccountInfo *msg2)
{
	u64 result;

	mayan_debug("check vaa pair");
	result = check_vaa_pair(msg1, msg2);
	if (result != SUCCESS)
		return result;

	if (!is_emitter_token_bridge(msg1->data)) {
		mayan_error("msg1 is not from token bridge");
		return ERROR_INVALID_ACCOUNT_DATA;
	}

	if (!is_emitter_mayan_bridge(msg2->data)) {
		mayan_error("msg2 is not from mayan bridge");
		return ERROR_INVALID_ACCOUNT_DATA;
	}

	if (vaa_payload_id(msg1->data) != 1) {
		mayan_error("msg1 is not transfer token");
		return ERROR_INVALID_ACCOUNT_DATA;
	}

	if (vaa_payload_id(msg2->data) != 1) {
		mayan_error("msg2 is not swap token");
		return ERROR_INVALID_ACCOUNT_DATA;
	}
	return SUCCESS;
}


static u64 mayan_claim(struct prog_ctx *ctx)
{
	struct claim_acc mayan;
	SolAccountInfo *claim;
	u64 result;
	u8 nonce1;
	u8 nonce2;
	u8 claim_nonce;
	const u8* hash1;
	const u8* hash2;

	mayan_debug("mayan claim");

	result = parse_claim_accounts(ctx, &mayan);
	if (result != SUCCESS)
		return result;

	nonce1 = read_u8(ctx);
	nonce2 = read_u8(ctx);
	hash1 = read_buffer(ctx, 32);
	hash2 = read_buffer(ctx, 32);

	claim = ctx->cursor++;
	claim_nonce = read_u8(ctx);

	result = parse_rent_account(ctx, &ctx->rent);
	if (result != SUCCESS)
		return result;

	// check cursor
	result = check_cursors(ctx);
	if (result != SUCCESS)
		return result;
	
	mayan_debug("checks");
	result = validate_vaas(mayan.msg1, mayan.msg2);
	if (result != SUCCESS)
		return result;

	mayan_debug(" >> msg1");
	result = wh_check_msg_addr(ctx, mayan.msg1->key, nonce1, hash1);
	if (result != SUCCESS)
		return result;
	
	mayan_debug(" >> msg2");
	result = wh_check_msg_addr(ctx, mayan.msg2->key, nonce2, hash2);
	if (result != SUCCESS)
		return result;
	
	result = wh_check_claimed(ctx, mayan.msg1, claim_nonce, claim);
	if (result != SUCCESS)
		return result;

	mayan_debug("let's claim");
	if (!mayan_init_state(ctx, &mayan)) {
		mayan_error("cannot initialize state");
		return ERROR_CUSTOM_ZERO;
	}

	return SUCCESS;
}

static u64 mayan_swap_x(struct prog_ctx *ctx, bool transitive)
{
	struct swap_transitive_acc swap = {0};

	u64 result;

	u64 before;
	u64 after;
	u64 diff;
	
	u64 amount = 0;
	u64 amount_min = 0;
	u64 fee = 0;
	u64 rate = 0;
	u8 decimal = 0;

	if (transitive) {
		mayan_debug("mayan swap transitive");
	} else {
		mayan_debug("mayan swap simple");
	}

	result = parse_swap_x_accounts(ctx, &swap, transitive);
	if (result != SUCCESS)
		return result;

	result = parse_rent_account(ctx, &ctx->rent);
	if (result != SUCCESS)
		return result;

	// check cursor
	result = check_cursors(ctx);
	if (result != SUCCESS)
		return result;


	mayan_debug("get before!");
	result = spl_get_amount(swap.s_acc.to, &before);
	if (result != SUCCESS)
		return result;
	
	mayan_debug("calcs!");
	amount = mayan_data_amount(swap.state->data);
	amount_min = mayan_data_amount_min(swap.state->data);

	rate = mayan_data_rate(swap.state->data);
	decimal = mayan_data_decimal(swap.state->data);
	fee = mayan_data_fee_swap(swap.state->data);

	mayan_debug_64(amount, 0, rate, decimal, fee);

	if (fee >= amount) {
		mayan_error("fee > amount!");
		// TODO change this to permission to cancel error
		return ERROR_CUSTOM_ZERO; 
	}

	amount -= fee;
	mayan_debug_64(amount, 0, before, 0, 0);

	mayan_debug("swap!");

	if (transitive) {
		result = dex_swap_transitive(ctx, &swap.m1, &swap.m2,
		                             &swap.s_acc, amount,
		                             rate, decimal);
	} else {
		result = dex_swap_simple(ctx, &swap.m1, &swap.s_acc,
		                         swap.side, amount, rate,
		                         decimal);
	}

	if (result != SUCCESS) {
		mayan_debug("swap returned error!");
		return result;
	}

	mayan_debug("get after!");
	result = spl_get_amount(swap.s_acc.to, &after);
	if (result != SUCCESS)
		return result;
	diff = after - before;

	mayan_debug("swap amount:");
	mayan_debug_64(before, after, 0, 0, diff);

	if (diff == 0) {
		mayan_error("in pool chera 0 e?");
		mayan_error("probably slippage violation!");
		return ERROR_CUSTOM_ZERO;
	}

	if (diff < amount_min) {
		mayan_error("Slippage violation");
		mayan_debug_64(amount, amount_min, before, after, diff);
		return ERROR_CUSTOM_ZERO;
	}

	mayan_debug("setting state");
	mayan_data_set_state(swap.state->data, STATE_SWAP_DONE);
	mayan_data_set_amount(swap.state->data, diff);

	mayan_debug("Everythin's fine! done!");
	return SUCCESS;
}

static bool can_cancel(struct prog_ctx *ctx, struct transfer_acc *trx)
{
	u64 now;
	u64 deadline;

	mayan_debug("can cancel?");
	if (!trx->try_cancel) {
		mayan_debug("no try cancel");
		return false;
	}

	now = ctx->clock.posix;
	deadline = mayan_data_deadline(trx->state->data);

	mayan_debug(" times?");
	mayan_debug_64(0, now, 0, deadline, now > deadline);

	if (now > deadline) {
		mayan_debug("deadline expired. you can cancel!");
		return true;
	}

	mayan_debug("wait for deadline, please.");
	return false;
}

static u64 mayan_trx(struct prog_ctx *ctx, bool is_wrapped)
{
	struct transfer_acc trx = {0};

	u64 amount;
	u64 result;
	u64 seq_id;

	mayan_debug("mayan transfer native");
	result = parse_transfer_accounts(ctx, &trx, is_wrapped);
	if (result != SUCCESS)
		return result;

	result = parse_rent_account(ctx, &ctx->rent);
	if (result != SUCCESS)
		return result;

	result = parse_clock_account(ctx, &ctx->clock);
	if (result != SUCCESS)
		return result;

	// check cursor
	result = check_cursors(ctx);
	if (result != SUCCESS)
		return result;

	// can cancel?
	if (trx.try_cancel && !can_cancel(ctx, &trx)) {
		mayan_error("you cannot cancel. sorry!");
		return ERROR_CUSTOM_ZERO;
	}

	// set amount
	amount = trx.transfer.amount;
	mayan_debug("amounts");
	mayan_debug_64(amount, 0, 0, trx.transfer.fee, trx.transfer.relayer_fee);

	mayan_debug("transfer wormhole first fee");
	result = system_transfer(ctx, trx.owner->key, trx.transfer.fee_acc->key,
				 trx.transfer.fee);
	if (result != SUCCESS) {
		mayan_debug("cannot transfer fee");
		return result;
	}

	mayan_debug("spl approve");
	result = spl_approve(ctx, trx.main->key, trx.transfer.acc->key,
			     trx.transfer.auth_signer->key, amount);

	if (result != SUCCESS) {
		mayan_debug("cannot approve transfer");
		return result;
	}

	mayan_debug("transfer!");
	if (is_wrapped) {
		result = wh_transfer_wrapped(ctx, &trx.transfer);
	} else {
		result = wh_transfer_native(ctx, &trx.transfer);
	}
	
	if (result != SUCCESS) {
		mayan_debug("transfer returned error!");
		return result;
	}

	result = wh_seq_id(trx.transfer.seq_key, &seq_id);

	if (result != SUCCESS) {
		mayan_debug("cannot get seq id");
		return result;
	}

	mayan_debug("setting state");
	mayan_data_set_state(trx.state->data, trx.success_state);
	mayan_data_set_seq(trx.state->data, seq_id);

	mayan_debug("Everythin's fine!");
	return SUCCESS;
}


static u64 mayan_test(struct prog_ctx *ctx)
{
	struct close_acc close;
	SolAccountInfo *owner;

	u64 result;

	u8 state;
	u64 lamports;
	
	mayan_debug("mayan test");
	result = parse_close_accounts(ctx, &close);
	if (result != SUCCESS) {
		mayan_error("cannot parse accounts");
		return result;
	}
	owner = ctx->cursor++;

	if (!owner->is_signer) {
		mayan_error("owner is not signer");
		return ERROR_CUSTOM_ZERO;
	}

	mayan_debug(">>");
	lamports = *close.state->lamports;
	mayan_debug_64(close.state->data_len, lamports, 0, 0, 0);

	state = mayan_data_state(close.state->data);
	mayan_debug_64(state, 0, 0, 0, 0);

	//result = system_transfer(ctx, close.state->key, owner->key, lamports);

	*owner->lamports += lamports;
	*close.state->lamports = 0;
	
	if (result != SUCCESS) {
		mayan_error("cannot transfer lamports");
		return result;
	}

	return SUCCESS;
}

#define MAXIMUM_KA_NUM 30
extern u64 entrypoint(const uint8_t *input)
{
	sol_log(BUILD_TEXT);

	SolAccountInfo accounts[MAXIMUM_KA_NUM];
	SolParameters params = (SolParameters){.ka = accounts};
	struct prog_ctx ctx = {0};
	
	if (!sol_deserialize(input, &params, SOL_ARRAY_SIZE(accounts))) {
		return ERROR_INVALID_ARGUMENT;
	}

	if (params.data_len < sizeof(u8)) {
		mayan_error("too small to hold instruction enum value");
		return ERROR_INVALID_ARGUMENT;
	}

	u8 instruction = *((u8 *)params.data);
	ctx_init(&ctx, &params);

	switch (instruction) {
	case 50:
		return mayan_test(&ctx);
	case 100:
		return mayan_claim(&ctx);
	case 110:
		return mayan_swap_x(&ctx, true);
	case 111:
		return mayan_swap_x(&ctx, false);
	case 120:
		return mayan_trx(&ctx, false);
	case 121:
		return mayan_trx(&ctx, true);
	default:
		return ERROR_INVALID_INSTRUCTION_DATA;
	}
}
