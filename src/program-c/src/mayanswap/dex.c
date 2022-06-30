#include "dex.h"
#include "sol/types.h"
#include "utils.h"

u64 dex_swap_transitive(struct prog_ctx *ctx, struct serum_market *m1,
			struct serum_market *m2, struct serum_accs *acc,
			u64 amount, u64 rate, u8 decimal)
{
	SolInstruction ix;
	u8 data[27];
	u8* data_ptr;

	mayan_debug("dex transitive swap > accs");

	SolAccountMeta accounts[] = {
		{m1->market->key, true, false},
		{m1->open_orders->key, true, false},
		{m1->req_queue->key, true, false},
		{m1->event_queue->key, true, false},
		{m1->bids->key, true, false},
		{m1->asks->key, true, false},
		{acc->from->key, true, false},
		{m1->base_vault->key, true, false},
		{m1->quote_vault->key, true, false},
		{m1->vault_signer->key, false, false},
		{acc->from->key, true, false},

		{m2->market->key, true, false},
		{m2->open_orders->key, true, false},
		{m2->req_queue->key, true, false},
		{m2->event_queue->key, true, false},
		{m2->bids->key, true, false},
		{m2->asks->key, true, false},
		{acc->tmp->key, true, false},
		{m2->base_vault->key, true, false},
		{m2->quote_vault->key, true, false},
		{m2->vault_signer->key, false, false},
		{acc->to->key, true, false},
		
		{acc->main, false, true},
		{acc->tmp->key, true, false},

		{&ctx->progs.dex, false, false},
		{&ctx->progs.spl, false, false},
		{&ctx->rent.key, false, false},
	};


	mayan_debug("dex swap > data");
	data_ptr = data;
	write_u8(data, &data_ptr, 129);
	write_u8(data, &data_ptr, 109);
	write_u8(data, &data_ptr, 254);
	write_u8(data, &data_ptr, 207);
	write_u8(data, &data_ptr, 31);
	write_u8(data, &data_ptr, 192);
	write_u8(data, &data_ptr, 47);
	write_u8(data, &data_ptr, 51);

	write_u64(data, &data_ptr, amount);

	// min exchange rate
	write_u64(data, &data_ptr, rate);
	write_u8(data, &data_ptr, decimal);
	write_u8(data, &data_ptr, 0);
	write_u8(data, &data_ptr, 1);

	if (!check_buffer_is_done(data, data_ptr, SOL_ARRAY_SIZE(data))) {
		mayan_error("data is not full!");
		return ERROR_CUSTOM_ZERO;
	}

	mayan_debug("dex swap > ix");
	ix.program_id = &(ctx->progs.swap);
	ix.accounts = accounts;
	ix.account_len = SOL_ARRAY_SIZE(accounts);
	ix.data = data;
	ix.data_len = SOL_ARRAY_SIZE(data);

	return mayan_invoke(ctx, &ix);
}

u64 dex_swap_simple(struct prog_ctx *ctx, struct serum_market *m1,
		    struct serum_accs *acc, u8 side, u64 amount, u64 rate,
		    u8 decimal)
{
	SolInstruction ix;
	u8 data[28];
	u8* data_ptr;

	mayan_debug("dex simple swap > accs");
	mayan_debug_64(rate, decimal, 0, amount, side);

	SolAccountMeta accounts[] = {
		{m1->market->key, true, false},
		{m1->open_orders->key, true, false},
		{m1->req_queue->key, true, false},
		{m1->event_queue->key, true, false},
		{m1->bids->key, true, false},
		{m1->asks->key, true, false},
		{acc->from->key, true, false},
		{m1->base_vault->key, true, false},
		{m1->quote_vault->key, true, false},
		{m1->vault_signer->key, false, false},
		{acc->base, true, false},
		{acc->main, false, true},
		{acc->quote, true, false},

		{&ctx->progs.dex, false, false},
		{&ctx->progs.spl, false, false},
		{&ctx->rent.key, false, false},
	};


	mayan_debug("dex swap > data");
	data_ptr = data;
	write_u8(data, &data_ptr, 248);
	write_u8(data, &data_ptr, 198);
	write_u8(data, &data_ptr, 158);
	write_u8(data, &data_ptr, 145);
	write_u8(data, &data_ptr, 225);
	write_u8(data, &data_ptr, 117);
	write_u8(data, &data_ptr, 135);
	write_u8(data, &data_ptr, 200);

	write_u8(data, &data_ptr, side);
	write_u64(data, &data_ptr, amount);

	// min exchange rate
	write_u64(data, &data_ptr, rate);
	write_u8(data, &data_ptr, decimal);
	write_u8(data, &data_ptr, 0);
	write_u8(data, &data_ptr, 1);

	if (!check_buffer_is_done(data, data_ptr, SOL_ARRAY_SIZE(data))) {
		mayan_error("data is not full!");
		return ERROR_CUSTOM_ZERO;
	}

	mayan_debug("dex swap > ix");
	ix.program_id = &(ctx->progs.swap);
	ix.accounts = accounts;
	ix.account_len = SOL_ARRAY_SIZE(accounts);
	ix.data = data;
	ix.data_len = SOL_ARRAY_SIZE(data);

	return mayan_invoke(ctx, &ix);
}
