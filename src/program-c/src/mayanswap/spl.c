#include "spl.h"
#include "sol/pubkey.h"
#include "sol/types.h"
#include "utils.h"

#define TOKEN_INSTRUCTION_APPROVE 4
u64 spl_approve(struct prog_ctx *ctx, SolPubkey *owner, SolPubkey *acc,
		SolPubkey *delegate, u64 amount)
{
	SolInstruction ix;
	u8 data[9];
	u8* data_ptr;

	mayan_debug("spl approve > accs");

	SolAccountMeta accounts[] = {
	    {acc, true, false},
	    {delegate, false, false},
	    {owner, false, true},
	};


	mayan_debug("spl approve > data");
	data_ptr = data;
	write_u8(data, &data_ptr, TOKEN_INSTRUCTION_APPROVE);
	write_u64(data, &data_ptr, amount);
	check_buffer_done(data, data_ptr);

	mayan_debug("spl approve > ix");
	ix.program_id = &(ctx->progs.spl);
	ix.accounts = accounts;
	ix.account_len = SOL_ARRAY_SIZE(accounts);
	ix.data = data;
	ix.data_len = SOL_ARRAY_SIZE(data);

	return mayan_invoke(ctx, &ix);
}

#define SYSTEM_INSTRUCTION_TRANSFER 2
u64 system_transfer(struct prog_ctx *ctx, SolPubkey *from, SolPubkey *to,
		    u64 amount)
{
	SolInstruction ix;
	u8 data[12];
	u8* data_ptr;

	mayan_debug("system transfer > accs");

	SolAccountMeta accounts[] = {
		{from, true, true},
		{to, true, false},
	};


	mayan_debug("system transfer > data");
	data_ptr = data;
	write_u32(data, &data_ptr, SYSTEM_INSTRUCTION_TRANSFER); // TODO check
	write_i64(data, &data_ptr, amount);
	check_buffer_done(data, data_ptr);

	mayan_debug("system transfer > ix");
	ix.program_id = &(ctx->progs.system);
	ix.accounts = accounts;
	ix.account_len = SOL_ARRAY_SIZE(accounts);
	ix.data = data;
	ix.data_len = SOL_ARRAY_SIZE(data);

	return mayan_invoke(ctx, &ix);
}

#define SYSTEM_INSTRUCTION_CREATE 0
u64 system_create_account(struct prog_ctx *ctx, SolPubkey *account,
			  SolPubkey *payer, const SolPubkey *prog_id,
			  u64 lamports, u64 space)
{
	SolInstruction ix;
	u8 data[52];
	u8* data_ptr;

	mayan_debug("system transfer > accs");

	SolAccountMeta accounts[] = {
		{payer, true, true},
		{account, true, true},
	};


	mayan_debug("system transfer > data");
	data_ptr = data;
	write_u32(data, &data_ptr, SYSTEM_INSTRUCTION_CREATE);
	write_i64(data, &data_ptr, lamports);
	write_i64(data, &data_ptr, space);
	write_buffer(data, &data_ptr, (u8*)(prog_id), 32);
	check_buffer_done(data, data_ptr);

	mayan_debug("system transfer > ix");
	ix.program_id = &(ctx->progs.system);
	ix.accounts = accounts;
	ix.account_len = SOL_ARRAY_SIZE(accounts);
	ix.data = data;
	ix.data_len = SOL_ARRAY_SIZE(data);

	return mayan_invoke(ctx, &ix);
}

u64 ctx_create_account(struct prog_ctx *ctx, SolPubkey *account, u64 space)
{
	u64 lamports;

	mayan_debug("create account");

	lamports = rent_minimum_balance(&ctx->rent, space);

	mayan_debug("lamports?");
	mayan_debug_64(lamports, 0, 0, 0, 0);
	
	if (ctx->payer == NULL) {
		mayan_error("payer not found");
		return ERROR_CUSTOM_ZERO;
	}

	system_create_account(ctx, account, ctx->payer, ctx->prog_id, lamports,
			      space);

	return SUCCESS;
}
