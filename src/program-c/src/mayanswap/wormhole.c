#include "wormhole.h"
#include "sol/pubkey.h"
#include "sol/string.h"
#include "sol/types.h"
#include "utils.h"

#define CHAIN_ID_SOLANA 1
#define CHAIN_ID_BSC 4
#define CHAIN_ID_POLYGON 5

#define POLYGON_TOKEN_BRIDGE (SolPubkey){.x={0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 90, 88, 80, 90, 150, 209, 219, 248, 223, 145, 203, 33, 181, 68, 25, 252, 54, 233, 63, 222}}
#define POLYGON_MAYAN_BRIDGE (SolPubkey){.x={0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 208, 248, 138, 236, 92, 77, 226, 12, 201, 112, 223, 209, 115, 86, 76, 177, 132, 37, 225, 238}}

#define BSC_TOKEN_BRIDGE (SolPubkey){.x={0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 182, 246, 216, 106, 143, 152, 121, 169, 200, 127, 100, 55, 104, 217, 239, 195, 140, 29, 166, 231}}
#define BSC_MAYAN_BRIDGE (SolPubkey){.x={0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 215, 147, 245, 20, 205, 227, 116, 67, 223, 200, 14, 57, 216, 228, 14, 147, 35, 166, 60, 171}}

/*
  msg1 --> is token transfer msg
  msg2 --> is swap msg
 */
u64 check_vaa_pair(const SolAccountInfo *msg1, const SolAccountInfo *msg2)
{
	u64 seq1;
	u64 seq2;
	u16 chain_id1;
	u16 chain_id2;

	mayan_debug("check vaa pair!");
	if (msg1->data_len < 96) {
		mayan_error("msg1 data len");
		return ERROR_ACCOUNT_DATA_TOO_SMALL;
	}
	if (msg2->data_len != 396) {
		mayan_error("msg2 data len");
		return ERROR_ACCOUNT_DATA_TOO_SMALL;
	}

	chain_id1 = vaa_chain_id(msg1->data);
	chain_id2 = vaa_chain_id(msg2->data);

	if (chain_id1 != chain_id2) {
		mayan_error("chain id error");
		mayan_debug_64(chain_id1, chain_id2, 0, 0, 0);
		return ERROR_INVALID_ACCOUNT_DATA;
	}

	seq1 = vaa_seq_id(msg1->data);
	seq2 = vaa_mayan_ref_seq_id(msg2->data);

	if (seq1 != seq2) {
		mayan_error("seq reference is not valid");
		mayan_debug_64(seq1, seq2, 0, 0, 0);
		return ERROR_INVALID_ACCOUNT_DATA;
	}

	return SUCCESS;
}

bool is_emitter_token_bridge(const u8 *vaa)
{
	mayan_debug("is emitter token bridge?");
	u16 chain_id;
	SolPubkey bridge;

	chain_id = vaa_chain_id(vaa);

	if (chain_id == CHAIN_ID_POLYGON) {
		bridge = POLYGON_TOKEN_BRIDGE;
	} else if (chain_id == CHAIN_ID_BSC) {
		bridge = BSC_TOKEN_BRIDGE;
	} else {
		mayan_error("Unknown chain id");
		mayan_debug_64(chain_id, 0, 0, 0, 0);
		return false;
	}

	if (!buf_pubkey_same(vaa_emitter_addr_buf(vaa), &bridge)) {
		mayan_debug("emitter is not token bridge.");
		return false;
	}

	return true;
}

bool is_emitter_mayan_bridge(const u8 *vaa)
{
	mayan_debug("is emitter mayan bridge?");
	u16 chain_id;
	SolPubkey bridge;

	chain_id = vaa_chain_id(vaa);

	if (chain_id == CHAIN_ID_POLYGON) {
		bridge = POLYGON_MAYAN_BRIDGE;
	} else if (chain_id == CHAIN_ID_BSC) {
		bridge = BSC_MAYAN_BRIDGE;
	} else {
		mayan_error("Unknown chain id");
		mayan_debug_64(chain_id, 0, 0, 0, 0);
		return false;
	}

	if (!buf_pubkey_same(vaa_emitter_addr_buf(vaa), &bridge)) {
		mayan_error("emitter is not mayan bridge!");
		return false;
	}

	return true;
}

u64 wh_check_msg_addr(const struct prog_ctx *ctx, const SolPubkey *msg,
		      u8 nonce, const u8 *hash)
{
	SolPubkey addr;
	u64 result;
	const u8 seed[] = {'P', 'o', 's', 't', 'e', 'd', 'V', 'A', 'A'};
	const SolSignerSeed seeds[] = {
		{.addr=seed, .len=SOL_ARRAY_SIZE(seed)},
		{.addr=hash, .len=32},
		{.addr=&nonce, .len=1},
	};

	mayan_debug("wormhole check msg addr");

	result = sol_create_program_address(seeds, SOL_ARRAY_SIZE(seeds),
					    &ctx->progs.wh_core, &addr);

	if (result != SUCCESS) {
		mayan_error("cannot create prog addr");
		mayan_debug_64(hash[0], hash[1], hash[2], hash[3], nonce);
		return result;
	}

	if (!SolPubkey_same(&addr, msg)) {
		mayan_error("msg addr is wrong!");
		mayan_debug_64(hash[0], hash[1], hash[2], hash[3], nonce);
		return ERROR_CUSTOM_ZERO;
	}

	return SUCCESS;
}

u64 wh_check_claimed(const struct prog_ctx *ctx, const SolAccountInfo *msg,
                     u8 nonce, const SolAccountInfo *claim)
{
	SolPubkey emitter;
	SolPubkey addr;
	u8 buf[10] = {0};

	u16 chain_id;
	u64 seq_id;
	u64 result;

	const SolSignerSeed seeds[] = {
		{.addr=(u8*)(&emitter), .len=32},
		{.addr=buf, .len=SOL_ARRAY_SIZE(buf)},
		{.addr=&nonce, .len=1},
	};

	mayan_debug("wormhole: check claimed");
	chain_id = vaa_chain_id(msg->data);
	seq_id = vaa_seq_id(msg->data);

	if (chain_id == CHAIN_ID_POLYGON) {
		emitter = POLYGON_TOKEN_BRIDGE;
	} else if (chain_id == CHAIN_ID_BSC) {
		emitter = BSC_TOKEN_BRIDGE;
	} else {
		mayan_error("Unknown chain id");
		mayan_debug_64(chain_id, 0, 0, 0, 0);
		return false;
	}

	write_u16_be(buf, chain_id);
	write_u64_be(buf + 2, seq_id);

	result = sol_create_program_address(seeds, SOL_ARRAY_SIZE(seeds), &ctx->progs.wh_bridge, &addr);
	if (result != SUCCESS)
		return result;

	if (!SolPubkey_same(&addr, claim->key)) {
		mayan_error("claim addr is wrong!");
		mayan_debug_64(chain_id, seq_id, 0, 0, nonce);


		return ERROR_CUSTOM_ZERO;
	}

	if (claim->data_len < 1) {
		mayan_error("not claimed yet!");
		return ERROR_ACCOUNT_DATA_TOO_SMALL;
	}

	if (claim->data[0] != 1) {
		mayan_error("claim flag is not set");
		return ERROR_INVALID_ACCOUNT_DATA;
	}

	return SUCCESS;
}

u64 wh_get_mint(const struct prog_ctx *ctx, const u8 *buf, u16 chain_id,
		u8 nonce, SolPubkey *result)
{
	u64 res;
	u8 buf_cid[2] = {0};
	const u8 seed[] = {'w', 'r', 'a', 'p', 'p', 'e', 'd'};
	const SolSignerSeed seeds[] = {
		{.addr=seed, .len=SOL_ARRAY_SIZE(seed)},
		{.addr=buf_cid, .len=SOL_ARRAY_SIZE(buf_cid)},
		{.addr=buf, .len=32},
		{.addr=&nonce, .len=1},
	};

	mayan_debug("wormhole get mint");

	if (chain_id == CHAIN_ID_SOLANA) {
		sol_memcpy(result, buf, 32);
		return SUCCESS;
	}

	//write_u16_be(token_id, chain_id);
	write_u16_be(buf_cid, chain_id);
	//sol_memcpy(token_id + 2, buf, 32);

	res = sol_create_program_address(seeds, SOL_ARRAY_SIZE(seeds),
					 &ctx->progs.wh_bridge, result);

	if (res != SUCCESS) {
		mayan_error("cannot create prog addr for mint");
		return res;
	}

	return SUCCESS;
}

u64 wh_transfer_native(struct prog_ctx *ctx, struct wh_transfer_acc *transfer)
{
	SolInstruction ix;
	u8 data[55];
	u8* data_ptr;

	mayan_debug("wormhole transfer native > accs");

	SolAccountMeta accounts[] = {
		{transfer->payer, true, true},
		{transfer->config->key, false, false},
		{transfer->acc->key, true, false},
		{transfer->mint->key, true, false},
		{transfer->custody->key, true, false},
		{transfer->auth_signer->key, false, false},
		{transfer->custody_signer->key, false, false},
		{transfer->bridge_conf->key, true, false},
		{transfer->new_msg->key, true, true},
		{transfer->emitter->key, false, false},
		{transfer->seq_key->key, true, false},
		{transfer->fee_acc->key, true, false},

		// sysvar
		{&ctx->clock.key, false, false},
		{&ctx->rent.key, false, false},

		// programs
		{&ctx->progs.system, false, false},
		{&ctx->progs.wh_core, false, false},
		{&ctx->progs.spl, false, false},
	};


	mayan_debug("wormhole transfer native > data");
	data_ptr = data;
	write_u8(data, &data_ptr, 5);
	write_u32(data, &data_ptr, transfer->nonce);
	write_u64(data, &data_ptr, transfer->amount);
	write_u64(data, &data_ptr, transfer->relayer_fee);
	write_buffer(data, &data_ptr, transfer->address, 32);
	write_u16(data, &data_ptr, transfer->chain);

	if (!check_buffer_is_done(data, data_ptr, SOL_ARRAY_SIZE(data))) {
		mayan_error("data is not full!");
		return ERROR_CUSTOM_ZERO;
	}

	mayan_debug("wormhole transfer native > ix");
	ix.program_id = &(ctx->progs.wh_bridge);
	ix.accounts = accounts;
	ix.account_len = SOL_ARRAY_SIZE(accounts);
	ix.data = data;
	ix.data_len = SOL_ARRAY_SIZE(data);

	return mayan_invoke(ctx, &ix);
}

u64 wh_transfer_wrapped(struct prog_ctx *ctx, struct wh_transfer_acc *transfer)
{
	SolInstruction ix;
	u8 data[55];
	u8* data_ptr;

	mayan_debug("wormhole transfer wrapped > accs");

	SolAccountMeta accounts[] = {
		{transfer->payer, true, true},
		{transfer->config->key, false, false},
		{transfer->acc->key, true, false},
		{transfer->owner, false, true},
		{transfer->mint->key, true, false},
		{transfer->meta->key, false, false},
		{transfer->auth_signer->key, false, false},
		{transfer->bridge_conf->key, true, false},
		{transfer->new_msg->key, true, true},
		{transfer->emitter->key, false, false},
		{transfer->seq_key->key, true, false},
		{transfer->fee_acc->key, true, false},

		// sysvar
		{&ctx->clock.key, false, false},
		{&ctx->rent.key, false, false},

		// programs
		{&ctx->progs.system, false, false},
		{&ctx->progs.wh_core, false, false},
		{&ctx->progs.spl, false, false},
	};


	mayan_debug("wormhole transfer wrapped > data");
	data_ptr = data;
	write_u8(data, &data_ptr, 4);
	write_u32(data, &data_ptr, transfer->nonce);
	write_u64(data, &data_ptr, transfer->amount);
	write_u64(data, &data_ptr, transfer->relayer_fee);
	write_buffer(data, &data_ptr, transfer->address, 32);
	write_u16(data, &data_ptr, transfer->chain);

	if (!check_buffer_is_done(data, data_ptr, SOL_ARRAY_SIZE(data))) {
		mayan_error("data is not full!");
		return ERROR_CUSTOM_ZERO;
	}

	mayan_debug("wormhole transfer wrapped > ix");
	ix.program_id = &(ctx->progs.wh_bridge);
	ix.accounts = accounts;
	ix.account_len = SOL_ARRAY_SIZE(accounts);
	ix.data = data;
	ix.data_len = SOL_ARRAY_SIZE(data);

	return mayan_invoke(ctx, &ix);
}
