#pragma once
#include "ICommand.h"
#include <memory>
#include <vector>

// コマンドの実行履歴とUndo/Redoを管理するクラス
class CommandManager
{
public:
    CommandManager() = default;
    ~CommandManager() = default;

    // コマンドを実行して履歴（Undoスタック）に追加
    void ExecuteCommand(std::unique_ptr<ICommand> command);

    // 直前の操作を取り消す
    bool Undo();

    // 取り消した操作をやり直す
    bool Redo();

    bool CanUndo() const { return !undoStack_.empty(); }
    bool CanRedo() const { return !redoStack_.empty(); }

    // 履歴をクリア
    void Clear();

    // 保持する最大履歴数の設定
    void SetMaxHistory(size_t maxHistory) { maxHistory_ = maxHistory; }

    size_t GetUndoCount() const { return undoStack_.size(); }
    size_t GetRedoCount() const { return redoStack_.size(); }

private:
    std::vector<std::unique_ptr<ICommand>> undoStack_;
    std::vector<std::unique_ptr<ICommand>> redoStack_;
    size_t maxHistory_ = 100;
};
