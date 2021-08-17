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
            if (tokens_.empty() || CurrentToken().Is<token_type::Newline>()) {
                if (ReadIndent()) break;
            }
            if (ch == ' ') {
//                if (CurrentToken().Is<token_type::Newline>()) {
//                    if (ReadIndent()) break;
//                } else {
                SkipSpaces();
//                }
                continue;
            } else if (ch == '\n') {
                if (in_.peek() == ' ' || in_.eof()) {
                    tokens_.emplace_back(token_type::Newline{});
                    break;
                }
                continue;
            }
            if (ch == '#') {
                SkipComment();
                continue;
            }
            if (ch == EOF) {
                tokens_.emplace_back(token_type::Eof{});
                break;
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
                ReadComparison(ch);
                break;
            } else {
                tokens_.emplace_back(token_type::Char{static_cast<char>(ch)});
                break;
            }
        }
        return tokens_.back();
    }


    std::vector<Token> Lexer::TextLine::ReadLine(std::istream &in) {
        indent_ = SkipSpaces(in);
        int ch;
        do {
            ch = in.get();
            if (ch == ' ') {
                SkipSpaces(in);
            } else if (ch == '#') {
                SkipComment(in);
            } else if (ch == '\n') {
                tokens_.emplace_back(token_type::Newline{});
            } else if (ch == EOF) {
                tokens_.emplace_back(token_type::Eof{});
            } else if (ch == '"' || ch == '\'') {
                ReadString(in, ch);
            }
        } while (!(ch == '\n' || ch == EOF));
        return tokens_;
    }

    int Lexer::TextLine::SkipSpaces(std::istream &in) {
        int space_count = 0;
        for (; in.get() == ' '; ++space_count);
        in.unget();    // вернем последний считанный символ (не пробельный) в поток
        return space_count;
    }

    void Lexer::TextLine::SkipComment(std::istream &in) {
        for (; !(in.get() == '\n' || in.eof()););
        in.unget();    // вернем последний считанный символ (\n) в поток
    }

    void Lexer::TextLine::ReadString(std::istream &in, const int begin_quote) {
        std::string s;
        for (int ch = in.get(), prev_ch = '\0';
             in && !(ch == begin_quote && prev_ch != '\\');
             prev_ch = ch, ch = in.get()) {
            s += static_cast<char>(ch);
        }
        tokens_.emplace_back(token_type::String{std::move(s)});
    }

//    bool Lexer::TextLine::ReadIndent(std::istream &in) {
//        int indent = SkipSpaces() + 1; // один пробел уже считан уровнем выше
//        if (indent % 2 != 0) throw LexerError("Bad indent size");
//        if (indent > current_indent_) {
//            for (int i=0; i < (indent - current_indent_) / 2; ++i) {
//                tokens_.emplace_back(token_type::Indent{});
//            }
//            current_indent_ = indent;
//            return true;
//        }
//        if (indent < current_indent_) {
//            for (int i=0; i < (current_indent_ - indent) / 2; ++i) {
//                tokens_.emplace_back(token_type::Dedent{});
//            }
//            current_indent_ = indent;
//            return true;
//        }
//        return false;
//    }



























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
        } else if (s == "return"sv) {
            tokens_.emplace_back(token_type::Return{});
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

    void Lexer::ReadComparison(const int ch) {
        int next = in_.get();
        if (ch == '!') {
            if (next == '=') {
                tokens_.emplace_back(token_type::NotEq{});
            } else {
                in_.unget();
                throw LexerError("! without =");
            }
        } else if (ch == '=' && next == '=') {
            tokens_.emplace_back(token_type::Eq{});
        } else if (ch == '<' && next == '=') {
            tokens_.emplace_back(token_type::LessOrEq{});
        } else if (ch == '>' && next == '=') {
            tokens_.emplace_back(token_type::GreaterOrEq{});
        } else {
            tokens_.emplace_back(token_type::Char{static_cast<char>(ch)});
            in_.unget();
        }
    }

}  // namespace parse
