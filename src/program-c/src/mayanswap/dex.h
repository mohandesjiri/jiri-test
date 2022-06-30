#ifndef _DEX_H_
#define _DEX_H_

#include "sol/entrypoint.h"
#include "ctx.h"
#include "sol/pubkey.h"

struct serum_market {
	SolAccountInfo* market;
	SolAccountInfo* open_orders;
	SolAccountInfo* req_queue;
	SolAccountInfo* event_queue;
	SolAccountInfo* bids;
	SolAccountInfo* asks;
	SolAccountInfo* base_vault;
	SolAccountInfo* quote_vault;
	SolAccountInfo* vault_signer;	
};

struct serum_accs {
	SolPubkey *main;

	SolAccountInfo *from;
	SolAccountInfo *to;
	SolAccountInfo *tmp;

	// only for single
	SolPubkey *base;
	SolPubkey *quote;
};

enum swap_side {
	SWAP_SIDE_BID,
	SWAP_SIDE_ASK,
};

static inline u64 parse_market_accounts(struct prog_ctx *ctx,
					struct serum_market *market)
{
	market->market = ctx->cursor++;
	market->open_orders = ctx->cursor++;
	market->req_queue = ctx->cursor++;
	market->event_queue = ctx->cursor++;
	market->bids = ctx->cursor++;
	market->asks = ctx->cursor++;
	market->base_vault = ctx->cursor++;
	market->quote_vault = ctx->cursor++;
	market->vault_signer = ctx->cursor++;	

	return SUCCESS;
}

u64 dex_swap_transitive(struct prog_ctx *ctx, struct serum_market *m1,
			struct serum_market *m2, struct serum_accs *acc,
			u64 amount, u64 rate, u8 decimal);

u64 dex_swap_simple(struct prog_ctx *ctx, struct serum_market *m1,
		    struct serum_accs *acc, u8 side, u64 amount, u64 rate,
		    u8 decimal);

#endif // _DEX_H_
