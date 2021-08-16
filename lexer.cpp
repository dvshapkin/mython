#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>
#include <cassert>
#include <cctype>

using namespace std;

namespace parse {

    bool operator==(const Token &lhs, const Token &rhs) {
        using namespace token_type;

        if (lhs.index() != rhs.index()) {
            return false;
        }
        if (lhs.Is<Char>()) {
            return lhs.As<Char>().value == rhs.As<Char>().value;
        }
        if (lhs.Is<Number>()) {
            return lhs.As<Number>().value == rhs.As<Number>().value;
        }
        if (lhs.Is<String>()) {
            return lhs.As<String>().value == rhs.As<String>().value;
        }
        if (lhs.Is<Id>()) {
            return lhs.As<Id>().value == rhs.As<Id>().value;
        }
        return true;
    }

    bool operator!=(const Token &lhs, const Token &rhs) {
        return !(lhs == rhs);
    }

    std::ostream &operator<<(std::ostream &os, const Token &rhs) {
        using namespace token_type;

#define VALUED_OUTPUT(type) \
if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

        VALUED_OUTPUT(Number);
        VALUED_OUTPUT(Id);
        VALUED_OUTPUT(String);
        VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
if (rhs.Is<type>()) return os << #type;

        UNVALUED_OUTPUT(Class);
        UNVALUED_OUTPUT(Return);
        UNVALUED_OUTPUT(If);
        UNVALUED_OUTPUT(Else);
        UNVALUED_OUTPUT(Def);
        UNVALUED_OUTPUT(Newline);
        UNVALUED_OUTPUT(Print);
        UNVALUED_OUTPUT(Indent);
        UNVALUED_OUTPUT(Dedent);
        UNVALUED_OUTPUT(And);
        UNVALUED_OUTPUT(Or);
        UNVALUED_OUTPUT(Not);
        UNVALUED_OUTPUT(Eq);
        UNVALUED_OUTPUT(NotEq);
        UNVALUED_OUTPUT(LessOrEq);
        UNVALUED_OUTPUT(GreaterOrEq);
        UNVALUED_OUTPUT(None);
        UNVALUED_OUTPUT(True);
        UNVALUED_OUTPUT(False);
        UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

        return os << "Unknown token :("sv;
    }

    Lexer::Lexer(std::istream &input)
            : in_(input) {
        NextToken();
    }

    const Token &Lexer::CurrentToken() const {
        assert(!tokens_.empty());
        return tokens_.back();
    }

    Token Lexer::NextToken() {
        while (char ch = static_cast<char>(in_.get())) {
            if (ch == EOF) {
                tokens_.emplace_back(token_type::Eof{});
            } else if (ch == '\r') {
                tokens_.emplace_back(token_type::Return{});
            } else if (ch == '\n') {
                tokens_.emplace_back(token_type::Newline{});
            } else if (ch == '#') {
                SkipComment();
            } else if (ch == ' ') {
                if (CurrentToken().Is<token_type::Newline>()) {
                    ReadIndent();
                } else {
                    SkipSpaces();
                }
            } else if (ch == '"' || ch == '\'') {
                ReadString();
            } else if (isdigit(ch)) {
                ReadNumber();
            } else if (isalpha(ch)) {
                ReadIdentifier();
            }
        }
        return tokens_.back();
    }

    void Lexer::SkipComment() {
        for (; in_.get() != '\n';);
        in_.unget();    // вернем последний считанный символ (\n) в поток
    }

    int Lexer::SkipSpaces() {
        int space_count = 0;
        for (; in_.get() == ' '; ++space_count);
        in_.unget();    // вернем последний считанный символ (не пробельный) в поток
        return space_count;
    }

    void Lexer::ReadIndent() {
        int indent = SkipSpaces() + 1; // один пробел уже считан уровнем выше
        if (indent % 2 != 0) throw LexerError("Bad indent size");
        if (indent > current_indent_) {
            tokens_.emplace_back(token_type::Indent{});
        }
        if (indent < current_indent_) {
            tokens_.emplace_back(token_type::Dedent{});
        }
        current_indent_ = indent;
    }

    void Lexer::ReadString() {
        in_.unget();
        std::string s;
        char ch, prev_ch = '\0';
        const char begin_quote = static_cast<char>(in_.get());
        while ((ch = static_cast<char>(in_.get()))
               && !(ch == begin_quote && prev_ch != '\\')) {
            s += ch;
            prev_ch = ch;
        }
        tokens_.emplace_back(token_type::String{std::move(s)});
    }

    void Lexer::ReadNumber() {
        in_.unget();
        std::string s;
        char ch;
        while ((ch = static_cast<char>(in_.get())) && isdigit(ch)) {
            s += ch;
        }
        in_.unget();    // вернем последний считанный символ (не цифру) в поток
        tokens_.emplace_back(token_type::Number{stoi(s)});
    }

    void Lexer::ReadIdentifier() {

    }

}  // namespace parse
