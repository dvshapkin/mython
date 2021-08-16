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
        while (in_) {
            int ch = in_.get();
            if (ch == EOF) {
                tokens_.emplace_back(token_type::Eof{});
                break;
            } else if (ch == '\r') {
                tokens_.emplace_back(token_type::Return{});
                break;
            } else if (ch == '\n') {
                tokens_.emplace_back(token_type::Newline{});
                break;
            } else if (ch == '#') {
                SkipComment();
            } else if (ch == ' ') {
                if (CurrentToken().Is<token_type::Newline>()) {
                    ReadIndent();
                    break;
                } else {
                    SkipSpaces();
                }
            } else if (ch == '"' || ch == '\'') {
                ReadString();
                break;
            } else if (isdigit(ch)) {
                ReadNumber();
                break;
            } else if (isalpha(ch) || ch == '_') {
                ReadIdentifier();
                break;
            } else if (ch == '!' || ch == '=' || ch == '<' || ch == '>') {
                if (ReadComparison(ch)) break;
            } else {
                tokens_.emplace_back(token_type::Char{static_cast<char>(ch)});
                break;
            }
        }
        return tokens_.back();
    }

    void Lexer::SkipComment() {
        for (; !(in_.get() == '\n' || in_.eof()););
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
        const int begin_quote = in_.get();
        for (int ch = in_.get(), prev_ch = '\0';
             in_ && !(ch == begin_quote && prev_ch != '\\');
             prev_ch = ch, ch = in_.get()) {
            s += static_cast<char>(ch);
        }
        tokens_.emplace_back(token_type::String{std::move(s)});
    }

    void Lexer::ReadNumber() {
        in_.unget();
        std::string s;
        for (int ch = in_.get(); in_ && isdigit(ch); ch = in_.get()) {
            s += static_cast<char>(ch);
        }
        in_.unget();    // вернем последний считанный символ (не цифру) в поток
        tokens_.emplace_back(token_type::Number{stoi(s)});
    }

    void Lexer::ReadIdentifier() {
        in_.unget();
        std::string s;
        for (int ch = in_.get();
             in_ && (isalpha(ch) || ch == '_' || isdigit(ch));
             ch = in_.get()) {
            s += static_cast<char>(ch);
        }
        in_.unget();    // вернем последний считанный символ в поток

        if (s == "class"sv) {
            tokens_.emplace_back(token_type::Class{});
        } else if (s == "if"sv) {
            tokens_.emplace_back(token_type::If{});
        } else if (s == "else"sv) {
            tokens_.emplace_back(token_type::Else{});
        } else if (s == "def"sv) {
            tokens_.emplace_back(token_type::Def{});
        } else if (s == "print"sv) {
            tokens_.emplace_back(token_type::Print{});
        } else if (s == "and"sv) {
            tokens_.emplace_back(token_type::And{});
        } else if (s == "or"sv) {
            tokens_.emplace_back(token_type::Or{});
        } else if (s == "not"sv) {
            tokens_.emplace_back(token_type::Not{});
        } else if (s == "None"sv) {
            tokens_.emplace_back(token_type::None{});
        } else if (s == "True"sv) {
            tokens_.emplace_back(token_type::True{});
        } else if (s == "False"sv) {
            tokens_.emplace_back(token_type::False{});
        } else {
            tokens_.emplace_back(token_type::Id{std::move(s)});
        }
    }

    bool Lexer::ReadComparison(const char ch) {
        int next = in_.get();
        if (ch == '!') {
            if (next == '=') {
                tokens_.emplace_back(token_type::NotEq{});
                return true;
            } else {
                throw LexerError("! without =");
            }
        } else if (ch == '=' && next == '=') {
            tokens_.emplace_back(token_type::Eq{});
            return true;
        } else if (ch == '<' && next == '=') {
            tokens_.emplace_back(token_type::LessOrEq{});
            return true;
        } else if (ch == '>' && next == '=') {
            tokens_.emplace_back(token_type::GreaterOrEq{});
            return true;
        }
        //in_.unget();
        return false;
    }

}  // namespace parse
