#include <types.hpp>

namespace utils
{
   string uint64_string(uint64_t input)
   {
      string result;
      uint8_t base = 10;
      do
      {
         char c = input % base;
         input /= base;
         if (c < 10)
            c += '0';
         else
            c += 'A' - 10;
         result = c + result;
      } while (input);
      return result;
   }

   asset get_supply(const name &token_contract_account, const symbol_code &sym_code)
   {
      stats statstable(token_contract_account, sym_code.raw());
      string err_msg = "invalid token contract: ";
      err_msg.append(token_contract_account.to_string());
      const auto &st = statstable.require_find(sym_code.raw(), err_msg.c_str());
      return st->supply;
   }

   asset get_balance(const name &token_contract_account, const name &owner, const symbol_code &sym_code)
   {
      accounts accountstable(token_contract_account, owner.value);

      auto itr = accountstable.find(sym_code.raw());
      if (itr == accountstable.end())
      {
         asset supply = get_supply(token_contract_account, sym_code);
         return asset(0, supply.symbol);
      }
      else
      {
         return itr->balance;
      }
   }

   void inline_transfer(name contract, name from, name to, asset quantity, string memo)
   {
      if (quantity.amount > 0)
         action(permission_level{ from, "active"_n }, contract, name("transfer"), make_tuple(from, to, quantity, memo)).send();
   }

   vector<string> split(const string &str, const string &delim)
   {
      vector<string> strs;
      size_t prev = 0, pos = 0;
      do
      {
         pos = str.find(delim, prev);
         if (pos == string::npos)
            pos = str.length();
         string s = str.substr(prev, pos - prev);
         if (!str.empty())
            strs.push_back(s);
         prev = pos + delim.length();
      } while (pos < str.length() && prev < str.length());
      return strs;
   }

} // namespace utils