/**
* @brief C-based Helloworld BPF program
*/
#include <solana_sdk.h>
#include "sha1.h"

uint64_t jiri_test(SolParameters *params) {

       if (params->ka_num != 1) {
	       sol_log("Greeted account not included in the instruction");
	       return ERROR_NOT_ENOUGH_ACCOUNT_KEYS;
       }

       // Get the account to say hello to
       SolAccountInfo *sha_holder_account = &params->ka[0];

       // The account must be owned by the program in order to modify its data
       if (!SolPubkey_same(sha_holder_account->owner, params->program_id)) {
	       sol_log("Greeted account does not have the correct program id");
	       return ERROR_INCORRECT_PROGRAM_ID;
       }

       // The data must be large enough to hold an uint32_t value
       if (sha_holder_account->data_len < 20) {
	       sol_log("Greeted account data length too small to hold uint32_t value");
	       return ERROR_INVALID_ACCOUNT_DATA;
       }

       // Increment and store the number of times the account has been greeted
       char *message = (char *)params->data;

       SHA1_CTX sha_ctx = {0};
       SHA1Init(&sha_ctx);

       SHA1Final((unsigned char *)sha_holder_account->data, &sha_ctx);


       sol_log("DONE!");

       return SUCCESS;
}

extern uint64_t entrypoint(const uint8_t *input) {
       sol_log("Helloworld C program entrypoint");

       SolAccountInfo accounts[1];
       SolParameters params = (SolParameters){.ka = accounts};

       if (!sol_deserialize(input, &params, SOL_ARRAY_SIZE(accounts))) {
	       return ERROR_INVALID_ARGUMENT;
       }

       if (params.ka_num != 1) {
	       return ERROR_CUSTOM_ZERO;
       }

}