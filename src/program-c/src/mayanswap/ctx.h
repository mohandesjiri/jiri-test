#ifndef _CTX_H_
#define _CTX_H_

#include "sol/entrypoint.h"
#include "sol/pubkey.h"
#include "sol/types.h"
#include "utils.h"

struct solprogs {
	// solana
	SolPubkey system;
	SolPubkey spl;

	// wormhole
	SolPubkey wh_core;
	SolPubkey wh_bridge;

	// swap
	SolPubkey dex;
	SolPubkey swap;

	// references
	SolPubkey *zero;
};

struct rent {
	SolPubkey key;

	u64 lamport_byte_year;
	double threshold;
	u8 burn_percent;
};

struct clock {
	SolPubkey key;

	u64 posix;
};

struct prog_ctx {
	SolParameters *params;
	const SolPubkey *prog_id;
	SolAccountInfo *cursor;
	const u8* data_ptr;

	bool invoke_with_seed;
	
	SolSignerSeed main_seed[2];
	SolSignerSeed state_seed[4];
	SolSignerSeeds seeds[2];

	struct solprogs progs;
	struct rent rent;
	struct clock clock;

	SolPubkey *payer;
};


#define SYSTEM_PROGRAM_ID (SolPubkey){.x={0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
#define SPL_PROGRAM_ID (SolPubkey){.x={6, 221, 246, 225, 215, 101, 161, 147, 217, 203, 225, 70, 206, 235, 121, 172, 28, 180, 133, 237, 95, 91, 55, 145, 58, 140, 245, 133, 126, 255, 0, 169}}
#define WORMHOLE_PROGRAM_ID (SolPubkey){.x={14, 10, 88, 154, 65, 165, 95, 189, 102, 197, 42, 71, 95, 45, 146, 166, 211, 220, 155, 71, 71, 17, 76, 185, 175, 130, 90, 152, 181, 69, 211, 206}}
#define TOKEN_BRIDGE_PROGRAM_ID (SolPubkey){.x={14, 10, 88, 158, 100, 136, 20, 122, 148, 220, 250, 89, 43, 144, 253, 212, 17, 82, 187, 44, 167, 123, 246, 1, 103, 88, 166, 244, 223, 157, 33, 180}}
#define DEX_PROGRAM_ID (SolPubkey){.x={133, 15, 45, 110, 2, 164, 122, 248, 36, 208, 154, 182, 157, 196, 45, 112, 203, 40, 203, 250, 36, 159, 183, 238, 87, 185, 210, 86, 193, 39, 98, 239}}
#define SWAP_PROGRAM_ID (SolPubkey){.x={15, 64, 97, 8, 49, 70, 197, 30, 250, 81, 166, 230, 52, 144, 92, 204, 10, 35, 233, 104, 95, 215, 2, 103, 44, 57, 150, 188, 186, 245, 81, 58}}

static inline void solprogs_init(struct solprogs *progs)
{
	*progs = (struct solprogs){
		.system = SYSTEM_PROGRAM_ID,
		.spl = SPL_PROGRAM_ID,
		.wh_core = WORMHOLE_PROGRAM_ID,
		.wh_bridge = TOKEN_BRIDGE_PROGRAM_ID,
		.dex = DEX_PROGRAM_ID,
		.swap = SWAP_PROGRAM_ID,
	};

	progs->zero = &progs->system;
}

// rent
#define RENT_VAR_KEY (SolPubkey){.x={6, 167, 213, 23, 25, 44, 92, 81, 33, 140, 201, 76, 61, 74, 241, 127, 88, 218, 238, 8, 155, 161, 253, 68, 227, 219, 217, 138, 0, 0, 0, 0}}
static inline u64 parse_rent_account(struct prog_ctx *ctx, struct rent *rent)
{
	SolAccountInfo *acc;

	rent->key = RENT_VAR_KEY;

	mayan_debug("parse rent account");
	acc = ctx->cursor++;

	if (!SolPubkey_same(&rent->key, acc->key)) {
		mayan_error("rent address is wrong!");
		return ERROR_INCORRECT_PROGRAM_ID;
	}

	if (acc->data_len < 17) {
		mayan_error("why rent data is wrong?");
		return ERROR_ACCOUNT_DATA_TOO_SMALL;
	}

	rent->lamport_byte_year = *(u64 *)(acc->data);
	rent->threshold = *(double *)(acc->data + 8);
	rent->burn_percent = acc->data[16];

	return SUCCESS;
}

static inline u64 rent_minimum_balance(const struct rent *rent, u64 size)
{
	return ((size + 128) * rent->lamport_byte_year) * rent->threshold;
}

// clock
#define CLOCK_VAR_KEY (SolPubkey){.x={6, 167, 213, 23, 24, 199, 116, 201, 40, 86, 99, 152, 105, 29, 94, 182, 139, 94, 184, 163, 155, 75, 109, 92, 115, 85, 91, 33, 0, 0, 0, 0}}
static inline u64 parse_clock_account(struct prog_ctx *ctx, struct clock *clk)
{
	SolAccountInfo *acc;

	clk->key = CLOCK_VAR_KEY;

	mayan_debug("parse clock account");
	acc = ctx->cursor++;

	if (!SolPubkey_same(&clk->key, acc->key)) {
		mayan_error("clock address is wrong!");
		return ERROR_INCORRECT_PROGRAM_ID;
	}

	if (acc->data_len < 40) {
		mayan_error("why clcok data is wrong?");
		return ERROR_ACCOUNT_DATA_TOO_SMALL;
	}

	clk->posix = *(u64 *)(acc->data + 32);

	return SUCCESS;
}


static inline void ctx_init(struct prog_ctx *ctx, SolParameters *params)
{
	ctx->params = params;
	ctx->prog_id = params->program_id;

	ctx->invoke_with_seed = true;
	
	ctx->seeds[0] = (SolSignerSeeds){
		.addr=ctx->state_seed,
		.len=SOL_ARRAY_SIZE(ctx->state_seed)
	};

	ctx->seeds[1] = (SolSignerSeeds){
		.addr=ctx->main_seed,
		.len=SOL_ARRAY_SIZE(ctx->main_seed)
	};


	ctx->data_ptr = params->data + 1;
	ctx->cursor = params->ka;
	ctx->payer = NULL;

	solprogs_init(&ctx->progs);
}


inline static u64 mayan_invoke(const struct prog_ctx *ctx,
			       const SolInstruction *ix)
{
	if (ctx->invoke_with_seed) {
		mayan_debug("invoke signed!");
		return sol_invoke_signed(ix, ctx->params->ka,
					 ctx->params->ka_num, ctx->seeds,
					 SOL_ARRAY_SIZE(ctx->seeds));
	}
	mayan_debug("invoke!");
	return sol_invoke(ix, ctx->params->ka, ctx->params->ka_num);
}


static inline bool validate_seed_addr(const struct prog_ctx *ctx,
				      int seed_idx, const SolPubkey *key)
{
	SolPubkey tmp;
	const SolSignerSeeds *seed;
	u64 result;

	seed = &(ctx->seeds[seed_idx]);

	result = sol_create_program_address(seed->addr, seed->len,
					    ctx->prog_id, &tmp);

	if (result != SUCCESS) {
		mayan_error("cannot create program address");
		return false;
	}

	if (!SolPubkey_same(key, &tmp))
		return false;

	return true;
}

static const u8 state_seed[] = {'V', '2', 'S', 'T', 'A', 'T', 'E'};
static const u8 main_seed[] = {'M', 'A', 'I', 'N'};
static inline void set_ctx_seed(struct prog_ctx *ctx, const u8 *msg1,
				const u8 *msg2, const u8 *state_nonce,
				const u8 *main_nonce)
{
	ctx->state_seed[0] = (SolSignerSeed){
		.addr=state_seed,
		.len=SOL_ARRAY_SIZE(state_seed)
	};
	ctx->state_seed[1] = (SolSignerSeed){.addr=msg1, .len=32};
	ctx->state_seed[2] = (SolSignerSeed){.addr=msg2, .len=32};
	ctx->state_seed[3] = (SolSignerSeed){
		.addr=state_nonce,
		.len=1
	};

	
	ctx->main_seed[0] = (SolSignerSeed){
		.addr=main_seed,
		.len=SOL_ARRAY_SIZE(main_seed)
	};
	ctx->main_seed[1] = (SolSignerSeed){
		.addr = main_nonce,
		.len = 1
	};
}

static inline u64 ctx_check_seed_addr(struct prog_ctx *ctx,
				      const SolPubkey *state_key)
{
	SolPubkey state_exp;
	u64 result;
	

	result = sol_create_program_address(ctx->state_seed,
					    SOL_ARRAY_SIZE(ctx->state_seed),
					    ctx->prog_id, &state_exp);
	if (result != SUCCESS) {
		mayan_error("can't create state prog addr");
		return result;
	}

	if (!SolPubkey_same(&state_exp, state_key)) {
		mayan_error("state addr is wrong");
		return ERROR_INVALID_ARGUMENT;
	}

	return SUCCESS;
}

static inline u64 ctx_check_main_addr(struct prog_ctx *ctx,
				  const SolPubkey *main_key)
{
	SolPubkey main_exp;
	u64 result;

	result = sol_create_program_address(ctx->main_seed,
					    SOL_ARRAY_SIZE(ctx->main_seed),
					    ctx->prog_id, &main_exp);
	if (result != SUCCESS) {
		mayan_error("can't create main prog addr");
		return result;
	}

	if (!SolPubkey_same(&main_exp, main_key)) {
		mayan_error("main addr is wrong");
		return ERROR_INVALID_ARGUMENT;
	}

	return SUCCESS;
}

#endif // _CTX_H_
