#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/currency.hpp>
#include <eosiolib/eosio.hpp>

#include <string>

#include "exchange_state.hpp"



using namespace eosio;
using namespace std;

class justexe : public eosio::contract {

public:
  justexe(account_name self): contract(self),_global_inx(_self, _self),_dev_inx(_self,_self)  {}

  // @abi table globalinfo i64
  struct globalinfo {
    uint64_t id;
    uint64_t dev_pool;
    uint64_t dev_pool_with;
    uint64_t global_state;
    uint64_t primary_key() const { return id; }
    EOSLIB_SERIALIZE(globalinfo, (id)(dev_pool)(dev_pool_with)(global_state))
  };
  typedef eosio::multi_index<N(globalinfo), globalinfo> globalinfos;

  // @abi table devlist i64
  struct devlist {
    account_name user;
    account_name primary_key() const { return user; }
    EOSLIB_SERIALIZE(devlist, (user))
  };
  typedef eosio::multi_index<N(devlist), devlist> devlists;

  //@abi table accounts i64
  struct account {
    asset balance;
    uint64_t primary_key() const { return balance.symbol.name(); }
  };
  typedef eosio::multi_index<N(accounts), account> accounts;

  //@abi table stat i64
  struct cstats {
    asset supply;
    asset max_supply;
    account_name issuer;
    uint64_t primary_key() const { return supply.symbol.name(); }
  };
  typedef eosio::multi_index<N(stat), cstats> stats;

  devlists _dev_inx;
  globalinfos _global_inx;

  // @abi action
  void hi(account_name user);

  // @abi action
  void test(account_name user);

  // @abi action
  void create();
  
  // @abi action
  void adddevlist(account_name user);

  // @abi action
  void setting(uint64_t global_state);

  // @abi action
  void devwith(account_name user);
  
  void apply(account_name code, account_name action);

private:
  void on(const currency::transfer &t, account_name code);
  void update_quote(uint64_t delta);
  void update_base(uint64_t delta);
  void sell(account_name seller, asset quantity);
  void buy(account_name buyer, asset quantity);
  asset getbalance(account_name code,account_name scope, symbol_type sym);
  uint64_t get_flow(asset quantity);
  uint64_t get_charge_m(asset quantity);

public:
  struct transfer_args {
    account_name from;
    account_name to;
    asset quantity;
    string memo;
  };
};
