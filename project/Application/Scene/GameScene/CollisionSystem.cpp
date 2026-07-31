// CollisionSystem.cpp
// GE3_Game ブランチでは使用しない（エンジン基盤確認用ブランチのため）
// ファイルは将来の参照用として保持。実装は GamePlayScene の刷新に伴い無効化。
#include "CollisionSystem.h"
#include "GamePlayScene.h"

CollisionSystem::CollisionSystem(GamePlayScene* scene) : scene_(scene) {}

void CollisionSystem::Update() {}
void CollisionSystem::ResolveBulletCollisions() {}
void CollisionSystem::ResolveObstacleCollisions() {}
void CollisionSystem::ResolveContactDamage() {}
