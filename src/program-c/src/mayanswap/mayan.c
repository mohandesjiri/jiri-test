#include "mayan.h"
#include "sol/entrypoint.h"
#include "sol/pubkey.h"
#include "sol/string.h"
#include "sol/types.h"
#include "spl.h"
#include "utils.h"

bool validate_mint_accounts(struct prog_ctx *ctx, struct claim_acc *mayan)
{
	SolPubkey from;
	SolPubkey to;
	u8* addr_from = vaa_transfer_tkn_addr(mayan->msg1->data);
	u8* addr_to = vaa_mayan_tkn_addr(mayan->msg2->data);
	u64 result;

	u16 chain_from = vaa_transfer_chain_id(mayan->msg1->data);
	u16 chain_to = vaa_mayan_tkn_chain_id(mayan->msg2->data);

	u8 n1 = mayan->mint_from_nonce;
	u8 n2 = mayan->mint_to_nonce;

	mayan_debug("validate mint accounts");
	mayan_debug_64(chain_from, chain_to, 0, n1, n2);

	mayan_debug("> from");
	result = wh_get_mint(ctx, addr_from, chain_from, n1, &from);
	if (result != SUCCESS) {
		mayan_error("cannot parse `from` mint");
		return false;
	}

	if (!SolPubkey_same(&from, mayan->mint_from->key)) {
		mayan_error("`from` mint key mismatches");
		return false;
	}

	mayan_debug("> to");
	result = wh_get_mint(ctx, addr_to, chain_to, n2, &to);
	if (result != SUCCESS) {
		mayan_error("cannot parse `to` mint");
		return false;
	}

	if (!SolPubkey_same(&to, mayan->mint_to->key)) {
		mayan_error("`to` mint key mismatches");
		return false;
	}

	mayan_debug("mints were fine (for now)!");

	return true;
}

static inline u64 denormalize_amount(u64 amount, const SolAccountInfo *mint)
{
	u8 decimal = spl_get_decimals(mint);
	if (decimal > 8) {
		mayan_debug(">> amount denormalizing!!!");
		amount *= decimal_pow(decimal - 8);
		mayan_debug_64(decimal, 0, decimal_pow(decimal - 8), 0, amount);
	}
	return amount;
}

#define MAYAN_STATE_TMP_SIZE (MAYAN_STATE_DATA_SIZE + 20)
bool mayan_init_state(struct prog_ctx *ctx, struct claim_acc *mayan)
{
	u64 amount;
	u64 amount_min;

	u64 fee_swap;
	u64 fee_cancel;
	u64 fee_return;
	u64 deadline;

	u8 *data;
	u8 *data_ptr;

	const u8 *msg1;
	const u8 *msg2;
	const u8 *market1;
	const u8 *market2;
	const u8 *mint_from;
	const u8 *mint_to;
	const u8 *to_addr;
	u16 to_chain;

	u64 rate;
	u8 decimal;

	
	mayan_debug("mayan init state");
	ctx_create_account(ctx, mayan->state->key, MAYAN_STATE_TMP_SIZE);

	mayan_debug("account created");
	mayan_debug_64(mayan->state->data_len, 0, 0, 0, 0);

	mayan_debug("calculating state");
	msg1 = (u8 *)mayan->msg1->key;
	msg2 = (u8 *)mayan->msg2->key;

	amount = vaa_mayan_amount(mayan->msg2->data);
	amount_min = vaa_mayan_amount_min(mayan->msg2->data);

	amount = denormalize_amount(amount, mayan->mint_from);
	amount_min = denormalize_amount(amount_min, mayan->mint_to);

	decimal = spl_get_decimals(mayan->mint_from);
	rate = amount_min * decimal_pow(decimal) / amount;

	mint_from = mayan->mint_from->key->x;
	mint_to = mayan->mint_to->key->x;

	to_addr = vaa_mayan_to_addr_buf(mayan->msg2->data);
	to_chain = vaa_mayan_to_chain(mayan->msg2->data);

	market1 = vaa_mayan_market1(mayan->msg2->data);
	market2 = vaa_mayan_market2(mayan->msg2->data);

	fee_swap = vaa_mayan_fee_swap(mayan->msg2->data);
	fee_cancel = 0;
	fee_return = vaa_mayan_fee_return(mayan->msg2->data);

	fee_swap = denormalize_amount(fee_swap, mayan->mint_from);
	fee_cancel = denormalize_amount(fee_cancel, mayan->mint_from);
	fee_return = denormalize_amount(fee_return, mayan->mint_to);

	deadline = vaa_mayan_deadline(mayan->msg2->data);

	mayan_debug("> rate / decimals");
	mayan_debug_64(rate, decimal, 0, 0, 0);

	mayan_debug("setting state");
	data = mayan->state->data;
	data_ptr = data;

	write_u8(data, &data_ptr, STATE_CLAIMED);
	write_buffer(data, &data_ptr, msg1, 32);
	write_buffer(data, &data_ptr, msg2, 32);
	write_u64(data, &data_ptr, amount);
	write_u8(data, &data_ptr, decimal);
	write_u64(data, &data_ptr, rate);
	write_buffer(data, &data_ptr, mint_from, 32);
	write_buffer(data, &data_ptr, mint_to, 32);
	write_buffer(data, &data_ptr, to_addr, 32);
	write_u16(data, &data_ptr, to_chain);

	write_buffer(data, &data_ptr, market1, 32);
	write_buffer(data, &data_ptr, market2, 32);

	write_u64(data, &data_ptr, fee_swap);
	write_u64(data, &data_ptr, fee_cancel);
	write_u64(data, &data_ptr, fee_return);

	write_u64(data, &data_ptr, deadline);

	write_u64(data, &data_ptr, amount_min); // state v1.1

	if (!check_buffer_is_ok(data, data_ptr, MAYAN_STATE_TMP_SIZE)) {
		return false;
	}


	return true;
}

static inline bool check_vault_mint(const SolAccountInfo *vault, const u8 *mint)
{
	SolPubkey tmp;
	
	if (vault->data_len < 32) {
		mayan_error("vault data is small!");
		return false;
	}

	sol_memcpy(&tmp, vault->data, 32);

	if (!buf_pubkey_same(mint, &tmp)) {
		mayan_error("mint is not same as vault!");
		return false;
	}

	return true;
}


static inline u64 parse_state(SolAccountInfo *state, u8 *result)
{
	if (state->data_len < MAYAN_STATE_DATA_SIZE) {
		mayan_error("state size is low");
		mayan_debug_64(MAYAN_STATE_DATA_SIZE, state->data_len, 0, 0, 0);
		return ERROR_ACCOUNT_DATA_TOO_SMALL;
	}

	*result = mayan_data_state(state->data);
	return SUCCESS;
}

static inline bool check_transitive_mints(struct swap_transitive_acc *swap)
{
	const u8 *mint;

	mint = mayan_data_mint_from(swap->state->data);
	if (!check_vault_mint(swap->m1.base_vault, mint)) {
		mayan_error("`from` mint/vault error");
		return false;
	}

	mint = mayan_data_mint_to(swap->state->data);
	if (!check_vault_mint(swap->m2.base_vault, mint)) {
		mayan_error("`to` mint/vault error");
		return false;
	}

	return true;
}

static inline bool check_simple_mints(struct swap_transitive_acc *swap)
{
	SolAccountInfo *ref;
	const u8 *mint;

	mint = mayan_data_mint_from(swap->state->data);

	if (check_vault_mint(swap->m1.base_vault, mint)) {
		swap->side = SWAP_SIDE_ASK;
		ref = swap->m1.quote_vault;
	} else if (check_vault_mint(swap->m1.quote_vault, mint)) {
		swap->side = SWAP_SIDE_BID;
		ref = swap->m1.base_vault;
	} else {
		mayan_error("from is not base or quote!");
		return false;
	}

	mint = mayan_data_mint_to(swap->state->data);

	if (!check_vault_mint(ref, mint)) {
		mayan_error("`to` vault is not mint!");
		mayan_debug_64(swap->side, SWAP_SIDE_ASK, SWAP_SIDE_BID, 0, 0);
		return false;
	}

	if(swap->side == SWAP_SIDE_ASK) {
		swap->s_acc.base = swap->s_acc.from->key;
		swap->s_acc.quote = swap->s_acc.to->key;
	} else {
		swap->s_acc.base = swap->s_acc.to->key;
		swap->s_acc.quote = swap->s_acc.from->key;
	}

	return true;
}

u64 parse_swap_x_accounts(struct prog_ctx *ctx,
			  struct swap_transitive_acc *swap, bool transitive)
{
	u64 result;
	bool is_ok;
	const u8 *msg1_buf;
	const u8 *msg2_buf;
	const u8 *market1;
	const u8 *market2;

	u8 state;

	mayan_debug("parse swap transitive accounts");

	swap->state = ctx->cursor++;
	swap->main = ctx->cursor++;
	
	swap->state_nonce = *ctx->data_ptr;
	ctx->data_ptr++;
	swap->main_nonce = *ctx->data_ptr;
	ctx->data_ptr++;

	result = parse_market_accounts(ctx, &swap->m1);
	if (result != SUCCESS) {
		mayan_error("market parser error");
		return result;
	}
		
	if (transitive) {
		result = parse_market_accounts(ctx, &swap->m2);
		if (result != SUCCESS) {
			mayan_error("market parser error");
			return result;
		}
	}

	swap->s_acc.main = swap->main->key;
	swap->s_acc.from = ctx->cursor++;
	swap->s_acc.to = ctx->cursor++;

	if (transitive) {
		swap->s_acc.tmp = ctx->cursor++;
	}

	result = parse_state(swap->state, &state);
	if (state != STATE_CLAIMED) {
		mayan_error("state's state is wrong!");
		mayan_debug_64(state, STATE_CLAIMED, 0, 0, 0);
		return ERROR_INVALID_ACCOUNT_DATA;
	}


	msg1_buf = mayan_data_msg1(swap->state->data);
	msg2_buf = mayan_data_msg2(swap->state->data);

	// TODO
	// this duplicate code is bad. but hey
	// it's the best I could at the moment.
	// refactor later. --ise

	// set seed
	set_ctx_seed(ctx, msg1_buf, msg2_buf, &swap->state_nonce,
		     &swap->main_nonce);

	// validate seed account
	result = ctx_check_seed_addr(ctx, swap->state->key);
	if (result != SUCCESS) {
		mayan_error("cannot validate seed addr");
		return result;
	}

	// validate main account
	result = ctx_check_main_addr(ctx, swap->main->key);
	if (result != SUCCESS) {
		mayan_error("cannot validate main addr");
		return result;
	}
		
	// validate markets
	market1 = mayan_data_market1(swap->state->data);
	market2 = mayan_data_market2(swap->state->data);
	
	if (!buf_pubkey_same(market1, swap->m1.market->key)) {
		mayan_error("market 1 is wrong");
		return ERROR_INVALID_ARGUMENT;
	}

	if (transitive) {
		if (!buf_pubkey_same(market2, swap->m2.market->key)) {
			mayan_error("market 2 is wrong");
			return ERROR_INVALID_ARGUMENT;
		}
	} else {
		if (!buf_pubkey_same(market2, ctx->progs.zero)) {
			mayan_error("market 2 is wrong [not all zero]");
			return ERROR_INVALID_ARGUMENT;
		}
	}

	if (transitive) {
		is_ok = check_transitive_mints(swap);
	} else {
		is_ok = check_simple_mints(swap);
	}

	if (!is_ok) {
		mayan_error("mint/vault check error");
		return ERROR_CUSTOM_ZERO;
	}

	return SUCCESS;
}

u64 parse_swap_transitive_accounts(struct prog_ctx *ctx,
				   struct swap_transitive_acc *swap)
{
	u64 result;
	const u8 *msg1_buf;
	const u8 *msg2_buf;
	const u8 *market1;
	const u8 *market2;
	const u8 *mint;

	u8 state;

	mayan_debug("parse swap transitive accounts");

	swap->state = ctx->cursor++;
	swap->main = ctx->cursor++;
	
	swap->state_nonce = *ctx->data_ptr;
	ctx->data_ptr++;
	swap->main_nonce = *ctx->data_ptr;
	ctx->data_ptr++;

	result = parse_market_accounts(ctx, &swap->m1);
	if (result != SUCCESS) {
		mayan_error("market parser error");
		return result;
	}
		
	result = parse_market_accounts(ctx, &swap->m2);
	if (result != SUCCESS) {
		mayan_error("market parser error");
		return result;
	}

	swap->s_acc.main = swap->main->key;
	swap->s_acc.from = ctx->cursor++;
	swap->s_acc.to = ctx->cursor++;
	swap->s_acc.tmp = ctx->cursor++;

	result = parse_state(swap->state, &state);
	if (state != STATE_CLAIMED) {
		mayan_error("state's state is wrong!");
		mayan_debug_64(state, STATE_CLAIMED, 0, 0, 0);
		return ERROR_INVALID_ACCOUNT_DATA;
	}


	msg1_buf = mayan_data_msg1(swap->state->data);
	msg2_buf = mayan_data_msg2(swap->state->data);

	// TODO
	// this duplicate code is bad. but hey
	// it's the best I could at the moment.
	// refactor later. --ise

	// set seed
	set_ctx_seed(ctx, msg1_buf, msg2_buf, &swap->state_nonce,
		     &swap->main_nonce);

	// validate seed account
	result = ctx_check_seed_addr(ctx, swap->state->key);
	if (result != SUCCESS) {
		mayan_error("cannot validate seed addr");
		return result;
	}

	// validate main account
	result = ctx_check_main_addr(ctx, swap->main->key);
	if (result != SUCCESS) {
		mayan_error("cannot validate main addr");
		return result;
	}
		
	// validate markets
	market1 = mayan_data_market1(swap->state->data);
	market2 = mayan_data_market2(swap->state->data);
	
	if (!buf_pubkey_same(market1, swap->m1.market->key)) {
		mayan_error("market 1 is wrong");
		return ERROR_INVALID_ARGUMENT;
	}

	if (!buf_pubkey_same(market2, swap->m2.market->key)) {
		mayan_error("market 2 is wrong");
		return ERROR_INVALID_ARGUMENT;
	}

	mint = mayan_data_mint_from(swap->state->data);
	if (!check_vault_mint(swap->m1.base_vault, mint)) {
		mayan_error("`from` mint/vault error");
		return ERROR_CUSTOM_ZERO;
	}

	mint = mayan_data_mint_to(swap->state->data);
	if (!check_vault_mint(swap->m2.base_vault, mint)) {
		mayan_error("`to` mint/vault error");
		return ERROR_CUSTOM_ZERO;
	}

	return SUCCESS;
}

u64 parse_transfer_accounts(struct prog_ctx *ctx, struct transfer_acc *trn,
			    bool is_wrapped)
{
	const u8 *msg1_buf;
	const u8 *msg2_buf;
	const u8 *mint_from;
	const u8 *mint_to;
	const u8 *mint_ref;

	u64 rfee;
	u64 result;
	u8 state;

	mayan_debug("parse trn accounts");

	trn->owner = ctx->cursor++;
	trn->state = ctx->cursor++;
	trn->main = ctx->cursor++;
	
	trn->state_nonce = *ctx->data_ptr;
	ctx->data_ptr++;
	trn->main_nonce = *ctx->data_ptr;
	ctx->data_ptr++;

	if (is_wrapped) {
		result = parse_wh_trw_accounts(ctx, &trn->transfer);
	} else {
		result = parse_wh_trn_accounts(ctx, &trn->transfer);
	}

	if (result != SUCCESS) {
		mayan_error("cannot process wh_trn_accounts");
		return result;
	}

	mayan_debug("account checks");
	result = parse_state(trn->state, &state);

	if (state != STATE_SWAP_DONE && state != STATE_CLAIMED) {
		mayan_error("state's state is wrong!");
		mayan_debug_64(state, STATE_SWAP_CANCELED, STATE_SWAP_DONE, 0, 0);
		return ERROR_INVALID_ACCOUNT_DATA;
	}

	trn->try_cancel = false;
	if (state != STATE_SWAP_DONE) {
		trn->try_cancel = true;
	}

	trn->transfer.owner = trn->main->key;
	trn->transfer.payer = trn->owner->key;

	msg1_buf = mayan_data_msg1(trn->state->data);
	msg2_buf = mayan_data_msg2(trn->state->data);

	// set seed
	set_ctx_seed(ctx, msg1_buf, msg2_buf, &trn->state_nonce,
		     &trn->main_nonce);

	// validate seed account
	result = ctx_check_seed_addr(ctx, trn->state->key);
	if (result != SUCCESS) {
		mayan_error("cannot validate seed addr");
		return result;
	}

	// validate main account
	result = ctx_check_main_addr(ctx, trn->main->key);
	if (result != SUCCESS) {
		mayan_error("cannot validate main addr");
		return result;
	}

	// validate mint
	mint_from = mayan_data_mint_from(trn->state->data);
	mint_to = mayan_data_mint_to(trn->state->data);
	mint_ref = (state == STATE_SWAP_DONE) ? mint_to : mint_from;

	if (!buf_pubkey_same(mint_ref, trn->transfer.mint->key)) {
		mayan_error("mint is not correct!");
		mayan_debug_64(0, state, STATE_SWAP_DONE,
			       state == STATE_SWAP_DONE, 0);
		mayan_log_buf_32(mint_from);
		mayan_log_buf_32(mint_to);
		mayan_log_buf_32(mint_ref);
		mayan_log_buf_32(trn->transfer.mint->key->x);

		return ERROR_CUSTOM_ZERO;
	}

	rfee = mayan_data_fee_return(trn->state->data);
	if (trn->try_cancel) {
		rfee = mayan_data_fee_cancel(trn->state->data);
	}

	trn->transfer.address = mayan_data_to_addr(trn->state->data);
	trn->transfer.chain = mayan_data_to_chain(trn->state->data);
	trn->transfer.amount = mayan_data_amount(trn->state->data);
	trn->transfer.relayer_fee = rfee;

	trn->success_state = (state == STATE_SWAP_DONE) ? STATE_DONE_SWAPPED : STATE_DONE_NOT_SWAPPED;

	return SUCCESS;
}
