#include "justexe.hpp"

#define JUST_SYMBOL S(4, JUST)
#define EOS_SYMBOL S(4, EOS)

#define EOS_TOKEN_CONTRACT N(eosio.token)
#define JUST_TOKEN_CONTRACT N(eosjusttoken)
#define JUST_MINE_CONTRACT N(justminepool)

const uint64_t GLOBAL_OFF = 2;
const uint64_t GLOBAL_LIMIT = 4;
const uint64_t GLOBAL_ON = 6;
const uint64_t INIT_BASE=50000000000ll;
const uint64_t INIT_QUOTE=5000000ll;

void justexe::hi(account_name user) { require_auth(user); }

void justexe::test(account_name user) {
  uint128_t a=2*(uint128_t)UINT64_MAX;
  uint64_t b=(uint64_t)a;
  eosio_assert(a==b,"yes");
}

void justexe::create() {
  require_auth(_self);
  auto sym = S(4, SMT);
  tokenmarket tokenmarkettable(_self, _self);
  auto existing = tokenmarkettable.find(sym);
  eosio_assert(existing == tokenmarkettable.end(),
               "token with symbol already exists");
  auto itr_global = _global_inx.find(0);
  if (itr_global == _global_inx.end()) {
    _global_inx.emplace(_self, [&](auto &m) {
      m.id = 0;
      m.dev_pool = 0;
      m.dev_pool_with = 0;
      m.global_state = GLOBAL_OFF;
    });
  }
  tokenmarkettable.emplace(_self, [&](auto &m) {
    m.supply.amount = 100000000000000ll;
    m.supply.symbol = S(0, SMT);
    m.base.balance.amount = int64_t(INIT_BASE);
    m.base.balance.symbol = JUST_SYMBOL;
    m.base.weight = 1000;
    m.quote.balance.amount = int64_t(INIT_QUOTE);
    m.quote.balance.symbol = EOS_SYMBOL;
    m.quote.weight = 1000;
  });
}
void justexe::buy(account_name buyer, asset quantity) {
  require_auth(buyer);
  eosio_assert(quantity.amount > 0, "must purchase a positive amount");
  eosio_assert(quantity.symbol.is_valid(), "invalid symbol name");
  asset tokenout;
  tokenmarket tokenmarkettable(_self, _self);
  const auto &market =
      tokenmarkettable.get(S(0, SMT), "SMT market does not exist");
  tokenmarkettable.modify(market, 0, [&](auto &es) {
    tokenout = es.convert(quantity, JUST_SYMBOL);
  });
  
  uint64_t flow=get_flow(tokenout);
  if(flow<(INIT_BASE/2)){//开局早期，单账号不允许持仓大于100w
    accounts buy_account_inx(JUST_TOKEN_CONTRACT,buyer);
    uint64_t maybe=tokenout.amount;
    auto itr_acc_buyer=buy_account_inx.find(tokenout.symbol.name());
    if(itr_acc_buyer!=buy_account_inx.end()){
      maybe+=itr_acc_buyer->balance.amount;
    }
    eosio_assert(maybe<(INIT_BASE/5),"cant buy JUST more than 100w");
  }
  action(permission_level{_self, N(active)}, JUST_TOKEN_CONTRACT, N(transfer),
         make_tuple(_self, buyer, tokenout,
                    std::string("send JUST token to buyer")))
      .send();
}

void justexe::sell(account_name seller, asset quantity) {
  require_auth(seller);
  eosio_assert(quantity.amount > 0, "must purchase a positive amount");
  eosio_assert(quantity.symbol.is_valid(), "invalid symbol name");

  tokenmarket tokenmarkettable(_self, _self);
  auto itr = tokenmarkettable.find(S(0, SMT));
  asset eosout;
  tokenmarkettable.modify(
      itr, 0, [&](auto &es) { eosout = es.convert(quantity, EOS_SYMBOL); });

  // 5% fee
  uint64_t feeamount = 0;
  uint64_t charge_m = 150;
  uint64_t cal_charge_m = get_charge_m(quantity);
  if (cal_charge_m > 0 && cal_charge_m < 200) {
    charge_m = cal_charge_m;
  }
  uint128_t feeamount128 =
      (((uint128_t)eosout.amount) * ((uint128_t)charge_m)) / 1000;
  feeamount = (uint64_t)feeamount128;
  eosio_assert(feeamount == feeamount128, "fee overflow");
  eosio_assert(feeamount>0, "sell amount is too small");
  eosio_assert(eosout.amount>=feeamount, "fee overflow");
  eosout.amount -= feeamount;
  eosio_assert(eosout.amount>0, "sell amount is too small");
  // send eos back
  action(permission_level{_self, N(active)}, EOS_TOKEN_CONTRACT, N(transfer),
         make_tuple(_self, seller, eosout, std::string("sell JUST token")))
      .send();
  
  auto itr_global = _global_inx.find(0);
  _global_inx.modify(itr_global, _self,
                     [&](auto &m) { m.dev_pool += feeamount; });
}

uint64_t justexe::get_flow(asset quantity) {
  accounts mine_inx(JUST_TOKEN_CONTRACT, JUST_MINE_CONTRACT);
  accounts exe_inx(JUST_TOKEN_CONTRACT, _self);
  stats stats_inx(JUST_TOKEN_CONTRACT, quantity.symbol.name());
  auto itr_mine = mine_inx.find(quantity.symbol.name());
  auto itr_exe = exe_inx.find(quantity.symbol.name());
  auto itr_stat = stats_inx.find(quantity.symbol.name());
  uint64_t mine_amount = 0;
  uint64_t exe_amount = 0;
  uint64_t supply = 0;
  if (itr_mine != mine_inx.end()) {
    mine_amount = itr_mine->balance.amount;
  }
  if (itr_exe != exe_inx.end()) {
    exe_amount = itr_exe->balance.amount;
  }
  if (itr_stat != stats_inx.end()) {
    supply = itr_stat->supply.amount;
  }
  eosio_assert(supply >= mine_amount + exe_amount, "supply overflow");
  uint64_t flowamount = supply - (mine_amount + exe_amount);
  return flowamount;
}
uint64_t justexe::get_charge_m(asset quantity) {
  uint64_t charge_max = 150;
  uint64_t charge_min = 5;
  uint64_t charge_m = charge_max;
  uint64_t flowamount = get_flow(quantity);
  double permax = 200000000000.0;
  if (flowamount >= permax) {
    charge_m = charge_min;
  } else {
    double per = (double)flowamount / permax;
    charge_m = charge_max - (charge_max - charge_min) * (per);
  }
  return charge_m;
}

void justexe::update_quote(uint64_t delta) {
  tokenmarket tokenmarkettable(_self, _self);
  auto itr = tokenmarkettable.find(S(0, SMT));
  tokenmarkettable.modify(itr, 0,
                          [&](auto &m) { m.quote.balance.amount += delta; });
}

void justexe::update_base(uint64_t delta) {
  tokenmarket tokenmarkettable(_self, _self);
  auto itr = tokenmarkettable.find(S(0, SMT));
  tokenmarkettable.modify(itr, 0,
                          [&](auto &m) { m.base.balance.amount += delta; });
}
void justexe::adddevlist(account_name user) {
  require_auth(_self);
  eosio_assert(is_account(user), "user not found");
  auto itr_user = _dev_inx.find(user);
  eosio_assert(itr_user == _dev_inx.end(), "user in devlist");
  _dev_inx.emplace(_self, [&](auto &a) { a.user = user; });
}

void justexe::setting(uint64_t global_state) {
  require_auth(_self);
  auto itr_global = _global_inx.find(0);
  //游戏是否存在
  eosio_assert(itr_global != _global_inx.end(), "The game does not exist");
  _global_inx.modify(itr_global, _self,
                     [&](auto &a) { a.global_state = global_state; });
}
void justexe::devwith(account_name user) {
  require_auth(_self);
  auto itr_global = _global_inx.find(0);
  eosio_assert(itr_global != _global_inx.end(), "game not found");
  eosio_assert(is_account(user), "user not found");
  auto itr_user = _dev_inx.find(user);
  eosio_assert(itr_user != _dev_inx.end(), "user not in devlist");
  account_name dev_user = itr_user->user;
  uint64_t can_withdraw = itr_global->dev_pool - itr_global->dev_pool_with;
  asset withasset = asset(can_withdraw, string_to_symbol(4, "EOS"));
  action(
      permission_level{_self, N(active)}, EOS_TOKEN_CONTRACT, N(transfer),
      make_tuple(_self, dev_user, withasset, std::string("withdraw dev fee")))
      .send();
  _global_inx.modify(itr_global, _self,
                     [&](auto &a) { a.dev_pool_with = itr_global->dev_pool; });
}

void justexe::on(const currency::transfer &t, account_name code) {
  account_name from = t.from;
  account_name to = t.to;
  asset quantity = t.quantity;
  std::string memo = t.memo;
  if (from == _self || to != _self) {
    return;
  }
  require_auth(from);
  eosio_assert(quantity.amount > 0, "must purchase a positive amount");
  eosio_assert(quantity.symbol.is_valid(), "invalid symbol name");

  auto itr_global = _global_inx.find(0);
  if (itr_global != _global_inx.end()) {
    if (itr_global->global_state == GLOBAL_OFF) {
      eosio_assert(false, "ex not start yet");
    } else if (itr_global->global_state == GLOBAL_LIMIT) {
      auto itr_dev_user = _dev_inx.find(from);
      eosio_assert(itr_dev_user != _dev_inx.end(), "ex not start yet");
    }
  }
  // sell JUST
  if (quantity.symbol == JUST_SYMBOL) {
    if ("addbase" == memo) {
      update_base(quantity.amount);
    } else if ("no" == memo) {
      // do noting
    } else {
      sell(from, quantity);
    }
    // buy JUST
  } else if (quantity.symbol == EOS_SYMBOL) {
    if ("addquote" == memo) {
      update_quote(quantity.amount);
    } else if ("no" == memo) {
      // do noting
    } else {
      buy(from, quantity);
    }
  }
}

void justexe::apply(uint64_t code, uint64_t action) {
  if (action == N(onerror)) {
    eosio_assert(
        code == N(eosio),
        "onerror action's are only valid from the \"eosio\" system account");
  }
  if ((action == N(transfer) && code == JUST_TOKEN_CONTRACT) ||
      (action == N(transfer) && code == EOS_TOKEN_CONTRACT)) {
    on(unpack_action_data<currency::transfer>(), code);
    return;
  }
  auto &thiscontract = *this;
  switch (action) { EOSIO_API(justexe, (hi)(test)(create)(adddevlist)(setting)(devwith)); };
}
extern "C" {
[[noreturn]] void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  justexe gtb(receiver);
  gtb.apply(code, action);
  eosio_exit(0);
}
}