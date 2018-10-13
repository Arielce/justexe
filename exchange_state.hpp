#pragma once

#include <cmath>
#include <eosiolib/asset.hpp>

using eosio::asset;
using eosio::symbol_type;

typedef double real_type;

// @abi table tokenmarket i64
struct exchange_state {
  asset supply;

  struct connector {
    asset balance;
    double weight;

    EOSLIB_SERIALIZE(connector, (balance)(weight))
  };

  connector base;
  connector quote;

  uint64_t primary_key() const { return supply.symbol; }

  asset convert_to_exchange(connector &c, asset in);
  asset convert_from_exchange(connector &c, asset in);
  asset convert(asset from, symbol_type to);
  asset convert2(asset from, symbol_type to);

  EOSLIB_SERIALIZE(exchange_state, (supply)(base)(quote))
};

typedef eosio::multi_index<N(tokenmarket), exchange_state> tokenmarket;

asset exchange_state::convert_to_exchange(connector &c, asset in) {

  real_type R(supply.amount);
  real_type C(c.balance.amount + in.amount);
  real_type F(c.weight / 1000.0);
  real_type T(in.amount);
  real_type ONE(1.0);

  real_type E = -R * (ONE - std::pow(ONE + T / C, F));
  // print( "E: ", E, "\n");
  int64_t issued = int64_t(E);

  supply.amount += issued;
  c.balance.amount += in.amount;

  return asset(issued, supply.symbol);
}

asset exchange_state::convert_from_exchange(connector &c, asset in) {
  eosio_assert(in.symbol == supply.symbol, "unexpected asset symbol input");

  real_type R(supply.amount - in.amount);
  real_type C(c.balance.amount);
  real_type F(1000.0 / c.weight);
  real_type E(in.amount);
  real_type ONE(1.0);
  real_type T = C * (std::pow(ONE + E / R, F) - ONE);
  // print( "T: ", T, "\n");
  int64_t out = int64_t(T);

  supply.amount -= in.amount;
  c.balance.amount -= out;

  return asset(out, c.balance.symbol);
}

asset exchange_state::convert(asset from, symbol_type to) {
  auto sell_symbol = from.symbol;
  auto base_symbol = base.balance.symbol;
  auto quote_symbol = quote.balance.symbol;
  real_type cw = real_type(base.weight) * real_type(quote.weight) / 1000000.0;
  real_type c2 = quote.balance.amount;
  real_type c1 = base.balance.amount;
  real_type x = from.amount;

  if (sell_symbol == quote_symbol) {
    c2 = base.balance.amount;
    c1 = quote.balance.amount;
  }

  real_type T = c2 * (std::pow(1 + x / (c1 + x), cw) - 1);
  int64_t out = int64_t(T);

  if (sell_symbol == quote_symbol) {
    quote.balance.amount += from.amount;
    base.balance.amount -= out;
    return asset(out, to);
  } else {
    base.balance.amount += from.amount;
    quote.balance.amount -= out;
    return asset(out, to);
  }
}

asset exchange_state::convert2(asset from, symbol_type to) {
  auto sell_symbol = from.symbol;
  auto ex_symbol = supply.symbol;
  auto base_symbol = base.balance.symbol;
  auto quote_symbol = quote.balance.symbol;
  if (sell_symbol != ex_symbol) {
    if (sell_symbol == base_symbol) {
      from = convert_to_exchange(base, from);
    } else if (sell_symbol == quote_symbol) {
      from = convert_to_exchange(quote, from);
    } else {
      eosio_assert(false, "invalid sell");
    }
  } else {
    if (to == base_symbol) {
      from = convert_from_exchange(base, from);
    } else if (to == quote_symbol) {
      from = convert_from_exchange(quote, from);
    } else {
      eosio_assert(false, "invalid conversion");
    }
  }

  if (to != from.symbol)
    return convert(from, to);
  return from;
}