#pragma once
#include <string>
#include <functional>

// 操作の実行・取り消し（Undo/Redo）をカプセル化するコマンドインターフェース
class ICommand
{
public:
    virtual ~ICommand() = default;

    // コマンドを実行
    virtual void Execute() = 0;

    // コマンドを取り消す（Undo対応の場合に実装）
    virtual void Undo() {}

    // デバッグ・ログ用コマンド名
    virtual std::string GetName() const { return "Command"; }
};

// ラムダ式から手軽にコマンドを作れるヘルパークラス
class LambdaCommand : public ICommand
{
public:
    LambdaCommand(std::function<void()> onExecute, std::function<void()> onUndo = nullptr, const std::string& name = "LambdaCommand")
        : onExecute_(std::move(onExecute)), onUndo_(std::move(onUndo)), name_(name)
    {
    }

    void Execute() override
    {
        if (onExecute_) onExecute_();
    }

    void Undo() override
    {
        if (onUndo_) onUndo_();
    }

    std::string GetName() const override { return name_; }

private:
    std::function<void()> onExecute_;
    std::function<void()> onUndo_;
    std::string name_;
};
