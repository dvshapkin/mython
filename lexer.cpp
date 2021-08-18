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
        return tokens_.at(curr_pos_);
    }

    Token Lexer::NextToken() {
        if (static_cast<size_t>(++curr_pos_) == tokens_.size()) {
            if (!tokens_.empty() && tokens_.back().Is<token_type::Eof>()) {
                --curr_pos_;
                return tokens_.back();
            }
            TextLine line;
            for (line = TextLine::ReadLine(in_);
                 line.IsEmpty();
                 line = TextLine::ReadLine(in_));
            if (line.indent_ % 2 != 0) throw LexerError("Bad indent size.");
            if (!line.IsEofOnly() && line.indent_ > current_indent_) {
                for (int i = 0; i < (line.indent_ - current_indent_) / 2; ++i) {
                    tokens_.emplace_back(token_type::Indent{});
                }
                current_indent_ = line.indent_;
            }
            if (!line.IsEofOnly() && line.indent_ < current_indent_) {
                for (int i = 0; i < (current_indent_ - line.indent_) / 2; ++i) {
                    tokens_.emplace_back(token_type::Dedent{});
                }
                current_indent_ = line.indent_;
            }
            if (line.IsEofOnly() && current_indent_ > 0) {
                for (int i = 0; i < current_indent_ / 2; ++i) {
                    tokens_.emplace_back(token_type::Dedent{});
                }
                current_indent_ = line.indent_;
            }
            for (const auto &t: line.tokens_) {
                tokens_.emplace_back(t);
            }
        }
        return tokens_.at(curr_pos_);
    }


    Lexer::TextLine Lexer::TextLine::ReadLine(std::istream &in) {
        TextLine line;
        line.indent_ = SkipSpaces(in);

        int ch;
        do {
            ch = in.get();
            if (ch == ' ') {
                SkipSpaces(in);
            } else if (ch == '#') {
                SkipComment(in);
            } else if (ch == '\n') {
                line.tokens_.emplace_back(token_type::Newline{});
            } else if (ch == EOF) {
                if (!line.IsEmpty() && !line.tokens_.back().Is<token_type::Newline>()) {
                    line.tokens_.emplace_back(token_type::Newline{});
                }
                line.tokens_.emplace_back(token_type::Eof{});
            } else if (ch == '"' || ch == '\'') {
                line.ReadString(in, ch);
            } else if (isdigit(ch)) {
                line.ReadNumber(in, ch);
            } else if (isalpha(ch) || ch == '_') {
                line.ReadIdentifier(in, ch);
            } else if ((ch == '!' || ch == '=' || ch == '<' || ch == '>') && in.peek() == '=') {
                line.ReadComparison(in, ch);
            } else {
                line.tokens_.emplace_back(token_type::Char{static_cast<char>(ch)});
            }
        } while (!(ch == '\n' || ch == EOF));

        return line;
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

        auto it = std::istreambuf_iterator<char>(in);
        auto end = std::istreambuf_iterator<char>();
        std::string s;
        while (true) {
            if (it == end) {
                throw LexerError("String parsing error");
            }
            const char ch = *it;
            if (ch == begin_quote) {
                ++it;
                break;
            } else if (ch == '\\') {
                ++it;
                if (it == end) {
                    throw LexerError("String parsing error");
                }
                const char escaped_char = *(it);
                switch (escaped_char) {
                    case 'n':
                        s.push_back('\n');
                        break;
                    case 't':
                        s.push_back('\t');
                        break;
                    case 'r':
                        s.push_back('\r');
                        break;
                    case '"':
                        s.push_back('"');
                        break;
                    case '\'':
                        s.push_back('\'');
                        break;
                    case '\\':
                        s.push_back('\\');
                        break;
                    default:
                        throw LexerError("Unrecognized escape sequence \\"s + escaped_char);
                }
            } else if (ch == '\n' || ch == '\r') {
                throw LexerError("Unexpected end of line"s);
            } else {
                s.push_back(ch);
            }
            ++it;
        }

        tokens_.emplace_back(token_type::String{std::move(s)});
    }

    void Lexer::TextLine::ReadNumber(std::istream &in, const int first_dig) {
        std::string s{static_cast<char>(first_dig)};
        for (int ch = in.get(); in && isdigit(ch); ch = in.get()) {
            s += static_cast<char>(ch);
        }
        in.unget();    // вернем последний считанный символ (не цифру) в поток
        tokens_.emplace_back(token_type::Number{stoi(s)});
    }

    void Lexer::TextLine::ReadIdentifier(std::istream &in, const int first_sym) {
        std::string s{static_cast<char>(first_sym)};
        for (int ch = in.get();
             in && (isalpha(ch) || ch == '_' || isdigit(ch));
             ch = in.get()) {
            s += static_cast<char>(ch);
        }
        in.unget();    // вернем последний считанный символ в поток

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

    void Lexer::TextLine::ReadComparison(std::istream &in, const int ch) {
        if (ch == '!') {
            tokens_.emplace_back(token_type::NotEq{});
        } else if (ch == '=') {
            tokens_.emplace_back(token_type::Eq{});
        } else if (ch == '<') {
            tokens_.emplace_back(token_type::LessOrEq{});
        } else if (ch == '>') {
            tokens_.emplace_back(token_type::GreaterOrEq{});
        }
        in.get(); // считаем второй символ
    }

    bool Lexer::TextLine::IsEmpty() const {
        return tokens_.empty()
               || std::all_of(tokens_.cbegin(), tokens_.cend(), [](const auto &t) {
            return t.template Is<token_type::Newline>();
        });
    }

    bool Lexer::TextLine::IsEofOnly() const {
        return tokens_.empty()
               || std::all_of(tokens_.cbegin(), tokens_.cend(), [](const auto &t) {
            return t.template Is<token_type::Eof>();
        });
    }

}  // namespace parse
