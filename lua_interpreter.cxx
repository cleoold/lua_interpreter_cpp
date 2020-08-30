#include "lua.hpp"

#include "lua_interpreter.hxx"

using namespace luai;

// tags
enum class whahaha {
    GLOBAL, TABLE
};

struct lua_interpreter::impl {
    lua_State *L;

    impl() {
        auto state = luaL_newstate();
        if (state == NULL)
            throw std::runtime_error{"cannot create lua state: out of memory"};
        L = state;
    }

    impl(impl &&) noexcept = default;
    impl &operator=(impl &&) noexcept = default;

    void openlibs() noexcept {
        luaL_openlibs(L);
    }

    // pop 0, push 0
    std::tuple<bool, std::string> run_chunk(const char *code) noexcept {
        auto error = luaL_loadstring(L, code) || lua_pcall(L, 0, 0, 0);
        if (error) {
            auto errmsg = lua_tostring(L, -1);
            lua_pop(L, 1); // remove err msg
            return { false, errmsg };
        }
        return { true, {} };
    }

    // pop 0, push 1
    template<whahaha VarWhere>
    void get_by_name(const char *varname) noexcept;

    // pop 0, push 0
    template<whahaha VarWhere, class R, class Cvrt, class Check>
    R get_what_impl(const char *varname, Cvrt &&cvrtfunc, Check &&checkfunc, const char *throwmsg) {
        get_by_name<VarWhere>(varname);
        if (!checkfunc(L, -1)) {
            lua_pop(L, 1);
            throw std::runtime_error{std::string{"variable "} + varname + " is not " + throwmsg};
        }
        auto result = cvrtfunc(L, -1, NULL);
        lua_pop(L, 1);
        return {result};
    }

    // pop 0, push 1
    template<whahaha VarWhere>
    void push_table(const char *varname) {
        get_by_name<VarWhere>(varname);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            throw std::runtime_error{std::string{"variable "} + varname + " is not table"};
        }
    }

    // pop 1, push 0
    void pop_top_table() {
        lua_pop(L, 1);
    }

    ~impl() {
        lua_close(L);
    }
};

template<>
void lua_interpreter::impl::get_by_name<whahaha::GLOBAL>(const char *varname) noexcept {
    lua_getglobal(L, varname);
}

template<>
void lua_interpreter::impl::get_by_name<whahaha::TABLE>(const char *varname) noexcept {
    lua_getfield(L, -1, varname);
}

const int lua_interpreter::lua_version {LUA_VERSION_NUM};

lua_interpreter::lua_interpreter()
    : pimpl{std::make_unique<lua_interpreter::impl>()}
{}

void lua_interpreter::openlibs() noexcept {
    return pimpl->openlibs();
}

std::tuple<bool, std::string> lua_interpreter::run_chunk(const char *code) noexcept {
    return pimpl->run_chunk(code);
}

template<>
long lua_interpreter::get_global<types::INT>(const char *varname) {
    return pimpl->get_what_impl<whahaha::GLOBAL, long>(varname, lua_tointegerx, lua_isinteger,
        "integer");
}

template<>
double lua_interpreter::get_global<types::NUM>(const char *varname) {
    return pimpl->get_what_impl<whahaha::GLOBAL, double>(varname, lua_tonumberx, lua_isnumber,
        "number or string convertible to number");
}

template<>
std::string lua_interpreter::get_global<types::STR>(const char *varname) {
    return pimpl->get_what_impl<whahaha::GLOBAL, std::string>(varname, lua_tolstring, lua_isstring,
        "string or number");
}

template<>
table_handle lua_interpreter::get_global<types::TABLE>(const char *varname) {
    pimpl->push_table<whahaha::GLOBAL>(varname);
    return {pimpl};
}

// must push the table on the top of the stack before constructing
table_handle::table_handle(std::shared_ptr<lua_interpreter::impl> wp)
    : wpimpl{std::move(wp)}
{}

// pops the table out of the stack when destructed
table_handle::~table_handle() {
    wpimpl->pop_top_table();
}

template<>
long table_handle::get_field<types::INT>(const char *varname) {
    return wpimpl->get_what_impl<whahaha::TABLE, long>(varname, lua_tointegerx, lua_isinteger,
        "integer");
}

template<>
double table_handle::get_field<types::NUM>(const char *varname) {
    return wpimpl->get_what_impl<whahaha::TABLE, double>(varname, lua_tonumberx, lua_isnumber,
        "number or string convertible to number");
}

template<>
std::string table_handle::get_field<types::STR>(const char *varname) {
    return wpimpl->get_what_impl<whahaha::TABLE, std::string>(varname, lua_tolstring, lua_isstring,
        "string or number");
}

template<>
table_handle table_handle::get_field<types::TABLE>(const char *varname) {
    wpimpl->push_table<whahaha::TABLE>(varname);
    return {wpimpl};
}
