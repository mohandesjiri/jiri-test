#ifndef _SPL_H_
#define _SPL_H_

#include "sol/entrypoint.h"
#include "sol/types.h"
#include "utils.h"
#include "ctx.h"
#include "sol/pubkey.h"

u64 spl_approve(struct prog_ctx *ctx, SolPubkey *owner, SolPubkey *acc,
		SolPubkey *delegate, u64 amount);

u64 system_transfer(struct prog_ctx *ctx, SolPubkey *from, SolPubkey *to,
		    u64 amount);

u64 system_create_account(struct prog_ctx *ctx, SolPubkey *account,
			  SolPubkey *payer, const SolPubkey *prog_id,
			  u64 lamports, u64 space);

u64 ctx_create_account(struct prog_ctx *ctx, SolPubkey *account, u64 space);


static inline u64 spl_get_amount(const SolAccountInfo *acc, u64 *amount)
{
	mayan_debug("spl get amount");

	if (acc->data_len < 72) {
		mayan_error("spl account data problem");
		return ERROR_ACCOUNT_DATA_TOO_SMALL;
	}

	*amount = *(u64*)(acc->data + 64);
	return SUCCESS;
}

#endif // _SPL_H_
