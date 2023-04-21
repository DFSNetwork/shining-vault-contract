#include <utils.hpp>
CONTRACT shiningvault : public contract{
   public:
      using contract::contract;
      static constexpr symbol rex_symbol = symbol(symbol_code("REX"), 4);
      static constexpr symbol EOS_TOKEN_SYMBOL = symbol("EOS", 4);
      static constexpr symbol USD_TOKEN_SYMBOL = symbol("USDT", 4);

      [[eosio::on_notify("eosio.token::transfer")]]
      void eos_in(name from, name to, asset quantity, string memo);
      ACTION unstake(name user, asset quantity);
      ACTION refund(uint64_t id);
      ACTION cancelrefund(uint64_t id);
      ACTION claim(name user);

      [[eosio::on_notify("tethertether::transfer")]]
      void usdt_in(name from, name to, asset quantity, string memo);
      ACTION unstake2(name user, asset quantity);
      ACTION refund2(uint64_t id);
      ACTION cancelrefund2(uint64_t id);
      ACTION claim2(name user);

      ACTION claimlog(name user, asset user_liquidity, asset pool_liquidity, double liquidity_ratio, uint32_t time_elapsed, asset pool_balance, asset reward);

   private:
      TABLE holder
      {
         name holder;
         asset bal;
         time_point_sec join_time;
         time_point_sec last_drip;
         uint64_t bybal() const { return bal.amount; }
         uint64_t primary_key() const { return holder.value; }
      };

      typedef multi_index<"holders"_n, holder, indexed_by<"bybal"_n, const_mem_fun<holder, uint64_t, &holder::bybal>>> holders;
      typedef multi_index<"holders2"_n, holder, indexed_by<"bybal"_n, const_mem_fun<holder, uint64_t, &holder::bybal>>> holders2;

      TABLE refund_st
      {
         uint64_t id;
         name user;
         asset bal;
         time_point_sec release_time;
         uint64_t byuser() const { return user.value; }
         uint64_t primary_key() const { return id; }
      };

      typedef multi_index<"refunds"_n, refund_st, indexed_by<"byuser"_n, const_mem_fun<refund_st, uint64_t, &refund_st::byuser>>> refunds;
      typedef multi_index<"refunds2"_n, refund_st, indexed_by<"byuser"_n, const_mem_fun<refund_st, uint64_t, &refund_st::byuser>>> refunds2;

      TABLE profit_st
      {
         name user;
         asset total_profit_eos;
         asset total_profit_usdt;
         uint64_t primary_key() const { return user.value; }
      };

      typedef multi_index<"profits"_n, profit_st> profits;

      TABLE global_var
      {
         name key;
         uint64_t val;
         uint64_t primary_key() const { return key.value; }
      };

      typedef multi_index<name("globals"), global_var> globals;
      uint64_t get_id(name key);

      struct rex_balance
      {
         uint8_t version = 0;
         name owner;
         asset vote_stake;
         asset rex_balance;
         int64_t matured_rex = 0;
         std::deque<std::pair<time_point_sec, int64_t>> rex_maturities; /// REX daily maturity buckets

         uint64_t primary_key() const { return owner.value; }
      };

      typedef multi_index<"rexbal"_n, rex_balance> rex_balance_table;

      TABLE logs_st
      {
         uint64_t id;
         name user;
         asset bal;
         string type;
         time_point_sec time;
         uint64_t byuser() const { return user.value; }
         uint64_t primary_key() const { return id; }
      };

      typedef multi_index<"logs"_n, logs_st, indexed_by<"byuser"_n, const_mem_fun<logs_st, uint64_t, &logs_st::byuser>>> logs;
      void write_log(name user, asset quantity, string type);
};