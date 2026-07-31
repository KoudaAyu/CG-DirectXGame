// CombatSystem.cpp
// GE3_Game ブランチでは使用しない（エンジン基盤確認用ブランチのため）
// ファイルは将来の参照用として保持。実装は GamePlayScene の刷新に伴い無効化。
#include "CombatSystem.h"
#include "GamePlayScene.h"

CombatSystem::CombatSystem(GamePlayScene* scene) : scene_(scene) {}

void CombatSystem::Update(float /*deltaTime*/) {}
void CombatSystem::UpdateBullets(float /*deltaTime*/) {}
void CombatSystem::RemoveDeadBullets() {}
void CombatSystem::AddBullet(std::unique_ptr<Bullet> /*bullet*/) {}
