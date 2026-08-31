#pragma once

#include "DebugCamera.h"
#include "DirectXCom.h"
#include "MappedResource.h"
#include "MaterialManager.h"
#include "Matrix4x4.h"
#include "Vector.h"
#include <memory>


class SpriteCom;
class TextureManager;

/**
 * @brief 2Dスプライトの描画・制御を行うクラス
 */
class Sprite {
public:
  struct Transform {
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
  };

  struct VertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
  };

  Sprite();
  ~Sprite();

  /**
   * @brief スプライトの初期化処理を行います
   * @param spriteCom スプライト共通設定オブジェクトのポインタ
   * @param textureFilePath テクスチャファイルのパス
   */
  void Initialize(SpriteCom *spriteCom, const std::string &textureFilePath,
                  TextureManager *textureManager = nullptr);

  /**
   * @brief スプライトの更新（行列計算など）を行います
   */
  void Update();

  /**
   * @brief スプライトの描画コマンドを発行します
   */
  void Draw(ID3D12GraphicsCommandList *commandList = nullptr);

  /**
   * @brief スプライトの終了・リソース解放処理を行います
   */
  void Finalize();

  void CreateIndexBufferView();
  void CreateVertexBufferView();
  void CreateVertexData();
  void CreateIndexData();

  void ReflectionProcessing();

  // インデックスから生成する簡易ヘルパー
  /**
   * @brief スプライトを生成する簡易ヘルパー（シングルトンインスタンスを利用）
   * @param spriteCom スプライト共通設定オブジェクト
   * @param textureHandle テクスチャのSRVインデックス
   * @param position 初期配置位置
   * @return 生成されたスプライトのunique_ptr
   */
  static std::unique_ptr<Sprite>
  Create(SpriteCom *spriteCom, uint32_t textureHandle, const Vector2 &position,
         TextureManager *textureManager = nullptr);

  // 最も推奨される簡易生成関数：テクスチャパスと初期位置（省略可）を指定するだけで生成完了
  // SceneManager から SpriteCom を自動取得する
  // 引数 texturePath: テクスチャファイルパス（例: "Resources/hp_bar.png"）
  // 引数 position: 画面上の初期配置位置（ピクセル、デフォルト: {0.0f, 0.0f}）
  // 戻り値: 生成された Sprite の unique_ptr（失敗時は nullptr）
  static std::unique_ptr<Sprite>
  Create(std::string_view texturePath, const Vector2 &position = {0.0f, 0.0f});

  /**
   * @brief スプライトを生成する簡易ヘルパー（詳細指定オーバーロード）
   * @param spriteCom スプライト共通設定オブジェクト
   * @param transform 初期Transformパラメータ
   * @param texturePath テクスチャファイルのパス
   * @return 生成されたスプライトのunique_ptr
   */
  static std::unique_ptr<Sprite>
  Create(SpriteCom *spriteCom, const Sprite::Transform &transform,
         const std::string &texturePath,
         TextureManager *textureManager = nullptr);

public:
  Microsoft::WRL::ComPtr<ID3D12Resource> GetVertexResourceSprite() const {
    return vertexResourceSprite_;
  }
  const D3D12_VERTEX_BUFFER_VIEW &GetVertexBufferViewSprite() const {
    return vertexBufferViewSprite_;
  }
  const D3D12_INDEX_BUFFER_VIEW &GetIndexBufferViewSprite() const {
    return indexBufferViewSprite_;
  }

  void SetUVTransform(const Matrix4x4 &uv) { materialData_.uvTransform = uv; }
  Material *GetMaterialDataSprite() { return &materialData_; }
  void SetTransformationMatrix(const Matrix4x4 &wvp, const Matrix4x4 &world) {
    transformationMatrixDataSprite_.WVP = wvp;
    transformationMatrixDataSprite_.World = world;
  }

  // Spriteの座標関係
  const Vector2 &GetPosition() const { return position_; }
  void SetPosition(const Vector2 &position) { this->position_ = position; }

  // Spriteの回転関係
  const float GetRotation() const { return rotation_; }
  void SetRotation(const float rotation) { this->rotation_ = rotation; }

  // Spriteの色
  const Vector4 &GetColor() const { return materialData_.color; }
  void SetColor(const Vector4 &color) {
    const_cast<Sprite *>(this)->materialData_.color = color;
  }

  // Spriteの大きさ関係
  const Vector2 &GetSize() const { return size_; }
  void SetSize(const Vector2 &size) { this->size_ = size; }

  const Vector2 &GetAnchorPoint() const { return anchorPoint_; }
  void SetAnchorPoint(const Vector2 &anchorPoint) {
    this->anchorPoint_ = anchorPoint;
  }

  void SetTextureLeftTop(const Vector2 &leftTop) { textureLeftTop_ = leftTop; }

  void SetTextureSize(const Vector2 &size) { textureSize_ = size; }

  void SetTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) {
    textureHandleGPU_ = handle;
  }

  void SetDirectionalLightResource(
      const Microsoft::WRL::ComPtr<ID3D12Resource> &light) {
    directionalLightResource_ = light;
  }

  void SetUVParams(const Vector3 &scale, float rotZ, const Vector3 &translate);

private:
  void AdjustTextureSize();
  void RecalculateUVMatrix();

private:
  Transform transform_{
      {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
  Vector2 position_ = {0.0f, 0.0f};
  float rotation_ = 0.0f;
  Vector2 size_ = {640.0f, 360.0f};
  Vector2 anchorPoint_ = {0.0f, 0.0f};
  bool isFlipX_ = false;
  bool isFlipY_ = false;
  // テクスチャ左上座標
  Vector2 textureLeftTop_ = {0.0f, 0.0f};
  Vector2 textureSize_ = {100.0f, 100.0f};

  Transform uvParams_{
      {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
  bool uvDirty_ = true;

private:
  DirectXCom *dxCommon_ = nullptr;
  SpriteCom *spriteCom_ = nullptr;
  VertexData *vertexData_ = nullptr;
  Material materialData_{};

  Baziru3::PersistentMap<VertexData> vertexMap_;

private:
  Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite_ = nullptr;
  Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite_ = nullptr;
  D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite_{};
  D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite_{};
  uint32_t *indexDataSprite_ = nullptr;
  TransformationMatrix transformationMatrixDataSprite_{};

  // 新規: 描画時に使うハンドル/リソース
  D3D12_GPU_DESCRIPTOR_HANDLE textureHandleGPU_{};
  Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_ = nullptr;

  uint32_t textureIndex_ = 0;
  TextureManager *textureManager_ = nullptr;
};