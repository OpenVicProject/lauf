// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/frontend/text.h>

#include <lauf/asm/builder.h>
#include <lauf/asm/module.h>
#include <lauf/reader.hpp>
#include <lauf/runtime/builtin.h>
#include <lexy/action/parse.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy_ext/report_error.hpp>
#include <map>
#include <string>

const lauf_frontend_text_options lauf_frontend_default_text_options = {nullptr};

namespace
{
template <typename T>
class symbol_table
{
public:
    const T* try_lookup(std::string_view name) const
    {
        auto iter = map.find(name);
        if (iter != map.end())
            return &iter->second;
        else
            return nullptr;
    }
    const T& lookup(std::string_view name) const
    {
        auto ptr = try_lookup(name);
        if (!ptr)
        {
            std::fprintf(stderr, "[lauf] text: unknown identifier '%s'\n",
                         std::string(name).c_str());
            std::abort();
        }
        return *ptr;
    }

    void clear()
    {
        map.clear();
    }

    void insert(std::string_view name, const T& data)
    {
        auto [pos, inserted] = map.emplace(name, data);
        if (!inserted)
            std::fprintf(stderr, "[lauf] text: duplicate declaration '%s'\n",
                         std::string(name).c_str());
    }

private:
    std::map<std::string, T, std::less<>> map;
};

struct parse_state
{
    lauf_asm_builder*                                  builder;
    symbol_table<const lauf_runtime_builtin_function*> builtins;

    mutable lauf_asm_module*                 mod;
    mutable symbol_table<lauf_asm_global*>   globals;
    mutable symbol_table<lauf_asm_function*> functions;
    mutable symbol_table<lauf_asm_block*>    blocks;

    parse_state(lauf_frontend_text_options opts)
    : builder(lauf_asm_create_builder(lauf_asm_default_build_options)), mod(nullptr)
    {
        for (auto builtin = opts.builtins; builtin != nullptr; builtin = builtin->next)
            builtins.insert(builtin->name, builtin);
    }
};
} // namespace

namespace lauf::text_grammar
{
namespace dsl = lexy::dsl;

template <typename Ret = void, typename... Fn>
constexpr auto callback(Fn... fn)
{
    return lexy::bind(lexy::callback<Ret>(fn...), lexy::parse_state, lexy::values);
}

//=== common ===//
struct identifier
{
    static constexpr auto rule = [] {
        auto unquoted = dsl::identifier(dsl::ascii::alpha_underscore / dsl::period,
                                        dsl::ascii::alpha_digit_underscore / dsl::period);

        auto quoted = dsl::single_quoted(dsl::unicode::print);

        return unquoted | quoted;
    }();

    static constexpr auto value = lexy::as_string<std::string>;
};

struct builtin_identifier
{
    static constexpr auto rule  = dsl::dollar_sign >> dsl::p<identifier>;
    static constexpr auto value = lexy::forward<std::string>;
};
struct global_identifier
{
    static constexpr auto rule  = dsl::at_sign >> dsl::p<identifier>;
    static constexpr auto value = lexy::forward<std::string>;
};
struct local_identifier
{
    static constexpr auto rule  = dsl::percent_sign >> dsl::p<identifier>;
    static constexpr auto value = lexy::forward<std::string>;
};

struct signature
{
    static constexpr auto rule  = dsl::parenthesized(dsl::integer<std::uint8_t> + LEXY_LIT("=>")
                                                     + dsl::integer<std::uint8_t>);
    static constexpr auto value = lexy::construct<lauf_asm_signature>;
};

struct builtin_ref
{
    static constexpr auto rule  = dsl::p<builtin_identifier>;
    static constexpr auto value = callback<lauf_runtime_builtin_function>(
        [](const parse_state& state, const std::string& name) {
            return *state.builtins.lookup(name);
        });
};
struct global_ref
{
    static constexpr auto rule = dsl::p<global_identifier>;
    static constexpr auto value
        = callback<lauf_asm_global*>([](const parse_state& state, const std::string& name) {
              return state.globals.lookup(name);
          });
};
struct function_ref
{
    static constexpr auto rule  = dsl::p<global_identifier> + dsl::if_(dsl::p<signature>);
    static constexpr auto value = callback<lauf_asm_function*>( //
        [](const parse_state& state, const std::string& name) {
            return state.functions.lookup(name);
        },
        [](const parse_state& state, const std::string& name, lauf_asm_signature sig) {
            if (auto f = state.functions.try_lookup(name))
                return *f;

            auto f = lauf_asm_add_function(state.mod, name.c_str(), sig);
            state.functions.insert(name, f);
            return f;
        });
};
struct block_ref
{
    static constexpr auto rule  = dsl::p<local_identifier> + dsl::if_(dsl::p<signature>);
    static constexpr auto value = callback<lauf_asm_block*>( //
        [](const parse_state& state, const std::string& name) { return state.blocks.lookup(name); },
        [](const parse_state& state, const std::string& name, lauf_asm_signature sig) {
            if (auto block = state.blocks.try_lookup(name))
                return *block;

            auto block = lauf_asm_declare_block(state.builder, sig);
            state.blocks.insert(name, block);
            return block;
        });
};

//=== global ===//
struct data_expr
{
    struct byte
    {
        static constexpr auto rule = dsl::integer<unsigned char>(dsl::digits<dsl::hex>);
        static constexpr auto value
            = lexy::callback<std::string>([](unsigned char c) { return std::string(1, char(c)); });
    };

    struct string
    {
        static constexpr auto rule  = dsl::quoted(dsl::ascii::print);
        static constexpr auto value = lexy::as_string<std::string>;
    };

    struct repetition
    {
        static constexpr auto rule = dsl::square_bracketed(dsl::recurse<data_expr>)
                                     >> dsl::lit_c<'*'> + dsl::integer<std::size_t>;

        static constexpr auto value
            = lexy::callback<std::string>([](const std::string& data, std::size_t n) {
                  std::string result;
                  for (auto i = 0u; i != n; ++i)
                      result += data;
                  return result;
              });
    };

    static constexpr auto rule
        = dsl::list(dsl::p<byte> | dsl::p<string> | dsl::p<repetition>, dsl::sep(dsl::comma));
    static constexpr auto value = lexy::concat<std::string>;
};

struct global_decl
{
    struct const_global
    {
        static constexpr auto rule
            = LEXY_LIT("const") >> dsl::p<global_identifier> + dsl::equal_sign + dsl::p<data_expr>;

        static constexpr auto value = callback(
            [](const parse_state& state, const std::string& name, const std::string& data) {
                auto g = lauf_asm_add_global_const_data(state.mod, data.c_str(), data.size());
                state.globals.insert(name, g);
            });
    };

    struct mut_global
    {
        static constexpr auto rule = [] {
            auto zero_expr = LEXY_LIT("zero") >> dsl::lit_c<'*'> + dsl::integer<std::size_t>;
            auto expr      = zero_expr | dsl::else_ >> dsl::p<data_expr>;

            return dsl::else_ >> dsl::p<global_identifier> + dsl::equal_sign + expr;
        }();

        static constexpr auto value = callback(
            [](const parse_state& state, const std::string& name, const std::string& data) {
                auto g = lauf_asm_add_global_mut_data(state.mod, data.c_str(), data.size());
                state.globals.insert(name, g);
            },
            [](const parse_state& state, const std::string& name, std::size_t size) {
                auto g = lauf_asm_add_global_zero_data(state.mod, size);
                state.globals.insert(name, g);
            });
    };

    static constexpr auto rule
        = LEXY_LIT("global") >> (dsl::p<const_global> | dsl::p<mut_global>)+dsl::semicolon;
    static constexpr auto value = lexy::forward<void>;
};

//=== instruction ===//
template <typename Fn>
constexpr auto inst(Fn fn)
{
    return callback([fn](const parse_state& state, auto... args) { fn(state.builder, args...); });
}

struct inst_return
{
    static constexpr auto rule  = LEXY_LIT("return");
    static constexpr auto value = inst(&lauf_asm_inst_return);
};
struct inst_jump
{
    static constexpr auto rule  = LEXY_LIT("jump") >> dsl::p<block_ref>;
    static constexpr auto value = inst(&lauf_asm_inst_jump);
};
struct inst_branch2
{
    static constexpr auto rule  = LEXY_LIT("branch2") >> dsl::p<block_ref> + dsl::p<block_ref>;
    static constexpr auto value = inst(&lauf_asm_inst_branch2);
};
struct inst_branch3
{
    static constexpr auto rule
        = LEXY_LIT("branch3") >> dsl::p<block_ref> + dsl::p<block_ref> + dsl::p<block_ref>;
    static constexpr auto value = inst(&lauf_asm_inst_branch3);
};
struct inst_panic
{
    static constexpr auto rule  = LEXY_LIT("panic");
    static constexpr auto value = inst(&lauf_asm_inst_panic);
};

struct inst_sint
{
    static constexpr auto rule  = LEXY_LIT("sint") >> dsl::integer<lauf_sint>;
    static constexpr auto value = inst(&lauf_asm_inst_sint);
};
struct inst_uint
{
    static constexpr auto rule  = LEXY_LIT("uint") >> dsl::integer<lauf_uint>;
    static constexpr auto value = inst(&lauf_asm_inst_uint);
};
struct inst_global_addr
{
    static constexpr auto rule  = LEXY_LIT("global_addr") >> dsl::p<global_ref>;
    static constexpr auto value = inst(&lauf_asm_inst_global_addr);
};

struct inst_pop
{
    static constexpr auto rule  = LEXY_LIT("pop") >> dsl::integer<std::uint16_t>;
    static constexpr auto value = inst(&lauf_asm_inst_pop);
};
struct inst_pick
{
    static constexpr auto rule  = LEXY_LIT("pick") >> dsl::integer<std::uint16_t>;
    static constexpr auto value = inst(&lauf_asm_inst_pick);
};
struct inst_roll
{
    static constexpr auto rule  = LEXY_LIT("roll") >> dsl::integer<std::uint16_t>;
    static constexpr auto value = inst(&lauf_asm_inst_roll);
};

struct inst_call
{
    static constexpr auto rule  = LEXY_LIT("call") >> dsl::p<function_ref>;
    static constexpr auto value = inst(&lauf_asm_inst_call);
};
struct inst_call_builtin
{
    static constexpr auto rule  = dsl::p<builtin_ref>;
    static constexpr auto value = inst(&lauf_asm_inst_call_builtin);
};

struct instruction
{
    static constexpr auto rule = [] {
        auto nested = dsl::square_bracketed.list(dsl::recurse<instruction>);

        auto single = dsl::p<inst_return> | dsl::p<inst_jump>                            //
                      | dsl::p<inst_branch2> | dsl::p<inst_branch3> | dsl::p<inst_panic> //
                      | dsl::p<inst_sint> | dsl::p<inst_uint> | dsl::p<inst_global_addr> //
                      | dsl::p<inst_pop> | dsl::p<inst_pick> | dsl::p<inst_roll>         //
                      | dsl::p<inst_call> | dsl::p<inst_call_builtin>;

        return nested | dsl::else_ >> single + dsl::semicolon;
    }();

    static constexpr auto value = lexy::forward<void>;
};

//=== function ===//
struct block
{
    struct header
    {
        static constexpr auto rule
            = LEXY_LIT("block") + dsl::p<local_identifier> + dsl::p<signature>;
        static constexpr auto value = callback(
            [](const parse_state& state, const std::string& name, lauf_asm_signature sig) {
                lauf_asm_block* block;
                if (auto b = state.blocks.try_lookup(name))
                {
                    block = *b;
                }
                else
                {
                    block = lauf_asm_declare_block(state.builder, sig);
                    state.blocks.insert(name, block);
                }

                lauf_asm_build_block(state.builder, block);
            });
    };

    static constexpr auto rule  = dsl::p<header> + dsl::curly_bracketed.list(dsl::p<instruction>);
    static constexpr auto value = lexy::forward<void>;
};

struct function_decl
{
    struct header
    {
        static constexpr auto rule = dsl::p<global_identifier> + dsl::p<signature>;

        static constexpr auto value = callback(
            [](const parse_state& state, const std::string& name, lauf_asm_signature sig) {
                lauf_asm_function* fn;
                if (auto f = state.functions.try_lookup(name))
                {
                    fn = *f;
                }
                else
                {
                    fn = lauf_asm_add_function(state.mod, name.c_str(), sig);
                    state.functions.insert(name, fn);
                }

                lauf_asm_build(state.builder, state.mod, fn);
                state.blocks.clear();
            });
    };

    struct body
    {
        static constexpr auto rule = dsl::curly_bracketed.list(dsl::p<block>);

        static constexpr auto value = lexy::noop >> callback([](const parse_state& state) {
                                          lauf_asm_build_finish(state.builder);
                                      });
    };

    static constexpr auto rule
        = LEXY_LIT("function") >> dsl::p<header> + (dsl::semicolon | dsl::p<body>);
    static constexpr auto value = lexy::forward<void>;
};

//=== module ===//
struct module_header
{
    static constexpr auto rule  = LEXY_LIT("module") + dsl::p<global_identifier> + dsl::semicolon;
    static constexpr auto value = callback([](const parse_state& state, const std::string& name) {
        state.mod = lauf_asm_create_module(name.c_str());
    });
};

struct module_decl
{
    static constexpr auto whitespace
        = dsl::ascii::space | dsl::hash_sign >> dsl::until(dsl::newline);

    static constexpr auto rule
        = dsl::p<module_header> //
          + dsl::terminator(dsl::eof).opt_list(dsl::p<global_decl> | dsl::p<function_decl>);
    static constexpr auto value = lexy::forward<void>;
};
} // namespace lauf::text_grammar

lauf_asm_module* lauf_frontend_text(lauf_reader* reader, lauf_frontend_text_options opts)
{
    parse_state state(opts);

    auto result = lexy::parse<lauf::text_grammar::module_decl>(reader->buffer, state,
                                                               lexy_ext::report_error);
    if (!result)
    {
        if (state.mod == nullptr)
            lauf_asm_destroy_module(state.mod);
        state.mod = nullptr;
    }

    lauf_asm_destroy_builder(state.builder);
    return state.mod;
}
