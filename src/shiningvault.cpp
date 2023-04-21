#include <shiningvault.hpp>

void shiningvault::eos_in(name from, name to, asset quantity, string memo)
{
   if (from == _self || to != _self)
      return;

   if (from == name("eosio.rex"))
      return;

   vector<string> strs = utils::split(memo, ":");
   string act = strs[0];

   if (act == "stake")
   {
      holders _holders(get_self(), get_self().value);
      auto h_itr = _holders.find(from.value);

      if (h_itr != _holders.end())
      {
         claim(from);
         _holders.modify(h_itr, same_payer, [ & ](auto &s) {
            s.bal += quantity;
            s.last_drip = current_time_point();
         });
      }
      else
      {
         _holders.emplace(get_self(), [ & ](auto &s) {
            s.holder = from;
            s.bal = quantity;
            s.join_time = current_time_point();
            s.last_drip = current_time_point();
         });
      }

      // buy rex
      action(permission_level{ get_self(), "active"_n }, name("eosio"), name("deposit"), make_tuple(get_self(), quantity)).send();
      action(permission_level{ get_self(), "active"_n }, name("eosio"), name("buyrex"), make_tuple(get_self(), quantity)).send();

      // log 
      write_log(from, quantity, "stake");
   }
   else
   {
      check(false, "invalid memo");
   }
}

ACTION shiningvault::unstake(name user, asset quantity)
{
   require_auth(user);
   holders _holders(get_self(), get_self().value);
   auto hitr = _holders.require_find(user.value, "holder not found");

   check(quantity.amount > 0, "invalid quantity, shoule be positive");
   check(hitr->bal >= quantity, "unstake quantity shoule less than stake balance");

   _holders.modify(hitr, user, [ & ](auto &s) {
      s.bal -= quantity;
   });

   refunds _refunds(get_self(), get_self().value);
   uint32_t refund_delay_sec = 86400 * 3; // 3天
   const time_point_sec new_release_time{ current_time_point().sec_since_epoch() + refund_delay_sec };
   _refunds.emplace(user, [ & ](auto &s) {
      s.id = get_id(name("refund"));
      s.user = user;
      s.bal = quantity;
      s.release_time = new_release_time;
   });

   // log 
   write_log(user, quantity, "unstake");
}

ACTION shiningvault::refund(uint64_t id)
{
   refunds _refunds(get_self(), get_self().value);
   auto itr = _refunds.require_find(id, "refund not found");
   check(itr->release_time.sec_since_epoch() <= current_time_point().sec_since_epoch(), "refund is not available yet");

   // sell rex and withraw rex fund
   asset rex_balance(itr->bal.amount * 10000, rex_symbol);
   action(permission_level{ get_self(), "active"_n }, name("eosio"), name("sellrex"), make_tuple(get_self(), rex_balance)).send();
   action(permission_level{ get_self(), "active"_n }, name("eosio"), name("withdraw"), make_tuple(get_self(), itr->bal)).send();
   // send to user
   utils::inline_transfer(name("eosio.token"), get_self(), itr->user, itr->bal, string("unstake refund"));

   // log 
   write_log(itr->user, itr->bal, "refund");

   _refunds.erase(itr);
}

ACTION shiningvault::cancelrefund(uint64_t id)
{
   refunds _refunds(get_self(), get_self().value);
   auto itr = _refunds.require_find(id, "refund not found");
   require_auth(itr->user);

   holders _holders(get_self(), get_self().value);
   auto hitr = _holders.require_find(itr->user.value, "holder not found");
   _holders.modify(hitr, same_payer, [ & ](auto &s) {
      s.bal += itr->bal;
   });

   // log 
   write_log(itr->user, itr->bal, "cancelrefund");

   _refunds.erase(itr);
}

uint64_t shiningvault::get_id(name key)
{
   globals _globals(get_self(), get_self().value);
   auto itr = _globals.find(key.value);
   uint64_t id = 1;
   if (itr != _globals.end())
   {
      id = itr->val + 1;
      _globals.modify(itr, same_payer, [ & ](auto &s) {
         s.key = key;
         s.val = id;
      });
   }
   else
   {
      _globals.emplace(get_self(), [ & ](auto &s) {
         s.key = key;
         s.val = id;
      });
   }
   return id;
}

ACTION shiningvault::claim(name user)
{
   require_auth(user);

   // step1: check holder
   holders _holders(get_self(), get_self().value);
   auto h_itr = _holders.find(user.value);
   if (h_itr == _holders.end())
      return;

   if (current_time_point().sec_since_epoch() <= h_itr->last_drip.sec_since_epoch())
      return;

   uint32_t time_elapsed = current_time_point().sec_since_epoch() - h_itr->last_drip.sec_since_epoch();

   rex_balance_table _rexbalance(name("eosio"), name("eosio").value);
   auto rex_bal_itr = _rexbalance.require_find(get_self().value, "rexbal not found");
   asset rex_bal = rex_bal_itr->vote_stake;
   double liquidity_ratio = (double) h_itr->bal.amount / (double) rex_bal.amount;
   if (liquidity_ratio > 1)
      liquidity_ratio = 1;

   asset eos_mining_pool_balance = utils::get_balance(name("eosio.token"), name("shiningpool1"), EOS_TOKEN_SYMBOL.code());

   uint64_t eos_mining_reward_amount = eos_mining_pool_balance.amount - eos_mining_pool_balance.amount * pow(0.9999, time_elapsed * liquidity_ratio);
   asset eos_mining_reward = asset(eos_mining_reward_amount, EOS_TOKEN_SYMBOL);
   utils::inline_transfer(name("eosio.token"), name("shiningpool1"), user, eos_mining_reward, string("DFS Vault mining reward"));
   action(permission_level{ get_self(), name("active") }, get_self(), name("claimlog"),
      make_tuple(user, h_itr->bal, rex_bal, liquidity_ratio, time_elapsed, eos_mining_pool_balance, eos_mining_reward)).send();

   // update user state
   _holders.modify(h_itr, same_payer, [ & ](auto &s) {
      s.last_drip = current_time_point();
   });

   // log 
   write_log(user, eos_mining_reward, "claim");
}

ACTION shiningvault::claimlog(name user, asset user_liquidity, asset pool_liquidity, double liquidity_ratio, uint32_t time_elapsed, asset pool_balance, asset reward)
{
   require_auth(get_self());

   // 历史收益统计
   profits _profits(get_self(), get_self().value);
   auto itr = _profits.find(user.value);
   if (itr == _profits.end())
   {
      _profits.emplace(get_self(), [ & ](auto &s) {
         s.user = user;
         if (reward.symbol == EOS_TOKEN_SYMBOL)
         {
            s.total_profit_eos = reward;
            s.total_profit_usdt = asset(0, USD_TOKEN_SYMBOL);
         }
         if (reward.symbol == USD_TOKEN_SYMBOL)
         {
            s.total_profit_eos = asset(0, EOS_TOKEN_SYMBOL);
            s.total_profit_usdt = reward;
         }
      });
   }
   else
   {
      _profits.modify(itr, same_payer, [ & ](auto &s) {
         if (reward.symbol == EOS_TOKEN_SYMBOL)
         {
            s.total_profit_eos += reward;
         }
         if (reward.symbol == USD_TOKEN_SYMBOL)
         {
            s.total_profit_usdt += reward;
         }
      });
   }
}

// ------------ usdt 部分
void shiningvault::usdt_in(name from, name to, asset quantity, string memo)
{
   if (from == _self || to != _self)
      return;

   vector<string> strs = utils::split(memo, ":");
   string act = strs[0];

   if (act == "test")
      return;

   if (act == "stake")
   {
      holders2 _holders(get_self(), get_self().value);
      auto h_itr = _holders.find(from.value);

      if (h_itr != _holders.end())
      {
         claim2(from);
         _holders.modify(h_itr, same_payer, [ & ](auto &s) {
            s.bal += quantity;
            s.last_drip = current_time_point();
         });
      }
      else
      {
         _holders.emplace(get_self(), [ & ](auto &s) {
            s.holder = from;
            s.bal = quantity;
            s.join_time = current_time_point();
            s.last_drip = current_time_point();
         });
      }

      // log 
      write_log(from, quantity, "stake");
   }
   else
   {
      check(false, "invalid memo");
   }
}

ACTION shiningvault::unstake2(name user, asset quantity)
{
   require_auth(user);
   holders2 _holders(get_self(), get_self().value);
   auto hitr = _holders.require_find(user.value, "holder not found");

   check(quantity.amount > 0, "invalid quantity, shoule be positive");
   check(hitr->bal >= quantity, "unstake quantity shoule less than stake balance");

   _holders.modify(hitr, user, [ & ](auto &s) {
      s.bal -= quantity;
   });

   refunds2 _refunds(get_self(), get_self().value);
   uint32_t refund_delay_sec = 86400 * 1; // 1天
   const time_point_sec new_release_time{ current_time_point().sec_since_epoch() + refund_delay_sec };
   _refunds.emplace(user, [ & ](auto &s) {
      s.id = get_id(name("refund"));
      s.user = user;
      s.bal = quantity;
      s.release_time = new_release_time;
   });

   // log 
   write_log(user, quantity, "unstake");
}

ACTION shiningvault::refund2(uint64_t id)
{
   refunds2 _refunds(get_self(), get_self().value);
   auto itr = _refunds.require_find(id, "refund not found");
   check(itr->release_time.sec_since_epoch() <= current_time_point().sec_since_epoch(), "refund is not available yet");
   // send to user
   utils::inline_transfer(name("tethertether"), get_self(), itr->user, itr->bal, string("unstake refund"));
   // log 
   write_log(itr->user, itr->bal, "refund");
   _refunds.erase(itr);
}

ACTION shiningvault::cancelrefund2(uint64_t id)
{
   refunds2 _refunds(get_self(), get_self().value);
   auto itr = _refunds.require_find(id, "refund not found");
   require_auth(itr->user);

   holders2 _holders(get_self(), get_self().value);
   auto hitr = _holders.require_find(itr->user.value, "holder not found");
   _holders.modify(hitr, same_payer, [ & ](auto &s) {
      s.bal += itr->bal;
   });

   // log 
   write_log(itr->user, itr->bal, "cancelrefund");

   _refunds.erase(itr);
}

ACTION shiningvault::claim2(name user)
{
   require_auth(user);

   // step1: check holder
   holders2 _holders(get_self(), get_self().value);
   auto h_itr = _holders.find(user.value);
   if (h_itr == _holders.end())
      return;

   if (current_time_point().sec_since_epoch() <= h_itr->last_drip.sec_since_epoch())
      return;

   uint32_t time_elapsed = current_time_point().sec_since_epoch() - h_itr->last_drip.sec_since_epoch();
   asset usdt_bal = utils::get_balance(name("tethertether"), get_self(), USD_TOKEN_SYMBOL.code());
   double liquidity_ratio = (double) h_itr->bal.amount / (double) usdt_bal.amount;
   if (liquidity_ratio > 1)
      liquidity_ratio = 1;

   asset mining_pool_balance = utils::get_balance(name("tethertether"), name("shiningpool1"), USD_TOKEN_SYMBOL.code());
   uint64_t mining_reward_amount = mining_pool_balance.amount - mining_pool_balance.amount * pow(0.9999, time_elapsed * liquidity_ratio);
   asset mining_reward = asset(mining_reward_amount, USD_TOKEN_SYMBOL);
   utils::inline_transfer(name("tethertether"), name("shiningpool1"), user, mining_reward, string("DFS Vault mining reward"));
   action(permission_level{ get_self(), name("active") }, get_self(), name("claimlog"),
      make_tuple(user, h_itr->bal, usdt_bal, liquidity_ratio, time_elapsed, mining_pool_balance, mining_reward)).send();

   // update user state
   _holders.modify(h_itr, same_payer, [ & ](auto &s) {
      s.last_drip = current_time_point();
   });

   // log 
   write_log(user, mining_reward, "claim");
}

void shiningvault::write_log(name user, asset quantity, string type)
{
   logs _logs(get_self(), get_self().value);

   _logs.emplace(get_self(), [ & ](auto &s) {
      s.id = get_id(name("log"));
      s.user = user;
      s.bal = quantity;
      s.type = type;
      s.time = current_time_point();
   });
}