#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

    using runtime::Closure;
    using runtime::Context;
    using runtime::ObjectHolder;

    namespace {
        const string ADD_METHOD = "__add__"s;
        const string INIT_METHOD = "__init__"s;
    }  // namespace

    VariableValue::VariableValue(const std::string &var_name) {
        dotted_ids_.push_back(var_name);
    }

    VariableValue::VariableValue(std::vector<std::string> dotted_ids)
            : dotted_ids_(std::move(dotted_ids)) {
    }

    ObjectHolder VariableValue::Execute(Closure &closure, Context & /*context*/) {
        ObjectHolder result;
        Closure *p_closure = &closure;
        for (const auto &id : dotted_ids_) {
            if (!p_closure->count(id)) {
                throw runtime_error("Uncknown variable name: " + GetName());
            }
            result = p_closure->at(id);
            if (result.TryAs<runtime::ClassInstance>()) {
                p_closure = &result.TryAs<runtime::ClassInstance>()->Fields();
            }
        }
        return result;
    }

    std::string VariableValue::GetName() const {
        std::string full_name;
        for (const auto &id: dotted_ids_) {
            full_name += full_name.empty() ? id : '.' + id;
        }
        return full_name;
    }

    const std::vector<std::string> &VariableValue::GetDottedIds() const {
        return dotted_ids_;
    }

    ObjectHolder Assignment::Execute(Closure &closure, Context &context) {
        context.SetSelfName(var_name_);
        closure[var_name_] = rv_->Execute(closure, context);
        return closure.at(var_name_);
    }

    Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
            : var_name_(std::move(var)), rv_(std::move(rv)) {
    }

    FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
                                     std::unique_ptr<Statement> rv)
            : object_(std::move(object)), field_name_(std::move(field_name)), rv_(std::move(rv)) {
    }

    ObjectHolder FieldAssignment::Execute(Closure &closure, Context &context) {
        auto dotted_ids = object_.GetDottedIds();
        Closure *p_closure = &closure;
        for (const auto &id: dotted_ids) {
            auto &val = p_closure->at(id);
            p_closure = &val.TryAs<runtime::ClassInstance>()->Fields();
        }
        (*p_closure)[field_name_] = std::move(rv_->Execute(closure, context));
        return p_closure->at(field_name_);
    }

    unique_ptr<Print> Print::Variable(const std::string &name) {
        return make_unique<Print>(Print(make_unique<VariableValue>(VariableValue(name))));
    }

    Print::Print(unique_ptr<Statement> argument) {
        args_.push_back(std::move(argument));
    }

    Print::Print(vector<unique_ptr<Statement>> args) {
        args_ = std::move(args);
    }

    ObjectHolder Print::Execute(Closure &closure, Context &context) {
        auto &out = context.GetOutputStream();
        for (auto it = args_.cbegin(); it != args_.cend(); ++it) {
            auto holder = (*it)->Execute(closure, context);
            if (it != args_.cbegin()) out << ' ';
            if (holder) {
                holder->Print(out, context);
            } else {
                out << "None";
            }
        }
        out << '\n';
        return {};
    }

    MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
                           std::vector<std::unique_ptr<Statement>> args)
            : object_(std::move(object)), method_(std::move(method)), args_(std::move(args)) {
    }

    ObjectHolder MethodCall::Execute(Closure &closure, Context &context) {
        auto holder = object_->Execute(closure, context);
        auto instance = holder.TryAs<runtime::ClassInstance>();
        if (instance && instance->HasMethod(method_, args_.size())) {
            std::vector<ObjectHolder> actual_args;
            for (const auto &stmt: args_) {
                actual_args.push_back(stmt->Execute(closure, context));
            }
            return instance->Call(method_, actual_args, context);
        }
        return {};
    }

    ObjectHolder Stringify::Execute(Closure &closure, Context &context) {
        auto holder = argument_->Execute(closure, context);
        if (holder) {
            std::stringstream ss;
            holder->Print(ss, context);
            return ObjectHolder::Own(runtime::String(ss.str()));
        } else {
            return ObjectHolder::Own(runtime::String("None"));
        }
    }

    ObjectHolder Add::Execute(Closure &closure, Context &context) {
        auto lhs_holder = lhs_->Execute(closure, context);
        auto rhs_holder = rhs_->Execute(closure, context);
        if (lhs_holder && rhs_holder) {
            // Number
            auto lhs_as_num = lhs_holder.TryAs<runtime::Number>();
            auto rhs_as_num = rhs_holder.TryAs<runtime::Number>();
            if (lhs_as_num && rhs_as_num) {
                return ObjectHolder::Own(runtime::Number{lhs_as_num->GetValue() + rhs_as_num->GetValue()});
            }
            // String
            auto lhs_as_str = lhs_holder.TryAs<runtime::String>();
            auto rhs_as_str = rhs_holder.TryAs<runtime::String>();
            if (lhs_as_str && rhs_as_str) {
                return ObjectHolder::Own(runtime::String{lhs_as_str->GetValue() + rhs_as_str->GetValue()});
            }
            // ClassInstance
            auto lhs_as_obj = lhs_holder.TryAs<runtime::ClassInstance>();
            if (lhs_as_obj && lhs_as_obj->HasMethod(ADD_METHOD, 1)) {
                return lhs_as_obj->Call(ADD_METHOD, {rhs_holder}, context);
            }
        }
        throw runtime_error("Invalid arguments in Add");
    }

    ObjectHolder Sub::Execute(Closure &closure, Context &context) {
        auto lhs_holder = lhs_->Execute(closure, context);
        auto rhs_holder = rhs_->Execute(closure, context);
        if (lhs_holder && rhs_holder) {
            auto lhs_as_num = lhs_holder.TryAs<runtime::Number>();
            auto rhs_as_num = rhs_holder.TryAs<runtime::Number>();
            if (lhs_as_num && rhs_as_num) {
                return ObjectHolder::Own(runtime::Number{lhs_as_num->GetValue() - rhs_as_num->GetValue()});
            }
        }
        throw runtime_error("Invalid arguments in Sub");
    }

    ObjectHolder Mult::Execute(Closure &closure, Context &context) {
        auto lhs_holder = lhs_->Execute(closure, context);
        auto rhs_holder = rhs_->Execute(closure, context);
        if (lhs_holder && rhs_holder) {
            auto lhs_as_num = lhs_holder.TryAs<runtime::Number>();
            auto rhs_as_num = rhs_holder.TryAs<runtime::Number>();
            if (lhs_as_num && rhs_as_num) {
                return ObjectHolder::Own(runtime::Number{lhs_as_num->GetValue() * rhs_as_num->GetValue()});
            }
        }
        throw runtime_error("Invalid arguments in Mul");
    }

    ObjectHolder Div::Execute(Closure &closure, Context &context) {
        auto lhs_holder = lhs_->Execute(closure, context);
        auto rhs_holder = rhs_->Execute(closure, context);
        if (lhs_holder && rhs_holder) {
            auto lhs_as_num = lhs_holder.TryAs<runtime::Number>();
            auto rhs_as_num = rhs_holder.TryAs<runtime::Number>();
            if (lhs_as_num && rhs_as_num) {
                return ObjectHolder::Own(runtime::Number{lhs_as_num->GetValue() / rhs_as_num->GetValue()});
            }
        }
        throw runtime_error("Invalid arguments in Div");
    }

    ObjectHolder Compound::Execute(Closure &closure, Context &context) {
        for (const auto &stmt: args_) {
            stmt->Execute(closure, context);
        }
        return ObjectHolder::None();
    }

    ObjectHolder Return::Execute(Closure &closure, Context &context) {
        auto holder = statement_->Execute(closure, context);
        throw ReturnException(std::move(holder));
    }

    ClassDefinition::ClassDefinition(ObjectHolder cls)
            : cls_(std::move(cls)) {
    }

    ObjectHolder ClassDefinition::Execute(Closure &closure, Context &) {
        closure[cls_.TryAs<runtime::Class>()->GetName()] = cls_;
        return cls_;
    }

    IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
                   std::unique_ptr<Statement> else_body)
            : condition_(std::move(condition)), if_body_(std::move(if_body)), else_body_(std::move(else_body)) {
    }

    ObjectHolder IfElse::Execute(Closure &closure, Context &context) {
        auto cond_holder = condition_->Execute(closure, context);
        if (runtime::IsTrue(cond_holder)) {
            return if_body_->Execute(closure, context);
        } else {
            if (else_body_) {
                return else_body_->Execute(closure, context);
            }
        }
        return ObjectHolder::None();
    }

    ObjectHolder Or::Execute(Closure &closure, Context &context) {
        auto lhs_holder = lhs_->Execute(closure, context);
        auto rhs_holder = rhs_->Execute(closure, context);
        if (lhs_holder && rhs_holder) {
            return ObjectHolder::Own(runtime::Bool{IsTrue(lhs_holder) || IsTrue(rhs_holder)});
        }
        throw runtime_error("Invalid arguments in Or");
    }

    ObjectHolder And::Execute(Closure &closure, Context &context) {
        auto lhs_holder = lhs_->Execute(closure, context);
        auto rhs_holder = rhs_->Execute(closure, context);
        if (lhs_holder && rhs_holder) {
            return ObjectHolder::Own(runtime::Bool{IsTrue(lhs_holder) && IsTrue(rhs_holder)});
        }
        throw runtime_error("Invalid arguments in And");
    }

    ObjectHolder Not::Execute(Closure &closure, Context &context) {
        auto holder = argument_->Execute(closure, context);
        if (holder) {
            return ObjectHolder::Own(runtime::Bool{!IsTrue(holder)});
        }
        throw runtime_error("Invalid arguments in Not");
    }

    Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
            : BinaryOperation(std::move(lhs), std::move(rhs)), cmp_(std::move(cmp)) {
    }

    ObjectHolder Comparison::Execute(Closure &closure, Context &context) {
        auto lhs_holder = lhs_->Execute(closure, context);
        auto rhs_holder = rhs_->Execute(closure, context);
        return ObjectHolder::Own(runtime::Bool{cmp_(lhs_holder, rhs_holder, context)});
    }

    NewInstance::NewInstance(const runtime::Class &cls, std::vector<std::unique_ptr<Statement>> args)
            : class_(cls), args_(std::move(args)) {
    }

    NewInstance::NewInstance(const runtime::Class &cls)
            : class_(cls) {
    }

    ObjectHolder NewInstance::Execute(Closure &closure, Context &context) {
        std::string self_name = context.GetSelfName();
        {
            runtime::ClassInstance inst{class_};
            closure[self_name] = ObjectHolder::Own(std::move(inst));
        }
        auto *instance = const_cast<runtime::ClassInstance *>(closure.at(self_name).TryAs<runtime::ClassInstance>());
        if (instance->HasMethod(INIT_METHOD, args_.size())) {
            std::vector<ObjectHolder> actual_args;
            for (const auto &stmt: args_) {
                actual_args.push_back(stmt->Execute(closure, context));
            }
            instance->Call(INIT_METHOD, actual_args, context);
        }
        return closure.at(self_name);
    }

    MethodBody::MethodBody(std::unique_ptr<Statement> body)
            : body_(std::move(body)) {
    }

    ObjectHolder MethodBody::Execute(Closure &closure, Context &context) {
        try {
            return body_->Execute(closure, context);
        } catch (ReturnException &ex) {
            return ex.GetResult();
        }
    }

    ReturnException::ReturnException(runtime::ObjectHolder result)
            : result_(std::move(result)) {
    }

    runtime::ObjectHolder ReturnException::GetResult() {
        return result_;
    }
}  // namespace ast
