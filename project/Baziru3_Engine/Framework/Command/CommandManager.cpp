#include "CommandManager.h"

void CommandManager::ExecuteCommand(std::unique_ptr<ICommand> command)
{
    if (!command)
    {
        return;
    }

    // コマンドを実行
    command->Execute();

    // 新しいコマンドを実行したため、Redo履歴は無効化（クリア）
    redoStack_.clear();

    // 履歴スタックが上限に達している場合は最古の履歴を破棄
    if (maxHistory_ > 0 && undoStack_.size() >= maxHistory_)
    {
        undoStack_.erase(undoStack_.begin());
    }

    // Undoスタックに蓄積
    undoStack_.push_back(std::move(command));
}

bool CommandManager::Undo()
{
    if (undoStack_.empty())
    {
        return false;
    }

    // 直前のコマンドを取り出し
    auto command = std::move(undoStack_.back());
    undoStack_.pop_back();

    // Undo処理を実行
    command->Undo();

    // やり直しできるようにRedoスタックへ移動
    redoStack_.push_back(std::move(command));
    return true;
}

bool CommandManager::Redo()
{
    if (redoStack_.empty())
    {
        return false;
    }

    // Redoスタックから取り出し
    auto command = std::move(redoStack_.back());
    redoStack_.pop_back();

    // 再度実行
    command->Execute();

    // Undoスタックへ移動
    undoStack_.push_back(std::move(command));
    return true;
}

void CommandManager::Clear()
{
    undoStack_.clear();
    redoStack_.clear();
}
