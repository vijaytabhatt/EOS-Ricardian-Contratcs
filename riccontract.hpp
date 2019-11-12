#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/eosio.hpp>

using namespace eosio;
using std::string;

namespace models {
	
	    struct claim_type {
        account_name name;
        account_name claimer;
        account_name contract_name;
        asset amount;
        string details;
        string language;
        uint64_t expiration;

        uint64_t primary_key() const { return name; }
    };
	
    struct contract_type {
        account_name name;
        account_name creator;
        asset deposit;
        string ricardian;
        uint64_t expiration;
        account_name arbitrator;
        uint32_t active_claims = 0;

        uint64_t primary_key() const { return name; }
    };

    typedef eosio::multi_index<N(contracts), contract_type> contracts_table;

    typedef eosio::multi_index<N(claims), claim_type> claims_table;
}

using namespace models;

class riccontract : public contract {
  public:
    using contract::contract;

    const uint64_t claimexpiretime = 14 * 24 * 60 * 60;
    const float claim_security_deposit = 0.1;
    const float arbitrator_fee = 0.5;

    void createcontract(account_name creator, account_name contract_name, asset deposit,
                    string ricardian, uint64_t expiration, account_name arbitrator);
   
    void closecontract(account_name contract_name);
    void createclaim(account_name claimer, account_name contract_name,
                     account_name claim_name, asset amount, string details, string language);
   
    void ruleclaim(account_name claim_name, bool authorize, string details);
    void closeclaim(account_name claim_name);
};

EOSIO_ABI(riccontract, (createcontract)(closecontract)(createclaim)(ruleclaim)(closeclaim))
