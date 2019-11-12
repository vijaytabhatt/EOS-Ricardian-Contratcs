#include "riccontract.hpp"

void riccontract::createcontract(account_name creator, account_name contract_name, asset deposit,
                          string ricardian, uint64_t expiration, account_name arbitrator) {
    require_auth(creator);

    eosio_assert(contract_name != 0, "please specify contract name");
    eosio_assert(deposit.amount > 0, "amount to be deposited must be greater than zero");
    eosio_assert(ricardian.length() > 0, "ricardian contract length cannot be zero");
    eosio_assert(expiration > now(), "expiration date must be greater then current time");
    eosio_assert(is_account(arbitrator), "Arbitrator must be a real account");

    contracts_table contracts(_self, _self);
    contracts.emplace(creator, [&](contract_type &contract) {
        contract.creator = creator;
        contract.name = contract_name;
        contract.deposit = deposit;
        contract.ricardian = ricardian;
        contract.expiration = expiration;
        contract.arbitrator = arbitrator;
    });

    action(permission_level{creator, N(active)},
           N(eosio.token), N(transfer),
           std::make_tuple(creator, _self, deposit,
                           string("security deposit for contract") + name{contract_name}.to_string()))
        .send();
}



void riccontract::closecontract(account_name contract_name) {
    contracts_table contracts(_self, _self);
    auto &contract = contracts.get(contract_name, "contract with given name does not exist");

    require_auth(contract.creator);

    eosio_assert(contract.active_claims == 0, "contract has claims which are active , cannot close contract");
    eosio_assert(contract.expiration <= now() || contract.deposit.amount == 0,
                 "contract not expired");

    contracts.erase(contract);

    if (contract.deposit.amount != 0) {
        action(permission_level{_self, N(active)},
               N(eosio.token), N(transfer),
               std::make_tuple(_self, contract.creator, contract.deposit,
                               string("close contract") + name{contract_name}.to_string()))
            .send();
    }
}

void riccontract::createclaim(account_name claimer, account_name contract_name,
                           account_name claim_name, asset amount, string details, string language) {
    require_auth(claimer);

    contracts_table contracts(_self, _self);
    auto &contract = contracts.get(contract_name, "contract with given name does not exist");

    eosio_assert(claim_name != 0, "please specify claim name");
    eosio_assert(contract.deposit.amount > 0, "no funds present in contract");
    eosio_assert(details.length() > 0, "please specify details of claim");
    eosio_assert(language.length() > 0, "please specify language for details of claim");

    
    eosio_assert(amount.amount > 0, "amount must be greater than zero");
    int64_t deposit = amount.amount * this->claim_security_deposit;
    int64_t fee = deposit * this->arbitrator_fee;
    eosio_assert(deposit > 0 && fee > 0, "amount too small to process");

    contracts.modify(contract, 0, [&](contract_type &contract) {
        contract.active_claims++;
    });

    claims_table claims(_self, _self);
    claims.emplace(claimer, [&](claim_type &claim) {
        claim.name = claim_name;
        claim.claimer = claimer;
        claim.contract_name = contract_name;
        claim.amount = amount;
        claim.details = details;
        claim.language = language;
        claim.expiration = now() + this->claimexpiretime;
    });

    action(permission_level{claimer, N(active)},
           N(eosio.token), N(transfer),
           std::make_tuple(claimer, _self, asset(deposit, S(4, EOS)),
                           string("deposit for claim ") + name{claim_name}.to_string()))
        .send();
}



void riccontract::ruleclaim(account_name claim_name, bool authorize, string details) {
    claims_table claims(_self, _self);
    auto &claim = claims.get(claim_name, "please specify claim name");
    contracts_table contracts(_self, _self);
    auto &contract = contracts.get(claim.contract_name, "contract with claim not found");

    require_auth(contract.arbitrator);

    eosio_assert(claim.expiration > now(), "claim expired");
    eosio_assert(contract.deposit.amount > 0, "deposit too less to rule contract");

    asset balance = asset(claim.amount.amount * this->claim_security_deposit, S(4, EOS));
    asset arbitrator_fee = asset(balance.amount * this->arbitrator_fee, S(4, EOS));
    balance -= arbitrator_fee;

    action(permission_level{_self, N(active)},
           N(eosio.token), N(transfer),
           std::make_tuple(_self, contract.arbitrator, arbitrator_fee,
                           string("fee for claim ") + name{claim_name}.to_string()))
        .send();

    if (authorize) {
        if (claim.amount <= contract.deposit) {
            balance += claim.amount;
            contracts.modify(contract, 0, [&](contract_type &contract) {
                contract.deposit -= claim.amount;
            });
        } else {
            balance += contract.deposit;
            contracts.modify(contract, 0, [&](contract_type &contract) {
                contract.deposit = asset(0, S(4, EOS));
            });
        }
        action(permission_level{_self, N(active)},
               N(eosio.token), N(transfer),
               std::make_tuple(_self, claim.claimer, balance,
                               string("approved claim ") + name{claim_name}.to_string()))
            .send();
    } else {
        action(permission_level{_self, N(active)},
               N(eosio.token), N(transfer),
               std::make_tuple(_self, contract.creator, balance,
                               string("Compensation for rejected claim ") + name{claim_name}.to_string()))
            .send();
    }

    claims.erase(claim);
    contracts.modify(contract, 0, [](contract_type &contract) {
        contract.active_claims--;
    });
}

void riccontract::closeclaim(account_name claim_name) {
    claims_table claims(_self, _self);
    auto &claim = claims.get(claim_name, "contract not found");
    contracts_table contracts(_self, _self);
    auto &contract = contracts.get(claim.contract_name, "contract with claim not found");

    require_auth(claim.claimer);

    eosio_assert(now() >= claim.expiration || contract.deposit.amount == 0,
                 "contract has funds , cannot be closed");

    action(permission_level{_self, N(active)},
           N(eosio.token), N(transfer),
           std::make_tuple(_self, claim.claimer,
                           asset(claim.amount.amount * this->claim_security_deposit, S(4, EOS)),
                           string("closed claim ") + name{claim_name}.to_string()))
        .send();

    claims.erase(claim);
    contracts.modify(contract, 0, [](contract_type &contract) {
        contract.active_claims--;
    });
}
